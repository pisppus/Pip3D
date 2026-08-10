#pragma once

#include <cstdint>

#include "Core/Platform.hpp"
#include "Math/Algebra.hpp"
#include "Rendering/Buffers/ZBuffer.hpp"
#include "Rendering/Lighting/Fog.hpp"

namespace pip3D
{
    namespace Rasterizer
    {
        namespace detail
        {

            __attribute__((always_inline, hot)) static inline void IRAM_ATTR
            fillScanlinePlain(uint16_t *__restrict__ buf,
                              uint16_t *__restrict__ fb,
                              uint32_t count,
                              int32_t depthStart,
                              int32_t depthStep,
                              uint16_t color) noexcept
            {
                int32_t depth = depthStart;

                while (count >= 4)
                {
                    PIP3D_PREFETCH_W(buf + 16);
                    PIP3D_PREFETCH_W(fb + 16);

                    const uint16_t d0 = static_cast<uint16_t>(depth >> kZDepthShift);
                    const uint16_t d1 = static_cast<uint16_t>((depth + depthStep) >> kZDepthShift);
                    const uint16_t d2 = static_cast<uint16_t>((depth + depthStep * 2) >> kZDepthShift);
                    const uint16_t d3 = static_cast<uint16_t>((depth + depthStep * 3) >> kZDepthShift);

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
                    const uint16_t d = static_cast<uint16_t>(depth >> kZDepthShift);
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
                            uint16_t color) noexcept
            {
                const uint8_t *__restrict__ lutAlpha = g_fogLut.alpha;

                const FogState &fog = g_fogState;
                const uint32_t src_rb = color & 0xF81Fu;
                const uint32_t src_g = color & 0x07E0u;
                const uint32_t fog_rb = fog.color_rb;
                const uint32_t fog_g = fog.color_g;
                const uint16_t fog_col = fog.color;

                int32_t depth = depthStart;

                while (count >= 2)
                {
                    const uint16_t d0 = static_cast<uint16_t>(depth >> kZDepthShift);
                    const uint16_t d1 = static_cast<uint16_t>((depth + depthStep) >> kZDepthShift);

                    const uint16_t c0 = buf[0] & Z_DEPTH_MASK;
                    const uint16_t c1 = buf[1] & Z_DEPTH_MASK;

                    if (d0 > c0)
                    {
                        buf[0] = d0;
                        const uint16_t bucket = d0 >> 7;
                        const uint16_t frac = d0 & 0x7Fu;
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
                        const uint16_t bucket = d1 >> 7;
                        const uint16_t frac = d1 & 0x7Fu;
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
                    const uint16_t d = static_cast<uint16_t>(depth >> kZDepthShift);
                    const uint16_t c = *buf & Z_DEPTH_MASK;
                    if (d > c)
                    {
                        *buf = d;
                        const uint16_t bucket = d >> 7;
                        const uint16_t frac = d & 0x7Fu;
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
        }

        struct alignas(4) SolidParams
        {
            uint16_t *frameBuffer;
            uint16_t *zbBase;
            int32_t dz_dx_fixed;
            int32_t dz_dy_fixed;
            uint16_t color;
            bool fogOn;
            int16_t width;
            int16_t height;
        };

        __attribute__((always_inline)) static inline void IRAM_ATTR
        fillSolidHalf(float xa0, float ya0, float xa1, float ya1,
                      float xb0, float yb0, float xb1, float yb1,
                      int clampStartY,
                      int endYExclusive,
                      int32_t z_start_fixed_base,
                      const SolidParams &params) noexcept
        {
            const float dya = ya1 - ya0;
            const float dyb = yb1 - yb0;
            if (unlikely((dya > -1e-6f && dya < 1e-6f) ||
                         (dyb > -1e-6f && dyb < 1e-6f)))
                return;

            const float invDya = FastMath::fastReciprocal(dya);
            const float invDyb = FastMath::fastReciprocal(dyb);
            const float dx_dy_A = (xa1 - xa0) * invDya;
            const float dx_dy_B = (xb1 - xb0) * invDyb;

            const int clampEndY = endYExclusive > params.height ? params.height : endYExclusive;
            if (unlikely(clampStartY >= clampEndY))
                return;

            const float initY = static_cast<float>(clampStartY) + 0.5f;
            int32_t xA_fixed = static_cast<int32_t>((xa0 + dx_dy_A * (initY - ya0)) * 65536.0f);
            int32_t xB_fixed = static_cast<int32_t>((xb0 + dx_dy_B * (initY - yb0)) * 65536.0f);
            const int32_t dxA_fixed = static_cast<int32_t>(dx_dy_A * 65536.0f);
            const int32_t dxB_fixed = static_cast<int32_t>(dx_dy_B * 65536.0f);

            const bool aIsLeft = (xA_fixed <= xB_fixed);
            int32_t xL_fixed = aIsLeft ? xA_fixed : xB_fixed;
            int32_t xR_fixed = aIsLeft ? xB_fixed : xA_fixed;
            int32_t dxL_fixed = aIsLeft ? dxA_fixed : dxB_fixed;
            int32_t dxR_fixed = aIsLeft ? dxB_fixed : dxA_fixed;

            const int32_t dz_dx = params.dz_dx_fixed;
            const int32_t dz_dy = params.dz_dy_fixed;
            const uint32_t widthU = static_cast<uint32_t>(params.width);
            const int16_t width = params.width;
            const uint16_t color = params.color;
            const bool fogOn = params.fogOn;

            uint16_t *__restrict__ zbRow = params.zbBase +
                                           static_cast<size_t>(clampStartY) * widthU;
            uint16_t *__restrict__ fbRow = params.frameBuffer +
                                           static_cast<size_t>(clampStartY) * widthU;

            int32_t z_row_fixed = z_start_fixed_base;

            int rowsRemaining = clampEndY - clampStartY;
            while (rowsRemaining-- > 0)
            {
                int16_t xStart = static_cast<int16_t>((xL_fixed + 32767) >> 16);
                int16_t xEnd = static_cast<int16_t>((xR_fixed - 32769) >> 16);

                if (unlikely(xStart < 0))
                    xStart = 0;
                if (unlikely(xEnd >= width))
                    xEnd = width - 1;

                if (likely(xStart <= xEnd))
                {
                    const uint32_t cnt = static_cast<uint32_t>(xEnd - xStart + 1);
                    const int32_t depthStart = z_row_fixed + dz_dx * xStart;

                    if (fogOn)
                        detail::fillScanlineFog(zbRow + xStart, fbRow + xStart,
                                                cnt, depthStart, dz_dx, color);
                    else
                        detail::fillScanlinePlain(zbRow + xStart, fbRow + xStart,
                                                  cnt, depthStart, dz_dx, color);
                }

                xL_fixed += dxL_fixed;
                xR_fixed += dxR_fixed;
                z_row_fixed += dz_dy;
                zbRow += widthU;
                fbRow += widthU;
            }
        }

        __attribute__((hot)) inline void IRAM_ATTR
        fillTriangle(float x0, float y0, float z0,
                     float x1, float y1, float z1,
                     float x2, float y2, float z2,
                     uint16_t color,
                     uint16_t *__restrict__ frameBuffer,
                     ZBuffer *zBuffer,
                     const DisplayConfig &config) noexcept
        {
            if (unlikely(!frameBuffer || !zBuffer))
                return;

            if (y0 > y1)
            {
                float t;
                t = x0;
                x0 = x1;
                x1 = t;
                t = y0;
                y0 = y1;
                y1 = t;
                t = z0;
                z0 = z1;
                z1 = t;
            }
            if (y1 > y2)
            {
                float t;
                t = x1;
                x1 = x2;
                x2 = t;
                t = y1;
                y1 = y2;
                y2 = t;
                t = z1;
                z1 = z2;
                z2 = t;
            }
            if (y0 > y1)
            {
                float t;
                t = x0;
                x0 = x1;
                x1 = t;
                t = y0;
                y0 = y1;
                y1 = t;
                t = z0;
                z0 = z1;
                z1 = t;
            }

            const float dx02 = x0 - x2;
            const float dy12 = y1 - y2;
            const float dy02 = y0 - y2;
            const float dx12 = x1 - x2;
            const float det = dx02 * dy12 - dy02 * dx12;
            if (unlikely(det > -1e-6f && det < 1e-6f))
                return;

            const float invDet = FastMath::fastReciprocal(det);

            const float dz02 = z0 - z2;
            const float dz12 = z1 - z2;
            const float dz_dx = (dz02 * dy12 - dy02 * dz12) * invDet;
            const float dz_dy = (dx02 * dz12 - dz02 * dx12) * invDet;

            const float dz_dx_scaled = dz_dx * kZDepthScale;
            const float dz_dy_scaled = dz_dy * kZDepthScale;
            const float z2_scaled = z2 * kZDepthScale;

            const int32_t dz_dx_fixed = static_cast<int32_t>(dz_dx_scaled);
            const int32_t dz_dy_fixed = static_cast<int32_t>(dz_dy_scaled);

            const int16_t height = config.height;

            const int startTop = fastCeilNonNeg(y0 - 0.5f);
            const int startBottom = fastCeilNonNeg(y1 - 0.5f);
            int endTopExclusive = startBottom;
            int endBottomExclusive = fastCeilNonNeg(y2 - 0.5f);
            if (endTopExclusive > height)
                endTopExclusive = height;
            if (endBottomExclusive > height)
                endBottomExclusive = height;

            int clampStartY_top = startTop < 0 ? 0 : startTop;
            int clampStartY_bottom = startBottom < 0 ? 0 : startBottom;

            const bool runTop = (clampStartY_top < endTopExclusive);
            const bool runBottom = (clampStartY_bottom < endBottomExclusive);
            if (!runTop && !runBottom)
                return;

            SolidParams params;
            params.frameBuffer = frameBuffer;
            params.zbBase = zBuffer->data();
            params.dz_dx_fixed = dz_dx_fixed;
            params.dz_dy_fixed = dz_dy_fixed;
            params.color = color;
            params.fogOn = g_fogState.enabled;
            params.width = config.width;
            params.height = height;

            const float half_dx = dz_dx_scaled * 0.5f;
            const float z_base_common = z2_scaled - dz_dx_scaled * x2 + half_dx;
            const float y2_minus_half = y2 - 0.5f;

            if (runTop)
            {
                const float dy_init = static_cast<float>(clampStartY_top) - y2_minus_half;
                const float z_base = z_base_common + dz_dy_scaled * dy_init;
                const int32_t z_start_fixed_base = static_cast<int32_t>(z_base);

                fillSolidHalf(x0, y0, x1, y1,
                              x0, y0, x2, y2,
                              clampStartY_top, endTopExclusive,
                              z_start_fixed_base, params);
            }

            if (runBottom)
            {
                const float dy_init = static_cast<float>(clampStartY_bottom) - y2_minus_half;
                const float z_base = z_base_common + dz_dy_scaled * dy_init;
                const int32_t z_start_fixed_base = static_cast<int32_t>(z_base);

                fillSolidHalf(x1, y1, x2, y2,
                              x0, y0, x2, y2,
                              clampStartY_bottom, endBottomExclusive,
                              z_start_fixed_base, params);
            }
        }
    }
}