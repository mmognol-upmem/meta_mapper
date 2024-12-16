#define _Bool bool
#define size_t uint32_t

extern "C"
{
#include <barrier.h>
#include <defs.h>
#include <mram.h>
#include <mutex_pool.h>
#include <perfcounter.h>
#include <stdint.h>
}

#include "dpu_utils.hpp"
#include "pim_common.hpp"

constexpr uint8_t NR_TASKLETS_MASK = (NR_TASKLETS - 1);

/* ---------------------------------- WRAM ---------------------------------- */

constexpr uint32_t SEQUENCE_MAX_SIZE = 1 << 22; // One DPU can hold a reference of up to ~16 M base pairs

constexpr uint32_t CACHE32_SIZE = 512;
constexpr uint32_t CACHE64_SIZE = CACHE32_SIZE >> 1;
constexpr uint32_t CACHE8_SIZE = CACHE32_SIZE << 2;
constexpr uint32_t CACHE8_SIZE_REDUCED = CACHE8_SIZE - 8; // must remove multiple of 8 at least >= (DPU_SEED_SIZE / 4)
constexpr uint32_t CACHE_SEED_SIZE =
	CEILN<8>(DPU_SEED_SIZE) + 8; // Start position will be aligned so reserve a bit more space
constexpr uint32_t CACHE_SEED_RESULT_SIZE = CEILN<8>(MAX_NB_QUERIES_PER_DPU / NR_TASKLETS) + 8;

__host IndexArgs index_args; // 8 B
bool index_built = false;

__dma_aligned uint32_t gcache32_cum_table[CACHE32_SIZE];

__dma_aligned bool GOOD_SEEDS[256] = { // Hardcoded array to indicate good seeds from 4 bases
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, true, true, true, false, true, true, true, false, true, true, true,
	false, false, false, false, false, true, true, true, false, true, true, true, false, true, true, true,
	false, false, false, false, false, true, true, true, false, true, true, true, false, true, true, true,
	true, false, true, true, false, false, false, false, true, false, true, true, true, false, true, true,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	true, false, true, true, false, false, false, false, true, false, true, true, true, false, true, true,
	true, false, true, true, false, false, false, false, true, false, true, true, true, false, true, true,
	true, true, false, true, true, true, false, true, false, false, false, false, true, true, false, true,
	true, true, false, true, true, true, false, true, false, false, false, false, true, true, false, true,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	true, true, false, true, true, true, false, true, false, false, false, false, true, true, false, true,
	true, true, true, false, true, true, true, false, true, true, true, false, false, false, false, false,
	true, true, true, false, true, true, true, false, true, true, true, false, false, false, false, false,
	true, true, true, false, true, true, true, false, true, true, true, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false};

/* ---------------------------------- MRAM ---------------------------------- */

__mram_noinit uint8_t sequence[SEQUENCE_MAX_SIZE]; //     4 MB
__mram_noinit MapArgs map_args = MapArgs();		   // < 100 KB
__mram MapResults map_results{};				   // <  10 KB

// Index variables
__mram uint32_t cnt_table[INDEX_SIZE]; // 16 MB (initialized with 0s)
// Packed 32 bits values: cum cnt (24) on lower, cnt on upper (8)

__mram_noinit uint32_t pos_table[INDEX_POS_SIZE]; // 32 MB
// 32 bits per pos is enough, has maximum 16 M bases in sequence

// Test / debug variable
__mram uint64_t checksum = 0;

/* ----------------------------- Synchronization ---------------------------- */

MUTEX_POOL_INIT(mutex_pool16, 16);
#ifdef DO_DPU_PERFCOUNTER
BARRIER_INIT(barrier_all, NR_TASKLETS);
#endif

/* -------------------------------------------------------------------------- */
/*                               Index reference                              */
/* -------------------------------------------------------------------------- */

inline uint32_t get_seq_key(uint32_t start, uint32_t offset, uint8_t *seq)
{
	// Encode as a key
	start += 4;
	uint32_t key = seq[start++] & EXTRACT_RIGHT_MASK[offset];
	uint32_t remaining_bases = DPU_SEED_SIZE - 4 + offset;
	for (uint32_t i = 0; i < remaining_bases; i += 4)
	{
		key = (key << 8) + (seq[start++] & 255);
	}
	remaining_bases &= 3;
	if (remaining_bases > 0)
	{
		key = key >> SHIFT_LENGTH[4 - remaining_bases];
	}
	// The index may not be big enough, so hash to fit into the table (will cause some collisions)
	return (key ^ (key >> INDEX_SIZE2)) & (INDEX_SIZE - 1);
}

