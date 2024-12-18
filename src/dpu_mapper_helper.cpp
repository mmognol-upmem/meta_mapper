#include <algorithm>
#include <array>
#include <cstdio>

#include "dpu_mapper_helper.hpp"
#include "read_mapper.hpp"

bool check_seed_param(size_t read_size, const std::array<ssize_t, 2> &round_shift, ssize_t range, ssize_t delta)
{
    const size_t min_size = *std::max_element(round_shift.begin(), round_shift.end()) + delta +
                            range * 2 + HASH_SIZE;
    return read_size >= min_size;
}

ssize_t adjust_seed_search_range(size_t read_size, const std::array<ssize_t, 2> &round_shift, ssize_t range, ssize_t delta)
{
    while (!check_seed_param(read_size, round_shift, range, delta))
    {
        range--;
        if (range == 0)
            exit(printf("Query size is too small to search seeds\n"));
    }

    printf("Seed search range: %ld\n", range);
    return range;
}

ssize_t min_query_size(ssize_t range, ssize_t delta)
{
    return delta + range * 2 + HASH_SIZE;
}

ssize_t find_good_pos(const Read &query, ssize_t range, size_t shift)
{
    std::pair<size_t, size_t> result;
    size_t start_pos = shift;
    std::array<uint8_t, 4> bases{
        query.seq[start_pos],
        query.seq[start_pos + 1],
        query.seq[start_pos + 2],
        query.seq[start_pos + 3]};
    while (((start_pos - shift) <= static_cast<size_t>(range)) && !(is_good_seed(bases)))
    {
        start_pos++;
        bases[0] = bases[1];
        bases[1] = bases[2];
        bases[2] = bases[3];
        bases[3] = query.seq[start_pos + 3];
    }
    return start_pos;
}

ssize_t get_round_shift(ssize_t shift, size_t read_size, ssize_t min_size)
{
    ssize_t diff = shift + min_size - read_size;
    if (diff >= 0)
        shift = std::max(shift - diff, 0L);

    return shift;
}