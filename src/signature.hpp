#include <cstdint>

#include "bloom_filter.hpp"
#include "read.hpp"

template <size_t HashSize>
class Signature
{
public:
    // Static member functions to compute the hash
    static constexpr hash_t hash(const Read &read, const size_t start_pos)
    {
        hash_t val = HASH_INIT_VALUE;
        for (size_t i = start_pos; i < HashSize + start_pos; ++i)
        {
            val = ((val << 5) + val) + read.seq[i];
        }
        return val;
    }

    // Static member functions to compute the hash
    static constexpr hash_t hash(const Reference &read, const size_t start_pos)
    {
        hash_t val = HASH_INIT_VALUE;
        for (size_t i = start_pos; i < HashSize + start_pos; ++i)
        {
            val = ((val << 5) + val) + read.seq[i];
        }
        return val;
    }

    // Static member functions to compute two hashes
    static constexpr std::pair<hash_t, hash_t> hash(const Read &read, const size_t start_pos, const size_t start_pos2)
    {
        hash_t val1 = HASH_INIT_VALUE;
        hash_t val2 = HASH_INIT_VALUE;
        size_t diff = start_pos2 - start_pos;
        for (size_t i = start_pos; i < HashSize + start_pos; ++i)
        {
            val1 = ((val1 << 5) + val1) + read.seq[i];
            val2 = ((val2 << 5) + val2) + read.seq[i + diff];
        }
        return std::make_pair(val1, val2);
    }

    static constexpr size_t hash_size() { return HashSize; }

private:
    static constexpr hash_t HASH_INIT_VALUE = 5381;
};