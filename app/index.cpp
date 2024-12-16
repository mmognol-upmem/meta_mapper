#include <filesystem>

#include "bloom_filter.hpp"
#include "file_utils.hpp"
#include "parse_command.hpp"
#include "read_mapper.hpp"
#include "pim_shared.hpp"

ssize_t compute_dpu_reference_size(size_t reference_size, ssize_t nb_dpu, ssize_t overlap)
{
    ssize_t dpu_ref_size = static_cast<ssize_t>(reference_size);
    dpu_ref_size = dpu_ref_size - ((nb_dpu + 1) * overlap);
    dpu_ref_size = dpu_ref_size / nb_dpu + (2 * overlap);
    dpu_ref_size = CEILN<4>(dpu_ref_size); // Align on 4 for the read packing
    if (dpu_ref_size > 16'000'000)
        exit(printf("Reference sequence is too long\n"));

    return dpu_ref_size;
}

int main(int argc, char *argv[])
{
    auto parsed = parse_index(argc, argv);

    auto reference_file = validate_reference_file(parsed["reference"].as<std::string>());
    auto nb_ranks = parsed["ranks"].as<ssize_t>();
    auto nb_dpu = nb_ranks * 64;

    ssize_t overlap = 400;

    graal::Bank reference_bank(reference_file);

    auto reference = load_and_compress_reference(reference_bank, 8);

    printf("Reference names: %zu\n", reference.names.size());
    print_reference_names(reference);

    auto dpu_ref_size = compute_dpu_reference_size(reference.seq.size(), nb_dpu, overlap);

    // Create a new BloomFilter object
    auto bf = build_bloom_filters(reference, nb_dpu, dpu_ref_size, overlap);
    printf("Bloom filter size: %lu\n", bf.get_size2());

    auto bloom_file = generate_bloom_file_path(reference_file, nb_dpu, HASH_SIZE);

    bf.save_to_file(bloom_file);

    return 0;
}