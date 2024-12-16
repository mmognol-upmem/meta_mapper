#ifndef READ_HPP
#define READ_HPP

#include <array>
#include <string>
#include <vector>

#include "compact_sequence.hpp"

consteval std::array<char, 256> make_complement_table()
{
    std::array<char, 256> comp{};
    for (auto &c : comp)
    {
        c = 'N';
    }
    comp['A'] = 'T';
    comp['C'] = 'G';
    comp['G'] = 'C';
    comp['T'] = 'A';
    return comp;
}

static constexpr std::array<char, 256> COMP_TABLE = make_complement_table();

constexpr char complement(char c)
{
    return COMP_TABLE[static_cast<unsigned char>(c)];
}

constexpr char N_to_G(char c)
{
    return c == 'N' ? 'G' : c;
}

inline void N_to_G_read(std::string &seq)
{
    for (auto &c : seq)
    {
        c = N_to_G(c);
    }
}

inline void complement_read(std::string &seq)
{
    for (auto &c : seq)
    {
        c = complement(c);
    }
}

class Read
{
public:
    Read() = default;
    Read(const std::string &sequence, ssize_t the_id) : seq(sequence), id(the_id) {}
    std::string seq; // Sequence encoded with 2 bits per base
    size_t id{};     // ID to differentiate queries
};

struct SequenceMetadata
{
    std::string name;
    size_t start_pos;
    size_t size;
};

struct Reference
{
    std::string seq;
    std::vector<SequenceMetadata> names;
};

struct CompactReference
{
    CompactSequence seq;
    std::vector<SequenceMetadata> names;
};

inline void print_reference_names(const Reference &reference)
{
    for (const auto &name : reference.names)
    {
        printf("\t%s\n", name.name.c_str());
    }
}

inline void print_reference_names(const CompactReference &reference)
{
    for (const auto &name : reference.names)
    {
        printf("\t%s\n", name.name.c_str());
    }
}

#endif // READ_HPP