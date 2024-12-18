#ifndef DPUMAPPER_HPP
#define DPUMAPPER_HPP

#include "pim_rankset.hpp"
#include "read.hpp"
#include "read_mapper.hpp"

class DpuMapper
{
public:
    DpuMapper(const std::string &reference_path, ssize_t nb_ranks, bool create_bf);

    void map(const std::string &queries_path, const std::string &output_path);

private:
    void build_index();
    size_t dispatch_query(const Read &query, ssize_t shift, MappingWorkerData &mapping_data);
    void dispatch_query_to_dpu(size_t dpu_id, const Read &query, size_t start_pos, MappingWorkerData &mapping_data);
    void launch_mapping(PimRankID rank_id, std::vector<MapAllArgs> *args, MappingWorkerData *mapping_data);

    CompactReference m_reference;
    PimRankSet<> m_rankset;
    ssize_t m_overlap{};
    ssize_t m_dpu_ref_size{};
    ssize_t m_min_query_size{};
    ssize_t m_max_query_size{};
    IndexArgs index_args{};
    std::vector<size_t> m_dpu_start_pos;

    MultiBloomFilter m_bloom_filters;

    struct
    {
        ssize_t range{15};
        ssize_t delta{1};
    } m_seed_search{};
};

#endif // DPUMAPPER_HPP