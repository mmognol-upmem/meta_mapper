#include <cstdio>

#include "dpu_mapper.hpp"
#include "file_utils.hpp"
#include "read_mapper.hpp"

#include "dpu_mapper_helper.hpp"

ssize_t compute_dpu_reference_size(size_t reference_size, ssize_t nb_dpu, ssize_t overlap)
{
    ssize_t dpu_ref_size = static_cast<ssize_t>(reference_size);
    dpu_ref_size = dpu_ref_size - ((nb_dpu + 1) * overlap);
    dpu_ref_size = dpu_ref_size / nb_dpu + (2 * overlap);
    dpu_ref_size = CEILN<4>(dpu_ref_size); // Align on 4 for the read packing
    if (dpu_ref_size > 16'000'000)
        exit(printf("Reference sequence is too long, max 16'000'000, current: %ld\n", dpu_ref_size));

    return dpu_ref_size;
}

MultiBloomFilter load_bloom_filter(const std::string &reference_file, ssize_t nb_dpu, ssize_t hash_size)
{
    printf("Loading bloom filter\n");

    auto bloom_file = generate_bloom_file_path(reference_file, nb_dpu, hash_size);
    validate_file(bloom_file);
    MultiBloomFilter bf{};
    bf.load_from_file(bloom_file);

    printf("Bloom filter size: %lu\n", bf.get_size2());
    return bf;
}

MultiBloomFilter get_bloom_filter(const std::string &reference_file, const CompactReference &reference, ssize_t nb_dpu, bool create_bf)
{
    if (create_bf)
    {
        printf("Building bloom filter\n");
        auto bf = build_bloom_filters(reference, nb_dpu, compute_dpu_reference_size(reference.seq.size(), nb_dpu, 400), 400);
        bf.save_to_file(generate_bloom_file_path(reference_file, nb_dpu, HASH_SIZE));
        printf("Bloom filter size: %lu\n", bf.get_size2());
        return bf;
    }
    else
        return load_bloom_filter(reference_file, nb_dpu, HASH_SIZE);
}

size_t dispatch_query(const Read &query, size_t shift, MappingWorkerData &mapping_data)
{
    size_t nb_dispatches = 0;
    auto start_pos = find_good_pos(query, get_round_shift(round_idx, query.seq.size()));
    if constexpr (DISPATCH_TO_ALL)
    {
        for (size_t dpu_id = 0; dpu_id < _nb_dpu; ++dpu_id)
        {
            dispatch_query_to_dpu(dpu_id, query, start_pos, mapping_data);
            ++nb_dispatches;
        }
    }
    else
    {
        auto start_pos2 = find_good_pos(query, start_pos + m_seed_search.delta);
        auto signatures = Signature<HASH_SIZE>::hash(query, start_pos, start_pos2);
        for (auto dpu_id : _bloom_filters.contains(signatures.first, signatures.second))
        {
            dispatch_query_to_dpu(dpu_id, query, start_pos, mapping_data);
            ++nb_dispatches;
        }
    }

    return nb_dispatches;
}

/* -------------------------------------------------------------------------- */
/*                               DpuMapper implem                             */
/* -------------------------------------------------------------------------- */

DpuMapper::DpuMapper(const std::string &reference_path, ssize_t nb_ranks, bool create_bf) : m_rankset(nb_ranks, nb_ranks)
{
    m_overlap = 400;
    m_rankset.initialize(DpuProfile{}, "./dpu/short_read_mapping");
    printf("Using %zu real PIM hardware ranks (%zu DPUs)\n", m_rankset.nb_ranks(), m_rankset.nb_dpu());

    printf("Loading reference\n");
    graal::Bank reference_bank(reference_path);
    m_reference = load_and_compress_reference(reference_bank, nb_ranks);

    m_dpu_ref_size = compute_dpu_reference_size(m_reference.seq.size(), m_rankset.nb_dpu(), m_overlap);

    MultiBloomFilter bf = get_bloom_filter(reference_path, m_reference, m_rankset.nb_dpu(), create_bf);

    printf("Building index\n");
    build_index();
    m_rankset.wait_all_ranks_done();
    printf("Index built\n");
}

void DpuMapper::build_index()
{
    index_args.seq_size = m_dpu_ref_size;

    std::vector<uint64_t> dpu_start_pos;
    dpu_start_pos.reserve(m_rankset.nb_dpu());
    size_t start_pos = 0;
    for (PimRankID rank_id = 0; rank_id < m_rankset.nb_ranks(); ++rank_id)
    {
        size_t nb_dpu_in_rank = m_rankset.nb_dpu_in_rank(rank_id);
        std::vector<uint8_t *> buffers(nb_dpu_in_rank);
        for (size_t dpu_id = 0; dpu_id < nb_dpu_in_rank; ++dpu_id)
        {
            buffers[dpu_id] = m_reference.seq.data(start_pos);
            dpu_start_pos.push_back(start_pos);
            start_pos = start_pos + m_dpu_ref_size - m_overlap; // Remains a multiple of 4
        }
        m_rankset.send_data_to_rank_async<uint8_t>(rank_id, "sequence", 0, buffers,
                                                   CEILN<8>((m_dpu_ref_size >> 2) * sizeof(uint8_t)));
        m_rankset.broadcast_to_rank_async(rank_id, "index_args", 0, &index_args, sizeof(index_args));
        m_rankset.launch_rank_async(rank_id);
    }
}

void DpuMapper::map(const std::string &queries_path, const std::string &output_path)
{
    const std::array<size_t, 2> &round_shift = {5, 15};

    graal::BankFastaMMap<false> queries_bank(queries_path);
    auto estimation = queries_bank.estimate();
    // Retrieve query size (NB: we assume this is the same for all queries)
    m_seed_search.range = adjust_seed_search_range(estimation.size_mean, round_shift, m_seed_search.range, m_seed_search.delta);
    // check_read_size(estimation.size_min, estimation.size_max);

    // Get estimated number of queries to reserve memory
    size_t estimated_nb_queries = estimation.sequences_number;

    std::vector<Mapping> Results;

    printf("Estimation: the bank contains around %lu queries (%lu-(%lu)-%lu bp)\n", estimated_nb_queries,
           estimation.size_min, estimation.size_mean, estimation.size_max);

    auto it_begin = queries_bank.begin();
    const auto it_end = queries_bank.end();

    constexpr auto QUERIES_BATCH_SIZE = (1 << 17);
    std::vector<Read> reads;
    reads.reserve(QUERIES_BATCH_SIZE);
    size_t nb_dispatches_r1 = 0;

    ssize_t query_id = 0;
    while (it_begin != it_end)
    {
        const auto &query = reads.emplace_back(*it_begin, query_id);
        if (query.seq.size() >= min_query_size(m_seed_search.range, m_seed_search.delta))
        {
            nb_dispatches_r1 += dispatch_query(query, round_shift[0], &Results);
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

void launch_mapping(PimRankSet<> &pim_rankset, PimRankID rank_id, std::vector<MapAllArgs> *args, MappingWorkerData *mapping_data)
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