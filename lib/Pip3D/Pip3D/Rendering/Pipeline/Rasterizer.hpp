#pragma once

#include <algorithm>

#include "Core/Viewport.hpp"
#include "Rendering/Display/ZBuffer.hpp"
#include "Shading.hpp"

namespace pip3D
{
    struct Sky;

    class Rasterizer
    {
    public:
        struct ShadowParams
        {
            uint16_t *frameBuffer;
            int16_t *zbBase;
            int16_t width;
            int16_t height;
            int32_t dz_dx_fixed;
            int32_t dz_dy_fixed;
            uint32_t s_rb;
            uint32_t s_g;
            uint8_t alpha;
            bool softEdges;
            int startTopGlobal;
            int endBottomGlobal;
            int16_t offsetY;
        };

    private:
        __attribute__((always_inline)) static inline void fillShadowHalf(
            float xa0, float ya0,
            float xa1, float ya1,
            float xb0, float yb0,
            float xb1, float yb1,
            int startY, int endYExclusive,
            int32_t z_start_fixed_base,
            int clampStartY,
            const ShadowParams &params)
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
                    int16_t x = xStart;
                    int16_t count = xEnd - xStart + 1;
                    int32_t depth_fixed = z_row_fixed + params.dz_dx_fixed * x;

                    int16_t *__restrict__ row = params.zbBase + static_cast<size_t>(y) * params.width;
                    int16_t yLocal = static_cast<int16_t>(y - params.offsetY);
                    uint16_t *__restrict__ fb = params.frameBuffer + static_cast<size_t>(yLocal) * params.width;

                    uint8_t edgeAlpha = params.alpha;
                    if (params.softEdges)
                    {
                        if (unlikely(y == params.startTopGlobal || y == params.endBottomGlobal - 1))
                        {
                            edgeAlpha = params.alpha >> 1;
                        }
                    }

                    const uint32_t a = edgeAlpha >> 3;
                    const uint32_t inv_a = 32 - a;
                    const uint32_t s_rb_a = params.s_rb * a;
                    const uint32_t s_g_a = params.s_g * a;

                    if (x & 1)
                    {
                        const int16_t stored = row[x];
                        if (stored >= 0 && stored != 0x7F7F)
                        {
                            const int16_t shadowDepth = static_cast<int16_t>(depth_fixed >> 12);
                            const int16_t backTolerance = 10 + (stored >> 11);
                            if ((stored - shadowDepth) >= -backTolerance)
                            {
                                const uint32_t bgColor = fb[x];
                                const uint32_t rb = (bgColor & 0xF81F);
                                const uint32_t g = (bgColor & 0x07E0);
                                const uint32_t blended_rb = ((rb * inv_a + s_rb_a) >> 5) & 0xF81F;
                                const uint32_t blended_g = ((g * inv_a + s_g_a) >> 5) & 0x07E0;

                                fb[x] = static_cast<uint16_t>(blended_rb | blended_g);
                                row[x] = static_cast<int16_t>(stored | 0x8000);
                            }
                        }
                        depth_fixed += params.dz_dx_fixed;
                        x++;
                        count--;
                    }

                    uint32_t *__restrict__ row32 = reinterpret_cast<uint32_t *>(&row[x]);
                    uint32_t *__restrict__ fb32 = reinterpret_cast<uint32_t *>(&fb[x]);
                    int16_t count32 = count >> 1;

                    PIP3D_PREFETCH_W(row32);
                    PIP3D_PREFETCH_W(fb32);

