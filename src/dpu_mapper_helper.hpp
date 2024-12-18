#ifndef DPU_MAPPER_HELPER_HPP
#define DPU_MAPPER_HELPER_HPP

#include <array>
#include <cstddef>

#include "read.hpp"

ssize_t adjust_seed_search_range(size_t read_size, const std::array<ssize_t, 2> &round_shift, ssize_t range, ssize_t delta);
ssize_t min_query_size(ssize_t range, ssize_t delta);
ssize_t find_good_pos(const Read &query, ssize_t range, size_t shift);
ssize_t get_round_shift(ssize_t shift, size_t read_size, ssize_t min_size);

#endif // DPU_MAPPER_HELPER_HPP#include <stddef.h>
