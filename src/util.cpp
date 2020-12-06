#include "util.hpp"

#include <algorithm>

float rescale(float val, float fromMin, float fromMax, float toMin, float toMax)
{
    const auto clampVal = std::clamp(val, fromMin, fromMax);
    const auto t = (clampVal - fromMin) / (fromMax - fromMin);
    return toMin + t * (toMax - toMin);
}
