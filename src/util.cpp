#include "util.hpp"

#include <algorithm>

#include <fmt/format.h>

float lerp(float a, float b, float t)
{
    return a + t * (b - a);
}

float unlerp(float val, float a, float b)
{
    const auto clamped = std::clamp(val, std::min(a, b), std::max(a, b));
    return (clamped - a) / (b - a);
}

float rescale(float val, float fromA, float fromB, float toA, float toB)
{
    return lerp(toA, toB, unlerp(val, fromA, fromB));
}

std::string hexStream(const uint8_t* data, size_t len)
{
    std::string str;
    for (size_t i = 0; i < len; ++i) {
        str.append(fmt::format("{:02x} ", data[i]));
    }
    return str;
}
