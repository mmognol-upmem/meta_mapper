#include "bloom_filter.hpp"

void MultiBloomFilter::initialize(const size_t nb_filters, const size_t size2)
{
    m_nb_filters = nb_filters;
    m_size2 = size2;
    m_size = 1UL << m_size2;
    m_size_reduced = m_size - 1;
    m_sub_size = (m_nb_filters + m_pack_size - 1) >> m_pack_size2;
    m_data.resize(m_size * m_sub_size, static_cast<pack_t>(0));
}

void MultiBloomFilter::insert(const size_t idx, const hash_t hash)
{
    auto i = idx >> m_pack_size2;
    auto h = hash & m_size_reduced;
    auto current_val = __sync_fetch_and_or(m_data.data() + h * m_sub_size + i, 0);
    if (__builtin_popcountll(current_val) < m_MAX_SET_BITS_PER_PACKED_VALUE)
    {
        auto mask = m_bit_mask[idx & (m_pack_size - 1)];
        __sync_fetch_and_or(m_data.data() + h * m_sub_size + i, mask);
    }
}

void MultiBloomFilter::save_to_file(std::string file_path)
{
    std::ofstream out;
    out.open(file_path);
    out.write(reinterpret_cast<const char *>(&m_nb_filters), sizeof(m_nb_filters));
    out.write(reinterpret_cast<const char *>(&m_size2), sizeof(m_size2));
    out.write(reinterpret_cast<const char *>(m_data.data()), m_size * m_sub_size * sizeof(pack_t));
    out.close();
}

void MultiBloomFilter::load_from_file(std::string file_path)
{
    std::ifstream in;
    in.open(file_path);
    in.read(reinterpret_cast<char *>(&m_nb_filters), sizeof(m_nb_filters));
    in.read(reinterpret_cast<char *>(&m_size2), sizeof(m_size2));
    initialize(m_nb_filters, m_size2);
    in.read(reinterpret_cast<char *>(m_data.data()), m_size * m_sub_size * sizeof(pack_t));
    in.close();
}