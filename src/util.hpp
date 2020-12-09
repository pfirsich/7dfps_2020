#pragma once

#include <algorithm>
#include <optional>
#include <string>

float rescale(float val, float fromMin, float fromMax, float toMin, float toMax);

// I would use boost's lexical cast, if I had another reason to use boost
template <typename T = long long>
std::optional<T> parseInt(const std::string& str, int base = 10)
{
    static constexpr auto min = std::numeric_limits<T>::min();
    static constexpr auto max = std::numeric_limits<T>::max();
    try {
        size_t pos = 0;
        const auto val = std::stoll(str, &pos, base);
        if (pos < str.size())
            return std::nullopt;
        if (val < min || val > max)
            return std::nullopt;
        return static_cast<T>(val);
    } catch (const std::exception& exc) {
        return std::nullopt;
    }
}

std::string hexStream(const uint8_t* data, size_t len);
