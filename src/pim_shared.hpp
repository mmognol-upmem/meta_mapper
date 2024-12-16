#include <cstdint>

template <typename T>
constexpr bool is_good_seed(T t, uint32_t i = 0)
{
    return (t[i] != t[i + 1]) && (t[i] != t[i + 2]) && (t[i] != t[i + 3]);
}

template <typename T>
constexpr bool IS_POWER_2(T n)
{
    return (n & (n - 1)) == 0;
}

template <unsigned int N, typename T>
constexpr T CEILN(T value)
{
    static_assert(IS_POWER_2(N), "N must be a power of 2");
    return (value + (N - 1)) & ~(N - 1);
}