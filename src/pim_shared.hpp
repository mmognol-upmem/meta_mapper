#include <cstdint>

template <typename T>
constexpr bool is_good_seed(T t, uint32_t i = 0)
{
    return (t[i] != t[i + 1]) && (t[i] != t[i + 2]) && (t[i] != t[i + 3]);
}