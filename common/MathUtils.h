#pragma once
#include <cmath>
#include <algorithm>

struct Vec2 {
    float x = 0.f, y = 0.f;

    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}

    Vec2 operator+(const Vec2& o) const { return { x + o.x, y + o.y }; }
    Vec2 operator-(const Vec2& o) const { return { x - o.x, y - o.y }; }
    Vec2 operator*(float s)       const { return { x * s, y * s }; }

    float length() const { return std::sqrt(x * x + y * y); }

    Vec2 normalized() const {
        float len = length();
        if (len < 1e-6f) return { 0.f, 0.f };
        return { x / len, y / len };
    }
};

inline float clampf(float v, float lo, float hi) {
    return (std::max)(lo, (std::min)(hi, v));
}

inline float distance(const Vec2& a, const Vec2& b) {
    return (a - b).length();
}
