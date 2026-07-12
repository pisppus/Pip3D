#pragma once

#include <cstdint>
#include <cmath>

#include "Core/Platform.hpp"
#include "Math/Algebra.hpp"

namespace pip3D
{
    namespace detail
    {
        constexpr float qAbs(float v) noexcept { return v < 0.0f ? -v : v; }
        constexpr float qCopysign(float magnitude, float sign) noexcept
        {
            return sign < 0.0f ? -magnitude : magnitude;
        }
    }

    constexpr uint16_t packNormalRaw(float x, float y, float z) noexcept
    {
        const float l1 = detail::qAbs(x) + detail::qAbs(y) + detail::qAbs(z);
        if (l1 <= 1e-6f)
            return 0;

        const float inv = 1.0f / l1;
        float ox = x * inv;
        float oy = y * inv;

        if (z < 0.0f)
        {
            const float sgnX = detail::qCopysign(1.0f, ox);
            const float sgnY = detail::qCopysign(1.0f, oy);
            const float ax = detail::qAbs(ox);
            const float ay = detail::qAbs(oy);
            ox = (1.0f - ay) * sgnX;
            oy = (1.0f - ax) * sgnY;
        }

        constexpr float kPack = 127.5f;
        const float fx = ox * kPack + kPack;
        const float fy = oy * kPack + kPack;

        const uint32_t px = static_cast<uint32_t>(
            fx < 0.0f ? 0.0f : (fx > 255.0f ? 255.0f : fx));
        const uint32_t py = static_cast<uint32_t>(
            fy < 0.0f ? 0.0f : (fy > 255.0f ? 255.0f : fy));

        return static_cast<uint16_t>((px << 8) | py);
    }

    struct PackedNormal
    {
        uint16_t data;

        static constexpr float kInvPack = 1.0f / 127.5f;
        static constexpr float kPack = 127.5f;

        constexpr PackedNormal() noexcept : data(0) {}
        constexpr explicit PackedNormal(uint16_t d) noexcept : data(d) {}

        PIP3D_FORCE_INLINE explicit PackedNormal(float nx, float ny, float nz) noexcept
            : data(packNormalRaw(nx, ny, nz)) {}

        PIP3D_FORCE_INLINE void set(float nx, float ny, float nz) noexcept
        {
            data = packNormalRaw(nx, ny, nz);
        }

        PIP3D_FORCE_INLINE void set(const Vector3 &n) noexcept
        {
            data = packNormalRaw(n.x, n.y, n.z);
        }

        [[nodiscard]] PIP3D_FORCE_INLINE Vector3 get() const noexcept
        {
            const float nx = static_cast<float>(data >> 8) * kInvPack - 1.0f;
            const float ny = static_cast<float>(data & 0xFF) * kInvPack - 1.0f;
            const float nz = 1.0f - fabsf(nx) - fabsf(ny);

            if (unlikely(nz < 0.0f))
            {
                const float sgnX = __builtin_copysignf(1.0f, nx);
                const float sgnY = __builtin_copysignf(1.0f, ny);
                const float ax = fabsf(nx);
                const float ay = fabsf(ny);
                return Vector3((1.0f - ay) * sgnX,
                               (1.0f - ax) * sgnY,
                               0.0f);
            }
            return Vector3(nx, ny, nz);
        }
    };
    static_assert(sizeof(PackedNormal) == 2);
    static_assert(alignof(PackedNormal) == 2);

    [[nodiscard]] constexpr uint16_t packNormalConstexpr(float x, float y, float z) noexcept
    {
        return packNormalRaw(x, y, z);
    }
}
