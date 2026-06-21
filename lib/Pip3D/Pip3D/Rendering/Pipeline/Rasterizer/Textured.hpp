#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "Core/Platform.hpp"
#include "Core/Viewport.hpp"
#include "Math/Algebra.hpp"
#include "Rendering/Display/ZBuffer.hpp"
#include "Rendering/Display/Texture.hpp"
#include "Rendering/Pipeline/Rasterizer/Common.hpp"

namespace pip3D
{
    namespace Rasterizer
    {
        inline void fillTriangleTextured(float x0, float y0, float z0,
                                         float x1, float y1, float z1,
                                         float x2, float y2, float z2,
                                         float u0, float v0,
                                         float u1, float v1,
                                         float u2, float v2,
                                         float w0, float w1, float w2,
                                         const Texture &tex,
                                         uint16_t *frameBuffer,
                                         ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer,
                                         const DisplayConfig &config)
        {
            const int16_t width = config.width;
            const int16_t height = config.height;

            if (unlikely(!frameBuffer || !zBuffer))
                return;

            if (y0 > y1)
            {
                std::swap(x0, x1);
                std::swap(y0, y1);
                std::swap(z0, z1);
                std::swap(u0, u1);
                std::swap(v0, v1);
                std::swap(w0, w1);
            }
            if (y1 > y2)
            {
                std::swap(x1, x2);
                std::swap(y1, y2);
                std::swap(z1, z2);
                std::swap(u1, u2);
                std::swap(v1, v2);
                std::swap(w1, w2);
            }
            if (y0 > y1)
            {
                std::swap(x0, x1);
                std::swap(y0, y1);
                std::swap(z0, z1);
                std::swap(u0, u1);
                std::swap(v0, v1);
                std::swap(w0, w1);
            }

            if (y0 == y2)
                return;
            if (unlikely(x0 == x1 && x1 == x2))
                return;

            const float dx02 = x0 - x2;
            const float dy12 = y1 - y2;
            const float dy02 = y0 - y2;
            const float dx12 = x1 - x2;

            const float det = dx02 * dy12 - dy02 * dx12;
            if (unlikely(fabsf(det) < 1e-6f))
                return;

            const float invDet = FastMath::fastReciprocal(det);

            const float dz02 = z0 - z2;
            const float dz12 = z1 - z2;
            const float dz_dx = (dz02 * dy12 - dy02 * dz12) * invDet;
            const float dz_dy = (dx02 * dz12 - dz02 * dx12) * invDet;

            constexpr float depthScale = 32638.0f;
            const float dz_dx_scaled = dz_dx * depthScale;
            const float dz_dy_scaled = dz_dy * depthScale;
            const float z2_scaled = z2 * depthScale;

            const float texW = static_cast<float>(tex.widthMask + 1);
            const float texH = static_cast<float>(tex.heightMask + 1);

            const float tu0 = u0 * texW;
            const float tu1 = u1 * texW;
            const float tu2 = u2 * texW;
            const float tv0 = v0 * texH;
            const float tv1 = v1 * texH;
            const float tv2 = v2 * texH;

            const float q0 = 1.0f / w0;
            const float q1 = 1.0f / w1;
            const float q2 = 1.0f / w2;

            const float u_over_z0 = tu0 * q0;
            const float u_over_z1 = tu1 * q1;
            const float u_over_z2 = tu2 * q2;
            const float v_over_z0 = tv0 * q0;
            const float v_over_z1 = tv1 * q1;
            const float v_over_z2 = tv2 * q2;

            const float dq02 = q0 - q2;
            const float dq12 = q1 - q2;
            const float dq_dx = (dq02 * dy12 - dy02 * dq12) * invDet;
            const float dq_dy = (dx02 * dq12 - dq02 * dx12) * invDet;

            const float du_over_z02 = u_over_z0 - u_over_z2;
            const float du_over_z12 = u_over_z1 - u_over_z2;
            const float du_over_z_dx = (du_over_z02 * dy12 - dy02 * du_over_z12) * invDet;
            const float du_over_z_dy = (dx02 * du_over_z12 - du_over_z02 * dx12) * invDet;

            const float dv_over_z02 = v_over_z0 - v_over_z2;
            const float dv_over_z12 = v_over_z1 - v_over_z2;
            const float dv_over_z_dx = (dv_over_z02 * dy12 - dy02 * dv_over_z12) * invDet;
            const float dv_over_z_dy = (dx02 * dv_over_z12 - dv_over_z02 * dx12) * invDet;

            const int startTop = static_cast<int>(ceilf(y0 - 0.5f));
            const int startBottom = static_cast<int>(ceilf(y1 - 0.5f));
            int endTopExclusive = startBottom;
            int endBottomExclusive = static_cast<int>(ceilf(y2 - 0.5f));

            int clampStartY_top = startTop < 0 ? 0 : startTop;
            int clampStartY_bottom = startBottom < 0 ? 0 : startBottom;

            if (endTopExclusive > height)
                endTopExclusive = height;
            if (endBottomExclusive > height)
                endBottomExclusive = height;

            const bool runTop = (clampStartY_top < endTopExclusive) && (clampStartY_top < height);
            const bool runBottom = (clampStartY_bottom < endBottomExclusive) && (clampStartY_bottom < height);
            if (!runTop && !runBottom)
                return;

            const float dy02_val = y2 - y0;
            const float dy01_val = y1 - y0;
            const float dy12_val = y2 - y1;

            const int32_t step_02 = (fabsf(dy02_val) > 1e-6f) ? static_cast<int32_t>(((x2 - x0) / dy02_val) * 65536.0f) : 0;
            const int32_t step_01 = (fabsf(dy01_val) > 1e-6f) ? static_cast<int32_t>(((x1 - x0) / dy01_val) * 65536.0f) : 0;
            const int32_t step_12 = (fabsf(dy12_val) > 1e-6f) ? static_cast<int32_t>(((x2 - x1) / dy12_val) * 65536.0f) : 0;

            int16_t *__restrict__ zbBase = const_cast<int16_t *>(zBuffer->getBufferPtr());

            const bool fogEnabled = g_fogState.enabled;
            const float fogKVal = g_fogState.kVal;
            const float fogKnVal = g_fogState.knVal;
            const float fogWorldNear = g_fogState.worldNear;
            const float fogWorldScale32 = g_fogState.worldScale32;
            const uint16_t fogColor = g_fogState.color;
            const uint32_t fogColorRb = g_fogState.color_rb;
            const uint32_t fogColorG = g_fogState.color_g;

            const uint16_t *const __restrict__ texData = tex.data;
            const uint32_t texShiftU = tex.widthShift;
            const uint32_t texMaskU = tex.widthMask;
            const uint32_t texMaskV = tex.heightMask;
            const int32_t z_step = static_cast<int32_t>(dz_dx_scaled * 16384.0f);

            auto drawSpanSubdivided = [&](int y, int16_t x_start, int16_t x_end)
            {
                int16_t count = x_end - x_start + 1;
                if (count <= 0)
                    return;

                const float dy_factor = (static_cast<float>(y) + 0.5f - y2);
                const float z_dy_term = z2_scaled + dy_factor * dz_dy_scaled;

                float q = q2 + dy_factor * dq_dy + ((static_cast<float>(x_start) + 0.5f) - x2) * dq_dx;
                float u_over_z = u_over_z2 + dy_factor * du_over_z_dy + ((static_cast<float>(x_start) + 0.5f) - x2) * du_over_z_dx;
                float v_over_z = v_over_z2 + dy_factor * dv_over_z_dy + ((static_cast<float>(x_start) + 0.5f) - x2) * dv_over_z_dx;

                float w_start = FastMath::fastReciprocal(q);
                float u = u_over_z * w_start;
                float v = v_over_z * w_start;

                int16_t cur_x = x_start;

                size_t index = static_cast<size_t>(y) * width + cur_x;
                int16_t *__restrict__ zb = zbBase + index;
                uint16_t *__restrict__ fb = frameBuffer + index;

                while (count > 0)
                {
                    const int16_t step = (count > 16) ? 16 : count;

                    const float next_q = q + step * dq_dx;
                    const float next_u_over_z = u_over_z + step * du_over_z_dx;
                    const float next_v_over_z = v_over_z + step * dv_over_z_dx;

                    const float next_w = FastMath::fastReciprocal(next_q);
                    const float next_u = next_u_over_z * next_w;
                    const float next_v = next_v_over_z * next_w;

                    const float inv_step = likely(step == 16) ? 0.0625f : FastMath::fastReciprocal(static_cast<float>(step));
                    const float du = (next_u - u) * inv_step;
                    const float dv = (next_v - v) * inv_step;

                    int32_t u_fixed = static_cast<int32_t>(u * 65536.0f);
                    int32_t v_fixed = static_cast<int32_t>(v * 65536.0f);
                    const int32_t du_fixed = static_cast<int32_t>(du * 65536.0f);
                    const int32_t dv_fixed = static_cast<int32_t>(dv * 65536.0f);

                    const float z_scaled_start = z_dy_term + (static_cast<float>(cur_x) + 0.5f - x2) * dz_dx_scaled;
                    int32_t z_val = static_cast<int32_t>(z_scaled_start * 16384.0f);

                    PIP3D_PREFETCH_W(zb + 16);
                    PIP3D_PREFETCH_R(fb + 16);

                    for (int16_t i = 0; i < step; ++i)
                    {
                        const int16_t d = static_cast<int16_t>(z_val >> 14);
                        const int16_t curr = *zb & 0x7FFF;
                        if (d < curr)
                        {
                            *zb = d;
                            const uint32_t tu = (static_cast<uint32_t>(u_fixed) >> 16) & texMaskU;
                            const uint32_t tv = (static_cast<uint32_t>(v_fixed) >> 16) & texMaskV;
                            const uint16_t texColor = texData[(tv << texShiftU) | tu];

                            if (fogEnabled)
                            {
                                float denom = fogKVal - static_cast<float>(d);
                                if (unlikely(denom < 1.0f))
                                    denom = 1.0f;
                                const float z_eye = fogKnVal * FastMath::fastReciprocal(denom);

                                const float fogF = (z_eye - fogWorldNear) * fogWorldScale32;
                                const int32_t f_alpha = static_cast<int32_t>(fogF);

                                if (f_alpha <= 0)
                                {
                                    *fb = texColor;
                                }
                                else if (f_alpha >= 32)
                                {
                                    *fb = fogColor;
                                }
                                else
                                {
                                    const uint32_t inv_f_alpha = 32 - f_alpha;

                                    const uint32_t rb1 = texColor & 0xF81F;
                                    const uint32_t g1 = texColor & 0x07E0;

                                    const uint32_t rb = ((rb1 * inv_f_alpha + fogColorRb * f_alpha) >> 5) & 0xF81F;
                                    const uint32_t g = ((g1 * inv_f_alpha + fogColorG * f_alpha) >> 5) & 0x07E0;
                                    *fb = static_cast<uint16_t>(rb | g);
                                }
                            }
                            else
                            {
                                *fb = texColor;
                            }
                        }
                        z_val += z_step;
                        u_fixed += du_fixed;
                        v_fixed += dv_fixed;
                        ++zb;
                        ++fb;
                    }

                    q = next_q;
                    u_over_z = next_u_over_z;
                    v_over_z = next_v_over_z;
                    u = next_u;
                    v = next_v;

                    cur_x += step;
                    count -= step;
                }
            };

            if (runTop)
            {
                const float slope_02 = (x2 - x0) / dy02_val;
                const float slope_01 = (x1 - x0) / dy01_val;
                const float dy_init = (static_cast<float>(clampStartY_top) + 0.5f) - y0;

                int32_t x02_fixed = static_cast<int32_t>((x0 + slope_02 * dy_init) * 65536.0f);
                int32_t x01_fixed = static_cast<int32_t>((x0 + slope_01 * dy_init) * 65536.0f);

                for (int y = clampStartY_top; y < endTopExclusive; ++y)
                {
                    int32_t xl = x02_fixed;
                    int32_t xr = x01_fixed;
                    if (xl > xr)
                        std::swap(xl, xr);

                    int16_t x_start = static_cast<int16_t>((xl + 32767) >> 16);
                    int16_t x_end = static_cast<int16_t>((xr - 32769) >> 16);

                    if (x_start < 0)
                        x_start = 0;
                    if (x_end >= width)
                        x_end = width - 1;

                    if (x_start <= x_end)
                        drawSpanSubdivided(y, x_start, x_end);

                    x02_fixed += step_02;
                    x01_fixed += step_01;
                }
            }

            if (runBottom)
            {
                const float slope_02 = (x2 - x0) / dy02_val;
                const float slope_12 = (x2 - x1) / dy12_val;
                const float dy_init_bottom = (static_cast<float>(clampStartY_bottom) + 0.5f) - y1;
                const float dy_init_long = (static_cast<float>(clampStartY_bottom) + 0.5f) - y0;

                int32_t x12_fixed = static_cast<int32_t>((x1 + slope_12 * dy_init_bottom) * 65536.0f);
                int32_t x02_bottom_fixed = static_cast<int32_t>((x0 + slope_02 * dy_init_long) * 65536.0f);

                for (int y = clampStartY_bottom; y < endBottomExclusive; ++y)
                {
                    int32_t xl = x02_bottom_fixed;
                    int32_t xr = x12_fixed;
                    if (xl > xr)
                        std::swap(xl, xr);

                    int16_t x_start = static_cast<int16_t>((xl + 32767) >> 16);
                    int16_t x_end = static_cast<int16_t>((xr - 32769) >> 16);

                    if (x_start < 0)
                        x_start = 0;
                    if (x_end >= width)
                        x_end = width - 1;

                    if (x_start <= x_end)
                        drawSpanSubdivided(y, x_start, x_end);

                    x02_bottom_fixed += step_02;
                    x12_fixed += step_12;
                }
            }
        }
    }
}
