#include <cmath>
#include <omp.h>
#include <algorithm>

#include "pim_shared.hpp"
#include "read_mapper.hpp"
#include "signature.hpp"

ssize_t ceil_log2(ssize_t x)
{
    return ceil(log(static_cast<double>(x)) / log(2));
}

std::vector<ssize_t> fill_bloom_filters(MultiBloomFilter &bloom_filters, const CompactReference &ref_read, ssize_t nb_dpu, ssize_t dpu_ref_size, ssize_t overlap)
{
    constexpr ssize_t build_nr_threads = 16;
    std::vector<ssize_t> nb_signatures(build_nr_threads, 0);

#pragma omp parallel for schedule(dynamic) num_threads(build_nr_threads)
    for (ssize_t dpu_id = 0; dpu_id < nb_dpu; ++dpu_id)
    {
        auto tid = omp_get_thread_num();
        auto start = dpu_id * (dpu_ref_size - overlap);
        std::array<uint8_t, 4> bases{};
        bases[0] = ref_read.seq[start];
        bases[1] = ref_read.seq[start + 1];
        bases[2] = ref_read.seq[start + 2];
        bases[3] = ref_read.seq[start + 3];
        std::vector<std::pair<uint32_t, uint64_t>> sigs;
        sigs.reserve(dpu_ref_size);

        for (size_t i = start; i < start + dpu_ref_size - HASH_SIZE; ++i)
        {
            if (is_good_seed(bases))
                sigs.emplace_back(bloom_filters.place_mask(dpu_id, Signature<HASH_SIZE>::hash(ref_read, i)));

            bases[0] = bases[1];
            bases[1] = bases[2];
            bases[2] = bases[3];
            bases[3] = ref_read.seq[i + 4];
        }

        std::sort(sigs.begin(), sigs.end(), [](const auto &a, const auto &b)
                  { return a.first < b.first; });

        for (const auto &[place, mask] : sigs)
        {
            bloom_filters.insert_computed(place, mask); // Relevant signature
            nb_signatures[tid]++;
        }
    }

    return nb_signatures;
}

MultiBloomFilter build_bloom_filters(const CompactReference &ref_read, ssize_t nb_dpu, ssize_t dpu_ref_size, ssize_t overlap)
{
    auto bloom_size2 = ceil_log2(dpu_ref_size * 8);
    MultiBloomFilter bloom_filters{};
    bloom_filters.initialize(nb_dpu, bloom_size2);
    auto nb_signatures = fill_bloom_filters(bloom_filters, ref_read, nb_dpu, dpu_ref_size, overlap);
    return bloom_filters;
}

void serialize_bloom_filters(const MultiBloomFilter &bloom_filters, const std::string &bloom_file_path)
{
    bloom_filters.save_to_file(bloom_file_path);
}