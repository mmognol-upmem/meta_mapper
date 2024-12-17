#ifndef DPUMAPPER_HPP
#define DPUMAPPER_HPP

#include "pim_rankset.hpp"
#include "read.hpp"

void build_index(PimRankSet<> &pim_rankset, CompactReference &reference, size_t dpu_ref_size, ssize_t nb_ranks, size_t nb_dpu, ssize_t overlap, IndexArgs &index_args);
ssize_t initialize_dpus(PimRankSet<> &pim_rankset, DpuProfile dpu_profile, std::string &binary_name);

#endif // DPUMAPPER_HPP