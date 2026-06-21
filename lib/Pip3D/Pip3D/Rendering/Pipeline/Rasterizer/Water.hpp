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

            const int startTop = static_cast<int>(ceilf(y0 - 0.5f));
            int endTopExclusive = static_cast<int>(ceilf(y1 - 0.5f));
            const int startBottom = static_cast<int>(ceilf(y1 - 0.5f));
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

            const int16_t invFlagsMask = static_cast<int16_t>(~Z_SHADOW_FLAG);

            const int16_t wl = static_cast<int16_t>(waterYGlobal);
            const int32_t wl2 = static_cast<int32_t>(wl) * 2;

            const float resScale = static_cast<float>(SCREEN_HEIGHT) / 240.0f;
            const float waveTimeA = time * 1.7f;
            const float waveTimeB = time * 1.15f;
            const float ampSwell = 1.6f * resScale;
            const float ampShimmerX = 1.7f * resScale;
            const float ampShimmerY = 1.3f * resScale;

            const uint16_t waterTint565 = Color::rgb(5, 55, 115).rgb565;
            const uint32_t tint_rb = waterTint565 & 0xF81Fu;
            const uint32_t tint_g = waterTint565 & 0x07E0u;

            const uint32_t bgTintAlpha = 55u >> 3;
            const uint32_t bgTintInvA = 32u - bgTintAlpha;

            const uint32_t reflAlpha = 125u >> 3;

            // Reflections are sampled from a half-resolution full-frame
            // snapshot (reflectionBuffer, reflectionWidth x reflectionHeight).
            // The downscale keeps it small (~38 KB) yet band-order
            // independent — every reflected pixel has a valid source.
            // When reflectionBuffer is null (e.g. OOM), water degrades to
            // a tint/sky-only surface.
            const bool hasReflection = (reflectionBuffer != nullptr);
            const int16_t reflW = static_cast<int16_t>(reflectionWidth);
            const int16_t reflH = static_cast<int16_t>(reflectionHeight);

            auto drawSpan = [&](int y, int16_t x_start, int16_t x_end, int32_t depthStart, int32_t depthStep)
            {
                const int16_t globalY = bandTop + y;
                int32_t depth = depthStart;

                int16_t perspY = SCREEN_HEIGHT - globalY;
                if (perspY < 0)
                    perspY = 0;

                const float pf = static_cast<float>(perspY);
                const float swellA = FastMath::fastSin(pf * 0.045f + waveTimeA);
                const float swellB = FastMath::fastSin(pf * 0.019f - waveTimeB * 0.7f);
                const int16_t rowRipple = static_cast<int16_t>((swellA + swellB * 0.6f) * ampSwell);

                const float shimmerPhaseX = waveTimeA * 0.9f;
                const float shimmerPhaseY = waveTimeB * 0.6f;

                // Mirror axis in global screen coordinates. The reflection
                // of a row at globalY lies at mirrorY = 2*waterline - globalY.
                int16_t mirrorY = static_cast<int16_t>(wl2) - globalY + rowRipple;

                bool waterSkyFallback = false;
                if (mirrorY < 0)
                {
                    waterSkyFallback = true;
                    mirrorY = 0;
                }
                if (mirrorY >= SCREEN_HEIGHT)
                    mirrorY = SCREEN_HEIGHT - 1;

                uint32_t refRowAlpha = reflAlpha;
                if (mirrorY < 32)
                    refRowAlpha = (reflAlpha * static_cast<uint32_t>(mirrorY)) >> 5;
                const uint32_t refRowInvA = 32u - refRowAlpha;

                const uint16_t skyColor = waterSkyFallback ? skybox.getColorAtY(0, SCREEN_HEIGHT).rgb565
                                                           : skybox.getColorAtY(mirrorY, SCREEN_HEIGHT).rgb565;

                const size_t offsetBase = static_cast<size_t>(y) * width;
                int16_t *__restrict__ zbRow = zBufferPtr + offsetBase;
                uint16_t *__restrict__ fbRow = frameBuffer + offsetBase;

                // Don't sample the water surface itself (rows at/above the
                // waterline) to avoid self-reflection feedback.
                const int16_t feedbackGuard = wl;

                for (int16_t x = x_start; x <= x_end; ++x)
                {
                    const int16_t stored = zbRow[x];
                    const int16_t depthNoShadow = stored & invFlagsMask;
                    const int16_t d = static_cast<int16_t>(depth);

                    if (d < depthNoShadow)
                    {
                        const float xf = static_cast<float>(x);
                        const float sx = FastMath::fastSin(xf * 0.34f + shimmerPhaseX);
                        const float sy = FastMath::fastCos(xf * 0.21f - shimmerPhaseY);

                        int16_t reflX = x + static_cast<int16_t>(sx * ampShimmerX);
                        int16_t reflY = mirrorY + static_cast<int16_t>(sy * ampShimmerY);

                        uint16_t reflColor;
                        if (waterSkyFallback || !hasReflection || reflY >= feedbackGuard)
                        {
                            reflColor = skyColor;
                        }
                        else
                        {
                            // Sample the half-resolution reflection buffer:
                            // global full-res mirror coords -> half-res index.
                            int16_t hx = reflX >> 1;
                            int16_t hy = reflY >> 1;
                            if (hx < 0)
                                hx = 0;
                            else if (hx >= reflW)
                                hx = reflW - 1;
                            if (hy < 0)
                                hy = 0;
                            else if (hy >= reflH)
                                hy = reflH - 1;
                            reflColor = reflectionBuffer[static_cast<size_t>(hy) * reflW + hx];
                        }

                        const uint16_t bgPixel = fbRow[x];
                        const uint32_t bg_rb = bgPixel & 0xF81Fu;
                        const uint32_t bg_g = bgPixel & 0x07E0u;

                        const uint32_t tinted_rb = ((bg_rb * bgTintInvA + tint_rb * bgTintAlpha) >> 5) & 0xF81Fu;
                        const uint32_t tinted_g = ((bg_g * bgTintInvA + tint_g * bgTintAlpha) >> 5) & 0x07E0u;

                        const uint32_t refl_rb = reflColor & 0xF81Fu;
                        const uint32_t refl_g = reflColor & 0x07E0u;

                        const uint32_t final_rb = ((tinted_rb * refRowInvA + refl_rb * refRowAlpha) >> 5) & 0xF81Fu;
                        const uint32_t final_g = ((tinted_g * refRowInvA + refl_g * refRowAlpha) >> 5) & 0x07E0u;
                        fbRow[x] = static_cast<uint16_t>(final_rb | final_g);

                        zbRow[x] = d | Z_SHADOW_FLAG;
                    }
                    depth += depthStep;
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
                        const float z_row_base = z2_scaled + (static_cast<float>(y) + 0.5f - y2) * dz_dy_scaled - x2 * dz_dx_scaled + dz_dx_scaled * 0.5f;
                        const int32_t depthStart = static_cast<int32_t>(z_row_base + static_cast<float>(x_start) * dz_dx_scaled);
                        const int32_t depthStep = static_cast<int32_t>(dz_dx_scaled);

                        drawSpan(y, x_start, x_end, depthStart, depthStep);
                    }

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
                        const float z_row_base = z2_scaled + (static_cast<float>(y) + 0.5f - y2) * dz_dy_scaled - x2 * dz_dx_scaled + dz_dx_scaled * 0.5f;
                        const int32_t depthStart = static_cast<int32_t>(z_row_base + static_cast<float>(x_start) * dz_dx_scaled);
                        const int32_t depthStep = static_cast<int32_t>(dz_dx_scaled);

                        drawSpan(y, x_start, x_end, depthStart, depthStep);
                    }

                    x02_bottom_fixed += step_02;
                    x12_fixed += step_12;
                }
            }
        }
    }
}
