#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "Core/Viewport.hpp"
#include "Math/Algebra.hpp"
#include "Rendering/Display/ZBuffer.hpp"
#include "Rendering/Pipeline/Rasterizer/Common.hpp"

namespace pip3D
{
    namespace Rasterizer
    {
        static constexpr int16_t kPlanarClearDepth = 0x7F7F;
        static constexpr int16_t kPlanarShadowFlag = static_cast<int16_t>(0x8000);
        static constexpr uint32_t kClearPack = static_cast<uint32_t>(kPlanarClearDepth) | (static_cast<uint32_t>(kPlanarClearDepth) << 16);
        static constexpr uint32_t kFlagMaskPack = static_cast<uint32_t>(kPlanarShadowFlag) | (static_cast<uint32_t>(kPlanarShadowFlag) << 16);

        struct alignas(16) PlanarBlend
        {
            uint32_t inv_a;
            uint32_t s_rb_a;
            uint32_t s_g_a;
        };

        __attribute__((always_inline)) static inline uint16_t blendScalar(uint16_t dst, const PlanarBlend &bl)
        {
            const uint32_t rb = dst & 0xF81F;
            const uint32_t g = dst & 0x07E0;
            const uint32_t blended_rb = ((rb * bl.inv_a + bl.s_rb_a) >> 6) & 0xF81F;
            const uint32_t blended_g = ((g * bl.inv_a + bl.s_g_a) >> 6) & 0x07E0;
            return static_cast<uint16_t>(blended_rb | blended_g);
        }

        __attribute__((always_inline)) static inline void shadeScalar(
            int16_t &stored, int32_t depth_fixed,
            uint16_t &fbPix, const PlanarBlend &bl)
        {
            const int16_t shadowDepth = static_cast<int16_t>(depth_fixed >> 12);
            const int16_t backTolerance = 10 + (stored >> 11);
            if ((stored - shadowDepth) >= -backTolerance)
            {
                fbPix = blendScalar(fbPix, bl);
                stored |= kPlanarShadowFlag;
            }
        }

        __attribute__((hot)) inline void fillPlanarHalf(
            float xa0, float ya0,
            float xa1, float ya1,
            float xb0, float yb0,
            float xb1, float yb1,
            int startY, int endYExclusive,
            int32_t z_start_fixed_base,
            int clampStartY,
            const PlanarParams &params)
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
            const int32_t dz_dx2 = dz_dx << 1;
            const int32_t dz_dy = params.dz_dy_fixed;
            const uint32_t s_rb = params.s_rb;
            const uint32_t s_g = params.s_g;
            const int16_t width = params.width;
            const int16_t offsetY = params.offsetY;
            const bool softEdges = params.softEdges;
            const bool hasEdges = softEdges && (params.startTopGlobal < params.endBottomGlobal);
            const int edgeTopY = params.startTopGlobal;
            const int edgeBotY = params.endBottomGlobal - 1;

            PlanarBlend blendFull;
            PlanarBlend blendEdge;
            {
                const uint32_t aFull = params.alpha >> 2;
                const uint32_t aEdge = (params.alpha >> 1) >> 2;
                blendFull.inv_a = 64 - aFull;
                blendFull.s_rb_a = s_rb * aFull;
                blendFull.s_g_a = s_g * aFull;
                blendEdge.inv_a = 64 - aEdge;
                blendEdge.s_rb_a = s_rb * aEdge;
                blendEdge.s_g_a = s_g * aEdge;
            }

            int32_t z_row_fixed = z_start_fixed_base;