void compute_cum_table()
{
	// Fill cumulative table using a cache in WRAM
	auto transfer_size = CACHE64_SIZE * sizeof(uint64_t);
	mram_read(cnt_table, gcache32_cum_table, transfer_size);
	auto cum_value = gcache32_cum_table[0];
	gcache32_cum_table[0] = 0;
	uint32_t cache_idx = 1, start_idx = 0;
	for (uint32_t i = 1; i < INDEX_SIZE; ++i)
	{
		if (cache_idx == CACHE64_SIZE)
		{
			mram_write(gcache32_cum_table, &cnt_table[start_idx], transfer_size);
			cache_idx = 0;
			start_idx += CACHE64_SIZE;
			mram_read(&cnt_table[start_idx], gcache32_cum_table, transfer_size);
		}
		auto x = gcache32_cum_table[cache_idx];
		gcache32_cum_table[cache_idx] = cum_value;
		cum_value += x;
		cache_idx++;
	}
	mram_write(gcache32_cum_table, &cnt_table[start_idx], transfer_size);
}

void index(uint8_t *cache8)
{
	auto size = index_args.seq_size - DPU_SEED_SIZE + 1 -
				4; // Keys ignore first 4 bases
				   // Since we filter seeds, maybe that affect the distribution into the hash space

	// Count kmers
	// Split work between tasklets
	uint32_t cache_idx = CACHE8_SIZE_REDUCED + me(), start_idx = 0;
	for (uint32_t i = me() << 2; i < size; i += (NR_TASKLETS << 2), cache_idx += NR_TASKLETS)
	{
		if (cache_idx >= CACHE8_SIZE_REDUCED)
		{
			mram_read(&sequence[start_idx], cache8, CACHE8_SIZE * sizeof(uint8_t));
			start_idx += CACHE8_SIZE_REDUCED;
			cache_idx -= CACHE8_SIZE_REDUCED;
		}
		uint8_t seeds[4];
		seeds[0] = cache8[cache_idx];
		seeds[1] = (cache8[cache_idx] << 2) | (cache8[cache_idx + 1] >> 6);
		seeds[2] = (cache8[cache_idx] << 4) | (cache8[cache_idx + 1] >> 4);
		seeds[3] = (cache8[cache_idx] << 6) | (cache8[cache_idx + 1] >> 2);
		for (uint32_t j = 0; j < 4; ++j)
		{
			if (i + j >= size)
			{
				break;
			}
			if (GOOD_SEEDS[seeds[j]])
			{
				auto key = get_seq_key(cache_idx, j, cache8);
				// NB : Two cells in same 8 bytes need to get the same mutex to
				// avoid data races (Implicit write access of MRAM variables is
				// not multi-tasklet safe for data types lower than 8 bytes)
				auto mutex_id = (key >> 1) & NR_TASKLETS_MASK;
				mutex_pool_lock(&mutex_pool16, mutex_id);
				if (cnt_table[key] < MAX_KEY_CNT)
				{
					cnt_table[key]++;
				}
				mutex_pool_unlock(&mutex_pool16, mutex_id);
			}
		}
	}

	// Sequential after that, exit all tasklets except 0
	if (me() != 0)
	{
		return;
	}

	// Compute cumulative cnt
	compute_cum_table();

	// Set checksum for debug
	// uint32_t nb_positions = UNPACK_LOWER24(cnt_table[INDEX_SIZE - 1]) + UNPACK_UPPER8(cnt_table[INDEX_SIZE - 1]);
	// checksum = nb_positions;

	// Register positions
	cache_idx = CACHE8_SIZE_REDUCED + me(), start_idx = 0;
	for (uint32_t i = 0; i < size; i += 4, ++cache_idx)
	{
		if (cache_idx >= CACHE8_SIZE_REDUCED)
		{
			mram_read(&sequence[start_idx], cache8, CACHE8_SIZE * sizeof(uint8_t));
			start_idx += CACHE8_SIZE_REDUCED;
			cache_idx -= CACHE8_SIZE_REDUCED;
		}
		uint8_t seeds[4];
		seeds[0] = cache8[cache_idx];
		seeds[1] = (cache8[cache_idx] << 2) | (cache8[cache_idx + 1] >> 6);
		seeds[2] = (cache8[cache_idx] << 4) | (cache8[cache_idx + 1] >> 4);
		seeds[3] = (cache8[cache_idx] << 6) | (cache8[cache_idx + 1] >> 2);
		for (uint32_t j = 0; j < 4; ++j)
		{
			if (i + j >= size)
			{
				break;
			}
			if (GOOD_SEEDS[seeds[j]])
			{
				auto key = get_seq_key(cache_idx, j, cache8);
				auto val = cnt_table[key];
				auto cnt = UNPACK_UPPER8(val);
				if (cnt < MAX_KEY_CNT)
				{
					auto x = UNPACK_LOWER24(val);
					assert(x < INDEX_POS_SIZE); // Only 8M space for potential 16M pos if ref max size and all
												// seeds are good. Unlikely, but check just in case
					if (x >= INDEX_POS_SIZE)
					{
						__asm__("fault 1");
					}
					pos_table[x] = i + j;
					// NB: the cum cnt table is modified, it won't start at 0 anymore
					cnt_table[key] = val + 1 + P2_24;
				}
			}
		}
	}
}

