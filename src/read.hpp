#ifndef READ_HPP
#define READ_HPP

#include <string>

class Read
{
public:
    Read(const std::string &sequence, ssize_t the_id) : seq(sequence), id(the_id) {}
    std::string seq; // Sequence encoded with 2 bits per base
    size_t id{};     // ID to differentiate queries
};

#endif // READ_HPP