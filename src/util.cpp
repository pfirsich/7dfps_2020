#include "util.hpp"

#include <algorithm>

#include <fmt/format.h>

float rescale(float val, float fromMin, float fromMax, float toMin, float toMax)
{
    const auto clampVal = std::clamp(val, fromMin, fromMax);
    const auto t = (clampVal - fromMin) / (fromMax - fromMin);
    return toMin + t * (toMax - toMin);
}

std::string hexStream(const uint8_t* data, size_t len)
{
    std::string str;
    for (size_t i = 0; i < len; ++i) {
        str.append(fmt::format("{:02x} ", data[i]));
    }
    return str;
}
