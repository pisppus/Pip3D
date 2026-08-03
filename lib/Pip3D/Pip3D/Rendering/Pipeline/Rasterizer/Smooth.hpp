#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "Core/Platform.hpp"
#include "Math/Algebra.hpp"
#include "Rendering/Buffers/ZBuffer.hpp"
#include "Rendering/Pipeline/Rasterizer/Common.hpp"

namespace pip3D
{
    namespace Rasterizer
    {
        inline void fillSmoothHalf(
            float xa0, float ya0,
            float xa1, float ya1,
            float xb0, float yb0,
            float xb1, float yb1,
            int startY, int endYExclusive,
            int32_t z_start_fixed_base,
            int32_t r_start_fixed_base,
            int32_t g_start_fixed_base,
            int32_t b_start_fixed_base,
            int clampStartY,
            const SmoothParams &params)
        {
            float dya = ya1 - ya0;
            float dyb = yb1 - yb0;
            if (unlikely(fabsf(dya) < 1e-6f || fabsf(dyb) < 1e-6f))
                return;

            float invDya = FastMath::fastReciprocal(dya);
            float invDyb = FastMath::fastReciprocal(dyb);

            float dx_dy_A = (xa1 - xa0) * invDya;
            float dx_dy_B = (xb1 - xb0) * invDyb;

            int clampEndY = endYExclusive > params.height ? params.height : endYExclusive;
            if (unlikely(clampStartY >= clampEndY))
                return;

            float initY = static_cast<float>(clampStartY) + 0.5f;
            float leftX = xa0 + dx_dy_A * (initY - ya0);
            float rightX = xb0 + dx_dy_B * (initY - yb0);

            int32_t leftX_fixed = static_cast<int32_t>(leftX * 65536.0f);
            int32_t rightX_fixed = static_cast<int32_t>(rightX * 65536.0f);
            int32_t dx_dy_A_fixed = static_cast<int32_t>(dx_dy_A * 65536.0f);
            int32_t dx_dy_B_fixed = static_cast<int32_t>(dx_dy_B * 65536.0f);

            if (leftX_fixed > rightX_fixed)
            {
                std::swap(leftX_fixed, rightX_fixed);
                std::swap(dx_dy_A_fixed, dx_dy_B_fixed);
            }

            int32_t z_row_fixed = z_start_fixed_base;
            int32_t r_row_fixed = r_start_fixed_base;
            int32_t g_row_fixed = g_start_fixed_base;
            int32_t b_row_fixed = b_start_fixed_base;

            for (int y = clampStartY; y < clampEndY; ++y)
            {
                int16_t xStart = static_cast<int16_t>((leftX_fixed + 65535) >> 16);
                int16_t xEnd = static_cast<int16_t>((rightX_fixed + 65535) >> 16) - 1;

                if (xStart < 0)
                    xStart = 0;
                if (xEnd >= params.width)
                    xEnd = params.width - 1;

                if (xStart <= xEnd)
                {
                    int32_t depth_fixed = z_row_fixed + params.dz_dx_fixed * xStart;
                    int32_t r_fixed = r_row_fixed + params.dr_dx_fixed * xStart;
                    int32_t g_fixed = g_row_fixed + params.dg_dx_fixed * xStart;
                    int32_t b_fixed = b_row_fixed + params.db_dx_fixed * xStart;

                    uint16_t *__restrict__ zPtr = params.zbBase + static_cast<size_t>(y) * params.width + xStart;
                    uint16_t *__restrict__ fbPtr = params.frameBuffer + static_cast<size_t>(y) * params.width + xStart;

                    const int16_t *bayerRow = detail::kBayerMatrix10Bit[y & 3];

                    const int32_t bayer0 = bayerRow[xStart & 3];
                    const int32_t bayer1 = bayerRow[(xStart + 1) & 3];
                    const int32_t bayer2 = bayerRow[(xStart + 2) & 3];
                    const int32_t bayer3 = bayerRow[(xStart + 3) & 3];

                    PIP3D_PREFETCH_W(zPtr);
                    PIP3D_PREFETCH_W(fbPtr);

                    int16_t count = xEnd - xStart + 1;

                    while (count >= 4)
                    {
                        PIP3D_PREFETCH_W(zPtr + 16);
                        PIP3D_PREFETCH_W(fbPtr + 16);

                        {
                            const uint16_t stored = zPtr[0];
                            const uint16_t depthNoShadow = stored & Z_DEPTH_MASK;
                            const uint16_t d = static_cast<uint16_t>(depth_fixed >> 14);

                            if (d > depthNoShadow)
                            {
                                zPtr[0] = d;

                                int32_t ir = (r_fixed + bayer0) >> 10;
                                int32_t ig = (g_fixed + bayer0) >> 10;
                                int32_t ib = (b_fixed + bayer0) >> 10;

                                uint16_t rc = ir < 0 ? 0 : (ir > 31 ? 31 : ir);
                                uint16_t gc = ig < 0 ? 0 : (ig > 63 ? 63 : ig);
                                uint16_t bc = ib < 0 ? 0 : (ib > 31 ? 31 : ib);

                                fbPtr[0] = (rc << 11) | (gc << 5) | bc;
                            }
                            depth_fixed += params.dz_dx_fixed;
                            r_fixed += params.dr_dx_fixed;
                            g_fixed += params.dg_dx_fixed;
                            b_fixed += params.db_dx_fixed;
                        }

                        {
                            const uint16_t stored = zPtr[1];
                            const uint16_t depthNoShadow = stored & Z_DEPTH_MASK;
                            const uint16_t d = static_cast<uint16_t>(depth_fixed >> 14);

                            if (d > depthNoShadow)
                            {
                                zPtr[1] = d;

                                int32_t ir = (r_fixed + bayer1) >> 10;
                                int32_t ig = (g_fixed + bayer1) >> 10;
                                int32_t ib = (b_fixed + bayer1) >> 10;

                                uint16_t rc = ir < 0 ? 0 : (ir > 31 ? 31 : ir);
                                uint16_t gc = ig < 0 ? 0 : (ig > 63 ? 63 : ig);
                                uint16_t bc = ib < 0 ? 0 : (ib > 31 ? 31 : ib);

                                fbPtr[1] = (rc << 11) | (gc << 5) | bc;
                            }
                            depth_fixed += params.dz_dx_fixed;
                            r_fixed += params.dr_dx_fixed;
                            g_fixed += params.dg_dx_fixed;
                            b_fixed += params.db_dx_fixed;
                        }

                        {
                            const uint16_t stored = zPtr[2];
                            const uint16_t depthNoShadow = stored & Z_DEPTH_MASK;
                            const uint16_t d = static_cast<uint16_t>(depth_fixed >> 14);

                            if (d > depthNoShadow)
                            {
                                zPtr[2] = d;

                                int32_t ir = (r_fixed + bayer2) >> 10;
                                int32_t ig = (g_fixed + bayer2) >> 10;
                                int32_t ib = (b_fixed + bayer2) >> 10;

                                uint16_t rc = ir < 0 ? 0 : (ir > 31 ? 31 : ir);
                                uint16_t gc = ig < 0 ? 0 : (ig > 63 ? 63 : ig);
                                uint16_t bc = ib < 0 ? 0 : (ib > 31 ? 31 : ib);

                                fbPtr[2] = (rc << 11) | (gc << 5) | bc;
                            }
                            depth_fixed += params.dz_dx_fixed;
                            r_fixed += params.dr_dx_fixed;
                            g_fixed += params.dg_dx_fixed;
                            b_fixed += params.db_dx_fixed;
                        }

                        {
                            const uint16_t stored = zPtr[3];
                            const uint16_t depthNoShadow = stored & Z_DEPTH_MASK;
                            const uint16_t d = static_cast<uint16_t>(depth_fixed >> 14);

                            if (d > depthNoShadow)
                            {
                                zPtr[3] = d;

                                int32_t ir = (r_fixed + bayer3) >> 10;
                                int32_t ig = (g_fixed + bayer3) >> 10;
                                int32_t ib = (b_fixed + bayer3) >> 10;

                                uint16_t rc = ir < 0 ? 0 : (ir > 31 ? 31 : ir);
                                uint16_t gc = ig < 0 ? 0 : (ig > 63 ? 63 : ig);
                                uint16_t bc = ib < 0 ? 0 : (ib > 31 ? 31 : ib);

                                fbPtr[3] = (rc << 11) | (gc << 5) | bc;
                            }
                            depth_fixed += params.dz_dx_fixed;
                            r_fixed += params.dr_dx_fixed;
                            g_fixed += params.dg_dx_fixed;
                            b_fixed += params.db_dx_fixed;
                        }

                        zPtr += 4;
                        fbPtr += 4;
                        count -= 4;
                    }

                    int16_t remIndex = 0;
                    while (count > 0)
                    {
                        const uint16_t stored = zPtr[0];
                        const uint16_t depthNoShadow = stored & Z_DEPTH_MASK;
                        const uint16_t d = static_cast<uint16_t>(depth_fixed >> 14);

                        if (d > depthNoShadow)
                        {
                            zPtr[0] = d;

                            const int32_t bayerValue = bayerRow[(xStart + remIndex) & 3];
                            int32_t ir = (r_fixed + bayerValue) >> 10;
                            int32_t ig = (g_fixed + bayerValue) >> 10;
                            int32_t ib = (b_fixed + bayerValue) >> 10;

                            uint16_t rc = ir < 0 ? 0 : (ir > 31 ? 31 : ir);
                            uint16_t gc = ig < 0 ? 0 : (ig > 63 ? 63 : ig);
                            uint16_t bc = ib < 0 ? 0 : (ib > 31 ? 31 : ib);

                            fbPtr[0] = (rc << 11) | (gc << 5) | bc;
                        }

                        depth_fixed += params.dz_dx_fixed;
                        r_fixed += params.dr_dx_fixed;
                        g_fixed += params.dg_dx_fixed;
                        b_fixed += params.db_dx_fixed;

                        zPtr++;
                        fbPtr++;
                        remIndex++;
                        count--;
                    }
                }

                leftX_fixed += dx_dy_A_fixed;
                rightX_fixed += dx_dy_B_fixed;
                z_row_fixed += params.dz_dy_fixed;
                r_row_fixed += params.dr_dy_fixed;
                g_row_fixed += params.dg_dy_fixed;
                b_row_fixed += params.db_dy_fixed;
            }
        }

