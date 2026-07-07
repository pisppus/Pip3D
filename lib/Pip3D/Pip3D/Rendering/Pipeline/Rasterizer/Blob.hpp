#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "Math/Algebra.hpp"
#include "Rendering/Display/ZBuffer.hpp"
#include "Rendering/Pipeline/Rasterizer/Common.hpp"

namespace pip3D
{
    namespace Rasterizer
    {
        static constexpr int32_t kBlobDistSqMax = 1 << 24;
        static constexpr int32_t kBlobDepthTolerance = 250;

        struct alignas(16) BlobParams
        {
            uint16_t *frameBuffer;
            int16_t *zbBase;
            int32_t dz_dx_fixed;
            int32_t dz_dy_fixed;
            int32_t du_dx_fixed;
            int32_t du_dy_fixed;
            int32_t dv_dx_fixed;
            int32_t dv_dy_fixed;
            int16_t width;
            int16_t height;
            uint32_t s_rb;
            uint32_t s_g;
            uint8_t baseAlpha;
        };

        __attribute__((hot)) inline void fillBlobHalf(
            float xa0, float ya0,
            float xa1, float ya1,
            float xb0, float yb0,
            float xb1, float yb1,
            int startY, int endYExclusive,
            int32_t z_start_fixed_base,
            int32_t u_start_fixed_base,
            int32_t v_start_fixed_base,
            int clampStartY,
            const BlobParams &params)
        {
            const float dya = ya1 - ya0;
            const float dyb = yb1 - yb0;
            if (unlikely(fabsf(dya) < 1e-6f || fabsf(dyb) < 1e-6f))
                return;

            const float invDya = FastMath::fastReciprocal(dya);
            const float invDyb = FastMath::fastReciprocal(dyb);

            const float dx_dy_A = (xa1 - xa0) * invDya;
            const float dx_dy_B = (xb1 - xb0) * invDyb;

            const int clampEndY = endYExclusive > params.height ? params.height : endYExclusive;
            if (unlikely(clampStartY >= clampEndY))
                return;

            const float initY = static_cast<float>(clampStartY) + 0.5f;
            const float leftX = xa0 + dx_dy_A * (initY - ya0);
            const float rightX = xb0 + dx_dy_B * (initY - yb0);

            int32_t leftX_fixed = static_cast<int32_t>(leftX * 65536.0f);
            int32_t rightX_fixed = static_cast<int32_t>(rightX * 65536.0f);
            int32_t dx_dy_A_fixed = static_cast<int32_t>(dx_dy_A * 65536.0f);
            int32_t dx_dy_B_fixed = static_cast<int32_t>(dx_dy_B * 65536.0f);

            if (leftX_fixed > rightX_fixed)
            {
                std::swap(leftX_fixed, rightX_fixed);
                std::swap(dx_dy_A_fixed, dx_dy_B_fixed);
            }

            const int32_t dz_dx = params.dz_dx_fixed;
            const int32_t du_dx = params.du_dx_fixed;
            const int32_t dv_dx = params.dv_dx_fixed;
            const int32_t dz_dy = params.dz_dy_fixed;
            const int32_t du_dy = params.du_dy_fixed;
            const int32_t dv_dy = params.dv_dy_fixed;
            const uint32_t s_rb = params.s_rb;
            const uint32_t s_g = params.s_g;
            const uint32_t baseAlpha = params.baseAlpha;
            const int16_t width = params.width;

            const int32_t du2_plus_dv2 = du_dx * du_dx + dv_dx * dv_dx;
            const int32_t dd = du2_plus_dv2 << 1;

            int32_t z_row_fixed = z_start_fixed_base;
            int32_t u_row_fixed = u_start_fixed_base;
            int32_t v_row_fixed = v_start_fixed_base;

            for (int y = clampStartY; y < clampEndY; ++y)
            {
                int16_t xStart = static_cast<int16_t>((leftX_fixed + 32767) >> 16);
                int16_t xEnd = static_cast<int16_t>((rightX_fixed - 32769) >> 16);

                if (xStart < 0)
                    xStart = 0;
                if (xEnd >= width)
                    xEnd = width - 1;

                if (xStart <= xEnd)
                {
                    const int32_t u0 = u_row_fixed + du_dx * xStart;
                    const int32_t v0 = v_row_fixed + dv_dx * xStart;

                    int32_t distSq = u0 * u0 + v0 * v0;
                    int32_t step = (du_dx * u0 + dv_dx * v0) * 2 + du2_plus_dv2;

                    int32_t depth_fixed = z_row_fixed + dz_dx * xStart;

                    int16_t *__restrict__ zPtr = params.zbBase + static_cast<size_t>(y) * width + xStart;
                    uint16_t *__restrict__ fbPtr = params.frameBuffer + static_cast<size_t>(y) * width + xStart;

                    int16_t count = xEnd - xStart + 1;
                    bool wasInside = false;

                    while (count > 0)
                    {
                        if (distSq < kBlobDistSqMax)
                        {
                            wasInside = true;

                            uint32_t factor = (kBlobDistSqMax - distSq) >> 16;
                            if (factor > 255)
                                factor = 255;

                            const uint32_t alpha = (baseAlpha * factor) >> 8;
                            if (alpha > 0)
                            {
                                const int16_t stored = zPtr[0];
                                const int16_t depthNoShadow = static_cast<int16_t>(stored & 0x7FFF);
                                const int16_t d = static_cast<int16_t>(depth_fixed >> 14);

                                if (d <= depthNoShadow + kBlobDepthTolerance)
                                {
                                    const uint16_t dst = fbPtr[0];
                                    const uint32_t a = alpha >> 3;
                                    const uint32_t inv_a = 32 - a;

                                    const uint32_t rb = dst & 0xF81F;
                                    const uint32_t g = dst & 0x07E0;
                                    const uint32_t blended_rb = ((rb * inv_a + s_rb * a) >> 5) & 0xF81F;
                                    const uint32_t blended_g = ((g * inv_a + s_g * a) >> 5) & 0x07E0;

                                    fbPtr[0] = static_cast<uint16_t>(blended_rb | blended_g);
                                }
                            }
                        }
                        else if (wasInside && step >= 0)
                        {
                            break;
                        }

                        distSq += step;
                        step += dd;
                        depth_fixed += dz_dx;
                        ++zPtr;
                        ++fbPtr;
                        --count;
                    }
                }

                leftX_fixed += dx_dy_A_fixed;
                rightX_fixed += dx_dy_B_fixed;
                z_row_fixed += dz_dy;
                u_row_fixed += du_dy;
                v_row_fixed += dv_dy;
            }
        }

