#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "components.hpp"

float lerp(float a, float b, float t);
float unlerp(float val, float a, float b);
float rescale(float val, float fromA, float fromB, float toA, float toB);

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

std::optional<float> parseFloat(const std::string& str);

std::string hexStream(const uint8_t* data, size_t len);

std::string toLower(const std::string& str);

std::vector<std::string> split(const std::string& str);

ecs::EntityHandle findEntity(ecs::World& world, const std::string& name);
