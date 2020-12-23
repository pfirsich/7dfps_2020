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

float approach(float current, float target, float delta)
{
    assert(delta > 0.0f);
    const auto diff = target - current;
    if (diff > 0.0f) {
        current += std::min(diff, delta);
    } else if (diff < 0.0f) {
        current -= std::min(-diff, delta);
    }
    return current;
}

std::optional<float> parseFloat(const std::string& str)
{
    try {
        size_t pos = 0;
        const auto val = std::stof(str, &pos);
        if (pos < str.size())
            return std::nullopt;
        return val;
    } catch (const std::exception& exc) {
        return std::nullopt;
    }
}

std::string hexStream(const uint8_t* data, size_t len)
{
    std::string str;
    for (size_t i = 0; i < len; ++i) {
        str.append(fmt::format("{:02x} ", data[i]));
    }
    return str;
}

std::string toLower(const std::string& str)
{
    std::string out;
    out.reserve(str.size());
    for (const auto ch : str)
        out.push_back(
            static_cast<char>(std::tolower(static_cast<int>(static_cast<unsigned char>(ch)))));
    return out;
}

std::vector<std::string> split(const std::string& str)
{
    std::vector<std::string> parts;
    // Welcome to clown town
    auto isWhitespace
        = [](char c) { return std::isspace(static_cast<int>(static_cast<unsigned char>(c))) != 0; };
    auto skip = [&str, &isWhitespace](size_t pos, bool skipWhitespace) {
        while (pos < str.size() && isWhitespace(str[pos]) == skipWhitespace)
            pos++;
        return pos;
    };
    size_t pos = 0;
    while (pos < str.size()) {
        pos = skip(pos, true);
        const auto len = skip(pos, false) - pos;
        if (len > 0)
            parts.push_back(str.substr(pos, len));
        pos += len;
    }
    return parts;
}
