#pragma once

#include <cmath>
#include <cstdint>

#include "Core/Platform.hpp"

namespace pip3D
{
    inline constexpr float kPi = 3.1415927f;
    inline constexpr float kTwoPi = 6.2831855f;
    inline constexpr float kHalfPi = 1.5707963f;
    inline constexpr float kInvPi = 1.0f / kPi;
    inline constexpr float kInvTwoPi = 1.0f / kTwoPi;
    inline constexpr float kDegToRad = 0.017453292f;
    inline constexpr float kRadToDeg = 57.29578f;

    namespace detail
    {
        inline const float kSinTable[256] PIP3D_FAST_DATA = {
            0.000000f, 0.024541f, 0.049068f, 0.073565f, 0.098017f, 0.122411f, 0.146730f, 0.170962f,
            0.195090f, 0.219101f, 0.242980f, 0.266713f, 0.290285f, 0.313682f, 0.336890f, 0.359895f,
            0.382683f, 0.405241f, 0.427555f, 0.449611f, 0.471397f, 0.492898f, 0.514103f, 0.534998f,
            0.555570f, 0.575808f, 0.595699f, 0.615232f, 0.634393f, 0.653173f, 0.671559f, 0.689541f,
            0.707107f, 0.724247f, 0.740951f, 0.757209f, 0.773010f, 0.788346f, 0.803208f, 0.817585f,
            0.831470f, 0.844854f, 0.857729f, 0.870087f, 0.881921f, 0.893224f, 0.903989f, 0.914210f,
            0.923880f, 0.932993f, 0.941544f, 0.949528f, 0.956940f, 0.963776f, 0.970031f, 0.975702f,
            0.980785f, 0.985278f, 0.989177f, 0.992480f, 0.995185f, 0.997290f, 0.998795f, 0.999699f,
            1.000000f, 0.999699f, 0.998795f, 0.997290f, 0.995185f, 0.992480f, 0.989177f, 0.985278f,
            0.980785f, 0.975702f, 0.970031f, 0.963776f, 0.956940f, 0.949528f, 0.941544f, 0.932993f,
            0.923880f, 0.914210f, 0.903989f, 0.893224f, 0.881921f, 0.870087f, 0.857729f, 0.844854f,
            0.831470f, 0.817585f, 0.803208f, 0.788346f, 0.773010f, 0.757209f, 0.740951f, 0.724247f,
            0.707107f, 0.689541f, 0.671559f, 0.653173f, 0.634393f, 0.615232f, 0.595699f, 0.575808f,
            0.555570f, 0.534998f, 0.514103f, 0.492898f, 0.471397f, 0.449611f, 0.427555f, 0.405241f,
            0.382683f, 0.359895f, 0.336890f, 0.313682f, 0.290285f, 0.266713f, 0.242980f, 0.219101f,
            0.195090f, 0.170962f, 0.146730f, 0.122411f, 0.098017f, 0.073565f, 0.049068f, 0.024541f,
            0.000000f, -0.024541f, -0.049068f, -0.073565f, -0.098017f, -0.122411f, -0.146730f, -0.170962f,
            -0.195090f, -0.219101f, -0.242980f, -0.266713f, -0.290285f, -0.313682f, -0.336890f, -0.359895f,
            -0.382683f, -0.405241f, -0.427555f, -0.449611f, -0.471397f, -0.492898f, -0.514103f, -0.534998f,
            -0.555570f, -0.575808f, -0.595699f, -0.615232f, -0.634393f, -0.653173f, -0.671559f, -0.689541f,
            -0.707107f, -0.724247f, -0.740951f, -0.757209f, -0.773010f, -0.788346f, -0.803208f, -0.817585f,
            -0.831470f, -0.844854f, -0.857729f, -0.870087f, -0.881921f, -0.893224f, -0.903989f, -0.914210f,
            -0.923880f, -0.932993f, -0.941544f, -0.949528f, -0.956940f, -0.963776f, -0.970031f, -0.975702f,
            -0.980785f, -0.985278f, -0.989177f, -0.992480f, -0.995185f, -0.997290f, -0.998795f, -0.999699f,
            -1.000000f, -0.999699f, -0.998795f, -0.997290f, -0.995185f, -0.992480f, -0.989177f, -0.985278f,
            -0.980785f, -0.975702f, -0.970031f, -0.963776f, -0.956940f, -0.949528f, -0.941544f, -0.932993f,
            -0.923880f, -0.914210f, -0.903989f, -0.893224f, -0.881921f, -0.870087f, -0.857729f, -0.844854f,
            -0.831470f, -0.817585f, -0.803208f, -0.788346f, -0.773010f, -0.757209f, -0.740951f, -0.724247f,
            -0.707107f, -0.689541f, -0.671559f, -0.653173f, -0.634393f, -0.615232f, -0.595699f, -0.575808f,
            -0.555570f, -0.534998f, -0.514103f, -0.492898f, -0.471397f, -0.449611f, -0.427555f, -0.405241f,
            -0.382683f, -0.359895f, -0.336890f, -0.313682f, -0.290285f, -0.266713f, -0.242980f, -0.219101f,
            -0.195090f, -0.170962f, -0.146730f, -0.122411f, -0.098017f, -0.073565f, -0.049068f, -0.024541f};
    }

