#include "bloom_filter.hpp"
#include "parse_command.hpp"

int main(int argc, char *argv[])
{
    auto parsed = parse_index(argc, argv);

    // Create a new BloomFilter object
    MultiBloomFilter bf{};
    // Initialize the BloomFilter object with 1 filter and size2 of 10
    bf.initialize(parsed["ranks"].as<ssize_t>(), 10);
    // Insert a hash into the BloomFilter object
    bf.insert(0, 123456);
    // Save the BloomFilter object to a file
    bf.save_to_file("bloom_filter.bin");
    return 0;
}