#ifndef F1732F2C_3933_4152_B61E_DE811DFBC127
#define F1732F2C_3933_4152_B61E_DE811DFBC127

/* -------------------------------------------------------------------------- */
/*                                Const values                                */
/* -------------------------------------------------------------------------- */

constexpr uint32_t DPU_SEED_SIZE = 13;

constexpr uint32_t INDEX_SIZE2 = 22; // If > (DPU_SEED_SIZE * 2), there will be collisions in the table
static_assert(INDEX_SIZE2 < 32, "Hash is on 32 bits");

constexpr uint32_t INDEX_SIZE = 1 << (INDEX_SIZE2);
constexpr uint32_t INDEX_POS_SIZE = 1 << 23;

constexpr uint8_t EXTRACT_RIGHT_MASK[4] = {0b1111'1111, 0b0011'1111, 0b0000'1111, 0b0000'0011};
constexpr uint8_t EXTRACT_LEFT_MASK[5] = {0, 0b1100'0000, 0b1111'0000, 0b1111'1100, 0b1111'1111};
constexpr uint8_t SHIFT_LENGTH[4] = {0, 2, 4, 6};

constexpr uint64_t MAX_KEY_CNT = 100;
static_assert(MAX_KEY_CNT < 512, "Max cnt for a seed must fit in 8 bits");

constexpr uint32_t SEED_SEARCH_RANGE = 30;
constexpr uint32_t DELTA = 30;
constexpr uint32_t GOOD_KEY_CNT = 30;

/* -------------------------------------------------------------------------- */
/*                              Useful functions                              */
/* -------------------------------------------------------------------------- */

#ifdef LOG_DPU
extern "C"
{
	int printf(const char *fmt, ...); // HACK: Cannot include stdio.h
}
#define dpu_printf(...) printf(__VA_ARGS__)
#define dpu_printf_me(fmt, ...) printf("[%02d] " fmt, me(), ##__VA_ARGS__)
#define dpu_printf_0(...)           \
	if (me() == 0)                  \
	{                               \
		dpu_printf_me(__VA_ARGS__); \
	}
#else
#define dpu_printf(...)
#define dpu_printf_me(...)
#define dpu_printf_0(...)
#endif

template <typename T>
constexpr inline T MIN(T a, T b)
{
	return a < b ? a : b;
}

template <typename T>
constexpr inline T MAX(T a, T b)
{
	return a > b ? a : b;
}

template <typename T>
constexpr T UNPACK_UPPER8(T val)
{
	return val >> 24;
}

template <typename T>
constexpr T UNPACK_LOWER24(T val)
{
	return val & 0x00FF'FFFF;
}

constexpr uint64_t PACK64(uint32_t val_upper, uint32_t val_lower)
{
	return (static_cast<uint64_t>(val_upper) << 32) + val_lower;
}

constexpr uint64_t P2_24 = (1UL << 24);

#endif /* F1732F2C_3933_4152_B61E_DE811DFBC127 */