    class FastMath
    {
    public:
        PIP3D_FORCE_INLINE static uint16_t radToBin(float rad) noexcept
        {
            return static_cast<uint16_t>(static_cast<int32_t>(rad * 10430.378f));
        }

        PIP3D_FORCE_INLINE static void fastSinCosBin(uint16_t angle,
                                                     float &outSin, float &outCos) noexcept
        {
            const uint8_t idx = static_cast<uint8_t>(angle >> 8);
            const uint8_t idxNext = static_cast<uint8_t>(idx + 1);
            const uint8_t idxCos = static_cast<uint8_t>(idx + 64);
            const uint8_t idxCosN = static_cast<uint8_t>(idxCos + 1);
            const float f = static_cast<float>(angle & 0xFF) * 0.00390625f;

            const float ys0 = detail::kSinTable[idx];
            const float ys1 = detail::kSinTable[idxNext];
            const float yc0 = detail::kSinTable[idxCos];
            const float yc1 = detail::kSinTable[idxCosN];

            outSin = ys0 + f * (ys1 - ys0);
            outCos = yc0 + f * (yc1 - yc0);
        }

        PIP3D_FORCE_INLINE static float fastSin(float rad) noexcept
        {
            return fastSinBin(radToBin(rad));
        }

        PIP3D_FORCE_INLINE static float fastCos(float rad) noexcept
        {
            return fastSinBin(static_cast<uint16_t>(radToBin(rad) + 16384u));
        }

        PIP3D_FORCE_INLINE static void fastSinCos(float rad,
                                                  float &outSin, float &outCos) noexcept
        {
            fastSinCosBin(radToBin(rad), outSin, outCos);
        }

        PIP3D_FORCE_INLINE static float fastReciprocal(float input) noexcept
        {
#if defined(ESP_PLATFORM) || defined(ESP32)
            float result, temp;
            __asm__(
                "recip0.s %0, %2\n"
                "const.s %1, 1\n"
                "msub.s %1, %2, %0\n"
                "madd.s %0, %0, %1\n"
                "const.s %1, 1\n"
                "msub.s %1, %2, %0\n"
                "maddn.s %0, %0, %1\n"
                : "=&f"(result), "=&f"(temp) : "f"(input));
            return result;
#else
            return 1.0f / input;
#endif
        }