                    while (count32 > 0)
                    {
                        PIP3D_PREFETCH_W(row32 + 8);
                        PIP3D_PREFETCH_W(fb32 + 8);

                        uint32_t z_pack = row32[0];

                        if ((z_pack & 0x80008000) == 0x80008000 || z_pack == 0x7F7F7F7F)
                        {
                            depth_fixed += params.dz_dx_fixed << 1;
                            row32++;
                            fb32++;
                            count32--;
                            continue;
                        }

                        int16_t stored0 = static_cast<int16_t>(z_pack & 0xFFFF);
                        int16_t stored1 = static_cast<int16_t>(z_pack >> 16);

                        int16_t shadowDepth0 = static_cast<int16_t>(depth_fixed >> 12);
                        int16_t shadowDepth1 = static_cast<int16_t>((depth_fixed + params.dz_dx_fixed) >> 12);

                        bool write0 = false;
                        bool write1 = false;

                        if (stored0 >= 0 && stored0 != 0x7F7F)
                        {
                            int16_t backTolerance = 10 + (stored0 >> 11);
                            if ((stored0 - shadowDepth0) >= -backTolerance)
                            {
                                write0 = true;
                                stored0 |= 0x8000;
                            }
                        }

                        if (stored1 >= 0 && stored1 != 0x7F7F)
                        {
                            int16_t backTolerance = 10 + (stored1 >> 11);
                            if ((stored1 - shadowDepth1) >= -backTolerance)
                            {
                                write1 = true;
                                stored1 |= 0x8000;
                            }
                        }

                        if (write0 || write1)
                        {
                            uint32_t bg_pack = fb32[0];
                            uint32_t m_pack = bg_pack;

                            if (write0)
                            {
                                uint32_t bg0 = bg_pack & 0xFFFF;
                                uint32_t rb = bg0 & 0xF81F;
                                uint32_t g = bg0 & 0x07E0;
                                uint32_t blended_rb = ((rb * inv_a + s_rb_a) >> 5) & 0xF81F;
                                uint32_t blended_g = ((g * inv_a + s_g_a) >> 5) & 0x07E0;
                                m_pack = (m_pack & 0xFFFF0000) | (blended_rb | blended_g);
                            }

                            if (write1)
                            {
                                uint32_t bg1 = bg_pack >> 16;
                                uint32_t rb = bg1 & 0xF81F;
                                uint32_t g = bg1 & 0x07E0;
                                uint32_t blended_rb = ((rb * inv_a + s_rb_a) >> 5) & 0xF81F;
                                uint32_t blended_g = ((g * inv_a + s_g_a) >> 5) & 0x07E0;
                                m_pack = (m_pack & 0x0000FFFF) | ((blended_rb | blended_g) << 16);
                            }

                            fb32[0] = m_pack;
                            row32[0] = static_cast<uint32_t>(stored0) | (static_cast<uint32_t>(stored1) << 16);
                        }

                        depth_fixed += params.dz_dx_fixed << 1;
                        row32++;
                        fb32++;
                        count32--;
                    }

                    x = static_cast<int16_t>(reinterpret_cast<int16_t *>(row32) - row);
                    count = xEnd - x + 1;
                    if (count > 0)
                    {
                        const int16_t stored = row[x];
                        if (stored >= 0 && stored != 0x7F7F)
                        {
                            const int16_t shadowDepth = static_cast<int16_t>(depth_fixed >> 12);
                            const int16_t backTolerance = 10 + (stored >> 11);
                            if ((stored - shadowDepth) >= -backTolerance)
                            {
                                const uint32_t bgColor = fb[x];
                                const uint32_t rb = (bgColor & 0xF81F);
                                const uint32_t g = (bgColor & 0x07E0);
                                const uint32_t blended_rb = ((rb * inv_a + s_rb_a) >> 5) & 0xF81F;
                                const uint32_t blended_g = ((g * inv_a + s_g_a) >> 5) & 0x07E0;

                                fb[x] = static_cast<uint16_t>(blended_rb | blended_g);
                                row[x] = static_cast<int16_t>(stored | 0x8000);
                            }
                        }
                    }
                }

