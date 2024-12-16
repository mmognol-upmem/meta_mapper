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
    auto parsed = parse_mapper(argc, argv);

    auto reference_file = validate_reference_file(parsed["reference"].as<std::string>());
    auto nb_ranks = parsed["ranks"].as<ssize_t>();
    auto nb_dpu = nb_ranks * 64;

    auto bloom_file = generate_bloom_file_path(reference_file, nb_dpu, HASH_SIZE);

    MultiBloomFilter bf{};
    bf.load_from_file(bloom_file);

    printf("Bloom filter size: %lu\n", bf.get_size2());

    graal::Bank reference_bank(reference_file);

    auto reference = load_and_compress_reference(reference_bank, 8);

    ref_size = reference.seq.ref_size();
    initialize_dpu();
    auto dpu_ref_size = compute_dpu_reference_size(_ref_size, _nb_dpu);
    ;

    return 0;
}