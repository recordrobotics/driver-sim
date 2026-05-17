#pragma once

#include <cstddef>
#include <bit>
#include <cstdint>

template <typename T>
inline void hash_combine(std::size_t &seed, const T &v)
{
    seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

inline bool bit_equal(float a, float b)
{
    return std::bit_cast<uint32_t>(a) ==
           std::bit_cast<uint32_t>(b);
}