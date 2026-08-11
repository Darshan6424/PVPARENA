#pragma once
// A 2D vector and a couple of helpers.
//
// Vec2 keeps x and y public on purpose. Encapsulation exists to protect an
// invariant - some rule about the data that must always hold. A 2D vector has
// no such rule: every pair of floats is a valid vector. Hiding them behind
// get/set pairs would add noise without protecting anything. Compare with
// Fighter, where health and state really do have rules, and are private.

#include <cmath>
#include <algorithm>

class Vec2 {
public:
    float x = 0.f;
    float y = 0.f;

    constexpr Vec2() = default;
    constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}

    constexpr Vec2 operator+(const Vec2& o) const { return { x + o.x, y + o.y }; }
    constexpr Vec2 operator-(const Vec2& o) const { return { x - o.x, y - o.y }; }
    constexpr Vec2 operator*(float s) const { return { x * s, y * s }; }
    constexpr Vec2 operator/(float s) const { return { x / s, y / s }; }
    constexpr Vec2 operator-() const { return { -x, -y }; }

    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    Vec2& operator*=(float s) { x *= s; y *= s; return *this; }

    constexpr bool operator==(const Vec2& o) const { return x == o.x && y == o.y; }
    constexpr bool operator!=(const Vec2& o) const { return !(*this == o); }

    constexpr float dot(const Vec2& o) const { return x * o.x + y * o.y; }

    // Prefer this over length() when you only need to compare distances - it
    // skips the square root.
    constexpr float lengthSquared() const { return x * x + y * y; }
    float length() const { return std::sqrt(lengthSquared()); }

    bool isNearlyZero(float epsilon = 1e-4f) const { return lengthSquared() < epsilon * epsilon; }

    Vec2 normalized() const {
        float len = length();
        if (len < 1e-6f) return {};
        return { x / len, y / len };
    }
};

// So both 2.f * v and v * 2.f work.
constexpr Vec2 operator*(float s, const Vec2& v) { return v * s; }

inline float clampf(float v, float lo, float hi) {
    return (std::max)(lo, (std::min)(hi, v));
}

inline float distance(const Vec2& a, const Vec2& b) {
    return (a - b).length();
}