/* -------------------------------------------------------------------------- */
/*                                 Map queries                                */
/* -------------------------------------------------------------------------- */

inline uint8_t get_seq_base(uint32_t idx, uint32_t offset, uint8_t *seq)
{
	return (seq[idx + (offset >> 2)] >> SHIFT_LENGTH[3 - (offset & 3)]) & 3;
}

inline uint8_t get_ref_value(uint8_t *ref_seq, uint32_t i, uint32_t offset)
{
	auto ref_value = ref_seq[i];
	// If offset, then keep lower bits and extract the rest from the higher bits of next cell
	if (offset > 0)
	{
		ref_value = ((ref_value & EXTRACT_RIGHT_MASK[offset]) << SHIFT_LENGTH[offset]) |
					((ref_seq[i + 1] & EXTRACT_LEFT_MASK[offset]) >> SHIFT_LENGTH[4 - offset]);
	}
	return ref_value;
}

constexpr int hammning_distance(uint32_t x1, uint32_t x2)
{
	uint32_t x = x1 ^ x2;
	x |= (x >> 1);				  // Or with same sequence shifted by one bit
	x &= 0x55555555;			  // Select odd bits
	return __builtin_popcount(x); // Count 1s (= differences) with pop count
}

uint32_t BLSR_MASK[16] = {0x3FFF'FFFF, 0xCFFF'FFFF, 0xF3FF'FFFF, 0xFCFF'FFFF, 0xFF3F'FFFF, 0xFFCF'FFFF,
						  0xFFF3'FFFF, 0xFFFC'FFFF, 0xFFFF'3FFF, 0xFFFF'CFFF, 0xFFFF'F3FF, 0xFFFF'FCFF,
						  0xFFFF'FF3F, 0xFFFF'FFCF, 0xFFFF'FFF3, 0xFFFF'FFFC};

inline void hammning_distance_positions(uint32_t x1, uint32_t x2, uint32_t &num_errors, uint32_t &pos_errors,
										uint32_t idx_offset)
{
	uint32_t x = x1 ^ x2;
	uint32_t idx;
	uint32_t mask;

	__asm__(

		"2:"
		"clz %[idx], %[x], max, 1f;"		 // Count number of leading 0s, if max then leave
											 //
		"lsr %[idx], %[idx], 1;"			 // Divide index by 2 because 2 bits per base
		"lsl_add %[pos], %[ofs], %[pos], 8;" // Shift pos to make space for next position and add current offset
		"add %[pos], %[idx], %[pos];"		 // Add index to pos
											 //
		"lsl %[idx], %[idx], 2;"			 // Multiply index by 4 to get the right cell index
		"lw %[mask], %[idx], BLSR_MASK;"	 // Retrieve appropriate mask from table
											 //
		"add %[ne], %[ne], 1;"				 // Increment error counter
		"and %[x], %[x], %[mask], nz, 2b;"	 // Apply mask to reset the leading 1 base bits to 0, and back to start if
											 // still more errors to found
		"1:"

		: [x] "+r"(x), [ne] "+r"(num_errors), [idx] "+r"(idx), [mask] "+r"(mask), [pos] "+r"(pos_errors),
		  [ofs] "+r"(idx_offset)
		:);
}

