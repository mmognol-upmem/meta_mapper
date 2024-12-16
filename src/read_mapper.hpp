#ifndef READ_MAPPER_HPP
#define READ_MAPPER_HPP

#include "read.hpp"
#include "bloom_filter.hpp"

constexpr size_t HASH_SIZE = 70;

MultiBloomFilter build_bloom_filters(const CompactReference &ref_read, ssize_t nb_dpu, ssize_t dpu_ref_size, ssize_t overlap);

#endif // READ_MAPPER_HPP