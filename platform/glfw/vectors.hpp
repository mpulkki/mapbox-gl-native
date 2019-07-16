#pragma once

#include <mbgl/util/mat4.hpp>
#include <float.h>

namespace mbgl {

struct Vec3f { float x; float y; float z; };
struct Vec4f { float x; float y; float z; float w; };

struct Bounds3f {
    Vec3f min;
    Vec3f max;

    Bounds3f()
        : min({ 0, 0, 0 })
        , max({ 0, 0, 0 })
    { }

    Bounds3f(const Vec3f& mn, const Vec3f& mx)
        : min(mn)
        , max(mx)
    { }

    inline void inflate(const Vec3f& v) {
        min.x = std::min(min.x, v.x);
        min.y = std::min(min.y, v.y);
        min.z = std::min(min.z, v.z);
        max.x = std::max(max.x, v.x);
        max.y = std::max(max.y, v.y);
        max.z = std::max(max.z, v.z);
    }
};

inline Bounds3f projectToClipSpace(const Bounds3f& bounds, const mat4& wvp) {
    const Vec3f& mn = bounds.min;
    const Vec3f& mx = bounds.max;

    vec4 corners[8] = {
        { mn.x, mn.y, mn.z, 1.0f},
        { mx.x, mn.y, mn.z, 1.0f},
        { mn.x, mx.y, mn.z, 1.0f},
        { mx.x, mx.y, mn.z, 1.0f},
        { mn.x, mn.y, mx.z, 1.0f},
        { mx.x, mn.y, mx.z, 1.0f},
        { mn.x, mx.y, mx.z, 1.0f},
        { mx.x, mx.y, mx.z, 1.0f}
    };

    Vec3f clipMin = { FLT_MAX, FLT_MAX, FLT_MAX };
    Vec3f clipMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

    for (const vec4& corner : corners) {
        vec4 clipPos;
        matrix::transformMat4(clipPos, corner, wvp);
        if (clipPos[3] != 0.0f) {
            clipPos[0] /= clipPos[3];
            clipPos[1] /= clipPos[3];
        }

        clipMin.x = std::min((float)clipPos[0], clipMin.x);
        clipMin.y = std::min((float)clipPos[1], clipMin.y);
        clipMax.x = std::max((float)clipPos[0], clipMax.x);
        clipMax.y = std::max((float)clipPos[1], clipMax.y);
    }

    return { clipMin, clipMax };
}

inline Vec3f cross(const Vec3f& a, const Vec3f& b) {
    Vec3f v;
    v.x = a.y * b.z - a.z * b.y;
    v.y = -a.x * b.z + a.z * b.x;
    v.z = a.x * b.y - a.y * b.x;
    return v;
}

inline Vec3f toVec3f(const Vec4f& v) {
    return { v.x, v.y, v.z };
}

inline Vec4f toVec4f(const Vec3f& v, float w = 1.0f) {
    return { v.x, v.y, v.z, w };
}

inline Vec3f operator-(const Vec3f& a, const Vec3f& b) {
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

inline Vec3f operator+(const Vec3f& a, const Vec3f& b) {
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

inline Vec3f operator/(const Vec3f& v, float s) {
    return { v.x / s, v.y / s, v.z / s };
}

inline float length(const Vec3f& v) {
    return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
}

inline Vec3f norm(const Vec3f& v) {
    return v / length(v);
}
}