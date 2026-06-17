#pragma once

namespace pip3D
{
    namespace Rasterizer
    {
        inline void fillTriangleWater(float x0, float y0, float z0,
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
    }
}