#pragma once

namespace pip3D
{
    namespace Rasterizer
    {
        inline void fillShadowHalf(
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

        inline void fillShadowTriangle(float x0, float y0, float z0,
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
    }
}