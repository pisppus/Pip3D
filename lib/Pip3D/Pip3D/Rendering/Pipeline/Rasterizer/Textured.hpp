#pragma once

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

            float texW = static_cast<float>(tex.widthMask + 1);
            float texH = static_cast<float>(tex.heightMask + 1);

            float tu0 = u0 * texW;
            float tu1 = u1 * texW;
            float tu2 = u2 * texW;
            float tv0 = v0 * texH;
            float tv1 = v1 * texH;
            float tv2 = v2 * texH;

            float q0 = 1.0f / w0;
            float q1 = 1.0f / w1;
            float q2 = 1.0f / w2;

            float u_over_z0 = tu0 * q0;
            float u_over_z1 = tu1 * q1;
            float u_over_z2 = tu2 * q2;
            float v_over_z0 = tv0 * q0;
            float v_over_z1 = tv1 * q1;
            float v_over_z2 = tv2 * q2;

            float dq02 = q0 - q2;
            float dq12 = q1 - q2;
            float dq_dx = (dq02 * dy12 - dy02 * dq12) * invDet;
            float dq_dy = (dx02 * dq12 - dq02 * dx12) * invDet;

            float du_over_z02 = u_over_z0 - u_over_z2;
            float du_over_z12 = u_over_z1 - u_over_z2;
            float du_over_z_dx = (du_over_z02 * dy12 - dy02 * du_over_z12) * invDet;
            float du_over_z_dy = (dx02 * du_over_z12 - du_over_z02 * dx12) * invDet;

            float dv_over_z02 = v_over_z0 - v_over_z2;
            float dv_over_z12 = v_over_z1 - v_over_z2;
            float dv_over_z_dx = (dv_over_z02 * dy12 - dy02 * dv_over_z12) * invDet;
            float dv_over_z_dy = (dx02 * dv_over_z12 - dv_over_z02 * dx12) * invDet;

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

            int16_t *__restrict__ zbBase = const_cast<int16_t *>(zBuffer->getBufferPtr());

            auto drawSpanSubdivided = [&](int y, int16_t x_start, int16_t x_end)
            {
                int16_t count = x_end - x_start + 1;
                if (count <= 0)
                    return;

                float dy_factor = (static_cast<float>(y) + 0.5f - y2);
                float dx_factor = (static_cast<float>(x_start) + 0.5f - x2);

                float q = q2 + dy_factor * dq_dy + dx_factor * dq_dx;
                float u_over_z = u_over_z2 + dy_factor * du_over_z_dy + dx_factor * du_over_z_dx;
                float v_over_z = v_over_z2 + dy_factor * dv_over_z_dy + dx_factor * dv_over_z_dx;

                float w_start = FastMath::fastReciprocal(q);
                float u = u_over_z * w_start;
                float v = v_over_z * w_start;

                int16_t cur_x = x_start;

                const uint16_t *const __restrict__ texData = tex.data;
                const uint32_t texShiftU = tex.widthShift;
                const uint32_t texMaskU = tex.widthMask;
                const uint32_t texMaskV = tex.heightMask;

                while (count > 0)
                {
                    int16_t step = (count > 16) ? 16 : count;

                    float next_q = q + step * dq_dx;
                    float next_u_over_z = u_over_z + step * du_over_z_dx;
                    float next_v_over_z = v_over_z + step * dv_over_z_dx;

                    float next_w = FastMath::fastReciprocal(next_q);
                    float next_u = next_u_over_z * next_w;
                    float next_v = next_v_over_z * next_w;

                    float inv_step = likely(step == 16) ? 0.0625f : FastMath::fastReciprocal(static_cast<float>(step));
                    float du = (next_u - u) * inv_step;
                    float dv = (next_v - v) * inv_step;

                    int32_t u_fixed = static_cast<int32_t>(u * 65536.0f);
                    int32_t v_fixed = static_cast<int32_t>(v * 65536.0f);
                    int32_t du_fixed = static_cast<int32_t>(du * 65536.0f);
                    int32_t dv_fixed = static_cast<int32_t>(dv * 65536.0f);

                    float z_scaled_start = z2_scaled + dy_factor * dz_dy_scaled + (static_cast<float>(cur_x) + 0.5f - x2) * dz_dx_scaled;
                    int32_t z_val = static_cast<int32_t>(z_scaled_start * 16384.0f);
                    int32_t z_step = static_cast<int32_t>(dz_dx_scaled * 16384.0f);

                    size_t index = static_cast<size_t>(y) * width + cur_x;
                    int16_t *__restrict__ zb = zbBase + index;
                    uint16_t *__restrict__ fb = frameBuffer + index;

                    PIP3D_PREFETCH(zb);
                    PIP3D_PREFETCH(fb);

                    for (int16_t i = 0; i < step; ++i)
                    {
                        int16_t d = static_cast<int16_t>(z_val >> 14);
                        int16_t curr = *zb & 0x7FFF;
                        if (d < curr)
                        {
                            *zb = d;
                            uint32_t tu = (static_cast<uint32_t>(u_fixed) >> 16) & texMaskU;
                            uint32_t tv = (static_cast<uint32_t>(v_fixed) >> 16) & texMaskV;
                            *fb = texData[(tv << texShiftU) | tu];
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
                        drawSpanSubdivided(y, x_start, x_end);

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
                        drawSpanSubdivided(y, x_start, x_end);

                    x02_bottom_fixed += step_02;
                    x12_fixed += step_12;
                }
            }
        }
    }
}