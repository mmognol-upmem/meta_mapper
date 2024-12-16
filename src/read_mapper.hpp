#ifndef READ_MAPPER_HPP
#define READ_MAPPER_HPP

#include "read.hpp"
#include "bloom_filter.hpp"
#include "pim_common.hpp"

#include "BS_thread_pool_light.hpp"

constexpr size_t HASH_SIZE = 70;
constexpr std::array<size_t, 2> ROUND_SHIFTS = {5, 15};

MultiBloomFilter build_bloom_filters(const CompactReference &ref_read, ssize_t nb_dpu, ssize_t dpu_ref_size, ssize_t overlap);

struct Mapping
{
    using distance_t = uint32_t;
    distance_t distance;      // Distance if a mapping was found
    uint64_t position;        // Position in reference genome
    uint64_t error_positions; // Error positions in read, encoded as packed 8 bits, starting from lower bits
    uint8_t read_size;

    uint32_t get_read_size() const { return static_cast<uint32_t>(read_size) + 1; }
};

class MappingStatistics
{
public:
    MappingStatistics() { stats.fill(0); }

    void print(size_t nb_queries) const
    {
        printf("Mapped %zu queries\n", nb_queries);
        printf("Statistics (count - percentage - cumulated per):\n");
        double cumulative_per = 0.0;
        for (size_t nb_errors = 0; nb_errors <= MAX_NB_ERRORS; ++nb_errors)
        {
            double per = static_cast<double>(stats[nb_errors]) * 100.0 / static_cast<double>(nb_queries);
            cumulative_per += per;
            printf("%zu error%s  %12zu  %6.2f%%  %6.2f%%\n", nb_errors, nb_errors > 1 ? "s" : " ",
                   stats[nb_errors], per, cumulative_per);
        }
    }

    MappingStatistics &operator+=(const MappingStatistics &other)
    {
        for (size_t i = 0; i < stats.size(); ++i)
        {
            stats[i] += other.stats[i];
        }
        return *this;
    }

    void update(size_t distance, size_t old_distance)
    {
        if (distance != old_distance)
        {
            ++stats[distance];
        }
        if (old_distance < stats.size())
        {
            --stats[old_distance];
        }
    }

private:
    std::array<size_t, MAX_NB_ERRORS + 1> stats{};
};

class MapAllArgsAllocator
{
public:
    using type_t = std::vector<MapAllArgs>;
    MapAllArgsAllocator() {}
    MapAllArgsAllocator(const MapAllArgsAllocator &other) : buffers(other.buffers), available(other.available) {}

    void initialize(size_t capacity)
    {
        buffers.resize(capacity);
        available.clear();
        available.reserve(capacity);
        for (auto &buffer : buffers)
        {
            available.push_back(&buffer);
        }
    }

    type_t *acquire()
    {
        mutex.lock();
        auto *res = available.empty() ? NULL : available.back();
        if (res != NULL)
        {
            available.pop_back();
        }
        mutex.unlock();
        return res;
    }

    void release(type_t *buffer)
    {
        mutex.lock();
        available.push_back(buffer);
        mutex.unlock();
    }

private:
    std::vector<type_t> buffers;
    std::vector<type_t *> available;
    std::mutex mutex;
};

class MappingWorkerData
{
public:
    MappingWorkerData(std::vector<Mapping> &res, size_t nb_ranks, BS::thread_pool_light &thread_pool,
                      std::vector<size_t> &pos)
        : result(res), pool(thread_pool), positions(pos), rank_args(nb_ranks, NULL)
    {
        nb_dispatches.fill(0);
        allocator.initialize(nb_ranks * 2);
    }
    MappingWorkerData(const MappingWorkerData &other)
        : allocator(other.allocator), result(other.result), pool(other.pool), positions(other.positions) {}
    std::array<size_t, ROUND_SHIFTS.size()> nb_dispatches{};
    MappingStatistics stats;
    MapAllArgsAllocator allocator;
    std::vector<Mapping> &result;
    BS::thread_pool_light &pool;
    const std::vector<size_t> &positions;
    std::vector<std::vector<MapAllArgs> *> rank_args;
    std::mutex mutex;
};

#endif // READ_MAPPER_HPP