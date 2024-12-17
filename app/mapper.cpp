#include "bloom_filter.hpp"
#include "file_utils.hpp"
#include "parse_command.hpp"
#include "read_mapper.hpp"
#include "dpu_mapper.hpp"

ssize_t compute_dpu_reference_size(size_t reference_size, ssize_t nb_dpu, ssize_t overlap)
{
    ssize_t dpu_ref_size = static_cast<ssize_t>(reference_size);
    dpu_ref_size = dpu_ref_size - ((nb_dpu + 1) * overlap);
    dpu_ref_size = dpu_ref_size / nb_dpu + (2 * overlap);
    dpu_ref_size = CEILN<4>(dpu_ref_size); // Align on 4 for the read packing
    if (dpu_ref_size > 16'000'000)
        exit(printf("Reference sequence is too long, max 16'000'000, current: %ld\n", dpu_ref_size));

    return dpu_ref_size;
}

int main(int argc, char *argv[])
{
    auto parsed = parse_mapper(argc, argv);

    auto reference_file = validate_reference_file(parsed["reference"].as<std::string>());
    auto nb_ranks = parsed["ranks"].as<ssize_t>();

    graal::Bank reference_bank(reference_file);

    auto reference = load_and_compress_reference(reference_bank, nb_ranks);

    auto ref_size = reference.seq.size();
    std::string binary_name = "./dpu";
    DpuProfile dpu_profile{};
    PimRankSet<> pim_rankset(nb_ranks, nb_ranks);

    auto nb_dpu = initialize_dpus(pim_rankset, dpu_profile, binary_name);
    auto dpu_ref_size = compute_dpu_reference_size(ref_size, nb_dpu, 400);

    printf("Reference size: %lu\n", ref_size);
    printf("DPU reference size: %lu\n", dpu_ref_size);

    auto bloom_file = generate_bloom_file_path(reference_file, nb_dpu, HASH_SIZE);
    MultiBloomFilter bf{};
    bf.load_from_file(bloom_file);
    printf("Bloom filter size: %lu\n", bf.get_size2());

    IndexArgs index_args{};

    build_index(pim_rankset, reference, dpu_ref_size, nb_ranks, nb_dpu, 400, index_args);
    pim_rankset.wait_all_ranks_done();

    printf("Index built\n");

    return 0;
}