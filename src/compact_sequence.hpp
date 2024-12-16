#ifndef COMPACT_SEQUENCE_HPP
#define COMPACT_SEQUENCE_HPP

#include <vector>
#include <array>
#include <string>

struct CompactSequence : public std::vector<uint8_t>
{
    static constexpr std::array<uint8_t, 4> SHIFT_PUSH = {6, 4, 2, 0};

    uint8_t operator[](std::size_t idx) const { return (data()[idx >> 2] >> SHIFT_PUSH[idx & 3]) & 3; }

    size_t seq_size() const { return size() * 4; }

    uint8_t *data() { return data(); }
    const uint8_t *data() const { return data(); }

    uint8_t *data(size_t idx) { return data() + (idx >> 2); }
    const uint8_t *data(size_t idx) const { return data() + (idx >> 2); }

    void append(const std::string &seq);
    void append(const CompactSequence &cseq);
    void append_revcomp(std::string &&seq);
};

#endif // COMPACT_SEQUENCE_HPP