        PIP3D_FORCE_INLINE static float fastInvSqrt(float input) noexcept
        {
#if defined(ESP_PLATFORM) || defined(ESP32)
            float result;
            float f0, f2, f4, f5;
            __asm__ __volatile__(
                "rsqrt0.s %1, %5\n"
                "mul.s %2, %5, %1\n"
                "const.s %3, 3\n"
                "mul.s %3, %3, %1\n"
                "const.s %4, 1\n"
                "msub.s %4, %2, %1\n"
                "maddn.s %1, %3, %4\n"
                "mov.s %0, %1\n"
                : "=f"(result), "=&f"(f0), "=&f"(f2), "=&f"(f4), "=&f"(f5)
                : "f"(input));
            return result;
#else
            return 1.0f / sqrtf(input);
#endif
        }

    private:
        PIP3D_FORCE_INLINE static float fastSinBin(uint16_t angle) noexcept
        {
            const uint8_t idx = static_cast<uint8_t>(angle >> 8);
            const uint8_t idxNext = static_cast<uint8_t>(idx + 1);
            const float f = static_cast<float>(angle & 0xFF) * 0.00390625f;

            const float y0 = detail::kSinTable[idx];
            const float y1 = detail::kSinTable[idxNext];
            return y0 + f * (y1 - y0);
        }
    };

    struct Vector3
    {
        float x, y, z;

        constexpr Vector3() noexcept : x(0.0f), y(0.0f), z(0.0f) {}
        constexpr Vector3(float x_, float y_, float z_) noexcept : x(x_), y(y_), z(z_) {}

        PIP3D_FORCE_INLINE constexpr Vector3 operator-() const noexcept
        {
            return Vector3(-x, -y, -z);
        }

        PIP3D_FORCE_INLINE constexpr Vector3 operator+(Vector3 v) const noexcept
        {
            return Vector3(x + v.x, y + v.y, z + v.z);
        }

        PIP3D_FORCE_INLINE constexpr Vector3 operator-(Vector3 v) const noexcept
        {
            return Vector3(x - v.x, y - v.y, z - v.z);
        }

        PIP3D_FORCE_INLINE constexpr Vector3 operator*(float s) const noexcept
        {
            return Vector3(x * s, y * s, z * s);
        }

        PIP3D_FORCE_INLINE friend constexpr Vector3 operator*(float s, Vector3 v) noexcept
        {
            return Vector3(v.x * s, v.y * s, v.z * s);
        }

        PIP3D_FORCE_INLINE Vector3 &operator+=(Vector3 v) noexcept
        {
            x += v.x;
            y += v.y;
            z += v.z;
            return *this;
        }

        PIP3D_FORCE_INLINE Vector3 &operator-=(Vector3 v) noexcept
        {
            x -= v.x;
            y -= v.y;
            z -= v.z;
            return *this;
        }

        PIP3D_FORCE_INLINE Vector3 &operator*=(float s) noexcept
        {
            x *= s;
            y *= s;
            z *= s;
            return *this;
        }

        PIP3D_FORCE_INLINE void normalize() noexcept
        {
            const float lenSq = x * x + y * y + z * z;
            if (lenSq <= 1e-12f)
            {
                x = 0.0f;
                y = 0.0f;
                z = 0.0f;
                return;
            }
            const float invLen = FastMath::fastInvSqrt(lenSq);
            x *= invLen;
            y *= invLen;
            z *= invLen;
        }

        PIP3D_FORCE_INLINE float length() const noexcept
        {
            const float lenSq = x * x + y * y + z * z;
            return lenSq <= 1e-12f ? 0.0f : lenSq * FastMath::fastInvSqrt(lenSq);
        }

        PIP3D_FORCE_INLINE constexpr float lengthSquared() const noexcept
        {
            return x * x + y * y + z * z;
        }

        PIP3D_FORCE_INLINE constexpr float dot(Vector3 v) const noexcept
        {
            return x * v.x + y * v.y + z * v.z;
        }

        PIP3D_FORCE_INLINE constexpr Vector3 cross(Vector3 v) const noexcept
        {
            return Vector3(
                y * v.z - z * v.y,
                z * v.x - x * v.z,
                x * v.y - y * v.x);
        }
    };

    static_assert(sizeof(Vector3) == 12);
    static_assert(alignof(Vector3) == 4);

    struct Matrix4x4
    {
        float m[16];