        inline void fillTriangleSmooth(
            int16_t x0, int16_t y0, float z0,
            int16_t x1, int16_t y1, float z1,
            int16_t x2, int16_t y2, float z2,
            float r0, float g0, float b0,
            float r1, float g1, float b1,
            float r2, float g2, float b2,
            uint16_t *frameBuffer,
            ZBuffer *zBuffer,
            const DisplayConfig &config)
        {
            const int16_t width = config.width;
            const int16_t height = config.height;

            if (unlikely(!frameBuffer || !zBuffer))
                return;

            uint16_t *const zBufferData = zBuffer->data();
            if (unlikely(!zBufferData))
                return;

            if (y0 > y1)
            {
                std::swap(x0, x1);
                std::swap(y0, y1);
                std::swap(z0, z1);
                std::swap(r0, r1);
                std::swap(g0, g1);
                std::swap(b0, b1);
            }
            if (y1 > y2)
            {
                std::swap(x1, x2);
                std::swap(y1, y2);
                std::swap(z1, z2);
                std::swap(r1, r2);
                std::swap(g1, g2);
                std::swap(b1, b2);
            }
            if (y0 > y1)
            {
                std::swap(x0, x1);
                std::swap(y0, y1);
                std::swap(z0, z1);
                std::swap(r0, r1);
                std::swap(g0, g1);
                std::swap(b0, b1);
            }

            if (y0 == y2)
                return;
            if (unlikely(x0 == x1 && x1 == x2))
                return;

            float dx02 = (float)(x0 - x2);
            float dy12 = (float)(y1 - y2);
            float dy02 = (float)(y0 - y2);
            float dx12 = (float)(x1 - x2);

            float det = dx02 * dy12 - dy02 * dx12;
            if (unlikely(fabsf(det) < 1e-6f))
                return;

            float invDet = FastMath::fastReciprocal(det);

            float dz02 = z0 - z2;
            float dz12 = z1 - z2;
            float dz_dx = (dz02 * dy12 - dy02 * dz12) * invDet;
            float dz_dy = (dx02 * dz12 - dz02 * dx12) * invDet;

            float dr02 = r0 - r2;
            float dr12 = r1 - r2;
            float dr_dx = (dr02 * dy12 - dy02 * dr12) * invDet;
            float dr_dy = (dx02 * dr12 - dr02 * dx12) * invDet;

            float dg02 = g0 - g2;
            float dg12 = g1 - g2;
            float dg_dx = (dg02 * dy12 - dy02 * dg12) * invDet;
            float dg_dy = (dx02 * dg12 - dg02 * dx12) * invDet;

            float db02 = b0 - b2;
            float db12 = b1 - b2;
            float db_dx = (db02 * dy12 - dy02 * db12) * invDet;
            float db_dy = (dx02 * db12 - db02 * dx12) * invDet;

            constexpr float depthScale = 16384.0f;
            const float r_scale = 31.0f * 1024.0f;
            const float g_scale = 63.0f * 1024.0f;
            const float b_scale = 31.0f * 1024.0f;

            int32_t dz_dx_fixed = static_cast<int32_t>(dz_dx * depthScale);
            int32_t dz_dy_fixed = static_cast<int32_t>(dz_dy * depthScale);

            int32_t dr_dx_fixed = static_cast<int32_t>(dr_dx * r_scale);
            int32_t dr_dy_fixed = static_cast<int32_t>(dr_dy * r_scale);

            int32_t dg_dx_fixed = static_cast<int32_t>(dg_dx * g_scale);
            int32_t dg_dy_fixed = static_cast<int32_t>(dg_dy * g_scale);

            int32_t db_dx_fixed = static_cast<int32_t>(db_dx * b_scale);
            int32_t db_dy_fixed = static_cast<int32_t>(db_dy * b_scale);

            SmoothParams params;
            params.frameBuffer = frameBuffer;
            params.zbBase = zBufferData;
            params.width = width;
            params.height = height;
            params.dz_dx_fixed = dz_dx_fixed;
            params.dz_dy_fixed = dz_dy_fixed;
            params.dr_dx_fixed = dr_dx_fixed;
            params.dr_dy_fixed = dr_dy_fixed;
            params.dg_dx_fixed = dg_dx_fixed;
            params.dg_dy_fixed = dg_dy_fixed;
            params.db_dx_fixed = db_dx_fixed;
            params.db_dy_fixed = db_dy_fixed;

            int startTop = static_cast<int>(ceilf((float)y0 - 0.5f));
            int endTopExclusive = static_cast<int>(ceilf(y1 - 0.5f));
            int startBottom = static_cast<int>(ceilf(y1 - 0.5f));
            int endBottomExclusive = static_cast<int>(ceilf(y2 - 0.5f));

            int clampStartY_top = startTop < 0 ? 0 : startTop;
            int clampStartY_bottom = startBottom < 0 ? 0 : startBottom;

            bool runTop = (clampStartY_top < endTopExclusive) && (clampStartY_top < height);
            bool runBottom = (clampStartY_bottom < endBottomExclusive) && (clampStartY_bottom < height);
            if (!runTop && !runBottom)
                return;

            float z2_scaled = (float)z2 * depthScale;
            float r2_scaled = r2 * r_scale;
            float g2_scaled = g2 * g_scale;
            float b2_scaled = b2 * b_scale;

            if (runTop)
            {
                float dy_init = static_cast<float>(clampStartY_top) + 0.5f - (float)y2;

                float z_base = z2_scaled + (dz_dy * depthScale) * dy_init - (dz_dx * depthScale) * (float)x2 + (dz_dx * depthScale) * 0.5f;
                int32_t z_start_fixed = static_cast<int32_t>(z_base);

                float r_base = r2_scaled + (dr_dy * r_scale) * dy_init - (dr_dx * r_scale) * (float)x2 + (dr_dx * r_scale) * 0.5f;
                int32_t r_start_fixed = static_cast<int32_t>(r_base);

                float g_base = g2_scaled + (dg_dy * g_scale) * dy_init - (dg_dx * g_scale) * (float)x2 + (dg_dx * g_scale) * 0.5f;
                int32_t g_start_fixed = static_cast<int32_t>(g_base);

                float b_base = b2_scaled + (db_dy * b_scale) * dy_init - (db_dx * b_scale) * (float)x2 + (db_dx * b_scale) * 0.5f;
                int32_t b_start_fixed = static_cast<int32_t>(b_base);

                fillSmoothHalf((float)x0, (float)y0, (float)x1, (float)y1,
                               (float)x0, (float)y0, (float)x2, (float)y2,
                               startTop, endTopExclusive,
                               z_start_fixed, r_start_fixed, g_start_fixed, b_start_fixed,
                               clampStartY_top,
                               params);
            }

            if (runBottom)
            {
                float dy_init = static_cast<float>(clampStartY_bottom) + 0.5f - (float)y2;

                float z_base = z2_scaled + (dz_dy * depthScale) * dy_init - (dz_dx * depthScale) * (float)x2 + (dz_dx * depthScale) * 0.5f;
                int32_t z_start_fixed = static_cast<int32_t>(z_base);

                float r_base = r2_scaled + (dr_dy * r_scale) * dy_init - (dr_dx * r_scale) * (float)x2 + (dr_dx * r_scale) * 0.5f;
                int32_t r_start_fixed = static_cast<int32_t>(r_base);

                float g_base = g2_scaled + (dg_dy * g_scale) * dy_init - (dg_dx * g_scale) * (float)x2 + (dg_dx * g_scale) * 0.5f;
                int32_t g_start_fixed = static_cast<int32_t>(g_base);

                float b_base = b2_scaled + (db_dy * b_scale) * dy_init - (db_dx * b_scale) * (float)x2 + (db_dx * b_scale) * 0.5f;
                int32_t b_start_fixed = static_cast<int32_t>(b_base);

                fillSmoothHalf((float)x1, (float)y1, (float)x2, (float)y2,
                               (float)x0, (float)y0, (float)x2, (float)y2,
                               startBottom, endBottomExclusive,
                               z_start_fixed, r_start_fixed, g_start_fixed, b_start_fixed,
                               clampStartY_bottom,
                               params);
            }
        }
    }
}
