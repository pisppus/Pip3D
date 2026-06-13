#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <array>

#if defined(_MSC_VER)
#define PIP3D_FORCE_INLINE __forceinline
#define PIP3D_RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
#define PIP3D_FORCE_INLINE inline __attribute__((always_inline))
#define PIP3D_RESTRICT __restrict__
#else
#define PIP3D_FORCE_INLINE inline
#define PIP3D_RESTRICT
#endif

#if defined(ESP_PLATFORM) || defined(ESP32)
#include <esp_attr.h>
#define PIP3D_FAST_DATA DRAM_ATTR
#else
#define PIP3D_FAST_DATA
#endif

#if defined(ARDUINO)
#include <Arduino.h>
#else
#ifndef PI
#define PI 3.1415927f
#endif
#ifndef TWO_PI
#define TWO_PI 6.2831855f
#endif
#ifndef DEG_TO_RAD
#define DEG_TO_RAD 0.017453292f
#endif
#ifndef RAD_TO_DEG
#define RAD_TO_DEG 57.29578f
#endif
#endif

namespace pip3D
{
    inline constexpr float kPi = 3.1415927f;
    inline constexpr float kTwoPi = 6.2831855f;
    inline constexpr float kDegToRad = 0.017453292f;
    inline constexpr float kRadToDeg = 57.29578f;

    namespace detail
    {
        inline const std::array<float, 256> PIP3D_FAST_DATA kSinTable = {
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
        PIP3D_FORCE_INLINE static uint16_t radToBin(float rad)
        {
            return static_cast<uint16_t>(static_cast<int32_t>(rad * 10430.378f));
        }

        PIP3D_FORCE_INLINE static uint16_t degToBin(float deg)
        {
            return static_cast<uint16_t>(static_cast<int32_t>(deg * 182.04445f));
        }

        PIP3D_FORCE_INLINE static float fastSinBinRaw(uint16_t angle)
        {
            return detail::kSinTable[static_cast<uint8_t>(angle >> 8)];
        }

        PIP3D_FORCE_INLINE static float fastCosBinRaw(uint16_t angle)
        {
            return fastSinBinRaw(angle + 16384);
        }

        PIP3D_FORCE_INLINE static float fastSinBin(uint16_t angle)
        {
            const uint8_t idx = static_cast<uint8_t>(angle >> 8);
            const uint8_t frac = static_cast<uint8_t>(angle & 0xFF);

            const float y0 = detail::kSinTable[idx];
            const float y1 = detail::kSinTable[static_cast<uint8_t>(idx + 1)];

            const float f = static_cast<float>(frac) * 0.00390625f;
            return y0 + f * (y1 - y0);
        }

        PIP3D_FORCE_INLINE static float fastCosBin(uint16_t angle)
        {
            return fastSinBin(angle + 16384);
        }

        PIP3D_FORCE_INLINE static void fastSinCosBin(uint16_t angle, float &outSin, float &outCos)
        {
            const uint8_t idxS = static_cast<uint8_t>(angle >> 8);
            const uint8_t idxC = static_cast<uint8_t>(idxS + 64);

            const uint8_t frac = static_cast<uint8_t>(angle & 0xFF);
            const float f = static_cast<float>(frac) * 0.00390625f;

            const float ys0 = detail::kSinTable[idxS];
            const float ys1 = detail::kSinTable[static_cast<uint8_t>(idxS + 1)];
            const float yc0 = detail::kSinTable[idxC];
            const float yc1 = detail::kSinTable[static_cast<uint8_t>(idxC + 1)];

            outSin = ys0 + f * (ys1 - ys0);
            outCos = yc0 + f * (yc1 - yc0);
        }

        PIP3D_FORCE_INLINE static float fastSin(float angle)
        {
            return fastSinBin(radToBin(angle));
        }

        PIP3D_FORCE_INLINE static float fastCos(float angle)
        {
            return fastCosBin(radToBin(angle));
        }

        PIP3D_FORCE_INLINE static void fastSinCos(float angle, float &outSin, float &outCos)
        {
            fastSinCosBin(radToBin(angle), outSin, outCos);
        }

        PIP3D_FORCE_INLINE static float fastReciprocal(float input)
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

        PIP3D_FORCE_INLINE static float fastInvSqrt(float x)
        {
            float x2 = x * 0.5f;
            float y = x;
            uint32_t i;
            std::memcpy(&i, &y, sizeof(uint32_t));
            i = 0x5f3759df - (i >> 1);
            std::memcpy(&y, &i, sizeof(uint32_t));
            return y * (1.5f - x2 * y * y);
        }
    };

