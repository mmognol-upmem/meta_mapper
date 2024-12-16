#include <cstdio>

#include "file_utils.hpp"
#include "graal/Bank.hpp"

std::string generate_bloom_file_path(const std::string &reference_uri, size_t nb_dpu, size_t hash_size)
{
    return reference_uri + "_d" + std::to_string(nb_dpu) + "_s" +
           std::to_string(hash_size) + std::string(BLOOM_FILTER_EXTENSION);
}

bool check_bloom_file_exists(const std::string &bloom_file_path, bool force_create_bloom)
{
    return !force_create_bloom && std::filesystem::exists(bloom_file_path);
}

std::string validate_reference_file(const std::string &reference_uri)
{
    if (!std::filesystem::exists(reference_uri))
        exit(printf("Reference file: %s does not exist\n", reference_uri.c_str()));
    return reference_uri;
}

ssize_t check_reference_estimated_size(graal::Bank &reference_bank, ssize_t nb_ranks)
{
    auto estimation = reference_bank.estimate();
    ssize_t ref_estimated_size = static_cast<ssize_t>(estimation.size_mean * estimation.sequences_number);

    if ((ref_estimated_size * 2) > (16'000'000 * nb_ranks * 64)) // Estimating 64 DPUs per rank
        throw std::invalid_argument("Reference sequence probably too long");

    return ref_estimated_size;
}

Reference load_reference(graal::Bank &reference_bank, ssize_t nb_ranks)
{
    Reference reference{};
    std::string revcomp_ref{};

    auto ref_estimated_size = check_reference_estimated_size(reference_bank, nb_ranks);

    reference.seq.reserve((ref_estimated_size * 3) / 2); // Reserve a little more to be sure to not reallocate
    revcomp_ref.reserve(reference.seq.capacity() / 2);

    reference_bank.visit([&reference, &revcomp_ref](auto bank)
                         {
		for (auto &seq : bank) {
			auto n = seq.size();
            reference.seq.append(seq);
            reference.names.push_back({seq.name(), reference.seq.size() - n, n});
            revcomp_ref.append(seq);
        } });

    N_to_G_read(reference.seq);
    complement_read(revcomp_ref);
    reference.seq.append(revcomp_ref);

    return reference;
}

CompactReference load_and_compress_reference(graal::Bank &reference_bank, ssize_t nb_ranks)
{
    CompactReference reference{};
    CompactSequence revcomp_ref{};

    auto ref_estimated_size = check_reference_estimated_size(reference_bank, nb_ranks);

    reference.seq.reserve((ref_estimated_size * 3) / 2); // Reserve a little more to be sure to not reallocate
    revcomp_ref.reserve(reference.seq.capacity() / 2);

    reference_bank.visit([&reference, &revcomp_ref](auto bank)
                         {
		for (auto &seq : bank) {
			auto n = seq.size();
            reference.seq.append(seq);
            reference.names.push_back({seq.name(), reference.seq.size() - n, n});
            revcomp_ref.append_revcomp(seq);
        } });

    reference.seq.append(revcomp_ref);

    return reference;
}