            for (int y = clampStartY; y < clampEndY; ++y)
            {
                int16_t xStart = static_cast<int16_t>((leftX_fixed + 65535) >> 16);
                int16_t xEnd = static_cast<int16_t>((rightX_fixed + 65535) >> 16) - 1;

                if (xStart < 0)
                    xStart = 0;
                if (xEnd >= width)
                    xEnd = width - 1;

                if (xStart <= xEnd)
                {
                    const PlanarBlend &bl = (hasEdges && (y == edgeTopY || y == edgeBotY)) ? blendEdge : blendFull;

                    const int16_t count = xEnd - xStart + 1;
                    int32_t depth_fixed = z_row_fixed + dz_dx * xStart;

                    int16_t *__restrict__ row = params.zbBase + static_cast<size_t>(y) * width;
                    const int16_t yLocal = static_cast<int16_t>(y - offsetY);
                    uint16_t *__restrict__ fb = params.frameBuffer + static_cast<size_t>(yLocal) * width;

                    int16_t x = xStart;
                    int16_t rem = count;

                    if (x & 1)
                    {
                        int16_t stored = row[x];
                        if (stored >= 0 && stored != kPlanarClearDepth)
                            shadeScalar(stored, depth_fixed, fb[x], bl);
                        depth_fixed += dz_dx;
                        ++x;
                        --rem;
                    }

                    uint32_t *__restrict__ row32 = reinterpret_cast<uint32_t *>(&row[x]);
                    uint32_t *__restrict__ fb32 = reinterpret_cast<uint32_t *>(&fb[x]);
                    int16_t count32 = rem >> 1;

                    while (count32 > 0)
                    {
                        const uint32_t z_pack = row32[0];

                        if ((z_pack & kFlagMaskPack) == kFlagMaskPack || z_pack == kClearPack)
                        {
                            depth_fixed += dz_dx2;
                            ++row32;
                            ++fb32;
                            --count32;
                            continue;
                        }

                        int16_t stored0 = static_cast<int16_t>(z_pack & 0xFFFF);
                        int16_t stored1 = static_cast<int16_t>(z_pack >> 16);

                        const int16_t shadowDepth0 = static_cast<int16_t>(depth_fixed >> 12);
                        const int16_t shadowDepth1 = static_cast<int16_t>((depth_fixed + dz_dx) >> 12);

                        bool write0 = false;
                        bool write1 = false;

                        if (stored0 >= 0 && stored0 != kPlanarClearDepth)
                        {
                            const int16_t backTolerance = 10 + (stored0 >> 11);
                            if ((stored0 - shadowDepth0) >= -backTolerance)
                            {
                                write0 = true;
                                stored0 |= kPlanarShadowFlag;
                            }
                        }

                        if (stored1 >= 0 && stored1 != kPlanarClearDepth)
                        {
                            const int16_t backTolerance = 10 + (stored1 >> 11);
                            if ((stored1 - shadowDepth1) >= -backTolerance)
                            {
                                write1 = true;
                                stored1 |= kPlanarShadowFlag;
                            }
                        }

                        if (write0 || write1)
                        {
                            uint32_t pack = fb32[0];
                            if (write0)
                            {
                                const Color lowColor(static_cast<uint16_t>(pack));
                                const uint16_t blended = blendScalar(lowColor.rgb565, bl);
                                pack = (pack & 0xFFFF0000) | blended;
                            }
                            if (write1)
                            {
                                const uint16_t high = static_cast<uint16_t>(pack >> 16);
                                const uint16_t blended = blendScalar(high, bl);
                                pack = (pack & 0x0000FFFF) | (static_cast<uint32_t>(blended) << 16);
                            }
                            fb32[0] = pack;
                            row32[0] = static_cast<uint32_t>(stored0) | (static_cast<uint32_t>(stored1) << 16);
                        }

                        depth_fixed += dz_dx2;
                        ++row32;
                        ++fb32;
                        --count32;
                    }

                    x = static_cast<int16_t>(reinterpret_cast<int16_t *>(row32) - row);
                    if (x <= xEnd)
                    {
                        int16_t stored = row[x];
                        if (stored >= 0 && stored != kPlanarClearDepth)
                            shadeScalar(stored, depth_fixed, fb[x], bl);
                    }
                }

                leftX_fixed += dx_dy_A_fixed;
                rightX_fixed += dx_dy_B_fixed;
                z_row_fixed += dz_dy;
            }
        }

