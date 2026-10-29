#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace neon {
constexpr float pi = 3.14159265358979323846f;
struct Vec2 {
    float x{}, y{};
    constexpr Vec2 operator+(Vec2 v) const { return {x + v.x, y + v.y}; }
    constexpr Vec2 operator-(Vec2 v) const { return {x - v.x, y - v.y}; }
    constexpr Vec2 operator*(float n) const { return {x * n, y * n}; }
    constexpr Vec2 operator/(float n) const { return {x / n, y / n}; }
    Vec2& operator+=(Vec2 v) {
        x += v.x;
        y += v.y;
        return *this;
    }
    Vec2& operator-=(Vec2 v) {
        x -= v.x;
        y -= v.y;
        return *this;
    }
    Vec2& operator*=(float n) {
        x *= n;
        y *= n;
        return *this;
    }
};
constexpr float dot(Vec2 a, Vec2 b) {
    return a.x * b.x + a.y * b.y;
}
inline float length(Vec2 v) {
    return std::sqrt(dot(v, v));
}
inline float distance(Vec2 a, Vec2 b) {
    return length(a - b);
}
inline bool finite(Vec2 v) {
    return std::isfinite(v.x) && std::isfinite(v.y);
}
inline Vec2 normalized(Vec2 v) {
    float n = std::hypot(v.x, v.y);
    return std::isfinite(n) && n > 0.0001f ? v / n : Vec2{};
}
inline Vec2 direction(float a) {
    return {std::cos(a), std::sin(a)};
}
inline float angle(Vec2 v) {
    return std::atan2(v.y, v.x);
}
inline float wrapAngle(float a) {
    return std::remainder(a, 2.0f * pi);
}
inline float approach(float a, float b, float amount) {
    return a < b ? std::min(a + amount, b) : std::max(a - amount, b);
}
inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}
inline Vec2 lerp(Vec2 a, Vec2 b, float t) {
    return a + (b - a) * t;
}
struct Rect {
    float x{}, y{}, w{}, h{};
    bool contains(Vec2 p) const { return p.x >= x && p.y >= y && p.x <= x + w && p.y <= y + h; }
    bool overlaps(Rect b) const { return x < b.x + b.w && x + w > b.x && y < b.y + b.h && y + h > b.y; }
    Vec2 center() const { return {x + w * 0.5f, y + h * 0.5f}; }
};
inline bool circleRect(Vec2 p, float radius, Rect r) {
    Vec2 q{std::clamp(p.x, r.x, r.x + r.w), std::clamp(p.y, r.y, r.y + r.h)};
    return dot(p - q, p - q) < radius * radius;
}
inline float segmentRectTime(Vec2 a, Vec2 b, Rect r) {
    if (!finite(a) || !finite(b))
        return std::numeric_limits<float>::infinity();
    float lo = 0.0f, hi = 1.0f;
    Vec2 d = b - a;
    const float starts[]{a.x, a.y}, deltas[]{d.x, d.y};
    const float mins[]{r.x, r.y}, maxs[]{r.x + r.w, r.y + r.h};
    for (int i = 0; i < 2; ++i) {
        if (std::abs(deltas[i]) < 0.00001f) {
            if (starts[i] < mins[i] || starts[i] > maxs[i])
                return std::numeric_limits<float>::infinity();
        } else {
            float p = (mins[i] - starts[i]) / deltas[i], q = (maxs[i] - starts[i]) / deltas[i];
            if (p > q)
                std::swap(p, q);
            lo = std::max(lo, p);
            hi = std::min(hi, q);
            if (lo > hi)
                return std::numeric_limits<float>::infinity();
        }
    }
    return lo;
}
inline bool segmentRect(Vec2 a, Vec2 b, Rect r) {
    return std::isfinite(segmentRectTime(a, b, r));
}
inline float segmentCircleTime(Vec2 a, Vec2 b, Vec2 center, float radius) {
    Vec2 offset = a - center, delta = b - a;
    float c = dot(offset, offset) - radius * radius;
    if (c <= 0)
        return 0;
    float aa = dot(delta, delta), bb = dot(offset, delta);
    float discriminant = bb * bb - aa * c;
    if (aa <= 0 || discriminant < 0)
        return std::numeric_limits<float>::infinity();
    float t = (-bb - std::sqrt(discriminant)) / aa;
    return t >= 0 && t <= 1 ? t : std::numeric_limits<float>::infinity();
}
class Rng {
    std::uint64_t state_;

  public:
    explicit Rng(std::uint64_t seed = 0x4e454f4eULL) : state_(seed ? seed : 1) {}
    std::uint32_t next() {
        state_ ^= state_ >> 12;
        state_ ^= state_ << 25;
        state_ ^= state_ >> 27;
        return static_cast<std::uint32_t>((state_ * 2685821657736338717ULL) >> 32);
    }
    int integer(int lo, int hi) {
        if (lo > hi)
            std::swap(lo, hi);
        const std::uint64_t span = static_cast<std::uint64_t>(static_cast<std::int64_t>(hi) - lo) + 1;
        constexpr std::uint64_t domain = std::uint64_t{1} << 32;
        const std::uint64_t limit = domain - domain % span;
        std::uint64_t value;
        do {
            value = next();
        } while (value >= limit);
        return static_cast<int>(static_cast<std::int64_t>(lo) + static_cast<std::int64_t>(value % span));
    }
    float unit() { return static_cast<float>(next() >> 8) / 16777216.0f; }
    float range(float lo, float hi) { return lerp(lo, hi, unit()); }
    std::uint64_t state() const { return state_; }
};
} // namespace neon
