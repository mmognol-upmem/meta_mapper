#include <cstdio>

#include "parse_command.hpp"

cxxopts::ParseResult parse_mapper(int argc, char *argv[])
{
    cxxopts::Options options("Mapper", "Run Short Read Mapping on PIM");

    options.add_options()(
        "r,reference", "URI to reference genome (e.g. 'file://genome.fa' or 'file://genome_album.txt')",
        cxxopts::value<std::string>())("U,queries", "Path to queries file", cxxopts::value<std::string>())(
        "k,ranks", "Number of PIM ranks", cxxopts::value<ssize_t>()->default_value("4"))(
        "b,bloom", "Force reconstruction of Bloom filter", cxxopts::value<bool>()->default_value("false"))(
        "s,sam", "Path of output in SAM format", cxxopts::value<bool>()->default_value("false"))("h,help", "Print usage");

    auto result = options.parse(argc, argv);

    if (result.count("help") > 0 || result.arguments().empty())
        exit(printf("%s\n", options.help().c_str()));

    return result;
}

cxxopts::ParseResult parse_index(int argc, char *argv[])
{
    cxxopts::Options options("Index", "Index reference genome for PIM mapping");

    options.add_options()(
        "r,reference", "URI to reference genome (e.g. 'file://genome.fa' or 'file://genome_album.txt')",
        cxxopts::value<std::string>())(
        "k,ranks", "Number of PIM ranks", cxxopts::value<ssize_t>()->default_value("4"))("h,help", "Print usage");

    auto result = options.parse(argc, argv);

    if (result.count("help") > 0 || result.count("reference") == 0 || result.count("ranks") == 0)
        exit(printf("%s\n", options.help().c_str()));

    return result;
}