        PIP3D_FORCE_INLINE constexpr Matrix4x4() noexcept
            : m{1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f} {}

        PIP3D_FORCE_INLINE constexpr void reset() noexcept
        {
            *this = Matrix4x4{};
        }

        PIP3D_FORCE_INLINE Matrix4x4 operator*(const Matrix4x4 &b) const noexcept
        {
            Matrix4x4 r;

            const float a0 = m[0], a1 = m[1], a2 = m[2], a3 = m[3];
            const float a4 = m[4], a5 = m[5], a6 = m[6], a7 = m[7];
            const float a8 = m[8], a9 = m[9], a10 = m[10], a11 = m[11];
            const float a12 = m[12], a13 = m[13], a14 = m[14], a15 = m[15];

            r.m[0] = a0 * b.m[0] + a4 * b.m[1] + a8 * b.m[2] + a12 * b.m[3];
            r.m[1] = a1 * b.m[0] + a5 * b.m[1] + a9 * b.m[2] + a13 * b.m[3];
            r.m[2] = a2 * b.m[0] + a6 * b.m[1] + a10 * b.m[2] + a14 * b.m[3];
            r.m[3] = a3 * b.m[0] + a7 * b.m[1] + a11 * b.m[2] + a15 * b.m[3];

            r.m[4] = a0 * b.m[4] + a4 * b.m[5] + a8 * b.m[6] + a12 * b.m[7];
            r.m[5] = a1 * b.m[4] + a5 * b.m[5] + a9 * b.m[6] + a13 * b.m[7];
            r.m[6] = a2 * b.m[4] + a6 * b.m[5] + a10 * b.m[6] + a14 * b.m[7];
            r.m[7] = a3 * b.m[4] + a7 * b.m[5] + a11 * b.m[6] + a15 * b.m[7];

            r.m[8] = a0 * b.m[8] + a4 * b.m[9] + a8 * b.m[10] + a12 * b.m[11];
            r.m[9] = a1 * b.m[8] + a5 * b.m[9] + a9 * b.m[10] + a13 * b.m[11];
            r.m[10] = a2 * b.m[8] + a6 * b.m[9] + a10 * b.m[10] + a14 * b.m[11];
            r.m[11] = a3 * b.m[8] + a7 * b.m[9] + a11 * b.m[10] + a15 * b.m[11];

            r.m[12] = a0 * b.m[12] + a4 * b.m[13] + a8 * b.m[14] + a12 * b.m[15];
            r.m[13] = a1 * b.m[12] + a5 * b.m[13] + a9 * b.m[14] + a13 * b.m[15];
            r.m[14] = a2 * b.m[12] + a6 * b.m[13] + a10 * b.m[14] + a14 * b.m[15];
            r.m[15] = a3 * b.m[12] + a7 * b.m[13] + a11 * b.m[14] + a15 * b.m[15];

            return r;
        }

