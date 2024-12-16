#ifndef B8927905_A612_4B68_96DB_72B88C732DCB
#define B8927905_A612_4B68_96DB_72B88C732DCB

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

/* -------------------------------------------------------------------------- */
/*                                 Utils for BF                               */
/* -------------------------------------------------------------------------- */

constexpr uint64_t PMASK32 = (1UL << 32) - 1;
using hash_t = uint32_t;

template <typename T, size_t S>
consteval auto generate_bit_mask()
{
    std::array<T, S> mask{};
    for (size_t i = 0; i < S; i++)
    {
        mask[i] = 1UL << i;
    }
    return mask;
}

/* -------------------------------------------------------------------------- */
/*                                 BloomFilter                                */
/* -------------------------------------------------------------------------- */

class MultiBloomFilter
{
private:
    using pack_t = uint64_t;

public:
    MultiBloomFilter() {}

    /// @brief Multi Bloom filter data structure with 3 hash functions
    /// @param nb_filters number of filters
    /// @param size2 power of 2 to use for the size of each filter that will be 2^size2
    void initialize(const ssize_t nb_filters, const ssize_t size2);

    /// @brief Insert one item in one of the filters
    /// @param idx idx of the filter to insert in
    /// @param hash hashed item to insert
    void insert(const size_t idx, const hash_t hash);

    void prefetch(const hash_t hash);

    /// @brief Save the data into a file
    /// @param file_path path of the file
    void save_to_file(std::string file_path) const;

    /// @brief Load from a file
    /// @param file_path pafth of the file
    void load_from_file(std::string file_path);

    std::pair<uint64_t, uint64_t> place_mask(const size_t idx, const hash_t hash) const;
    void insert_computed(const uint64_t place, const uint64_t mask);

    /// @brief Return size2 of the filters
    /// @return size2
    size_t get_size2() const { return m_size2; }

    double weight() const;

private:
    static constexpr ssize_t m_pack_size2 = 6;

    static constexpr ssize_t m_pack_size = 1 << m_pack_size2;

    ssize_t m_nb_filters{};
    ssize_t m_size2{};
    ssize_t m_size{};
    ssize_t m_size_reduced{};
    ssize_t m_sub_size{};

    std::vector<pack_t> m_data;

    static constexpr auto m_bit_mask = generate_bit_mask<pack_t, m_pack_size>();
    static constexpr size_t m_CONTAINS_RESERVE = 8;
    static constexpr size_t m_BLOCK_SIZE2 = 6;
    static constexpr size_t m_BLOCK_SIZE = (1 << m_BLOCK_SIZE2);

    static constexpr int m_MAX_SET_BITS_PER_PACKED_VALUE = 8;
};

#endif /* B8927905_A612_4B68_96DB_72B88C732DCB */
