#pragma once

#include <cstdint>
#include <cmath>

#include "Core/Platform.hpp"
#include "Math/Algebra.hpp"

namespace pip3D
{
    struct PackedNormal
    {
        uint16_t data;

        static constexpr float kInvPack = 1.0f / 127.5f;
        static constexpr float kPack = 127.5f;

        constexpr PackedNormal() : data(0) {}
        constexpr explicit PackedNormal(uint16_t d) : data(d) {}

        PIP3D_FORCE_INLINE void set(float nx, float ny, float nz)
        {
            const float l1 = fabsf(nx) + fabsf(ny) + fabsf(nz);
            if (likely(l1 > 1e-6f))
            {
                const float inv_l1 = FastMath::fastReciprocal(l1);
                float ox = nx * inv_l1;
                float oy = ny * inv_l1;

                if (unlikely(nz < 0.0f))
                {
                    const float sgnX = __builtin_copysignf(1.0f, ox);
                    const float sgnY = __builtin_copysignf(1.0f, oy);
                    const float ax = fabsf(ox);
                    const float ay = fabsf(oy);
                    ox = (1.0f - ay) * sgnX;
                    oy = (1.0f - ax) * sgnY;
                }

                const uint32_t px = static_cast<uint32_t>(ox * kPack + kPack);
                const uint32_t py = static_cast<uint32_t>(oy * kPack + kPack);
                data = static_cast<uint16_t>((px << 8) | py);
            }
        }

        PIP3D_FORCE_INLINE void set(const Vector3 &n) { set(n.x, n.y, n.z); }

        PIP3D_FORCE_INLINE Vector3 get() const
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

    namespace detail
    {
        constexpr float qAbs(float v) { return v < 0.0f ? -v : v; }
    }

    constexpr uint16_t packNormalConstexpr(float x, float y, float z)
    {
        const float l1 = detail::qAbs(x) + detail::qAbs(y) + detail::qAbs(z);
        if (l1 > 1e-6f)
        {
            const float inv = 1.0f / l1;
            float nx = x * inv;
            float ny = y * inv;

            if (z < 0.0f)
            {
                const float ax = detail::qAbs(nx);
                const float ay = detail::qAbs(ny);
                nx = (1.0f - ay) * (nx >= 0.0f ? 1.0f : -1.0f);
                ny = (1.0f - ax) * (ny >= 0.0f ? 1.0f : -1.0f);
            }

            const uint32_t px = static_cast<uint32_t>((nx * 0.5f + 0.5f) * 255.0f);
            const uint32_t py = static_cast<uint32_t>((ny * 0.5f + 0.5f) * 255.0f);
            return static_cast<uint16_t>((px << 8) | py);
        }
        return 0;
    }

}