#include "file_utils.hpp"
#include "parse_command.hpp"
#include "dpu_mapper.hpp"

int main(int argc, char *argv[])
{
    auto parsed = parse_mapper(argc, argv);

    auto reference_file = validate_file(parsed["reference"].as<std::string>());
    auto queries_file = validate_file(parsed["queries"].as<std::string>());
    auto nb_ranks = parsed["ranks"].as<ssize_t>();

    DpuMapper mapper(reference_file, nb_ranks, true);

    printf("Start mapping\n");

    mapper.map(queries_file, "output.sam");

    return 0;
}