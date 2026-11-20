#pragma once
#include <cmath>
#include <algorithm>

namespace MfgLE {
inline float Clamp(float x, float low, float high) {
    return std::isfinite(x) ? std::max(low, std::min(x, high)) : low;
}
inline float Step(float current, float target, float dt, float duration) {
    if (duration <= 0.0f) return target;
    const float step = Clamp(dt, 0.0f, 1.0f) / duration;
    if (std::fabs(target - current) <= step) return target;
    return current + (target > current ? step : -step);
}
inline bool ValidRequest(int mode, int id) {
    return mode == -1 || (mode == 0 && id >= 0 && id < 16) ||
           (mode == 1 && id >= 0 && id < 14) || (mode == 2 && id >= 0 && id < 17);
}
// Locale-independent decimal parser, used for every numeric configuration value.
inline bool Decimal(const char* text, float& result) {
    while (*text == ' ' || *text == '\t') ++text;
    bool neg = *text == '-';
    if (*text == '-' || *text == '+') ++text;
    double number = 0, fraction = 0.1;
    bool digit = false, separator = false;
    for (; *text; ++text) {
        if (*text >= '0' && *text <= '9') {
            digit = true;
            if (separator) { number += (*text - '0') * fraction; fraction *= 0.1; }
            else number = number * 10 + (*text - '0');
        } else if ((*text == '.' || *text == ',') && !separator) separator = true;
        else if (*text == ' ' || *text == '\t') break;
        else return false;
    }
    while (*text == ' ' || *text == '\t') ++text;
    if (!digit || *text || !std::isfinite(number) || number > 1000000) return false;
    result = static_cast<float>(neg ? -number : number);
    return true;
}
}