    struct Vector3
    {
        float x, y, z;

        constexpr Vector3() : x(0.0f), y(0.0f), z(0.0f) {}
        constexpr Vector3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

        PIP3D_FORCE_INLINE constexpr Vector3 operator-() const
        {
            return Vector3(-x, -y, -z);
        }

        PIP3D_FORCE_INLINE constexpr Vector3 operator+(Vector3 v) const
        {
            return Vector3(x + v.x, y + v.y, z + v.z);
        }

        PIP3D_FORCE_INLINE constexpr Vector3 operator-(Vector3 v) const
        {
            return Vector3(x - v.x, y - v.y, z - v.z);
        }

        PIP3D_FORCE_INLINE constexpr Vector3 operator*(float s) const
        {
            return Vector3(x * s, y * s, z * s);
        }

        PIP3D_FORCE_INLINE friend constexpr Vector3 operator*(float s, Vector3 v)
        {
            return Vector3(v.x * s, v.y * s, v.z * s);
        }

        PIP3D_FORCE_INLINE Vector3 operator/(float s) const
        {
            float invS = FastMath::fastReciprocal(s);
            return Vector3(x * invS, y * invS, z * invS);
        }

        PIP3D_FORCE_INLINE Vector3 &operator+=(Vector3 v)
        {
            x += v.x;
            y += v.y;
            z += v.z;
            return *this;
        }

        PIP3D_FORCE_INLINE Vector3 &operator*=(float s)
        {
            x *= s;
            y *= s;
            z *= s;
            return *this;
        }

        PIP3D_FORCE_INLINE Vector3 &operator-=(Vector3 v)
        {
            x -= v.x;
            y -= v.y;
            z -= v.z;
            return *this;
        }

        PIP3D_FORCE_INLINE Vector3 &operator/=(float s)
        {
            float invS = FastMath::fastReciprocal(s);
            x *= invS;
            y *= invS;
            z *= invS;
            return *this;
        }

        PIP3D_FORCE_INLINE float normalize()
        {
            float lenSq = x * x + y * y + z * z;
            if (lenSq > 1e-8f)
            {
                float invLen = FastMath::fastInvSqrt(lenSq);
                x *= invLen;
                y *= invLen;
                z *= invLen;
                return invLen;
            }
            return 0.0f;
        }

        PIP3D_FORCE_INLINE float length() const
        {
            float lenSq = x * x + y * y + z * z;
            return lenSq < 1e-8f ? 0.0f : lenSq * FastMath::fastInvSqrt(lenSq);
        }

        PIP3D_FORCE_INLINE constexpr float lengthSquared() const
        {
            return x * x + y * y + z * z;
        }

        PIP3D_FORCE_INLINE constexpr float dot(Vector3 v) const
        {
            return x * v.x + y * v.y + z * v.z;
        }

        PIP3D_FORCE_INLINE constexpr Vector3 cross(Vector3 v) const
        {
            return Vector3(
                y * v.z - z * v.y,
                z * v.x - x * v.z,
                x * v.y - y * v.x);
        }
    };

    struct __attribute__((aligned(16))) Matrix4x4
    {
        float m[16];

        PIP3D_FORCE_INLINE constexpr Matrix4x4()
            : m{1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f} {}

        PIP3D_FORCE_INLINE explicit constexpr Matrix4x4(bool initializeIdentity)
            : m{initializeIdentity ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f,
                0.0f, initializeIdentity ? 1.0f : 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, initializeIdentity ? 1.0f : 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, initializeIdentity ? 1.0f : 0.0f} {}

        PIP3D_FORCE_INLINE constexpr void identity()
        {
            m[0] = 1.0f;
            m[1] = 0.0f;
            m[2] = 0.0f;
            m[3] = 0.0f;
            m[4] = 0.0f;
            m[5] = 1.0f;
            m[6] = 0.0f;
            m[7] = 0.0f;
            m[8] = 0.0f;
            m[9] = 0.0f;
            m[10] = 1.0f;
            m[11] = 0.0f;
            m[12] = 0.0f;
            m[13] = 0.0f;
            m[14] = 0.0f;
            m[15] = 1.0f;
        }