        __attribute__((hot)) inline void fillTriangleBlob(
            int16_t x0, int16_t y0, float z0,
            int16_t x1, int16_t y1, float z1,
            int16_t x2, int16_t y2, float z2,
            float u0, float v0,
            float u1, float v1,
            float u2, float v2,
            uint16_t shadowColor,
            uint8_t alpha,
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
            }
            if (y1 > y2)
            {
                std::swap(x1, x2);
                std::swap(y1, y2);
                std::swap(z1, z2);
                std::swap(u1, u2);
                std::swap(v1, v2);
            }
            if (y0 > y1)
            {
                std::swap(x0, x1);
                std::swap(y0, y1);
                std::swap(z0, z1);
                std::swap(u0, u1);
                std::swap(v0, v1);
            }

            if (y0 == y2)
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

            const float du02 = u0 - u2;
            const float du12 = u1 - u2;
            const float du_dx = (du02 * dy12 - dy02 * du12) * invDet;
            const float du_dy = (dx02 * du12 - du02 * dx12) * invDet;

            const float dv02 = v0 - v2;
            const float dv12 = v1 - v2;
            const float dv_dx = (dv02 * dy12 - dy02 * dv12) * invDet;
            const float dv_dy = (dx02 * dv12 - dv02 * dx12) * invDet;

