#pragma once

#include <math.h>

#include "Math/Algebra.hpp"

namespace pip3D
{

    struct GJKVertex
    {
        Vector3 m;
        Vector3 pA;
        Vector3 pB;
    };

    __attribute__((always_inline)) inline Vector3 v3cross(const Vector3 &a, const Vector3 &b)
    {
        return Vector3(a.y * b.z - a.z * b.y,
                       a.z * b.x - a.x * b.z,
                       a.x * b.y - a.y * b.x);
    }
    __attribute__((always_inline)) inline float v3dot(const Vector3 &a, const Vector3 &b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    __attribute__((always_inline)) inline Vector3 v3sub(const Vector3 &a, const Vector3 &b)
    {
        return Vector3(a.x - b.x, a.y - b.y, a.z - b.z);
    }
    __attribute__((always_inline)) inline Vector3 v3add(const Vector3 &a, const Vector3 &b)
    {
        return Vector3(a.x + b.x, a.y + b.y, a.z + b.z);
    }
    __attribute__((always_inline)) inline Vector3 v3scale(const Vector3 &a, float s)
    {
        return Vector3(a.x * s, a.y * s, a.z * s);
    }
    __attribute__((always_inline)) inline float v3len(const Vector3 &a)
    {
        return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
    }
    __attribute__((always_inline)) inline Vector3 v3neg(const Vector3 &a)
    {
        return Vector3(-a.x, -a.y, -a.z);
    }
}
