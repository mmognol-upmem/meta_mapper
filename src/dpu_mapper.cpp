#include <cstdio>

#include "dpu_mapper.hpp"
#include "pim_common.hpp"
#include "read_mapper.hpp"

size_t initialize_dpus(PimRankSet<> &pim_rankset, DpuProfile dpu_profile, std::string &binary_name)
{
    pim_rankset.initialize(dpu_profile, std::string(binary_name) + "/short_read_mapping");
    auto nb_dpu = pim_rankset.get_nb_dpu();
    printf("Using %zu real PIM hardware ranks (%zu DPUs)\n", pim_rankset.get_nb_ranks(), nb_dpu);
    return nb_dpu;
}

void build_index(PimRankSet<> &pim_rankset, CompactReference &reference, size_t dpu_ref_size, ssize_t nb_ranks, size_t nb_dpu, ssize_t overlap)
{
    uint64_t seq_size = dpu_ref_size;

    std::vector<uint64_t> dpu_start_pos;
    dpu_start_pos.reserve(nb_dpu);
    size_t start_pos = 0;
    for (PimRankID rank_id = 0; rank_id < nb_ranks; ++rank_id)
    {
        size_t nb_dpu_in_rank = pim_rankset.get_nb_dpu_in_rank(rank_id);
        std::vector<uint8_t *> buffers(nb_dpu_in_rank);
        for (size_t dpu_id = 0; dpu_id < nb_dpu_in_rank; ++dpu_id)
        {
            buffers[dpu_id] = reference.seq.data(start_pos);
            dpu_start_pos.push_back(start_pos);
            start_pos = start_pos + dpu_ref_size - overlap; // Remains a multiple of 4
        }
        pim_rankset.send_data_to_rank_async<uint8_t>(rank_id, "sequence", 0, buffers,
                                                     CEILN<8>((dpu_ref_size >> 2) * sizeof(uint8_t)));
        pim_rankset.broadcast_to_rank_async(rank_id, "index_args", 0, &seq_size, sizeof(seq_size));
        pim_rankset.launch_rank_async(rank_id);
    }
}

void wait_dpus_done(PimRankSet<> &pim_rankset) { pim_rankset.wait_all_ranks_done(); }

void print_checksum(PimRankSet<> &pim_rankset)
{
    size_t dpu_id = 0;
    for (PimRankID rank_id = 0; rank_id < pim_rankset.get_nb_ranks(); rank_id++)
    {
        auto values = pim_rankset.get_data_from_rank_sync<uint64_t>(rank_id, "checksum", 0, sizeof(uint64_t));
        for (auto val : values)
        {
            printf("Checksum from dpu %zu is %lu\n", dpu_id, val);
            dpu_id++;
        }
    }
}

void post_process_mapping(std::vector<MapResults> rank_map_results, std::vector<MapAllArgs> *args,
                          size_t dpu_id, MappingWorkerData *mapping_data)
{
    mapping_data->mutex.lock();
    for (size_t k = 0; k < args->size(); ++k)
    {
        auto &map_results = rank_map_results[k];
        auto n = (*args)[k].dpu_args.nb_queries;
        auto &ids = (*args)[k].identifiers;
        for (size_t i = 0; i < n; ++i)
        {
            auto query_id = ids.data[i];
            auto distance = DECODE_MAP_RESULT_DISTANCE(map_results.data[i]);
            auto &data = mapping_data->result[query_id];
            auto current_distance = data.distance;
            if (current_distance == std::numeric_limits<Mapping::distance_t>::max())
                current_distance = MAX_NB_ERRORS + 1;
            if (distance < current_distance)
            {
                data.distance = distance;
                data.position = DECODE_MAP_RESULT_POSITION(map_results.data[i]) + mapping_data->positions[dpu_id];
                data.error_positions = map_results.error_positions[i];
                data.read_size = ids.read_sizes[i];
                mapping_data->stats.update(distance, current_distance);
            }
        }
        ++dpu_id;
    }
    mapping_data->mutex.unlock();
    mapping_data->allocator.release(args); // Give the buffer back as available
}

void launch_mapping(PimRankSet<> &pim_rankset, PimRankID rank_id, std::vector<MapAllArgs> *args,
                    MappingWorkerData *mapping_data)
{
    pim_rankset.lock_rank(rank_id);
    pim_rankset.send_data_to_rank_async<MapAllArgs, MapArgs>(rank_id, "map_args", 0, *args, sizeof(MapArgs));
    pim_rankset.launch_rank_async(rank_id);

    pim_rankset.add_callback_async(rank_id, [&pim_rankset, args, rank_id, mapping_data]()
                                   { mapping_data->pool.push_task(
                                         post_process_mapping,
                                         pim_rankset.get_data_from_rank_sync<MapResults>(rank_id, "map_results", 0, sizeof(MapResults)), args,
                                         pim_rankset.get_rank_start_dpu_id(rank_id), mapping_data); });
    pim_rankset.unlock_rank(rank_id);
}