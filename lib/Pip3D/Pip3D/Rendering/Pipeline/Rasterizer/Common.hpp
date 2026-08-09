#pragma once

#include <cstdint>

#include "Core/Platform.hpp"
#include "Math/Algebra.hpp"
#include "Rendering/Buffers/ZBuffer.hpp"

namespace pip3D
{
    namespace detail
    {
        alignas(16) static constexpr int16_t kBayerMatrix10Bit[4][4] = {
            {0, 512, 128, 640},
            {768, 256, 896, 384},
            {192, 704, 64, 576},
            {960, 448, 832, 320}};
    }

    namespace Rasterizer
    {
        struct alignas(16) FogState
        {
            float worldNear = 0.0f;
            float worldScale = 0.0f;
            float worldScale32 = 0.0f;
            float wScale = 0.0f;
            float invWScale = 0.0f;
            float color_r = 0.0f;
            float color_g_f = 0.0f;
            float color_b_f = 0.0f;
            uint32_t color_rb = 0;
            uint32_t color_g = 0;
            uint16_t color = 0;
            bool enabled = false;
        };

        inline FogState g_fogState{};
        inline bool g_mipmapsEnabled = true;
        inline float g_ambientScale = 1.0f;
        inline float g_exposureScale = 1.0f;

        struct FogLut
        {
            uint8_t alpha[257];
            bool valid;
        };

        inline FogLut g_fogLut{};

        __attribute__((hot)) inline void rebuildFogLut()
        {
            const FogState &f = g_fogState;
            if (!f.enabled)
            {
                g_fogLut.valid = false;
                return;
            }

            const float wScale = f.wScale;
            const float worldNear = f.worldNear;
            const float worldScale32 = f.worldScale32;

            for (uint32_t i = 0; i < 257; ++i)
            {
                const uint32_t d = i * 128u;
                if (d == 0u)
                {

                    g_fogLut.alpha[i] = 32u;
                    continue;
                }
                const float z_eye = wScale * FastMath::fastReciprocal(static_cast<float>(d));
                float a = (z_eye - worldNear) * worldScale32;
                if (a < 0.0f)
                    a = 0.0f;
                else if (a > 32.0f)
                    a = 32.0f;
                g_fogLut.alpha[i] = static_cast<uint8_t>(a + 0.5f);
            }
            g_fogLut.valid = true;
        }

        __attribute__((always_inline, hot)) static inline void IRAM_ATTR
        fillScanlinePlain(uint16_t *__restrict__ buf,
                          uint16_t *__restrict__ fb,
                          uint32_t count,
                          int32_t depthStart,
                          int32_t depthStep,
                          uint16_t color)
        {
            int32_t depth = depthStart;

            while (count >= 4)
            {
                PIP3D_PREFETCH_W(buf + 16);
                PIP3D_PREFETCH_W(fb + 16);

                const uint16_t d0 = static_cast<uint16_t>(depth >> 14);
                const uint16_t d1 = static_cast<uint16_t>((depth + depthStep) >> 14);
                const uint16_t d2 = static_cast<uint16_t>((depth + depthStep * 2) >> 14);
                const uint16_t d3 = static_cast<uint16_t>((depth + depthStep * 3) >> 14);

                const uint16_t c0 = buf[0] & Z_DEPTH_MASK;
                const uint16_t c1 = buf[1] & Z_DEPTH_MASK;
                const uint16_t c2 = buf[2] & Z_DEPTH_MASK;
                const uint16_t c3 = buf[3] & Z_DEPTH_MASK;

                if (d0 > c0)
                {
                    buf[0] = d0;
                    fb[0] = color;
                }
                if (d1 > c1)
                {
                    buf[1] = d1;
                    fb[1] = color;
                }
                if (d2 > c2)
                {
                    buf[2] = d2;
                    fb[2] = color;
                }
                if (d3 > c3)
                {
                    buf[3] = d3;
                    fb[3] = color;
                }

                depth += depthStep * 4;
                buf += 4;
                fb += 4;
                count -= 4;
            }

            while (count > 0)
            {
                const uint16_t d = static_cast<uint16_t>(depth >> 14);
                const uint16_t c = *buf & Z_DEPTH_MASK;
                if (d > c)
                {
                    *buf = d;
                    *fb = color;
                }
                depth += depthStep;
                ++buf;
                ++fb;
                --count;
            }
        }

