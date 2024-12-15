#include <filesystem>
#include <string>

constexpr std::string_view BLOOM_FILTER_EXTENSION = ".bf.bin";

std::string generate_bloom_file_path(const std::string &reference_uri, size_t nb_ranks, size_t hash_size)
{
    return reference_uri + "_k" + std::to_string(nb_ranks) + "_s" +
           std::to_string(hash_size) + std::string(BLOOM_FILTER_EXTENSION);
}

bool check_bloom_file_exists(const std::string &bloom_file_path, bool force_create_bloom)
{
    return !force_create_bloom && std::filesystem::exists(bloom_file_path);
}