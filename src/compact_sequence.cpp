#include <algorithm>

#include "compact_sequence.hpp"
#include "read.hpp"

uint8_t code_4base_n(const char *c)
{
    alignas(4) char d[4] = {c[3], c[2], c[1], c[0]};
    return __builtin_ia32_pext_si(*reinterpret_cast<uint32_t *>(d), 0X06060606);
}

size_t new_size(size_t s1, size_t s2)
{
    return ((s1 + 3) & ~3) + s2;
}

void CompactSequence::append(const std::string &seq)
{
    m_seq_size += new_size(m_seq_size, seq.size());
    for (size_t i = 0; i < seq.size(); i += 4)
    {
        auto c = code_4base_n(seq.data() + i);
        push_back(c);
    }
}

void CompactSequence::append(const CompactSequence &cseq2)
{
    m_seq_size += new_size(m_seq_size, cseq2.size());
    insert(end(), cseq2.begin(), cseq2.end());
}

void CompactSequence::append_revcomp(std::string &&seq)
{
    std::reverse(seq.begin(), seq.end());
    complement_read(seq);

    append(std::move(seq));
}

void CompactSequence::append_revcomp(const std::string &seq_const)
{
    std::string seq = seq_const;
    append_revcomp(std::move(seq));
}