        __attribute__((always_inline, hot)) static inline void IRAM_ATTR
        fillScanlineFog(uint16_t *__restrict__ buf,
                        uint16_t *__restrict__ fb,
                        uint32_t count,
                        int32_t depthStart,
                        int32_t depthStep,
                        uint16_t color)
        {
            const uint8_t *__restrict__ lutAlpha = g_fogLut.alpha;

            const Rasterizer::FogState &fog = g_fogState;
            const uint32_t src_rb = color & 0xF81Fu;
            const uint32_t src_g = color & 0x07E0u;
            const uint32_t fog_rb = fog.color_rb;
            const uint32_t fog_g = fog.color_g;
            const uint16_t fog_col = fog.color;

            int32_t depth = depthStart;

            while (count >= 2)
            {
                const uint16_t d0 = static_cast<uint16_t>(depth >> 14);
                const uint16_t d1 = static_cast<uint16_t>((depth + depthStep) >> 14);

                const uint16_t c0 = buf[0] & Z_DEPTH_MASK;
                const uint16_t c1 = buf[1] & Z_DEPTH_MASK;

                if (d0 > c0)
                {
                    buf[0] = d0;
                    const uint16_t b0 = d0;
                    const uint16_t bucket = b0 >> 7;
                    const uint16_t frac = b0 & 0x7Fu;
                    const uint8_t a0 = lutAlpha[bucket];
                    const uint8_t a1 = lutAlpha[bucket + 1];
                    const uint8_t alpha = a0 + (static_cast<uint8_t>((a1 - a0) * frac >> 7));

                    if (alpha == 0)
                        fb[0] = color;
                    else if (alpha == 32)
                        fb[0] = fog_col;
                    else
                    {
                        const uint32_t inv_a = 32u - alpha;
                        const uint32_t rb = ((src_rb * inv_a + fog_rb * alpha) >> 5) & 0xF81Fu;
                        const uint32_t g = ((src_g * inv_a + fog_g * alpha) >> 5) & 0x07E0u;
                        fb[0] = static_cast<uint16_t>(rb | g);
                    }
                }
                if (d1 > c1)
                {
                    buf[1] = d1;
                    const uint16_t b1 = d1;
                    const uint16_t bucket = b1 >> 7;
                    const uint16_t frac = b1 & 0x7Fu;
                    const uint8_t a0 = lutAlpha[bucket];
                    const uint8_t a1 = lutAlpha[bucket + 1];
                    const uint8_t alpha = a0 + (static_cast<uint8_t>((a1 - a0) * frac >> 7));

                    if (alpha == 0)
                        fb[1] = color;
                    else if (alpha == 32)
                        fb[1] = fog_col;
                    else
                    {
                        const uint32_t inv_a = 32u - alpha;
                        const uint32_t rb = ((src_rb * inv_a + fog_rb * alpha) >> 5) & 0xF81Fu;
                        const uint32_t g = ((src_g * inv_a + fog_g * alpha) >> 5) & 0x07E0u;
                        fb[1] = static_cast<uint16_t>(rb | g);
                    }
                }

                depth += depthStep * 2;
                buf += 2;
                fb += 2;
                count -= 2;
            }

            while (count > 0)
            {
                const uint16_t d = static_cast<uint16_t>(depth >> 14);
                const uint16_t c = *buf & Z_DEPTH_MASK;
                if (d > c)
                {
                    *buf = d;
                    const uint16_t b = d;
                    const uint16_t bucket = b >> 7;
                    const uint16_t frac = b & 0x7Fu;
                    const uint8_t a0 = lutAlpha[bucket];
                    const uint8_t a1 = lutAlpha[bucket + 1];
                    const uint8_t alpha = a0 + (static_cast<uint8_t>((a1 - a0) * frac >> 7));

                    if (alpha == 0)
                        *fb = color;
                    else if (alpha == 32)
                        *fb = fog_col;
                    else
                    {
                        const uint32_t inv_a = 32u - alpha;
                        const uint32_t rb = ((src_rb * inv_a + fog_rb * alpha) >> 5) & 0xF81Fu;
                        const uint32_t g = ((src_g * inv_a + fog_g * alpha) >> 5) & 0x07E0u;
                        *fb = static_cast<uint16_t>(rb | g);
                    }
                }
                depth += depthStep;
                ++buf;
                ++fb;
                --count;
            }
        }

        __attribute__((always_inline, hot)) static inline void IRAM_ATTR
        fillScanline(uint16_t *__restrict__ buf,
                     uint16_t *__restrict__ fb,
                     uint32_t count,
                     int32_t depthStart,
                     int32_t depthStep,
                     uint16_t color)
        {
            if (g_fogState.enabled && g_fogLut.valid)
                fillScanlineFog(buf, fb, count, depthStart, depthStep, color);
            else
                fillScanlinePlain(buf, fb, count, depthStart, depthStep, color);
        }

        struct alignas(16) PlanarParams
        {
            uint16_t *frameBuffer;
            uint16_t *zbBase;
            int32_t dz_dx_fixed;
            int32_t dz_dy_fixed;
            uint32_t s_rb;
            uint32_t s_g;
            uint8_t alpha;
            bool softEdges;
            int startTopGlobal;
            int endBottomGlobal;
            int16_t width;
            int16_t height;
            int16_t offsetY;
        };

        struct alignas(16) SmoothParams
        {
            uint16_t *frameBuffer;
            uint16_t *zbBase;
            int32_t dz_dx_fixed;
            int32_t dz_dy_fixed;
            int32_t dr_dx_fixed;
            int32_t dr_dy_fixed;
            int32_t dg_dx_fixed;
            int32_t dg_dy_fixed;
            int32_t db_dx_fixed;
            int32_t db_dy_fixed;
            int16_t width;
            int16_t height;
        };
    }
}
