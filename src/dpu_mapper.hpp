#ifndef DPUMAPPER_HPP
#define DPUMAPPER_HPP

#include "pim_rankset.hpp"
#include "read.hpp"

class DpuMapper
{
public:
    DpuMapper(const std::string &reference_path, ssize_t nb_ranks, bool create_bf);

    void map(const std::string &queries_path, const std::string &output_path);

private:
    void build_index();

    CompactReference m_reference;
    PimRankSet<> m_rankset;
    ssize_t m_overlap{};
    ssize_t m_dpu_ref_size{};
    IndexArgs index_args{};

    struct
    {
        size_t range{15};
        size_t delta{1};
    } m_seed_search{};
};

#endif // DPUMAPPER_HPP