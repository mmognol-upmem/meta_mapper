#ifndef C09A039F_60F7_4F0B_8F46_F4397CAEB0CE
#define C09A039F_60F7_4F0B_8F46_F4397CAEB0CE

extern "C"
{
#include <stdint.h>
}

/* ------------ To enable and read performances measures on DPUs ------------ */
// #define DO_DPU_PERFCOUNTER 0  // Counts cycles if 0, else instructions

/* ------------------------ To enable printf on DPUs ------------------------ */
// #define LOG_DPU

/* ---------------------------- Useful functions ---------------------------- */

/// @brief Checks if a number is a power of 2
/// @tparam T type of value
/// @param n value
/// @return true if value is a power of 2, false otherwise
template <typename T>
constexpr bool IS_POWER_2(T n)
{
	return (n & (n - 1)) == 0;
}

/// @brief Round a value to upper multiple of N
/// @tparam T type of value
/// @tparam N multiple to round to
/// @param value number to round up
/// @return next multiple of N
template <unsigned int N, typename T>
constexpr T CEILN(T value)
{
	static_assert(IS_POWER_2(N), "N must be a power of 2");
	return (value + (N - 1)) & ~(N - 1);
}

/// @brief Round a value to lower multiple of N
/// @tparam T type of value
/// @tparam N multiple to round to
/// @param value number to round down
/// @return previous multiple of N
template <unsigned int N, typename T>
constexpr T FLOORN(T value)
{
	static_assert(IS_POWER_2(N), "N must be a power of 2");
	return value & ~(N - 1);
}

/// @brief Computes remainder of divison by N
/// @tparam T type of value
/// @tparam N divider
/// @param size number to divide by N
/// @return remainder of the division
template <unsigned int N, typename T>
constexpr T REMAINDERN(T value)
{
	static_assert(IS_POWER_2(N), "N must be a power of 2");
	return value & (N - 1);
}

/* ------------------------------- Parameters ------------------------------- */

// Put these values to true to increase perfect coverage (but will be slower)
constexpr bool CONTINUE_MAPPING_IF_NOT_PERFECT_DPU = false;
constexpr bool CONTINUE_MAPPING_IF_NOT_PERFECT_HOST = false;

// Put this value to true to increase overall coverage (but will be **a lot** slower)
constexpr bool DISPATCH_TO_ALL = false;

constexpr uint32_t MAX_NB_QUERIES_PER_DPU = 512;
static_assert((MAX_NB_QUERIES_PER_DPU % 8) == 0,
			  "Max number of queries per DPU must be multiple of 8 (DPU transfers restrictions)");

constexpr uint32_t MAX_QUERY_SIZE = 256;
static_assert((MAX_QUERY_SIZE & 7) == 0, "Max size of query must be a multiple of 8 (DPU transfers restrictions");
static_assert(MAX_QUERY_SIZE <= 256,
			  "Cannot handle queries of size strictly larger than 256 (query positions must fit in 8 bits)");

constexpr uint32_t MAX_NB_ERRORS = 4;
static_assert(MAX_NB_ERRORS <= 8, "Not supporting to find more than 8 errors during the mapping");

/* ------------------------- Communication utilities ------------------------ */

struct IndexArgs
{
	uint64_t seq_size;
};

struct MapArgs
{
	uint8_t queries[MAX_NB_QUERIES_PER_DPU * (MAX_QUERY_SIZE >> 2)];
	uint32_t seed_positions[MAX_NB_QUERIES_PER_DPU];
	uint8_t query_sizes[MAX_NB_QUERIES_PER_DPU];
	uint32_t nb_queries;
	uint32_t unused; // Unused field, only there to align size on multiple of 8
};

struct MapIdentifiers
{
	uint64_t data[MAX_NB_QUERIES_PER_DPU];
	uint8_t read_sizes[MAX_NB_QUERIES_PER_DPU];
};

struct MapAllArgs
{
	MapAllArgs() : dpu_args() { dpu_args.nb_queries = 0; }
	MapArgs dpu_args;
	MapIdentifiers identifiers{};
};

constexpr uint64_t ENCODE_MAP_RESULT(uint32_t distance, uint32_t position)
{
	return (static_cast<uint64_t>(distance) << 32) + position;
}

constexpr uint32_t DECODE_MAP_RESULT_DISTANCE(uint64_t result) { return static_cast<uint32_t>(result >> 32); }
constexpr uint32_t DECODE_MAP_RESULT_POSITION(uint64_t result) { return static_cast<uint32_t>(result); }

struct MapResults
{
	uint64_t data[MAX_NB_QUERIES_PER_DPU];
	uint64_t error_positions[MAX_NB_QUERIES_PER_DPU];
};

/* -------------------------- Good seed definition -------------------------- */

template <typename T>
constexpr bool is_good_seed(T t, uint32_t i = 0)
{
	return (t[i] != t[i + 1]) && (t[i] != t[i + 2]) && (t[i] != t[i + 3]);
}

#endif /* C09A039F_60F7_4F0B_8F46_F4397CAEB0CE */