                leftX_fixed += dx_dy_A_fixed;
                rightX_fixed += dx_dy_B_fixed;
                z_row_fixed += params.dz_dy_fixed;
            }
        }

    public:
        static void fillShadowTriangle(float x0, float y0, float z0,
                                       float x1, float y1, float z1,
                                       float x2, float y2, float z2,
                                       uint16_t shadowColor,
                                       uint8_t alpha,
                                       uint16_t *frameBuffer,
                                       ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer,
                                       const DisplayConfig &config,
                                       bool softEdges = true,
                                       int16_t offsetY = 0,
                                       int16_t bandHeight = -1)
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

            float dx02 = x0 - x2;
            float dy12 = y1 - y2;
            float dy02 = y0 - y2;
            float dx12 = x1 - x2;

            float det = dx02 * dy12 - dy02 * dx12;
            if (unlikely(fabsf(det) < 1e-6f))
                return;

            const float depthScale = 32638.0f;
            float invDet = FastMath::fastReciprocal(det);

            float dz02 = z0 - z2;
            float dz12 = z1 - z2;

            float dz_dx = (dz02 * dy12 - dy02 * dz12) * invDet;
            float dz_dy = (dx02 * dz12 - dz02 * dx12) * invDet;

            float dz_dx_scaled = dz_dx * depthScale;
            float dz_dy_scaled = dz_dy * depthScale;
            float z2_scaled = z2 * depthScale;

            const float FP_SCALE = 4096.0f;
            int32_t dz_dx_fixed = static_cast<int32_t>(dz_dx_scaled * FP_SCALE);
            int32_t dz_dy_fixed = static_cast<int32_t>(dz_dy_scaled * FP_SCALE);

            int startTop = static_cast<int>(ceilf(y0 - 0.5f));
            int endTopExclusive = static_cast<int>(ceilf(y1 - 0.5f));
            int startBottom = static_cast<int>(ceilf(y1 - 0.5f));
            int endBottomExclusive = static_cast<int>(ceilf(y2 - 0.5f));

            int clampStartY_top = startTop < 0 ? 0 : startTop;
            int clampStartY_bottom = startBottom < 0 ? 0 : startBottom;

            bool runTop = (clampStartY_top < endTopExclusive) && (clampStartY_top < height);
            bool runBottom = (clampStartY_bottom < endBottomExclusive) && (clampStartY_bottom < height);
            if (!runTop && !runBottom)
                return;

            int16_t *__restrict__ zbBase = const_cast<int16_t *>(zBuffer->getBufferPtr());

            ShadowParams params;
            params.frameBuffer = frameBuffer;
            params.zbBase = zbBase;
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
                float z_base_top_scaled = z2_scaled + dz_dy_scaled * (static_cast<float>(clampStartY_top) + 0.5f - y2) - dz_dx_scaled * x2 + dz_dx_scaled * 0.5f;
                int32_t z_start_fixed_base_top = static_cast<int32_t>(z_base_top_scaled * FP_SCALE);

                fillShadowHalf(x0, y0, x1, y1,
                               x0, y0, x2, y2,
                               startTop, endTopExclusive,
                               z_start_fixed_base_top,
                               clampStartY_top,
                               params);
            }

            if (runBottom)
            {
                float z_base_bottom_scaled = z2_scaled + dz_dy_scaled * (static_cast<float>(clampStartY_bottom) + 0.5f - y2) - dz_dx_scaled * x2 + dz_dx_scaled * 0.5f;
                int32_t z_start_fixed_base_bottom = static_cast<int32_t>(z_base_bottom_scaled * FP_SCALE);

                fillShadowHalf(x1, y1, x2, y2,
                               x0, y0, x2, y2,
                               startBottom, endBottomExclusive,
                               z_start_fixed_base_bottom,
                               clampStartY_bottom,
                               params);
            }
        }

    public:
        struct SmoothParams
        {
            uint16_t *frameBuffer;
            int16_t *zbBase;
            int16_t width;
            int16_t height;
            int32_t dz_dx_fixed;
            int32_t dz_dy_fixed;
            int32_t dr_dx_fixed;
            int32_t dr_dy_fixed;
            int32_t dg_dx_fixed;
            int32_t dg_dy_fixed;
            int32_t db_dx_fixed;
            int32_t db_dy_fixed;
            int16_t shadowMask;
            int16_t invShadowMask;
        };

    private:
        __attribute__((always_inline, hot)) static inline void fillSmoothHalf(
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

            static const int32_t kBayerMatrix10Bit[4][4] = {
                {0, 512, 128, 640},
                {768, 256, 896, 384},
                {192, 704, 64, 576},
                {960, 448, 832, 320}};

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

                    int16_t *__restrict__ zPtr = params.zbBase + static_cast<size_t>(y) * params.width + xStart;
                    uint16_t *__restrict__ fbPtr = params.frameBuffer + static_cast<size_t>(y) * params.width + xStart;

                    const int32_t *bayerRow = kBayerMatrix10Bit[y & 3];

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
                            const int16_t stored = zPtr[0];
                            const int16_t depthNoShadow = static_cast<int16_t>(stored & params.invShadowMask);
                            const int16_t d = static_cast<int16_t>(depth_fixed >> 14);

                            if (d < depthNoShadow)
                            {
                                zPtr[0] = static_cast<int16_t>((stored & params.shadowMask) | d);

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
                            const int16_t stored = zPtr[1];
                            const int16_t depthNoShadow = static_cast<int16_t>(stored & params.invShadowMask);
                            const int16_t d = static_cast<int16_t>(depth_fixed >> 14);

                            if (d < depthNoShadow)
                            {
                                zPtr[1] = static_cast<int16_t>((stored & params.shadowMask) | d);

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
                            const int16_t stored = zPtr[2];
                            const int16_t depthNoShadow = static_cast<int16_t>(stored & params.invShadowMask);
                            const int16_t d = static_cast<int16_t>(depth_fixed >> 14);

                            if (d < depthNoShadow)
                            {
                                zPtr[2] = static_cast<int16_t>((stored & params.shadowMask) | d);

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
                            const int16_t stored = zPtr[3];
                            const int16_t depthNoShadow = static_cast<int16_t>(stored & params.invShadowMask);
                            const int16_t d = static_cast<int16_t>(depth_fixed >> 14);

                            if (d < depthNoShadow)
                            {
                                zPtr[3] = static_cast<int16_t>((stored & params.shadowMask) | d);

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
                        const int16_t stored = zPtr[0];
                        const int16_t depthNoShadow = static_cast<int16_t>(stored & params.invShadowMask);
                        const int16_t d = static_cast<int16_t>(depth_fixed >> 14);

                        if (d < depthNoShadow)
                        {
                            zPtr[0] = static_cast<int16_t>((stored & params.shadowMask) | d);

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

    public:
        __attribute__((always_inline, hot)) static inline void IRAM_ATTR fillTriangleSmooth(
            int16_t x0, int16_t y0, float z0,
            int16_t x1, int16_t y1, float z1,
            int16_t x2, int16_t y2, float z2,
            float r0, float g0, float b0,
            float r1, float g1, float b1,
            float r2, float g2, float b2,
            uint16_t *frameBuffer,
            ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer,
            const DisplayConfig &config)
        {
            const int16_t width = config.width;
            const int16_t height = config.height;

            if (unlikely(!frameBuffer || !zBuffer))
                return;

            int16_t *const zBufferData = const_cast<int16_t *>(zBuffer->getBufferPtr());
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

            const float depthScale = 32638.0f * 16384.0f;
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

            const int16_t shadowMask = ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT>::shadowFlagMask();
            const int16_t invShadowMask = static_cast<int16_t>(~shadowMask);

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
            params.shadowMask = shadowMask;
            params.invShadowMask = invShadowMask;

            int startTop = static_cast<int>(ceilf((float)y0 - 0.5f));
            int endTopExclusive = static_cast<int>(ceilf((float)y1 - 0.5f));
            int startBottom = static_cast<int>(ceilf((float)y1 - 0.5f));
            int endBottomExclusive = static_cast<int>(ceilf((float)y2 - 0.5f));

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

        static void fillTriangle(float x0, float y0, float z0,
                                 float x1, float y1, float z1,
                                 float x2, float y2, float z2,
                                 uint16_t color,
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
            if (unlikely(x0 == x1 && x1 == x2))
                return;

            float dx02 = x0 - x2;
            float dy12 = y1 - y2;
            float dy02 = y0 - y2;
            float dx12 = x1 - x2;

            float det = dx02 * dy12 - dy02 * dx12;
            if (unlikely(fabsf(det) < 1e-6f))
                return;

            float invDet = FastMath::fastReciprocal(det);

            float dz02 = z0 - z2;
            float dz12 = z1 - z2;
            float dz_dx = (dz02 * dy12 - dy02 * dz12) * invDet;
            float dz_dy = (dx02 * dz12 - dz02 * dx12) * invDet;

            const float depthScale = 32638.0f;
            float dz_dx_scaled = dz_dx * depthScale;
            float dz_dy_scaled = dz_dy * depthScale;
            float z2_scaled = z2 * depthScale;

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

            bool runTop = (clampStartY_top < endTopExclusive) && (clampStartY_top < height);
            bool runBottom = (clampStartY_bottom < endBottomExclusive) && (clampStartY_bottom < height);
            if (!runTop && !runBottom)
                return;

            float dy02_val = y2 - y0;
            float dy01_val = y1 - y0;
            float dy12_val = y2 - y1;

            int32_t step_02 = (fabsf(dy02_val) > 1e-6f) ? static_cast<int32_t>(((x2 - x0) / dy02_val) * 65536.0f) : 0;
            int32_t step_01 = (fabsf(dy01_val) > 1e-6f) ? static_cast<int32_t>(((x1 - x0) / dy01_val) * 65536.0f) : 0;
            int32_t step_12 = (fabsf(dy12_val) > 1e-6f) ? static_cast<int32_t>(((x2 - x1) / dy12_val) * 65536.0f) : 0;

            if (runTop)
            {
                float slope_02 = (x2 - x0) / dy02_val;
                float slope_01 = (x1 - x0) / dy01_val;
                float dy_init = (static_cast<float>(clampStartY_top) + 0.5f) - y0;

                int32_t x02_fixed = static_cast<int32_t>((x0 + slope_02 * dy_init) * 65536.0f);
                int32_t x01_fixed = static_cast<int32_t>((x0 + slope_01 * dy_init) * 65536.0f);

                for (int y = clampStartY_top; y < endTopExclusive; ++y)
                {
                    int16_t xa = static_cast<int16_t>((x02_fixed + 65535) >> 16);
                    int16_t xb = static_cast<int16_t>((x01_fixed + 65535) >> 16);

                    if (xa > xb)
                        std::swap(xa, xb);

                    int16_t x_start = xa;
                    int16_t x_end = xb - 1;

                    if (x_start < 0)
                        x_start = 0;
                    if (x_end >= width)
                        x_end = width - 1;

                    if (x_start <= x_end)
                    {
                        float z_row_base = z2_scaled + (static_cast<float>(y) + 0.5f - y2) * dz_dy_scaled - x2 * dz_dx_scaled + dz_dx_scaled * 0.5f;
                        int32_t depthStart = static_cast<int32_t>(z_row_base + static_cast<float>(x_start) * dz_dx_scaled);
                        int32_t depthStep = static_cast<int32_t>(dz_dx_scaled);

                        zBuffer->testAndSetScanline(static_cast<uint16_t>(y),
                                                    static_cast<uint16_t>(x_start),
                                                    static_cast<uint16_t>(x_end),
                                                    depthStart,
                                                    depthStep,
                                                    frameBuffer,
                                                    color);
                    }

                    x02_fixed += step_02;
                    x01_fixed += step_01;
                }
            }

            if (runBottom)
            {
                float slope_02 = (x2 - x0) / dy02_val;
                float slope_12 = (x2 - x1) / dy12_val;
                float dy_init_bottom = (static_cast<float>(clampStartY_bottom) + 0.5f) - y1;
                float dy_init_long = (static_cast<float>(clampStartY_bottom) + 0.5f) - y0;

                int32_t x12_fixed = static_cast<int32_t>((x1 + slope_12 * dy_init_bottom) * 65536.0f);
                int32_t x02_bottom_fixed = static_cast<int32_t>((x0 + slope_02 * dy_init_long) * 65536.0f);

                for (int y = clampStartY_bottom; y < endBottomExclusive; ++y)
                {
                    int16_t xa = static_cast<int16_t>((x02_bottom_fixed + 65535) >> 16);
                    int16_t xb = static_cast<int16_t>((x12_fixed + 65535) >> 16);

                    if (xa > xb)
                        std::swap(xa, xb);

                    int16_t x_start = xa;
                    int16_t x_end = xb - 1;

                    if (x_start < 0)
                        x_start = 0;
                    if (x_end >= width)
                        x_end = width - 1;

                    if (x_start <= x_end)
                    {
                        float z_row_base = z2_scaled + (static_cast<float>(y) + 0.5f - y2) * dz_dy_scaled - x2 * dz_dx_scaled + dz_dx_scaled * 0.5f;
                        int32_t depthStart = static_cast<int32_t>(z_row_base + static_cast<float>(x_start) * dz_dx_scaled);
                        int32_t depthStep = static_cast<int32_t>(dz_dx_scaled);

                        zBuffer->testAndSetScanline(static_cast<uint16_t>(y),
                                                    static_cast<uint16_t>(x_start),
                                                    static_cast<uint16_t>(x_end),
                                                    depthStart,
                                                    depthStep,
                                                    frameBuffer,
                                                    color);
                    }

                    x02_bottom_fixed += step_02;
                    x12_fixed += step_12;
                }
            }
        }

        static void fillTriangleTransparent(float x0, float y0, float z0,
                                            float x1, float y1, float z1,
                                            float x2, float y2, float z2,
                                            uint16_t color,
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
            if (unlikely(x0 == x1 && x1 == x2))
                return;

            float dx02 = x0 - x2;
            float dy12 = y1 - y2;
            float dy02 = y0 - y2;
            float dx12 = x1 - x2;

            float det = dx02 * dy12 - dy02 * dx12;
            if (unlikely(fabsf(det) < 1e-6f))
                return;

            float invDet = FastMath::fastReciprocal(det);

            float dz02 = z0 - z2;
            float dz12 = z1 - z2;
            float dz_dx = (dz02 * dy12 - dy02 * dz12) * invDet;
            float dz_dy = (dx02 * dz12 - dz02 * dx12) * invDet;

            const float depthScale = 32638.0f;
            float dz_dx_scaled = dz_dx * depthScale;
            float dz_dy_scaled = dz_dy * depthScale;
            float z2_scaled = z2 * depthScale;

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

            bool runTop = (clampStartY_top < endTopExclusive) && (clampStartY_top < height);
            bool runBottom = (clampStartY_bottom < endBottomExclusive) && (clampStartY_bottom < height);
            if (!runTop && !runBottom)
                return;

            float dy02_val = y2 - y0;
            float dy01_val = y1 - y0;
            float dy12_val = y2 - y1;

            int32_t step_02 = (fabsf(dy02_val) > 1e-6f) ? static_cast<int32_t>(((x2 - x0) / dy02_val) * 65536.0f) : 0;
            int32_t step_01 = (fabsf(dy01_val) > 1e-6f) ? static_cast<int32_t>(((x1 - x0) / dy01_val) * 65536.0f) : 0;
            int32_t step_12 = (fabsf(dy12_val) > 1e-6f) ? static_cast<int32_t>(((x2 - x1) / dy12_val) * 65536.0f) : 0;

            const int16_t shadowMask = ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT>::shadowFlagMask();
            const int16_t invShadowMask = static_cast<int16_t>(~shadowMask);

            auto drawSpan = [&](int y, int16_t x_start, int16_t x_end, int32_t depthStart, int32_t depthStep)
            {
                size_t index = static_cast<size_t>(y) * width + x_start;
                const int16_t *__restrict__ zb = zBuffer->getBufferPtr() + index;
                uint16_t *__restrict__ fb = frameBuffer + index;

                int32_t depth = depthStart;
                int16_t count = x_end - x_start + 1;

                while (count > 0)
                {
                    const int16_t stored = *zb;
                    const int16_t depthNoShadow = static_cast<int16_t>(stored & invShadowMask);
                    const int16_t d = static_cast<int16_t>(depth >> 14);

                    if (d < depthNoShadow)
                    {
                        Color bg(*fb);
                        *fb = bg.blend(Color(color), alpha).rgb565;
                    }

                    depth += depthStep;
                    ++zb;
                    ++fb;
                    --count;
                }
            };

            if (runTop)
            {
                float slope_02 = (x2 - x0) / dy02_val;
                float slope_01 = (x1 - x0) / dy01_val;
                float dy_init = (static_cast<float>(clampStartY_top) + 0.5f) - y0;

                int32_t x02_fixed = static_cast<int32_t>((x0 + slope_02 * dy_init) * 65536.0f);
                int32_t x01_fixed = static_cast<int32_t>((x0 + slope_01 * dy_init) * 65536.0f);

                for (int y = clampStartY_top; y < endTopExclusive; ++y)
                {
                    int16_t xa = static_cast<int16_t>((x02_fixed + 65535) >> 16);
                    int16_t xb = static_cast<int16_t>((x01_fixed + 65535) >> 16);

                    if (xa > xb)
                        std::swap(xa, xb);

                    int16_t x_start = xa;
                    int16_t x_end = xb - 1;

                    if (x_start < 0)
                        x_start = 0;
                    if (x_end >= width)
                        x_end = width - 1;

                    if (x_start <= x_end)
                    {
                        float z_row_base = z2_scaled + (static_cast<float>(y) + 0.5f - y2) * dz_dy_scaled - x2 * dz_dx_scaled + dz_dx_scaled * 0.5f;
                        int32_t depthStart = static_cast<int32_t>((z_row_base + static_cast<float>(x_start) * dz_dx_scaled) * 16384.0f);
                        int32_t depthStep = static_cast<int32_t>(dz_dx_scaled * 16384.0f);

                        drawSpan(y, x_start, x_end, depthStart, depthStep);
                    }

                    x02_fixed += step_02;
                    x01_fixed += step_01;
                }
            }

            if (runBottom)
            {
                float slope_02 = (x2 - x0) / dy02_val;
                float slope_12 = (x2 - x1) / dy12_val;
                float dy_init_bottom = (static_cast<float>(clampStartY_bottom) + 0.5f) - y1;
                float dy_init_long = (static_cast<float>(clampStartY_bottom) + 0.5f) - y0;

                int32_t x12_fixed = static_cast<int32_t>((x1 + slope_12 * dy_init_bottom) * 65536.0f);
                int32_t x02_bottom_fixed = static_cast<int32_t>((x0 + slope_02 * dy_init_long) * 65536.0f);

                for (int y = clampStartY_bottom; y < endBottomExclusive; ++y)
                {
                    int16_t xa = static_cast<int16_t>((x02_bottom_fixed + 65535) >> 16);
                    int16_t xb = static_cast<int16_t>((x12_fixed + 65535) >> 16);

                    if (xa > xb)
                        std::swap(xa, xb);

                    int16_t x_start = xa;
                    int16_t x_end = xb - 1;

                    if (x_start < 0)
                        x_start = 0;
                    if (x_end >= width)
                        x_end = width - 1;

                    if (x_start <= x_end)
                    {
                        float z_row_base = z2_scaled + (static_cast<float>(y) + 0.5f - y2) * dz_dy_scaled - x2 * dz_dx_scaled + dz_dx_scaled * 0.5f;
                        int32_t depthStart = static_cast<int32_t>((z_row_base + static_cast<float>(x_start) * dz_dx_scaled) * 16384.0f);
                        int32_t depthStep = static_cast<int32_t>(dz_dx_scaled * 16384.0f);

                        drawSpan(y, x_start, x_end, depthStart, depthStep);
                    }

                    x02_bottom_fixed += step_02;
                    x12_fixed += step_12;
                }
            }
        }
        static void fillTriangleWater(float x0, float y0, float z0,
                                      float x1, float y1, float z1,
                                      float x2, float y2, float z2,
                                      float time,
                                      float waterYGlobal,
                                      const Sky &skybox,
                                      uint16_t *frameBuffer,
                                      ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer,
                                      const DisplayConfig &config,
                                      int16_t bandTop,
                                      int16_t bandBottom,
                                      const uint16_t *reflectionBuffer,
                                      uint16_t reflectionWidth,
                                      uint16_t reflectionHeight)
        {
            const int16_t width = config.width;
            const int16_t height = config.height;

            if (unlikely(!frameBuffer || !zBuffer))
                return;

            int16_t *__restrict__ zBufferPtr = const_cast<int16_t *>(zBuffer->getBufferPtr());
            if (unlikely(!zBufferPtr))
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
            if (unlikely(x0 == x1 && x1 == x2))
                return;

            float dx02 = x0 - x2;
            float dy12 = y1 - y2;
            float dy02 = y0 - y2;
            float dx12 = x1 - x2;

            float det = dx02 * dy12 - dy02 * dx12;
            if (unlikely(fabsf(det) < 1e-6f))
                return;

            float invDet = FastMath::fastReciprocal(det);

            float dz02 = z0 - z2;
            float dz12 = z1 - z2;
            float dz_dx = (dz02 * dy12 - dy02 * dz12) * invDet;
            float dz_dy = (dx02 * dz12 - dz02 * dx12) * invDet;

            const float depthScale = 32638.0f;
            float dz_dx_scaled = dz_dx * depthScale;
            float dz_dy_scaled = dz_dy * depthScale;
            float z2_scaled = z2 * depthScale;

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

            bool runTop = (clampStartY_top < endTopExclusive) && (clampStartY_top < height);
            bool runBottom = (clampStartY_bottom < endBottomExclusive) && (clampStartY_bottom < height);
            if (!runTop && !runBottom)
                return;

            float dy02_val = y2 - y0;
            float dy01_val = y1 - y0;
            float dy12_val = y2 - y1;

            int32_t step_02 = (fabsf(dy02_val) > 1e-6f) ? static_cast<int32_t>(((x2 - x0) / dy02_val) * 65536.0f) : 0;
            int32_t step_01 = (fabsf(dy01_val) > 1e-6f) ? static_cast<int32_t>(((x1 - x0) / dy01_val) * 65536.0f) : 0;
            int32_t step_12 = (fabsf(dy12_val) > 1e-6f) ? static_cast<int32_t>(((x2 - x1) / dy12_val) * 65536.0f) : 0;

            const int16_t invFlagsMask = static_cast<int16_t>(~Z_SHADOW_FLAG);

            auto drawSpan = [&](int y, int16_t x_start, int16_t x_end, int32_t depthStart, int32_t depthStep)
            {
                int16_t globalY = bandTop + y;
                int32_t depth = depthStart;

                int16_t perspY = SCREEN_HEIGHT - globalY;
                if (perspY < 0)
                    perspY = 0;

                int32_t angle = (perspY * perspY * 18 / SCREEN_HEIGHT + static_cast<int32_t>(time * 120.0f)) % 360;
                if (angle < 0)
                    angle += 360;

                float sinVal = FastMath::fastSin(static_cast<float>(angle) * kDegToRad);
                float amp = 6.0f * (static_cast<float>(SCREEN_HEIGHT) / 240.0f);
                int16_t ripple = static_cast<int16_t>(sinVal * amp);

                int16_t wl = static_cast<int16_t>(waterYGlobal);
                int16_t mirrorY = 2 * wl - globalY + ripple;

                bool waterSkyFallback = false;
                if (mirrorY < 0)
                {
                    waterSkyFallback = true;
                    mirrorY = 0;
                }
                if (mirrorY >= SCREEN_HEIGHT)
                {
                    mirrorY = SCREEN_HEIGHT - 1;
                }

                size_t reflectionRowOffset = 0;
                if (reflectionBuffer)
                {
                    int16_t ry = mirrorY >> 1;
                    if (ry < 0)
                        ry = 0;
                    if (ry >= reflectionHeight)
                        ry = reflectionHeight - 1;
                    reflectionRowOffset = static_cast<size_t>(ry) * reflectionWidth;
                }

                uint16_t skyColor = skybox.getColorAtY(mirrorY, SCREEN_HEIGHT).rgb565;

                size_t offsetBase = static_cast<size_t>(y) * width;
                int16_t *__restrict__ zbRow = zBufferPtr + offsetBase;
                uint16_t *__restrict__ fbRow = frameBuffer + offsetBase;

                for (int16_t x = x_start; x <= x_end; ++x)
                {
                    int16_t stored = zbRow[x];
                    int16_t depthNoShadow = stored & invFlagsMask;
                    int16_t d = static_cast<int16_t>(depth);

                    if (d < depthNoShadow)
                    {
                        uint16_t reflColor;

                        if (unlikely(waterSkyFallback))
                        {
                            reflColor = skyColor;
                        }
                        else if (reflectionBuffer)
                        {
                            int16_t rx = x >> 1;
                            if (rx < 0)
                                rx = 0;
                            if (rx >= reflectionWidth)
                                rx = reflectionWidth - 1;
                            reflColor = reflectionBuffer[reflectionRowOffset + rx];
                        }
                        else
                        {
                            reflColor = skyColor;
                        }

                        uint16_t bgPixel = fbRow[x];
                        Color bgCol(bgPixel);

                        uint16_t waterTint = Color::rgb(5, 55, 115).rgb565;
                        Color tintedBg = bgCol.blend(Color(waterTint), 55);

                        Color finalRefl(reflColor);
                        fbRow[x] = finalRefl.blend(tintedBg, 125).rgb565;

                        zbRow[x] = d | Z_SHADOW_FLAG;
                    }
                    depth += depthStep;
                }
            };

            if (runTop)
            {
                float slope_02 = (x2 - x0) / dy02_val;
                float slope_01 = (x1 - x0) / dy01_val;
                float dy_init = (static_cast<float>(clampStartY_top) + 0.5f) - y0;

                int32_t x02_fixed = static_cast<int32_t>((x0 + slope_02 * dy_init) * 65536.0f);
                int32_t x01_fixed = static_cast<int32_t>((x0 + slope_01 * dy_init) * 65536.0f);

                for (int y = clampStartY_top; y < endTopExclusive; ++y)
                {
                    int16_t xa = static_cast<int16_t>((x02_fixed + 65535) >> 16);
                    int16_t xb = static_cast<int16_t>((x01_fixed + 65535) >> 16);

                    if (xa > xb)
                        std::swap(xa, xb);

                    int16_t x_start = xa;
                    int16_t x_end = xb - 1;

                    if (x_start < 0)
                        x_start = 0;
                    if (x_end >= width)
                        x_end = width - 1;

                    if (x_start <= x_end)
                    {
                        float z_row_base = z2_scaled + (static_cast<float>(y) + 0.5f - y2) * dz_dy_scaled - x2 * dz_dx_scaled + dz_dx_scaled * 0.5f;
                        int32_t depthStart = static_cast<int32_t>(z_row_base + static_cast<float>(x_start) * dz_dx_scaled);
                        int32_t depthStep = static_cast<int32_t>(dz_dx_scaled);

                        drawSpan(y, x_start, x_end, depthStart, depthStep);
                    }

                    x02_fixed += step_02;
                    x01_fixed += step_01;
                }
            }

            if (runBottom)
            {
                float slope_02 = (x2 - x0) / dy02_val;
                float slope_12 = (x2 - x1) / dy12_val;
                float dy_init_bottom = (static_cast<float>(clampStartY_bottom) + 0.5f) - y1;
                float dy_init_long = (static_cast<float>(clampStartY_bottom) + 0.5f) - y0;

                int32_t x12_fixed = static_cast<int32_t>((x1 + slope_12 * dy_init_bottom) * 65536.0f);
                int32_t x02_bottom_fixed = static_cast<int32_t>((x0 + slope_02 * dy_init_long) * 65536.0f);

                for (int y = clampStartY_bottom; y < endBottomExclusive; ++y)
                {
                    int16_t xa = static_cast<int16_t>((x02_bottom_fixed + 65535) >> 16);
                    int16_t xb = static_cast<int16_t>((x12_fixed + 65535) >> 16);

                    if (xa > xb)
                        std::swap(xa, xb);

                    int16_t x_start = xa;
                    int16_t x_end = xb - 1;

                    if (x_start < 0)
                        x_start = 0;
                    if (x_end >= width)
                        x_end = width - 1;

                    if (x_start <= x_end)
                    {
                        float z_row_base = z2_scaled + (static_cast<float>(y) + 0.5f - y2) * dz_dy_scaled - x2 * dz_dx_scaled + dz_dx_scaled * 0.5f;
                        int32_t depthStart = static_cast<int32_t>(z_row_base + static_cast<float>(x_start) * dz_dx_scaled);
                        int32_t depthStep = static_cast<int32_t>(dz_dx_scaled);

                        drawSpan(y, x_start, x_end, depthStart, depthStep);
                    }

                    x02_bottom_fixed += step_02;
                    x12_fixed += step_12;
                }
            }
        }
    };
}