            constexpr float depthScale = 32638.0f * 16384.0f;
            const float dz_dx_scaled = dz_dx * depthScale;
            const float dz_dy_scaled = dz_dy * depthScale;
            const float z2_scaled = z2 * depthScale;

            constexpr float uvScale = 4096.0f;
            const float du_dx_scaled = du_dx * uvScale;
            const float du_dy_scaled = du_dy * uvScale;
            const float u2_scaled = u2 * uvScale;

            const float dv_dx_scaled = dv_dx * uvScale;
            const float dv_dy_scaled = dv_dy * uvScale;
            const float v2_scaled = v2 * uvScale;

            BlobParams params;
            params.frameBuffer = frameBuffer;
            params.zbBase = const_cast<int16_t *>(zBuffer->getBufferPtr());
            params.width = width;
            params.height = height;
            params.dz_dx_fixed = static_cast<int32_t>(dz_dx_scaled);
            params.dz_dy_fixed = static_cast<int32_t>(dz_dy_scaled);
            params.du_dx_fixed = static_cast<int32_t>(du_dx_scaled);
            params.du_dy_fixed = static_cast<int32_t>(du_dy_scaled);
            params.dv_dx_fixed = static_cast<int32_t>(dv_dx_scaled);
            params.dv_dy_fixed = static_cast<int32_t>(dv_dy_scaled);

            const Color sCol(shadowColor);
            params.s_rb = sCol.rb();
            params.s_g = sCol.g();
            params.baseAlpha = alpha;

            int startTop = static_cast<int>(ceilf(y0 - 0.5f));
            int endTopExclusive = static_cast<int>(ceilf(y1 - 0.5f));
            int startBottom = static_cast<int>(ceilf(y1 - 0.5f));
            int endBottomExclusive = static_cast<int>(ceilf(y2 - 0.5f));

            int clampStartY_top = startTop < 0 ? 0 : startTop;
            int clampStartY_bottom = startBottom < 0 ? 0 : startBottom;

            if (endTopExclusive > height)
                endTopExclusive = height;
            if (endBottomExclusive > height)
                endBottomExclusive = height;

            const bool runTop = (clampStartY_top < endTopExclusive) && (clampStartY_top < height);
            const bool runBottom = (clampStartY_bottom < endBottomExclusive) && (clampStartY_bottom < height);

            if (runTop)
            {
                const float dy_init = (static_cast<float>(clampStartY_top) + 0.5f) - y2;
                const float z_base = z2_scaled + dz_dy_scaled * dy_init - dz_dx_scaled * x2 + dz_dx_scaled * 0.5f;
                const float u_base = u2_scaled + du_dy_scaled * dy_init - du_dx_scaled * x2 + du_dx_scaled * 0.5f;
                const float v_base = v2_scaled + dv_dy_scaled * dy_init - dv_dx_scaled * x2 + dv_dx_scaled * 0.5f;

                fillBlobHalf(x0, y0, x1, y1, x0, y0, x2, y2, startTop, endTopExclusive,
                             static_cast<int32_t>(z_base), static_cast<int32_t>(u_base), static_cast<int32_t>(v_base),
                             clampStartY_top, params);
            }

            if (runBottom)
            {
                const float dy_init = (static_cast<float>(clampStartY_bottom) + 0.5f) - y2;
                const float z_base = z2_scaled + dz_dy_scaled * dy_init - dz_dx_scaled * x2 + dz_dx_scaled * 0.5f;
                const float u_base = u2_scaled + du_dy_scaled * dy_init - du_dx_scaled * x2 + du_dx_scaled * 0.5f;
                const float v_base = v2_scaled + dv_dy_scaled * dy_init - dv_dx_scaled * x2 + dv_dx_scaled * 0.5f;

                fillBlobHalf(x1, y1, x2, y2, x0, y0, x2, y2, startBottom, endBottomExclusive,
                             static_cast<int32_t>(z_base), static_cast<int32_t>(u_base), static_cast<int32_t>(v_base),
                             clampStartY_bottom, params);
            }
        }
    }
}