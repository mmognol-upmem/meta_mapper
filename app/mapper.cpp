#include "bloom_filter.hpp"
#include "file_utils.hpp"
#include "parse_command.hpp"
#include "read_mapper.hpp"

int main(int argc, char *argv[])
{
    auto parsed = parse_mapper(argc, argv);

    auto reference_file = validate_reference_file(parsed["reference"].as<std::string>());
    auto nb_ranks = parsed["ranks"].as<ssize_t>();
    auto nb_dpu = nb_ranks * 64;

    auto bloom_file = generate_bloom_file_path(reference_file, nb_dpu, HASH_SIZE);

    MultiBloomFilter bf{};
    bf.load_from_file(bloom_file);

    printf("Bloom filter size: %lu\n", bf.get_size2());

    return 0;
}