        PIP3D_FORCE_INLINE Matrix4x4 operator*(const Matrix4x4 &b) const
        {
            Matrix4x4 result(false);
            const float *PIP3D_RESTRICT a = m;
            const float *PIP3D_RESTRICT bm = b.m;
            float *PIP3D_RESTRICT r = result.m;

            float b0 = bm[0], b1 = bm[1], b2 = bm[2], b3 = bm[3];
            r[0] = a[0] * b0 + a[4] * b1 + a[8] * b2 + a[12] * b3;
            r[1] = a[1] * b0 + a[5] * b1 + a[9] * b2 + a[13] * b3;
            r[2] = a[2] * b0 + a[6] * b1 + a[10] * b2 + a[14] * b3;
            r[3] = a[3] * b0 + a[7] * b1 + a[11] * b2 + a[15] * b3;

            float b4 = bm[4], b5 = bm[5], b6 = bm[6], b7 = bm[7];
            r[4] = a[0] * b4 + a[4] * b5 + a[8] * b6 + a[12] * b7;
            r[5] = a[1] * b4 + a[5] * b5 + a[9] * b6 + a[13] * b7;
            r[6] = a[2] * b4 + a[6] * b5 + a[10] * b6 + a[14] * b7;
            r[7] = a[3] * b4 + a[7] * b5 + a[11] * b6 + a[15] * b7;

            float b8 = bm[8], b9 = bm[9], b10 = bm[10], b11 = bm[11];
            r[8] = a[0] * b8 + a[4] * b9 + a[8] * b10 + a[12] * b11;
            r[9] = a[1] * b8 + a[5] * b9 + a[9] * b10 + a[13] * b11;
            r[10] = a[2] * b8 + a[6] * b9 + a[10] * b10 + a[14] * b11;
            r[11] = a[3] * b8 + a[7] * b9 + a[11] * b10 + a[15] * b11;

            float b12 = bm[12], b13 = bm[13], b14 = bm[14], b15 = bm[15];
            r[12] = a[0] * b12 + a[4] * b13 + a[8] * b14 + a[12] * b15;
            r[13] = a[1] * b12 + a[5] * b13 + a[9] * b14 + a[13] * b15;
            r[14] = a[2] * b12 + a[6] * b13 + a[10] * b14 + a[14] * b15;
            r[15] = a[3] * b12 + a[7] * b13 + a[11] * b14 + a[15] * b15;

            return result;
        }

        PIP3D_FORCE_INLINE Vector3 transform(Vector3 v) const
        {
            float w = m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15];
            float invW = FastMath::fastReciprocal(w);