        __attribute__((hot)) inline void fillPlanarShadowTriangle(float x0, float y0, float z0,
                                                                  float x1, float y1, float z1,
                                                                  float x2, float y2, float z2,
                                                                  uint16_t shadowColor,
                                                                  uint8_t alpha,
                                                                  uint16_t *frameBuffer,
                                                                  ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer,
                                                                  const DisplayConfig &config,
                                                                  bool softEdges = true,
                                                                  int16_t offsetY = 0)
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
            }
            if (y1 > y2)
            {
                std::swap(x1, x2);
                std::swap(y1, y2);
                std::swap(z1, z2);
            }
            if (y0 > y1)
            {
                std::swap(x0, x1);
                std::swap(y0, y1);
                std::swap(z0, z1);
            }

            if (y0 == y2)
                return;
            if (unlikely(fabsf(x0 - x1) < 1e-6f && fabsf(x1 - x2) < 1e-6f))
                return;

            const uint32_t s_rb = shadowColor & 0xF81F;
            const uint32_t s_g = shadowColor & 0x07E0;

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

            constexpr float FP_SCALE = 4096.0f;
            const int32_t dz_dx_fixed = static_cast<int32_t>(dz_dx_scaled * FP_SCALE);
            const int32_t dz_dy_fixed = static_cast<int32_t>(dz_dy_scaled * FP_SCALE);

            const int startTop = static_cast<int>(ceilf(y0 - 0.5f));
            const int endTopExclusive = static_cast<int>(ceilf(y1 - 0.5f));
            const int startBottom = static_cast<int>(ceilf(y1 - 0.5f));
            const int endBottomExclusive = static_cast<int>(ceilf(y2 - 0.5f));

            const int clampStartY_top = startTop < 0 ? 0 : startTop;
            const int clampStartY_bottom = startBottom < 0 ? 0 : startBottom;

            const bool runTop = (clampStartY_top < endTopExclusive) && (clampStartY_top < height);
            const bool runBottom = (clampStartY_bottom < endBottomExclusive) && (clampStartY_bottom < height);
            if (!runTop && !runBottom)
                return;

            PlanarParams params;
            params.frameBuffer = frameBuffer;
            params.zbBase = const_cast<int16_t *>(zBuffer->getBufferPtr());
            params.width = width;
            params.height = height;
            params.dz_dx_fixed = dz_dx_fixed;
            params.dz_dy_fixed = dz_dy_fixed;
            params.s_rb = s_rb;
            params.s_g = s_g;
            params.alpha = alpha;
            params.softEdges = softEdges;
            params.startTopGlobal = startTop;
            params.endBottomGlobal = endBottomExclusive;
            params.offsetY = offsetY;

            if (runTop)
            {
                const float z_base_top_scaled = z2_scaled + dz_dy_scaled * (static_cast<float>(clampStartY_top) + 0.5f - y2) - dz_dx_scaled * x2 + dz_dx_scaled * 0.5f;
                const int32_t z_start_fixed_base_top = static_cast<int32_t>(z_base_top_scaled * FP_SCALE);

                fillPlanarHalf(x0, y0, x1, y1,
                               x0, y0, x2, y2,
                               startTop, endTopExclusive,
                               z_start_fixed_base_top,
                               clampStartY_top,
                               params);
            }

            if (runBottom)
            {
                const float z_base_bottom_scaled = z2_scaled + dz_dy_scaled * (static_cast<float>(clampStartY_bottom) + 0.5f - y2) - dz_dx_scaled * x2 + dz_dx_scaled * 0.5f;
                const int32_t z_start_fixed_base_bottom = static_cast<int32_t>(z_base_bottom_scaled * FP_SCALE);

                fillPlanarHalf(x1, y1, x2, y2,
                               x0, y0, x2, y2,
                               startBottom, endBottomExclusive,
                               z_start_fixed_base_bottom,
                               clampStartY_bottom,
                               params);
            }
        }
    }
}
