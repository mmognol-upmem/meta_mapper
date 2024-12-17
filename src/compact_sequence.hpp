#ifndef COMPACT_SEQUENCE_HPP
#define COMPACT_SEQUENCE_HPP

#include <vector>
#include <array>
#include <string>

struct CompactSequence : public std::vector<uint8_t>
{
    static constexpr std::array<uint8_t, 4> SHIFT_PUSH = {6, 4, 2, 0};
    static constexpr std::array<char, 8> DECODER = {'A', 'C', 'T', 'G', 'N', 'N', 'N', 'N'};

    uint8_t operator[](std::size_t idx) const { return (data()[idx >> 2] >> SHIFT_PUSH[idx & 3]) & 3; }

    size_t size() const { return m_seq_size; }

    size_t data_size() const
    {
        return std::vector<uint8_t>::size();
    }

    uint8_t *data() { return std::vector<uint8_t>::data(); }
    const uint8_t *data() const { return std::vector<uint8_t>::data(); }

    uint8_t *data(size_t idx) { return data() + (idx >> 2); }
    const uint8_t *data(size_t idx) const { return data() + (idx >> 2); }

    void append(const std::string &seq);
    void append(const CompactSequence &cseq);
    void append_revcomp(std::string &&seq_const);
    void append_revcomp(const std::string &seq_const);

private:
    size_t m_seq_size = 0;
};

inline uint8_t code_base(char c) { return (c >> 1) & 3; }
inline char decode(uint8_t i) { return CompactSequence::DECODER[i]; }

#endif // COMPACT_SEQUENCE_HPP