void map(uint8_t *cache8, uint8_t *cache8bis, uint32_t *cache_seed, uint8_t *cache_sizes, uint64_t *cache_result)
{
	auto nb_queries = map_args.nb_queries;

	uint32_t D = nb_queries / NR_TASKLETS;
	uint32_t K = REMAINDERN<NR_TASKLETS>(nb_queries);

	uint32_t start_index = me() * D + (me() < K ? me() : K);
	uint32_t stop_index = start_index + D + (me() < K ? 1 : 0);
	uint32_t my_nb_queries = stop_index - start_index;

	mram_read(&map_args.seed_positions[FLOORN<8>(start_index)], cache_seed, CACHE_SEED_RESULT_SIZE * sizeof(uint32_t));
	mram_read(&map_args.query_sizes[FLOORN<8>(start_index)], cache_sizes, CACHE_SEED_RESULT_SIZE * sizeof(uint8_t));

	uint64_t results_data[CACHE_SEED_RESULT_SIZE];
	uint64_t results_error_positions[CACHE_SEED_RESULT_SIZE];

	// Each tasklet has its own read
	for (uint32_t query_idx = start_index; query_idx < stop_index; ++query_idx)
	{
		auto seed_pos = cache_seed[query_idx - start_index + REMAINDERN<8>(start_index)];
		uint32_t query_size = cache_sizes[query_idx - start_index + REMAINDERN<8>(start_index)] + 1;
		auto end_offset = (query_size & 3);
		auto data_size = (query_size + 3) >> 2;

		// Init results
		uint32_t result_distance = query_size + 1; // +1 to avoid useless comp in ties when host retrieves results
		uint32_t result_pos = 0;
		uint64_t error_positions = 0;

		uint8_t bases[4];

		/* ----------------------- Retrieve full query in WRAM ---------------------- */

		auto start_pos = query_idx * (MAX_QUERY_SIZE / 4);
		mram_read(&map_args.queries[start_pos], cache8, ((MAX_QUERY_SIZE / 4) + 8) * sizeof(uint8_t));

		/* ------------------------------ Compute key1 ------------------------------ */

		uint32_t offset2 = seed_pos & 3;
		uint32_t cache_idx = seed_pos >> 2;

		auto key1 = get_seq_key(cache_idx, offset2, cache8);
		auto cnt1 = UNPACK_UPPER8(cnt_table[key1]);

		uint32_t skip_offset = 0;

		// Start at pos given by host and try to find a seed with a low count to reduce the number of loop turns later
		if (cnt1 == 0 || cnt1 > GOOD_KEY_CNT)
		{
			bases[0] = get_seq_base(cache_idx, offset2, cache8);
			bases[1] = get_seq_base(cache_idx, offset2 + 1, cache8);
			bases[2] = get_seq_base(cache_idx, offset2 + 2, cache8);
			for (uint32_t test_offset = 0; test_offset <= SEED_SEARCH_RANGE; ++test_offset, ++offset2)
			{
				bases[3] = get_seq_base(cache_idx, offset2 + 3, cache8);
				if (is_good_seed(bases))
				{
					auto key = get_seq_key(cache_idx + (offset2 >> 2), offset2 & 3, cache8);
					auto cnt = UNPACK_UPPER8(cnt_table[key]);
					if (cnt < cnt1 || cnt1 == 0)
					{
						key1 = key;
						cnt1 = cnt;
						skip_offset = test_offset;
						if (cnt <= GOOD_KEY_CNT)
						{
							break;
						}
					}
				}
				bases[0] = bases[1];
				bases[1] = bases[2];
				bases[2] = bases[3];
			}
		}

		auto idx1 = UNPACK_LOWER24(cnt_table[key1]) - cnt1;
		auto pos1 = pos_table[idx1];

		/* ------------------------------ Compute key2 ------------------------------ */

		offset2 = (seed_pos + DELTA) & 3;
		cache_idx = (seed_pos + DELTA) >> 2;

		// Find another good seed further, again with low count if possible
		bases[0] = get_seq_base(cache_idx, offset2, cache8);
		bases[1] = get_seq_base(cache_idx, offset2 + 1, cache8);
		bases[2] = get_seq_base(cache_idx, offset2 + 2, cache8);
		uint32_t skip_offset2 = SEED_SEARCH_RANGE + 1;
		uint32_t key2;
		uint64_t cnt2 = UINT64_MAX;
		for (uint32_t test_offset = 0; test_offset <= SEED_SEARCH_RANGE; ++test_offset, ++offset2)
		{
			bases[3] = get_seq_base(cache_idx, offset2 + 3, cache8);
			if (is_good_seed(bases))
			{
				auto key = get_seq_key(cache_idx + (offset2 >> 2), offset2 & 3, cache8);
				auto cnt = UNPACK_UPPER8(cnt_table[key]);
				if (cnt != 0 && cnt < cnt2)
				{
					key2 = key;
					cnt2 = cnt;
					skip_offset2 = test_offset;
					if (cnt <= GOOD_KEY_CNT)
					{
						break;
					}
				}
			}
			bases[0] = bases[1];
			bases[1] = bases[2];
			bases[2] = bases[3];
		}
		if (skip_offset2 > SEED_SEARCH_RANGE)
		{
			results_data[query_idx - start_index] = ENCODE_MAP_RESULT(result_distance, result_pos);
			results_error_positions[query_idx - start_index] = error_positions;
			continue;
		}

		auto idx2 = UNPACK_LOWER24(cnt_table[key2]) - cnt2;
		auto pos2 = pos_table[idx2];

		/* --------------------------- Prepare comparison --------------------------- */

		uint8_t *query = cache8;
		if (end_offset > 0)
		{
			query[data_size - 1] &= EXTRACT_LEFT_MASK[end_offset];
		}
		query[data_size] = 0; // Pad some zeros at the end to not interfere during the 32 bits pop count or bit scan
		query[data_size + 1] = 0;
		query[data_size + 2] = 0;
		query[data_size + 3] = 0;

		/* ---------------------------- Compute distance ---------------------------- */

		seed_pos += skip_offset;
		uint32_t n1 = 0, n2 = 0;
		uint32_t dist_keys = DELTA + skip_offset2 - skip_offset;
		while ((n1 < cnt1) && (n2 < cnt2))
		{
			if (pos1 + dist_keys == pos2)
			{
				auto pos = pos1;
				if (pos >= seed_pos)
				{
					// Retrieve sequence part in WRAM
					start_pos = pos - seed_pos;
					mram_read(&sequence[FLOORN<8>(start_pos >> 2)], cache8bis,
							  (CEILN<8>(data_size) + 8) * sizeof(uint8_t));
					uint8_t *ref_seq = cache8bis + REMAINDERN<8>(start_pos >> 2);
					auto offset = start_pos & 3;
					auto cum_offset = end_offset + offset;
					auto ref_last_byte = end_offset == 0 ? data_size : data_size - 1;
					if (cum_offset > 4)
					{
						ref_last_byte++;
						cum_offset -= 4;
					}

					ref_seq[ref_last_byte] &= EXTRACT_LEFT_MASK[cum_offset];
					ref_seq[ref_last_byte + 1] = 0;
					ref_seq[ref_last_byte + 2] = 0;
					ref_seq[ref_last_byte + 3] = 0;
					ref_seq[ref_last_byte + 4] = 0;
					ref_seq[ref_last_byte + 5] = 0; // NB: necessary to do it until + 5: get_ref_value can read the
													// following value, so must do until data_size + 4 and account for
													// when ref_last_byte = data_size - 1

					// Compute Hamming distance
					uint32_t d = 0;
					uint32_t error_pos = 0;
					for (uint32_t i = 0; i < data_size; i += 4)
					{
						// Pack 4 values into 32 bits
						uint32_t query_value =
							(static_cast<uint32_t>(query[i]) << 24) | (static_cast<uint32_t>(query[i + 1]) << 16) |
							(static_cast<uint32_t>(query[i + 2]) << 8) | static_cast<uint32_t>(query[i + 3]);
						uint32_t ref_value = (static_cast<uint32_t>(get_ref_value(ref_seq, i, offset)) << 24) |
											 (static_cast<uint32_t>(get_ref_value(ref_seq, i + 1, offset)) << 16) |
											 (static_cast<uint32_t>(get_ref_value(ref_seq, i + 2, offset)) << 8) |
											 static_cast<uint32_t>(get_ref_value(ref_seq, i + 3, offset));

						// d += hammning_distance(query_value, ref_value);	 // Pop count, getting number of errors only

						hammning_distance_positions(query_value, ref_value, d, error_pos,
													i * 4); // Getting number of errors and positions with a bit scan

						if (d > MAX_NB_ERRORS)
						{
							break;
						}
					}

					// Check if result is better than already found
					if (d <= MAX_NB_ERRORS && d < result_distance)
					{
						result_distance = d;
						result_pos = pos - seed_pos;
						error_positions = error_pos;
						if constexpr (CONTINUE_MAPPING_IF_NOT_PERFECT_DPU)
						{
							// Break only if found perfect match
							if (d == 0)
							{
								break;
							}
						}
						else
						{
							// Break as soon as one position found
							break;
						}
					}
				}

				n1++;
				pos1 = pos_table[idx1 + n1];
				n2++;
				pos2 = pos_table[idx2 + n2];
			}
			else if (pos1 + dist_keys < pos2)
			{
				n1++;
				pos1 = pos_table[idx1 + n1];
			}
			else
			{
				n2++;
				pos2 = pos_table[idx2 + n2];
			}
		}

		// Write results in WRAM
		results_data[query_idx - start_index] = ENCODE_MAP_RESULT(result_distance, result_pos);
		results_error_positions[query_idx - start_index] = error_positions;
	}

	// Commit results in MRAM
	mutex_pool_lock(&mutex_pool16, 0);
	mram_read(&map_results.data[FLOORN<8>(start_index)], cache_result, CACHE_SEED_RESULT_SIZE * sizeof(uint64_t));
	for (uint32_t i = 0; i < my_nb_queries; ++i)
	{
		cache_result[i + REMAINDERN<8>(start_index)] = results_data[i];
	}
	mram_write(cache_result, &map_results.data[FLOORN<8>(start_index)], CACHE_SEED_RESULT_SIZE * sizeof(uint64_t));
	mram_read(&map_results.error_positions[FLOORN<8>(start_index)], cache_result,
			  CACHE_SEED_RESULT_SIZE * sizeof(uint64_t));
	for (uint32_t i = 0; i < my_nb_queries; ++i)
	{
		cache_result[i + REMAINDERN<8>(start_index)] = results_error_positions[i];
	}
	mram_write(cache_result, &map_results.error_positions[FLOORN<8>(start_index)],
			   CACHE_SEED_RESULT_SIZE * sizeof(uint64_t));
	mutex_pool_unlock(&mutex_pool16, 0);
}

