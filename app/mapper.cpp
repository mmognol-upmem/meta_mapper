#include "bloom_filter.hpp"

int main(int argc, char *argv[])
{
    // Create a new BloomFilter object
    MultiBloomFilter bf{};
    // Initialize the BloomFilter object with 1 filter and size2 of 10
    bf.initialize(1, 10);
    // Insert a hash into the BloomFilter object
    bf.insert(0, 123456);
    // Save the BloomFilter object to a file
    bf.save_to_file("bloom_filter.bin");
    // Load the BloomFilter object from a file
    bf.load_from_file("bloom_filter.bin");
    // Get the size2 of the BloomFilter object

    return 0;
}