            return Vector3(
                (m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12]) * invW,
                (m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13]) * invW,
                (m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]) * invW);
        }

        PIP3D_FORCE_INLINE constexpr Vector3 transformNoDiv(Vector3 v) const
        {
            return Vector3(
                m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12],
                m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13],
                m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]);
        }

        PIP3D_FORCE_INLINE Vector3 transformNormal(Vector3 n) const
        {
            Vector3 result(
                m[0] * n.x + m[4] * n.y + m[8] * n.z,
                m[1] * n.x + m[5] * n.y + m[9] * n.z,
                m[2] * n.x + m[6] * n.y + m[10] * n.z);
            result.normalize();
            return result;
        }

        PIP3D_FORCE_INLINE void setPerspective(float fov, float aspect, float nearPlane, float farPlane)
        {
            float f = FastMath::fastReciprocal(tanf(fov * 0.5f * kDegToRad));
            float rangeInv = FastMath::fastReciprocal(nearPlane - farPlane);
            float invAspect = FastMath::fastReciprocal(aspect);

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

        PIP3D_FORCE_INLINE void setOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane)
        {
            float rl = FastMath::fastReciprocal(right - left);
            float tb = FastMath::fastReciprocal(top - bottom);
            float fn = FastMath::fastReciprocal(farPlane - nearPlane);

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

        PIP3D_FORCE_INLINE void lookAt(const Vector3 &eye, const Vector3 &target, const Vector3 &up)
        {
            Vector3 f = target - eye;
            f.normalize();

            Vector3 s = f.cross(up);
            s.normalize();

            Vector3 u = s.cross(f);

            m[0] = s.x;
            m[4] = s.y;
            m[8] = s.z;
            m[1] = u.x;
            m[5] = u.y;
            m[9] = u.z;
            m[2] = -f.x;
            m[6] = -f.y;
            m[10] = -f.z;
            m[3] = 0.0f;
            m[7] = 0.0f;
            m[11] = 0.0f;

            m[12] = -s.dot(eye);
            m[13] = -u.dot(eye);
            m[14] = f.dot(eye);
            m[15] = 1.0f;
        }

        PIP3D_FORCE_INLINE constexpr void setTranslation(float x, float y, float z)
        {
            m[0] = 1.0f;
            m[1] = 0.0f;
            m[2] = 0.0f;
            m[3] = 0.0f;
            m[4] = 0.0f;
            m[5] = 1.0f;
            m[6] = 0.0f;
            m[7] = 0.0f;
            m[8] = 0.0f;
            m[9] = 0.0f;
            m[10] = 1.0f;
            m[11] = 0.0f;
            m[12] = x;
            m[13] = y;
            m[14] = z;
            m[15] = 1.0f;
        }

        PIP3D_FORCE_INLINE void setRotationX(float angle)
        {
            float s, c;
            FastMath::fastSinCosBin(FastMath::degToBin(angle), s, c);

            m[0] = 1.0f;
            m[1] = 0.0f;
            m[2] = 0.0f;
            m[3] = 0.0f;
            m[4] = 0.0f;
            m[5] = c;
            m[6] = s;
            m[7] = 0.0f;
            m[8] = 0.0f;
            m[9] = -s;
            m[10] = c;
            m[11] = 0.0f;
            m[12] = 0.0f;
            m[13] = 0.0f;
            m[14] = 0.0f;
            m[15] = 1.0f;
        }

        PIP3D_FORCE_INLINE void setRotationY(float angle)
        {
            float s, c;
            FastMath::fastSinCosBin(FastMath::degToBin(angle), s, c);

            m[0] = c;
            m[1] = 0.0f;
            m[2] = -s;
            m[3] = 0.0f;
            m[4] = 0.0f;
            m[5] = 1.0f;
            m[6] = 0.0f;
            m[7] = 0.0f;
            m[8] = s;
            m[9] = 0.0f;
            m[10] = c;
            m[11] = 0.0f;
            m[12] = 0.0f;
            m[13] = 0.0f;
            m[14] = 0.0f;
            m[15] = 1.0f;
        }

        PIP3D_FORCE_INLINE void setRotationZ(float angle)
        {
            float s, c;
            FastMath::fastSinCosBin(FastMath::degToBin(angle), s, c);

            m[0] = c;
            m[1] = s;
            m[2] = 0.0f;
            m[3] = 0.0f;
            m[4] = -s;
            m[5] = c;
            m[6] = 0.0f;
            m[7] = 0.0f;
            m[8] = 0.0f;
            m[9] = 0.0f;
            m[10] = 1.0f;
            m[11] = 0.0f;
            m[12] = 0.0f;
            m[13] = 0.0f;
            m[14] = 0.0f;
            m[15] = 1.0f;
        }

        PIP3D_FORCE_INLINE constexpr void setScale(float x, float y, float z)
        {
            m[0] = x;
            m[1] = 0.0f;
            m[2] = 0.0f;
            m[3] = 0.0f;
            m[4] = 0.0f;
            m[5] = y;
            m[6] = 0.0f;
            m[7] = 0.0f;
            m[8] = 0.0f;
            m[9] = 0.0f;
            m[10] = z;
            m[11] = 0.0f;
            m[12] = 0.0f;
            m[13] = 0.0f;
            m[14] = 0.0f;
            m[15] = 1.0f;
        }
    };

    struct __attribute__((aligned(16))) Quaternion
    {
        float x, y, z, w;

        constexpr Quaternion() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}
        constexpr Quaternion(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}

        PIP3D_FORCE_INLINE static Quaternion fromAxisAngle(Vector3 axis, float angle)
        {
            float s, c;
            FastMath::fastSinCosBin(FastMath::radToBin(angle * 0.5f), s, c);
            return Quaternion(axis.x * s, axis.y * s, axis.z * s, c);
        }

        PIP3D_FORCE_INLINE static Quaternion fromEuler(float pitch, float yaw, float roll)
        {
            float sy, cy, sp, cp, sr, cr;
            FastMath::fastSinCosBin(FastMath::radToBin(yaw * 0.5f), sy, cy);
            FastMath::fastSinCosBin(FastMath::radToBin(pitch * 0.5f), sp, cp);
            FastMath::fastSinCosBin(FastMath::radToBin(roll * 0.5f), sr, cr);

            return Quaternion(
                sr * cp * cy - cr * sp * sy,
                cr * sp * cy + sr * cp * sy,
                cr * cp * sy - sr * sp * cy,
                cr * cp * cy + sr * sp * sy);
        }

        PIP3D_FORCE_INLINE constexpr Quaternion conjugate() const
        {
            return Quaternion(-x, -y, -z, w);
        }

        PIP3D_FORCE_INLINE float normalize()
        {
            float lenSq = x * x + y * y + z * z + w * w;
            if (lenSq > 1e-8f)
            {
                float invLen = FastMath::fastInvSqrt(lenSq);
                x *= invLen;
                y *= invLen;
                z *= invLen;
                w *= invLen;
                return invLen;
            }
            return 0.0f;
        }

        PIP3D_FORCE_INLINE constexpr Quaternion operator*(const Quaternion &q) const
        {
            return Quaternion(
                w * q.x + x * q.w + y * q.z - z * q.y,
                w * q.y - x * q.z + y * q.w + z * q.x,
                w * q.z + x * q.y - y * q.x + z * q.w,
                w * q.w - x * q.x - y * q.y - z * q.z);
        }

        PIP3D_FORCE_INLINE constexpr Vector3 rotate(Vector3 v) const
        {
            float tx = y * v.z - z * v.y;
            float ty = z * v.x - x * v.z;
            float tz = x * v.y - y * v.x;

            return Vector3(
                v.x + 2.0f * (w * tx + (y * tz - z * ty)),
                v.y + 2.0f * (w * ty + (z * tx - x * tz)),
                v.z + 2.0f * (w * tz + (x * ty - y * tx)));
        }

        PIP3D_FORCE_INLINE void toMatrix(Matrix4x4 &out) const
        {
            float qx = x + x, qy = y + y, qz = z + z;
            float qxx = x * qx, qyy = y * qy, qzz = z * qz;
            float qxy = x * qy, qxz = x * qz, qyz = y * qz;
            float qwx = w * qx, qwy = w * qy, qwz = w * qz;

            out.m[0] = 1.0f - qyy - qzz;
            out.m[1] = qxy + qwz;
            out.m[2] = qxz - qwy;
            out.m[3] = 0.0f;

            out.m[4] = qxy - qwz;
            out.m[5] = 1.0f - qxx - qzz;
            out.m[6] = qyz + qwx;
            out.m[7] = 0.0f;

            out.m[8] = qxz + qwy;
            out.m[9] = qyz - qwx;
            out.m[10] = 1.0f - qxx - qyy;
            out.m[11] = 0.0f;

            out.m[12] = 0.0f;
            out.m[13] = 0.0f;
            out.m[14] = 0.0f;
            out.m[15] = 1.0f;
        }

        static Quaternion slerp(const Quaternion &a, const Quaternion &b, float t)
        {
            float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;

            Quaternion b_adjusted = b;
            if (dot < 0.0f)
            {
                b_adjusted = Quaternion(-b.x, -b.y, -b.z, -b.w);
                dot = -dot;
            }

            if (dot > 0.9995f)
            {
                Quaternion result(
                    a.x + t * (b_adjusted.x - a.x),
                    a.y + t * (b_adjusted.y - a.y),
                    a.z + t * (b_adjusted.z - a.z),
                    a.w + t * (b_adjusted.w - a.w));
                result.normalize();
                return result;
            }

            float theta = acosf(dot);

            float sinTheta = FastMath::fastSin(theta);
            float invSin = FastMath::fastReciprocal(sinTheta);

            float scale0 = FastMath::fastSin((1.0f - t) * theta) * invSin;
            float scale1 = FastMath::fastSin(t * theta) * invSin;

            return Quaternion(
                scale0 * a.x + scale1 * b_adjusted.x,
                scale0 * a.y + scale1 * b_adjusted.y,
                scale0 * a.z + scale1 * b_adjusted.z,
                scale0 * a.w + scale1 * b_adjusted.w);
        }
    };

}