#pragma once

#include <cstddef>
#include <bit>
#include <cstdint>

template <typename T>
inline constexpr void hash_combine(std::size_t &seed, const T &v)
{
    seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

inline constexpr bool bit_equal(float a, float b)
{
    return std::bit_cast<uint32_t>(a) ==
           std::bit_cast<uint32_t>(b);
}

template<typename T,
         typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
constexpr int64_t floorc(T num)
{
    int64_t res{};
    res = static_cast<int64_t>(num);
    if(res > num) --res;
    
    return res;
}

template <typename T>
constexpr T ipow(T num, unsigned int pow)
{
    return (pow >= sizeof(unsigned int)*8) ? 0 :
        pow == 0 ? 1 : num * ipow(num, pow-1);
}

constexpr float pow2_frac(float x)
{
    if(x < 0.0f || x >= 1.0f) {
        throw "pow2_frac argument must be in [0, 1)";
    }

    return 1.0f + x * (0.6931471805599453f + x * (0.240226506959101f + x * (0.05550410866482158f + x * (0.009618129107628477f + x * (0.001333355814642844f + x * (0.0001540353039338166f + x * (1.321543679207e-05f + x * 9.332594808954e-07f)))))));
}

// raises 2 to the power of x
constexpr float pow2(float x)
{
    if(x > 100000) {
        throw "pow2 overflow";
    }

    return ipow(2.0f, floorc(x)) * pow2_frac(x - floorc(x));
}