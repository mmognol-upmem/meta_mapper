#include "bloom_filter.hpp"

/* -------------------------------------------------------------------------- */
/*                              filter static data                            */
/* -------------------------------------------------------------------------- */

constexpr uint64_t BSF_DEBRUIJN64 = 0x03f79d71b4cb0a89UL;
constexpr int BSF_INDEX64[64] = {0, 1, 48, 2, 57, 49, 28, 3, 61, 58, 50, 42, 38, 29, 17, 4,
                                 62, 55, 59, 36, 53, 51, 43, 22, 45, 39, 33, 30, 24, 18, 12, 5,
                                 63, 47, 56, 27, 60, 41, 37, 16, 54, 35, 52, 21, 44, 32, 23, 11,
                                 46, 26, 40, 15, 34, 20, 31, 10, 25, 14, 19, 9, 13, 8, 7, 6};

/* -------------------------------------------------------------------------- */
/*                              utils functions                               */
/* -------------------------------------------------------------------------- */

template <typename T>
void write(const T &data, std::ofstream &out)
{
    if constexpr (requires { data.size(); })
        out.write(reinterpret_cast<const char *>(data.data()), data.size() * sizeof(typename T::value_type));
    else
        out.write(reinterpret_cast<const char *>(&data), sizeof(data));
}

template <typename T>
void read(T &data, std::ifstream &in)
{
    if constexpr (requires { data.size(); })
        in.read(reinterpret_cast<char *>(data.data()), data.size() * sizeof(typename T::value_type));
    else
        in.read(reinterpret_cast<char *>(&data), sizeof(data));
}

inline uint64_t blsr_u64(uint64_t val)
{
    return val & (val - 1);
}

/* -------------------------------------------------------------------------- */
/*                                 Lazy result                                */
/* -------------------------------------------------------------------------- */

void ResultIterator::next()
{
    for (; m_i >= 0; --m_i)
    {
        if (val == 0)
        {
            val = m_filter[m_h[0] * m_sub_size + m_i - 1] &
                  m_filter[m_h[1] * m_sub_size + m_i - 1];
        }
        else
        {
            result = (m_i << bf_pack_size2) + BSF_INDEX64[((val & -val) * BSF_DEBRUIJN64) >> 58];
            val = blsr_u64(val);
            return;
        }
    }
}

/* -------------------------------------------------------------------------- */
/*                          MultiBloomFilter implem                           */
/* -------------------------------------------------------------------------- */

void MultiBloomFilter::initialize(const ssize_t nb_filters, const ssize_t size2)
{
    m_nb_filters = nb_filters;
    m_size2 = size2;
    m_size = 1L << m_size2;
    m_size_reduced = m_size - 1;
    m_sub_size = (m_nb_filters + bf_pack_size - 1) >> bf_pack_size2;
    m_data.resize(m_size * m_sub_size, static_cast<pack_t>(0));
}

void MultiBloomFilter::insert(const size_t idx, const hash_t hash)
{
    auto i = idx >> bf_pack_size2;
    auto h = hash & m_size_reduced;
    auto current_val = __sync_fetch_and_or(m_data.data() + h * m_sub_size + i, 0);
    if (__builtin_popcountll(current_val) < m_MAX_SET_BITS_PER_PACKED_VALUE)
    {
        auto mask = m_bit_mask[idx & (bf_pack_size - 1)];
        __sync_fetch_and_or(m_data.data() + h * m_sub_size + i, mask);
    }
}

LazyBfResult MultiBloomFilter::contains(const hash_t hash1, const hash_t hash2)
{
    return LazyBfResult({hash1 & m_size_reduced, hash2 & m_size_reduced}, m_data.data(), m_sub_size);
}

void MultiBloomFilter::save_to_file(std::string file_path) const
{
    std::ofstream out(file_path, std::ios::binary);
    write(m_nb_filters, out);
    write(m_size2, out);
    write(m_data, out);
    out.close();
}

void MultiBloomFilter::load_from_file(std::string file_path)
{
    std::ifstream in(file_path, std::ios::binary);
    read(m_nb_filters, in);
    read(m_size2, in);
    initialize(m_nb_filters, m_size2);
    read(m_data, in);
    in.close();
}

/**
 * @brief Computes the place and mask for a given index and hash.
 *
 * @param idx The index to be used.
 * @param hash The hash value to be used.
 * @return A pair containing the place and mask.
 */
std::pair<uint64_t, uint64_t> MultiBloomFilter::place_mask(const size_t idx, const hash_t hash) const
{
    auto i = idx >> bf_pack_size2;
    auto h = hash & m_size_reduced;
    return {h * m_sub_size + i, m_bit_mask[idx & (bf_pack_size - 1)]};
}

void MultiBloomFilter::insert_computed(const uint64_t place, const uint64_t mask)
{
    __sync_fetch_and_or(m_data.data() + place, mask);
}