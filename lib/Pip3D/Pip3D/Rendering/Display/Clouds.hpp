#pragma once

#include "Core/Platform.hpp"
#include "Core/Color.hpp"
#include "Math/Algebra.hpp"
#include "Rendering/Display/CloudsMask.hpp"

namespace pip3D
{

    PIP3D_FORCE_INLINE PIP3D_HOT static uint16_t blendCloudPixel5Fast(uint32_t dst, uint32_t src, uint32_t a5) noexcept
    {
        const uint32_t invA = 32u - a5;
        const uint32_t rb = (((dst & 0xF81Fu) * invA + (src & 0xF81Fu) * a5) >> 5) & 0xF81Fu;
        const uint32_t g = (((dst & 0x07E0u) * invA + (src & 0x07E0u) * a5) >> 5) & 0x07E0u;
        return static_cast<uint16_t>(rb | g);
    }

    struct alignas(8) CloudLayer
    {
        Color cloudColor = Color(CLOUDS_DEFAULT_COLOR);
        float cloudAlpha = 1.0f;
        float cloudHeight = 800.0f;
        float cloudScale = 384.0f;
        float driftX = 0.0f;
        float driftZ = 0.0f;
        float driftSpeedX = 0.0f;
        float driftSpeedZ = 0.0f;
        bool enabled = false;

        mutable float cachedTanHalfVfov_ = -1.0f;
        mutable float cachedTanHalfHfov_ = -1.0f;
        mutable float cachedVfov_ = -1.0f;
        mutable float cachedHfov_ = -1.0f;
        mutable float cachedUvScale_ = 0.0f;
        mutable float cachedAlphaMul32_ = 32.0f;
        mutable bool lutDirty = true;
        mutable uint16_t shadeLut[256];

        CloudLayer() = default;
        ~CloudLayer() = default;
        CloudLayer(const CloudLayer &) = delete;
        CloudLayer &operator=(const CloudLayer &) = delete;

        PIP3D_FORCE_INLINE bool isReady() const noexcept { return enabled; }

        void free() noexcept {}
        bool reserveBuffer() noexcept { return true; }
        void generatePanorama(uint32_t, float, uint16_t, const Sky &) noexcept
        {
            cloudColor = Color(CLOUDS_DEFAULT_COLOR);
            cloudAlpha = 1.0f;
            lutDirty = true;
        }

        PIP3D_FORCE_INLINE void buildLuts() const noexcept
        {
            const uint32_t r5 = cloudColor.r5(), g6 = cloudColor.g6(), b5 = cloudColor.b5();

            for (uint32_t s = 0; s < 256u; ++s)
            {
                const uint32_t shade = 184u + (s * 90u) / 255u;

                uint32_t r = (r5 * shade + 128u) >> 8;
                if (r > 31u)
                    r = 31u;
                uint32_t g = (g6 * shade + 128u) >> 8;
                if (g > 63u)
                    g = 63u;
                uint32_t b = (b5 * shade + 128u) >> 8;
                if (b > 31u)
                    b = 31u;
                shadeLut[s] = static_cast<uint16_t>((r << 11) | (g << 5) | b);
            }
            lutDirty = false;
        }

        void setCloudColor(Color c) noexcept
        {
            if (c.rgb565 != cloudColor.rgb565)
            {
                cloudColor = c;
                lutDirty = true;
            }
        }

        void setCloudAlpha(float a) noexcept
        {
            float clamped = (a < 0.0f) ? 0.0f : (a > 1.0f) ? 1.0f
                                                           : a;
            cloudAlpha = clamped;
            cachedAlphaMul32_ = clamped * 32.0f;
        }

        void setCloudHeight(float metersAboveCam) noexcept
        {
            cloudHeight = (metersAboveCam < 1.0f) ? 1.0f : metersAboveCam;
        }

        void setCloudScale(float metersPerTile) noexcept
        {
            cloudScale = (metersPerTile < 1.0f) ? 1.0f : metersPerTile;
            cachedUvScale_ = 0.0f;
        }

        void setDrift(float speedXMps, float speedZMps) noexcept
        {
            driftSpeedX = speedXMps;
            driftSpeedZ = speedZMps;
        }

        void update(float dtSeconds) noexcept
        {
            driftX += driftSpeedX * dtSeconds;
            driftZ += driftSpeedZ * dtSeconds;
        }

        PIP3D_FORCE_INLINE void prepareForFrame(float vfovRad, float hfovRad) const noexcept
        {
            if (unlikely(lutDirty))
                buildLuts();

            if (unlikely(vfovRad != cachedVfov_))
            {
                cachedVfov_ = vfovRad;
                cachedTanHalfVfov_ = tanf(vfovRad * 0.5f);
            }
            if (unlikely(hfovRad != cachedHfov_))
            {
                cachedHfov_ = hfovRad;
                cachedTanHalfHfov_ = tanf(hfovRad * 0.5f);
            }
            if (unlikely(cachedUvScale_ == 0.0f))
            {
                cachedUvScale_ = static_cast<float>(CLOUDS_PANO_W) * FastMath::fastReciprocal(cloudScale);
            }
        }

