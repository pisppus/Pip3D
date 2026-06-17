#pragma once

namespace pip3D
{
    namespace Rasterizer
    {
        inline void fillTriangle(float x0, float y0, float z0,
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

        inline void fillTriangleTransparent(float x0, float y0, float z0,
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
    }
}