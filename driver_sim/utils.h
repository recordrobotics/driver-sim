#pragma once

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <string>
#include <string_view>

template <typename T> constexpr void hash_combine(std::size_t &seed, const T &value)
{
    seed ^= std::hash<T>{}(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

constexpr bool bit_equal(float compA, float compB)
{
    return std::bit_cast<uint32_t>(compA) == std::bit_cast<uint32_t>(compB);
}

template <typename T>
constexpr int64_t floorc(T num)
    requires std::is_floating_point<T>::value
{
    int64_t res{};
    res = static_cast<int64_t>(num);
    if (res > num)
    {
        --res;
    }

    return res;
}

template <typename T> constexpr T ipow(T num, unsigned int pow)
{
    return (pow >= sizeof(unsigned int) * 8) ? 0 : pow == 0 ? 1 : num * ipow(num, pow - 1);
}

// NOLINTBEGIN(modernize-use-std-numbers,readability-magic-numbers,readability-math-missing-parentheses)
constexpr float pow2_frac(float value)
{
    if (value < 0.0f || value >= 1.0f)
    {
        throw "pow2_frac argument must be in [0, 1)";
    }

    return 1.0f +
           value *
               (0.6931471805599453f +
                value *
                    (0.240226506959101f +
                     value *
                         (0.05550410866482158f +
                          value * (0.009618129107628477f +
                                   value * (0.001333355814642844f +
                                            value * (0.0001540353039338166f +
                                                     value * (1.321543679207e-05f +
                                                              value * 9.332594808954e-07f)))))));
}
// NOLINTEND(modernize-use-std-numbers,readability-magic-numbers,readability-math-missing-parentheses)

// raises 2 to the power of x
constexpr float pow2(float x)
{
    if (x > 100000)
    {
        throw "pow2 overflow";
    }

    return ipow(2.0f, floorc(x)) * pow2_frac(x - floorc(x));
}

template <typename T>
concept StringLike = std::convertible_to<T, std::string_view>;

template <std::ranges::input_range R>
    requires StringLike<std::ranges::range_reference_t<R>>
inline std::string string_join(R &&values, std::string_view delimiter)
{
    std::size_t size = 0;

    for (const auto &value : values)
    {
        size += value.size();
    }

    if (values.size() > 1)
    {
        size += (values.size() - 1) * delimiter.size();
    }

    std::string result;
    result.reserve(size);

    for (std::size_t i = 0; i < values.size(); ++i)
    {
        if (i != 0)
        {
            result += delimiter;
        }

        result += values[i];
    }

    return result;
}