        template <uint16_t WIDTH, uint16_t HEIGHT>
        PIP3D_FORCE_INLINE PIP3D_HOT IRAM_ATTR void
        drawClouds(uint16_t *__restrict__ buf,
                   int16_t bandOffY,
                   const Vector3 &camPos,
                   const Vector3 &camFwd,
                   const Vector3 &camRight,
                   const Vector3 &rawCamUp,
                   float vfovRad,
                   float hfovRad) const
        {
            if (unlikely(!enabled))
                return;

            prepareForFrame(vfovRad, hfovRad);

            Vector3 camUp = camRight.cross(camFwd);
            const float upLenSq = camUp.lengthSquared();
            camUp = (upLenSq >= 1e-6f) ? camUp * FastMath::fastInvSqrt(upLenSq) : rawCamUp;

            const float tanHalfVfov = cachedTanHalfVfov_;
            const float tanHalfHfov = cachedTanHalfHfov_;
            const float invHalfH = 2.0f / static_cast<float>(SCREEN_HEIGHT);
            const float invHalfW = 2.0f / static_cast<float>(SCREEN_WIDTH);
            const float uvScale = cachedUvScale_;

            constexpr float kMinFadeDist = 1200.0f;
            constexpr float kMaxFadeDist = 2400.0f;
            constexpr float kInvFadeRange = 1.0f / (kMaxFadeDist - kMinFadeDist);

            const float srx_uv = invHalfW * tanHalfHfov * camRight.x * uvScale;
            const float srz_uv = invHalfW * tanHalfHfov * camRight.z * uvScale;
            const float baseRx = camFwd.x - tanHalfHfov * camRight.x;
            const float baseRz = camFwd.z - tanHalfHfov * camRight.z;

            const float planeX = camPos.x - driftX;
            const float planeZ = camPos.z - driftZ;

            const uint8_t *PIP3D_RESTRICT alphaBase = &detail::s_cloudsAlphaData[0][0];
            const uint8_t *PIP3D_RESTRICT shadeBase = &detail::s_cloudsShadeData[0][0];
            const uint16_t *PIP3D_RESTRICT lutPtr = shadeLut;

            constexpr uint16_t widthHalf = WIDTH >> 1;

            for (uint16_t yb = 0; yb < HEIGHT; ++yb)
            {
                const int16_t globalY = bandOffY + static_cast<int16_t>(yb);

                const float yN = 1.0f - (static_cast<float>(globalY) + 0.5f) * invHalfH;
                const float yFactor = yN * tanHalfVfov;

                const float vy = camFwd.y + yFactor * camUp.y;
                if (vy <= 0.02f)
                    continue;

                const float t = cloudHeight * FastMath::fastReciprocal(vy);
                if (t >= kMaxFadeDist)
                    continue;

                const float distFade = (t > kMinFadeDist) ? (kMaxFadeDist - t) * kInvFadeRange : 1.0f;
                const uint32_t rowAlphaMul32 = static_cast<uint32_t>(cachedAlphaMul32_ * distFade + 0.5f);
                if (unlikely(rowAlphaMul32 == 0u))
                    continue;

                const float rx0 = baseRx + yFactor * camUp.x;
                const float rz0 = baseRz + yFactor * camUp.z;

                const float worldX0 = planeX + rx0 * t;
                const float worldZ0 = planeZ + rz0 * t;

                const float du = srx_uv * t;
                const float dv = srz_uv * t;

                int32_t u_fp = static_cast<int32_t>(worldX0 * uvScale * 65536.0f);
                int32_t v_fp = static_cast<int32_t>(worldZ0 * uvScale * 65536.0f);
                const int32_t du_fp2 = static_cast<int32_t>(du * 131072.0f);
                const int32_t dv_fp2 = static_cast<int32_t>(dv * 131072.0f);

                uint32_t *__restrict__ fbRow32 = reinterpret_cast<uint32_t *>(buf + static_cast<size_t>(yb) * WIDTH);

                for (uint16_t x2 = 0; x2 < widthHalf; ++x2)
                {
                    const uint32_t texIdx = (((uint32_t)v_fp >> 8) & 0xFF00u) | (((uint32_t)u_fp >> 16) & 0xFFu);
                    const uint8_t a8 = alphaBase[texIdx];

                    const uint32_t a5 = (static_cast<uint32_t>(a8) * rowAlphaMul32 + 127u) >> 8;
                    if (a5 > 0u)
                    {
                        const uint16_t shaded = lutPtr[shadeBase[texIdx]];
                        const uint16_t outPix = (a5 >= 32u) ? shaded : blendCloudPixel5Fast(static_cast<uint16_t>(fbRow32[x2]), shaded, a5);

                        fbRow32[x2] = static_cast<uint32_t>(outPix) | (static_cast<uint32_t>(outPix) << 16);
                    }

                    u_fp += du_fp2;
                    v_fp += dv_fp2;
                }

                if constexpr (WIDTH & 1u)
                {
                    const uint16_t xLast = WIDTH - 1;
                    const uint32_t texIdx = (((uint32_t)v_fp >> 8) & 0xFF00u) | (((uint32_t)u_fp >> 16) & 0xFFu);
                    const uint8_t a8 = alphaBase[texIdx];
                    const uint32_t a5 = (static_cast<uint32_t>(a8) * rowAlphaMul32 + 127u) >> 8;
                    if (a5 > 0u)
                    {
                        const uint16_t shaded = lutPtr[shadeBase[texIdx]];
                        buf[static_cast<size_t>(yb) * WIDTH + xLast] = (a5 >= 32u) ? shaded : blendCloudPixel5Fast(buf[static_cast<size_t>(yb) * WIDTH + xLast], shaded, a5);
                    }
                }
            }
        }
    };
}