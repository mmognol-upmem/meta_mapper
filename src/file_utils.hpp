#ifndef FILE_UTILS_HPP
#define FILE_UTILS_HPP

#include <filesystem>
#include <string>

#include "read.hpp"

#include "graal/Bank.hpp"

constexpr std::string_view BLOOM_FILTER_EXTENSION = ".bf.bin";

Reference load_reference(graal::Bank &reference_bank, ssize_t nb_ranks);
CompactReference load_and_compress_reference(graal::Bank &reference_bank, ssize_t nb_ranks);
std::string validate_reference_file(const std::string &reference_uri);
std::string generate_bloom_file_path(const std::string &reference_uri, size_t nb_dpu, size_t hash_size);
bool check_bloom_file_exists(const std::string &bloom_file_path, bool force_create_bloom);

#endif // FILE_UTILS_HPP