        PIP3D_FORCE_INLINE constexpr Vector3 transformNoDiv(Vector3 v) const noexcept
        {
            return Vector3(
                m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12],
                m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13],
                m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]);
        }

        PIP3D_FORCE_INLINE Vector3 transformNormal(Vector3 n) const noexcept
        {
            const float c0 = m[0], c1 = m[4], c2 = m[8];
            const float c3 = m[1], c4 = m[5], c5 = m[9];
            const float c6 = m[2], c7 = m[6], c8 = m[10];

            const float cf0 = c4 * c8 - c5 * c7;
            const float cf1 = c5 * c6 - c3 * c8;
            const float cf2 = c3 * c7 - c4 * c6;
            const float cf3 = c2 * c7 - c1 * c8;
            const float cf4 = c0 * c8 - c2 * c6;
            const float cf5 = c1 * c6 - c0 * c7;
            const float cf6 = c1 * c5 - c2 * c4;
            const float cf7 = c2 * c3 - c0 * c5;
            const float cf8 = c0 * c4 - c1 * c3;

            Vector3 result(
                cf0 * n.x + cf1 * n.y + cf2 * n.z,
                cf3 * n.x + cf4 * n.y + cf5 * n.z,
                cf6 * n.x + cf7 * n.y + cf8 * n.z);
            result.normalize();
            return result;
        }

        PIP3D_FORCE_INLINE void setPerspective(float fovRad, float aspect,
                                               float nearPlane, float farPlane) noexcept
        {
            float halfFovSin, halfFovCos;
            FastMath::fastSinCos(fovRad * 0.5f, halfFovSin, halfFovCos);
            const float f = halfFovCos * FastMath::fastReciprocal(halfFovSin);
            const float rangeInv = FastMath::fastReciprocal(nearPlane - farPlane);
            const float invAspect = FastMath::fastReciprocal(aspect);

            m[0] = f * invAspect;
            m[1] = 0.0f;
            m[2] = 0.0f;
            m[3] = 0.0f;
            m[4] = 0.0f;
            m[5] = f;
            m[6] = 0.0f;
            m[7] = 0.0f;
            m[8] = 0.0f;
            m[9] = 0.0f;
            m[10] = (farPlane + nearPlane) * rangeInv;
            m[11] = -1.0f;
            m[12] = 0.0f;
            m[13] = 0.0f;
            m[14] = 2.0f * nearPlane * farPlane * rangeInv;
            m[15] = 0.0f;
        }

        PIP3D_FORCE_INLINE void setOrthographic(float left, float right,
                                                float bottom, float top,
                                                float nearPlane, float farPlane) noexcept
        {
            const float rl = FastMath::fastReciprocal(right - left);
            const float tb = FastMath::fastReciprocal(top - bottom);
            const float fn = FastMath::fastReciprocal(farPlane - nearPlane);

            m[0] = 2.0f * rl;
            m[1] = 0.0f;
            m[2] = 0.0f;
            m[3] = 0.0f;
            m[4] = 0.0f;
            m[5] = 2.0f * tb;
            m[6] = 0.0f;
            m[7] = 0.0f;
            m[8] = 0.0f;
            m[9] = 0.0f;
            m[10] = -2.0f * fn;
            m[11] = 0.0f;
            m[12] = -(right + left) * rl;
            m[13] = -(top + bottom) * tb;
            m[14] = -(farPlane + nearPlane) * fn;
            m[15] = 1.0f;
        }

        PIP3D_FORCE_INLINE void lookAt(const Vector3 &eye,
                                       const Vector3 &target,
                                       const Vector3 &up) noexcept
        {
            Vector3 f = target - eye;
            const float fLenSq = f.lengthSquared();
            if (fLenSq <= 1e-10f)
            {
                *this = Matrix4x4{};
                return;
            }
            f *= FastMath::fastInvSqrt(fLenSq);

            Vector3 s = f.cross(up);
            const float sLenSq = s.lengthSquared();
            if (sLenSq <= 1e-10f)
            {
                Vector3 altUp;
                if (fabsf(f.x) <= fabsf(f.y) && fabsf(f.x) <= fabsf(f.z))
                    altUp = Vector3(1.0f, 0.0f, 0.0f);
                else if (fabsf(f.y) <= fabsf(f.z))
                    altUp = Vector3(0.0f, 1.0f, 0.0f);
                else
                    altUp = Vector3(0.0f, 0.0f, 1.0f);
                s = f.cross(altUp);
                s.normalize();
            }
            else
            {
                s *= FastMath::fastInvSqrt(sLenSq);
            }

            const Vector3 u = s.cross(f);

            m[0] = s.x;
            m[4] = s.y;
            m[8] = s.z;
            m[12] = -s.dot(eye);
            m[1] = u.x;
            m[5] = u.y;
            m[9] = u.z;
            m[13] = -u.dot(eye);
            m[2] = -f.x;
            m[6] = -f.y;
            m[10] = -f.z;
            m[14] = f.dot(eye);
            m[3] = 0.0f;
            m[7] = 0.0f;
            m[11] = 0.0f;
            m[15] = 1.0f;
        }

        PIP3D_FORCE_INLINE constexpr void setScale(float sx, float sy, float sz) noexcept
        {
            m[0] = sx;
            m[1] = 0.0f;
            m[2] = 0.0f;
            m[3] = 0.0f;
            m[4] = 0.0f;
            m[5] = sy;
            m[6] = 0.0f;
            m[7] = 0.0f;
            m[8] = 0.0f;
            m[9] = 0.0f;
            m[10] = sz;
            m[11] = 0.0f;
            m[12] = 0.0f;
            m[13] = 0.0f;
            m[14] = 0.0f;
            m[15] = 1.0f;
        }
    };

    static_assert(sizeof(Matrix4x4) == 64);
    static_assert(alignof(Matrix4x4) == 4);

    struct Quaternion
    {
        float x, y, z, w;

        constexpr Quaternion() noexcept : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}
        constexpr Quaternion(float x_, float y_, float z_, float w_) noexcept
            : x(x_), y(y_), z(z_), w(w_) {}

        PIP3D_FORCE_INLINE static Quaternion fromAxisAngle(Vector3 axis, float angleRad) noexcept
        {
            const float axisLenSq = axis.lengthSquared();
            if (axisLenSq <= 1e-12f)
                return Quaternion(0.0f, 0.0f, 0.0f, 1.0f);

            float s_half, c_half;
            FastMath::fastSinCos(angleRad * 0.5f, s_half, c_half);
            const float k = FastMath::fastInvSqrt(axisLenSq) * s_half;
            return Quaternion(axis.x * k, axis.y * k, axis.z * k, c_half);
        }

        PIP3D_FORCE_INLINE static Quaternion fromEuler(float pitchRad, float yawRad, float rollRad) noexcept
        {
            float sy, cy, sp, cp, sr, cr;
            FastMath::fastSinCosBin(FastMath::radToBin(yawRad * 0.5f), sy, cy);
            FastMath::fastSinCosBin(FastMath::radToBin(pitchRad * 0.5f), sp, cp);
            FastMath::fastSinCosBin(FastMath::radToBin(rollRad * 0.5f), sr, cr);

            const float sr_cp = sr * cp;
            const float cr_sp = cr * sp;
            const float sr_sp = sr * sp;
            const float cr_cp = cr * cp;

            return Quaternion(
                sr_cp * cy - cr_sp * sy,
                cr_sp * cy + sr_cp * sy,
                cr_cp * sy - sr_sp * cy,
                cr_cp * cy + sr_sp * sy);
        }

        PIP3D_FORCE_INLINE constexpr Quaternion conjugate() const noexcept
        {
            return Quaternion(-x, -y, -z, w);
        }

        PIP3D_FORCE_INLINE void normalize() noexcept
        {
            const float lenSq = x * x + y * y + z * z + w * w;
            if (lenSq <= 1e-12f)
            {
                x = 0.0f;
                y = 0.0f;
                z = 0.0f;
                w = 1.0f;
                return;
            }
            const float invLen = FastMath::fastInvSqrt(lenSq);
            x *= invLen;
            y *= invLen;
            z *= invLen;
            w *= invLen;
        }

        PIP3D_FORCE_INLINE constexpr Vector3 rotate(Vector3 v) const noexcept
        {
            const float tx = y * v.z - z * v.y;
            const float ty = z * v.x - x * v.z;
            const float tz = x * v.y - y * v.x;

            return Vector3(
                v.x + 2.0f * (w * tx + (y * tz - z * ty)),
                v.y + 2.0f * (w * ty + (z * tx - x * tz)),
                v.z + 2.0f * (w * tz + (x * ty - y * tx)));
        }

        PIP3D_FORCE_INLINE constexpr Quaternion
        operator*(const Quaternion &rhs) const noexcept
        {
            return Quaternion(
                w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
                w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
                w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w,
                w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z);
        }
    };

    static_assert(sizeof(Quaternion) == 16);
    static_assert(alignof(Quaternion) == 4);
}