/* -------------------------------------------------------------------------- */
/*                                    Main                                    */
/* -------------------------------------------------------------------------- */

int main()
{
	__dma_aligned uint8_t cache8[CACHE8_SIZE];
	__dma_aligned uint8_t cache8bis[MAX_QUERY_SIZE + 32];
	__dma_aligned uint32_t cache_seed[CACHE_SEED_RESULT_SIZE];
	__dma_aligned uint64_t cache_result[CACHE_SEED_RESULT_SIZE];

	uint8_t *cache_sizes = (uint8_t *)cache_result;

	if (index_built)
	{
#ifdef DO_DPU_PERFCOUNTER
		if (me() == 0)
		{
#if (DO_DPU_PERFCOUNTER == 0)
			perfcounter_config(COUNT_CYCLES, true);
#else
			perfcounter_config(COUNT_INSTRUCTIONS, true);
#endif
		}
#endif

		map(cache8, cache8bis, cache_seed, cache_sizes, cache_result);

#ifdef DO_DPU_PERFCOUNTER
		barrier_wait(&barrier_all);
		if (me() == 0)
		{
			checksum = perfcounter_get();
		}
#endif
	}
	else
	{
		index(cache8);
		if (me() == 0)
		{
			index_built = true;
		}
	}

	return 0;
}