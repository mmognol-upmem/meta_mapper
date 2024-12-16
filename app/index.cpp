#include <filesystem>

#include "bloom_filter.hpp"
#include "file_utils.hpp"
#include "parse_command.hpp"
#include "read_mapper.hpp"

int main(int argc, char *argv[])
{
    auto parsed = parse_index(argc, argv);

    auto reference_file = validate_reference_file(parsed["reference"].as<std::string>());

    graal::Bank reference_bank(reference_file);

    auto reference = load_reference(reference_bank, 8);

    printf("Reference names: %zu\n", reference.names.size());
    print_reference_names(reference);

    // Create a new BloomFilter object
    auto bf = build_bloom_filters(reference, 8, 100, 10);

    // Initialize the BloomFilter object with 1 filter and size2 of 10
    bf.initialize(parsed["ranks"].as<ssize_t>(), 10);
    // Insert a hash into the BloomFilter object
    bf.insert(0, 123456);
    // Save the BloomFilter object to a file
    bf.save_to_file("bloom_filter.bin");
    return 0;
}