#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "Core/Platform.hpp"
#include "Math/Algebra.hpp"
#include "Rendering/Buffers/ZBuffer.hpp"
#include "Rendering/Lighting/Baked.hpp"
#include "Rendering/Lighting/Fog.hpp"
#include "Rendering/Resources/Texture.hpp"

namespace pip3D
{
    namespace Rasterizer
    {
        namespace detail
        {
            static constexpr uint8_t MAX_MIP_TABLE = 16;

            struct MipTable
            {
                uint32_t offsets[MAX_MIP_TABLE + 1];
                uint8_t shiftU[MAX_MIP_TABLE];
                uint8_t shiftV[MAX_MIP_TABLE];
                uint32_t key = 0;
                bool valid = false;
            };

            inline MipTable g_mipTableCache{};

            inline const MipTable &getMipTables(const Texture &tex) noexcept
            {
                const uint32_t key = static_cast<uint32_t>(tex.shiftU) |
                                     (static_cast<uint32_t>(tex.vShift()) << 8) |
                                     (static_cast<uint32_t>(tex.mipCount) << 16);
                MipTable &mt = g_mipTableCache;
                if (mt.valid && mt.key == key)
                    return mt;

                const uint8_t baseShiftU = tex.shiftU;
                const uint8_t baseShiftV = tex.vShift();
                const uint8_t maxLevels = (tex.mipCount < MAX_MIP_TABLE) ? tex.mipCount : MAX_MIP_TABLE;
                uint32_t offset = 0;
                mt.offsets[0] = 0;
                for (uint8_t k = 0; k < maxLevels; ++k)
                {
                    const uint8_t su = (baseShiftU > (k + 1)) ? static_cast<uint8_t>(baseShiftU - (k + 1)) : 0;
                    const uint8_t sv = (baseShiftV > (k + 1)) ? static_cast<uint8_t>(baseShiftV - (k + 1)) : 0;
                    offset += (1u << su) * (1u << sv);
                    mt.offsets[k + 1] = offset;
                    mt.shiftU[k] = su;
                    mt.shiftV[k] = sv;
                }
                mt.key = key;
                mt.valid = true;
                return mt;
            }

            template <typename SpanFn>
            PIP3D_FORCE_INLINE static void rasterizeTriangleRows(
                bool runTop, int topStart, int topEnd,
                bool runBottom, int bottomStart, int bottomEnd,
                float x0, float y0, float x1, float y1, float x2, float y2,
                int16_t width, SpanFn &&drawSpan) noexcept
            {
                const float dy02_val = y2 - y0;
                const float dy01_val = y1 - y0;
                const float dy12_val = y2 - y1;
                const int32_t step_02 = (fabsf(dy02_val) > 1e-6f) ? static_cast<int32_t>(((x2 - x0) / dy02_val) * 65536.0f) : 0;
                const int32_t step_01 = (fabsf(dy01_val) > 1e-6f) ? static_cast<int32_t>(((x1 - x0) / dy01_val) * 65536.0f) : 0;
                const int32_t step_12 = (fabsf(dy12_val) > 1e-6f) ? static_cast<int32_t>(((x2 - x1) / dy12_val) * 65536.0f) : 0;

                if (runTop)
                {
                    const float slope_02 = (x2 - x0) / dy02_val;
                    const float slope_01 = (x1 - x0) / dy01_val;
                    const float dy_init = (static_cast<float>(topStart) + 0.5f) - y0;
                    int32_t x02_fixed = static_cast<int32_t>((x0 + slope_02 * dy_init) * 65536.0f);
                    int32_t x01_fixed = static_cast<int32_t>((x0 + slope_01 * dy_init) * 65536.0f);
                    for (int y = topStart; y < topEnd; ++y)
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
                            drawSpan(y, x_start, x_end);
                        x02_fixed += step_02;
                        x01_fixed += step_01;
                    }
                }

                if (runBottom)
                {
                    const float slope_02 = (x2 - x0) / dy02_val;
                    const float slope_12 = (x2 - x1) / dy12_val;
                    const float dy_init_bottom = (static_cast<float>(bottomStart) + 0.5f) - y1;
                    const float dy_init_long = (static_cast<float>(bottomStart) + 0.5f) - y0;
                    int32_t x12_fixed = static_cast<int32_t>((x1 + slope_12 * dy_init_bottom) * 65536.0f);
                    int32_t x02_bottom_fixed = static_cast<int32_t>((x0 + slope_02 * dy_init_long) * 65536.0f);
                    for (int y = bottomStart; y < bottomEnd; ++y)
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
                            drawSpan(y, x_start, x_end);
                        x02_bottom_fixed += step_02;
                        x12_fixed += step_12;
                    }
                }
            }
        }

        PIP3D_HOT inline bool IRAM_ATTR fillTriangleTextured(float x0, float y0, float z0,
                                                             float x1, float y1, float z1,
                                                             float x2, float y2, float z2,
                                                             float u0, float v0,
                                                             float u1, float v1,
                                                             float u2, float v2,
                                                             float w0, float w1, float w2,
                                                             float lr0, float lg0, float lb0,
                                                             float lr1, float lg1, float lb1,
                                                             float lr2, float lg2, float lb2,
                                                             const Texture &tex,
                                                             uint16_t *frameBuffer,
                                                             ZBuffer *zBuffer,
                                                             const DisplayConfig &config)
        {
            const int16_t width = config.width;
            const int16_t height = config.height;

            if (unlikely(!frameBuffer || !zBuffer))
                return true;

            if (y0 > y1)
            {
                std::swap(x0, x1);
                std::swap(y0, y1);
                std::swap(z0, z1);
                std::swap(u0, u1);
                std::swap(v0, v1);
                std::swap(w0, w1);
                std::swap(lr0, lr1);
                std::swap(lg0, lg1);
                std::swap(lb0, lb1);
            }
            if (y1 > y2)
            {
                std::swap(x1, x2);
                std::swap(y1, y2);
                std::swap(z1, z2);
                std::swap(u1, u2);
                std::swap(v1, v2);
                std::swap(w1, w2);
                std::swap(lr1, lr2);
                std::swap(lg1, lg2);
                std::swap(lb1, lb2);
            }
            if (y0 > y1)
            {
                std::swap(x0, x1);
                std::swap(y0, y1);
                std::swap(z0, z1);
                std::swap(u0, u1);
                std::swap(v0, v1);
                std::swap(w0, w1);
                std::swap(lr0, lr1);
                std::swap(lg0, lg1);
                std::swap(lb0, lb1);
            }

            if (y0 == y2)
                return false;
            if (unlikely(x0 == x1 && x1 == x2))
                return false;

            const float dx02 = x0 - x2;
            const float dy12 = y1 - y2;
            const float dy02 = y0 - y2;
            const float dx12 = x1 - x2;

            const float det = dx02 * dy12 - dy02 * dx12;
            if (unlikely(fabsf(det) < 1e-6f))
                return false;

            const float invDet = FastMath::fastReciprocal(det);

            const float dz02 = z0 - z2;
            const float dz12 = z1 - z2;
            const float dz_dx = (dz02 * dy12 - dy02 * dz12) * invDet;
            const float dz_dy = (dx02 * dz12 - dz02 * dx12) * invDet;

            const float texW = tex.widthFlt();
            const float texH = tex.heightFlt();

            const float tu0 = u0 * texW;
            const float tu1 = u1 * texW;
            const float tu2 = u2 * texW;
            const float tv0 = v0 * texH;
            const float tv1 = v1 * texH;
            const float tv2 = v2 * texH;

            const float q0 = FastMath::fastReciprocal(w0);
            const float q1 = FastMath::fastReciprocal(w1);
            const float q2 = FastMath::fastReciprocal(w2);
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

            const int startTop = fastCeilNonNeg(y0 - 0.5f);
            const int startBottom = fastCeilNonNeg(y1 - 0.5f);
            int endTopExclusive = startBottom;
            int endBottomExclusive = fastCeilNonNeg(y2 - 0.5f);

            int clampStartY_top = startTop < 0 ? 0 : startTop;
            int clampStartY_bottom = startBottom < 0 ? 0 : startBottom;

            if (endTopExclusive > height)
                endTopExclusive = height;
            if (endBottomExclusive > height)
                endBottomExclusive = height;

            const bool runTop = (clampStartY_top < endTopExclusive) && (clampStartY_top < height);
            const bool runBottom = (clampStartY_bottom < endBottomExclusive) && (clampStartY_bottom < height);
            if (!runTop && !runBottom)
                return true;

            uint16_t *__restrict__ zbBase = zBuffer->data();

            const bool fogEnabled = g_fogState.enabled;
            const uint16_t fogColor = g_fogState.color;
            const uint32_t fogColorRb = g_fogState.color_rb;
            const uint32_t fogColorG = g_fogState.color_g;

            const uint16_t *const __restrict__ texData = tex.data;
            const uint32_t texShiftU = tex.shiftU;
            const uint32_t texMaskU = tex.maskU();
            const uint32_t texMaskV = tex.maskV();
            const int32_t z_step = static_cast<int32_t>(dz_dx * 16384.0f);

            const uint16_t *const __restrict__ mipData = tex.mipData;
            const uint8_t maxMipLevel = tex.mipCount;
            const bool hasMipmaps = (maxMipLevel > 0 && mipData != nullptr && g_mipmapsEnabled);
            const detail::MipTable &mip = detail::getMipTables(tex);
            const uint32_t *const mipOffsets = mip.offsets;
            const uint8_t *const mipShiftUArr = mip.shiftU;
            const uint8_t *const mipShiftVArr = mip.shiftV;

            const bool flatLight =
                (fabsf(lr0 - lr1) < 1e-3f && fabsf(lr1 - lr2) < 1e-3f &&
                 fabsf(lg0 - lg1) < 1e-3f && fabsf(lg1 - lg2) < 1e-3f &&
                 fabsf(lb0 - lb1) < 1e-3f && fabsf(lb1 - lb2) < 1e-3f);

            const uint32_t flatRw = flatLight ? lmScale5(static_cast<uint32_t>(clamp(lr0, 0.0f, 1.0f) * 31.0f + 0.5f)) : 0;
            uint32_t flatGw = flatLight ? lmScale6(static_cast<uint32_t>(clamp(lg0, 0.0f, 1.0f) * 63.0f + 0.5f)) : 0;
            const uint32_t flatBw = flatLight ? lmScale5(static_cast<uint32_t>(clamp(lb0, 0.0f, 1.0f) * 31.0f + 0.5f)) : 0;

            float dlr_dx = 0.0f, dlg_dx = 0.0f, dlb_dx = 0.0f;
            int32_t lr_cur = 0, lg_cur = 0, lb_cur = 0;
            int32_t lr_step = 0, lg_step = 0, lb_step = 0;
            if (!flatLight)
            {
                const float dl_r02 = lr0 - lr2;
                const float dl_r12 = lr1 - lr2;
                const float dl_g02 = lg0 - lg2;
                const float dl_g12 = lg1 - lg2;
                const float dl_b02 = lb0 - lb2;
                const float dl_b12 = lb1 - lb2;

                dlr_dx = (dl_r02 * dy12 - dy02 * dl_r12) * invDet;
                dlg_dx = (dl_g02 * dy12 - dy02 * dl_g12) * invDet;
                dlb_dx = (dl_b02 * dy12 - dy02 * dl_b12) * invDet;

                lr_step = static_cast<int32_t>(dlr_dx * 65536.0f);
                lg_step = static_cast<int32_t>(dlg_dx * 65536.0f);
                lb_step = static_cast<int32_t>(dlb_dx * 65536.0f);
            }

            auto drawSpanSubdivided = [&](int y, int16_t x_start, int16_t x_end)
            {
                int16_t count = x_end - x_start + 1;
                if (count <= 0)
                    return;

                const float dy_factor = (static_cast<float>(y) + 0.5f - y2);
                const float z_dy_term = z2 + dy_factor * dz_dy;

                float q = q2 + dy_factor * dq_dy + ((static_cast<float>(x_start) + 0.5f) - x2) * dq_dx;
                float u_over_z = u_over_z2 + dy_factor * du_over_z_dy + ((static_cast<float>(x_start) + 0.5f) - x2) * du_over_z_dx;
                float v_over_z = v_over_z2 + dy_factor * dv_over_z_dy + ((static_cast<float>(x_start) + 0.5f) - x2) * dv_over_z_dx;

                float w_start = FastMath::fastReciprocal(q);
                float u = u_over_z * w_start;
                float v = v_over_z * w_start;

                int16_t cur_x = x_start;

                size_t index = static_cast<size_t>(y) * width + cur_x;
                uint16_t *__restrict__ zb = zbBase + index;
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

                    bool chunkDither = false;
                    uint32_t chunkFracLod = 0;
                    const uint16_t *chunkTexData = texData;
                    uint32_t chunkTexShift = texShiftU;
                    uint32_t chunkTexMaskU = texMaskU;
                    uint32_t chunkTexMaskV = texMaskV;
                    int32_t chunkUVShift = 0;
                    const uint16_t *chunkTexDataHi = texData;
                    uint32_t chunkTexShiftHi = texShiftU;
                    uint32_t chunkTexMaskUHi = texMaskU;
                    uint32_t chunkTexMaskVHi = texMaskV;
                    int32_t chunkUVShiftHi = 0;

                    if (hasMipmaps)
                    {
                        const float inv_q = FastMath::fastReciprocal(q);
                        const bool qValid = (q > 1e-6f) && (q < 1e6f) && (inv_q > 1e-6f) && (inv_q < 1e6f);
                        if (qValid)
                        {
                            const float du_dx_scr = (du_over_z_dx - u * dq_dx) * inv_q;
                            const float dv_dx_scr = (dv_over_z_dx - v * dq_dx) * inv_q;
                            const float du_dy_scr = (du_over_z_dy - u * dq_dy) * inv_q;
                            const float dv_dy_scr = (dv_over_z_dy - v * dq_dy) * inv_q;
                            const float du_dx_a = du_dx_scr < 0.0f ? -du_dx_scr : du_dx_scr;
                            const float dv_dx_a = dv_dx_scr < 0.0f ? -dv_dx_scr : dv_dx_scr;
                            const float du_dy_a = du_dy_scr < 0.0f ? -du_dy_scr : du_dy_scr;
                            const float dv_dy_a = dv_dy_scr < 0.0f ? -dv_dy_scr : dv_dy_scr;
                            float dmax = du_dx_a;
                            if (dv_dx_a > dmax)
                                dmax = dv_dx_a;
                            if (du_dy_a > dmax)
                                dmax = du_dy_a;
                            if (dv_dy_a > dmax)
                                dmax = dv_dy_a;

                            if (dmax > 1.0f && dmax < 1e8f)
                            {
                                uint32_t bits;
                                std::memcpy(&bits, &dmax, sizeof(bits));
                                float lodLevel = static_cast<float>(static_cast<int32_t>((bits >> 23) & 0xFF) - 127);
                                lodLevel += static_cast<float>((bits >> 15) & 0xFF) * (1.0f / 256.0f);

                                lodLevel += g_mipmapBias;

                                if (lodLevel < 0.0f)
                                    lodLevel = 0.0f;
                                if (lodLevel > static_cast<float>(maxMipLevel))
                                    lodLevel = static_cast<float>(maxMipLevel);

                                const int32_t intLod = static_cast<int32_t>(lodLevel);
                                const int32_t intLodClamped = (intLod > static_cast<int32_t>(detail::MAX_MIP_TABLE))
                                                                  ? static_cast<int32_t>(detail::MAX_MIP_TABLE)
                                                                  : intLod;
                                chunkFracLod = static_cast<uint32_t>((lodLevel - static_cast<float>(intLod)) * 256.0f);

                                if (intLodClamped == 0)
                                {
                                    chunkTexData = texData;
                                    chunkTexShift = texShiftU;
                                    chunkTexMaskU = texMaskU;
                                    chunkTexMaskV = texMaskV;
                                    chunkUVShift = 0;
                                }
                                else
                                {
                                    const uint8_t su = mipShiftUArr[intLodClamped - 1];
                                    const uint8_t sv = mipShiftVArr[intLodClamped - 1];
                                    chunkTexData = mipData + mipOffsets[intLodClamped - 1];
                                    chunkTexShift = su;
                                    chunkTexMaskU = (1U << su) - 1U;
                                    chunkTexMaskV = (1U << sv) - 1U;
                                    chunkUVShift = intLodClamped;
                                }

                                const int32_t hiLod = intLodClamped + 1;
                                if (hiLod <= static_cast<int32_t>(maxMipLevel) && hiLod <= static_cast<int32_t>(detail::MAX_MIP_TABLE) && chunkFracLod > 0)
                                {
                                    const uint8_t suHi = mipShiftUArr[hiLod - 1];
                                    const uint8_t svHi = mipShiftVArr[hiLod - 1];
                                    chunkTexDataHi = mipData + mipOffsets[hiLod - 1];
                                    chunkTexShiftHi = suHi;
                                    chunkTexMaskUHi = (1U << suHi) - 1U;
                                    chunkTexMaskVHi = (1U << svHi) - 1U;
                                    chunkUVShiftHi = hiLod;
                                    chunkDither = true;
                                }
                            }
                        }
                    }

                    const float z_scaled_start = z_dy_term + (static_cast<float>(cur_x) + 0.5f - x2) * dz_dx;
                    int32_t z_val = static_cast<int32_t>(z_scaled_start * 16384.0f);

                    if (!flatLight)
                    {
                        const float px = (static_cast<float>(cur_x) + 0.5f) - x2;
                        lr_cur = static_cast<int32_t>((lr2 + dlr_dx * px) * 65536.0f);
                        lg_cur = static_cast<int32_t>((lg2 + dlg_dx * px) * 65536.0f);
                        lb_cur = static_cast<int32_t>((lb2 + dlb_dx * px) * 65536.0f);
                    }

                    PIP3D_PREFETCH_W(zb + 16);
                    PIP3D_PREFETCH_R(fb + 16);
                    PIP3D_PREFETCH_R(chunkTexData);

                    const int16_t baseY = static_cast<int16_t>(y);

                    auto fetchTexel = [&](int16_t px_x) -> uint16_t
                    {
                        const uint16_t *td;
                        uint32_t tsh, tmu, tmv;
                        int32_t ush;
                        if (likely(!chunkDither))
                        {
                            td = chunkTexData;
                            tsh = chunkTexShift;
                            tmu = chunkTexMaskU;
                            tmv = chunkTexMaskV;
                            ush = chunkUVShift;
                        }
                        else
                        {
                            const uint8_t bayer = static_cast<uint8_t>(::pip3D::detail::kBayerMatrix10Bit[baseY & 3][px_x & 3] >> 2);
                            if (chunkFracLod > bayer)
                            {
                                td = chunkTexDataHi;
                                tsh = chunkTexShiftHi;
                                tmu = chunkTexMaskUHi;
                                tmv = chunkTexMaskVHi;
                                ush = chunkUVShiftHi;
                            }
                            else
                            {
                                td = chunkTexData;
                                tsh = chunkTexShift;
                                tmu = chunkTexMaskU;
                                tmv = chunkTexMaskV;
                                ush = chunkUVShift;
                            }
                        }
                        const uint32_t tu = (static_cast<uint32_t>(u_fixed >> (16 + ush))) & tmu;
                        const uint32_t tv = (static_cast<uint32_t>(v_fixed >> (16 + ush))) & tmv;
                        return td[(tv << tsh) | tu];
                    };

                    {
                        const int32_t lrs = lr_step, lgs = lg_step, lbs = lb_step;

                        for (int16_t i = 0; i < step; ++i)
                        {
                            const uint16_t d = static_cast<uint16_t>(z_val >> 14);
                            const uint16_t curr = *zb & Z_DEPTH_MASK;
                            if (d > curr)
                            {
                                *zb = d;
                                const uint16_t texColor = fetchTexel(cur_x + i);
                                uint16_t litColor;
                                if (flatLight)
                                {
                                    const Color tc(texColor);
                                    litColor = static_cast<uint16_t>(((tc.r5() * flatRw + 16) >> 5) << 11 |
                                                                     ((tc.g6() * flatGw + 32) >> 6) << 5 |
                                                                     ((tc.b5() * flatBw + 16) >> 5));
                                }
                                else
                                {
                                    const int32_t r5 = lr_cur >> 16;
                                    const int32_t g6 = lg_cur >> 16;
                                    const int32_t b5 = lb_cur >> 16;
                                    const uint32_t r5c = lmScale5(r5 < 0 ? 0u : static_cast<uint32_t>(r5 > 31 ? 31 : r5));
                                    const uint32_t g6c = lmScale6(g6 < 0 ? 0u : static_cast<uint32_t>(g6 > 63 ? 63 : g6));
                                    const uint32_t b5c = lmScale5(b5 < 0 ? 0u : static_cast<uint32_t>(b5 > 31 ? 31 : b5));
                                    const uint32_t r = (texColor >> 11) & 0x1F;
                                    const uint32_t g = (texColor >> 5) & 0x3F;
                                    const uint32_t b = texColor & 0x1F;
                                    litColor = static_cast<uint16_t>(((r * r5c + 16) >> 5) << 11 |
                                                                     ((g * g6c + 32) >> 6) << 5 |
                                                                     ((b * b5c + 16) >> 5));
                                }
                                if (fogEnabled)
                                    *fb = foggedColor(litColor, d, fogColorRb, fogColorG, fogColor);
                                else
                                {
                                    *fb = litColor;
                                }
                            }
                            z_val += z_step;
                            u_fixed += du_fixed;
                            v_fixed += dv_fixed;
                            if (!flatLight)
                            {
                                lr_cur += lrs;
                                lg_cur += lgs;
                                lb_cur += lbs;
                            }
                            ++zb;
                            ++fb;
                        }
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

            detail::rasterizeTriangleRows(runTop, clampStartY_top, endTopExclusive,
                                          runBottom, clampStartY_bottom, endBottomExclusive,
                                          x0, y0, x1, y1, x2, y2, width,
                                          [&](int y, int16_t x_start, int16_t x_end)
                                          {
                                              drawSpanSubdivided(y, x_start, x_end);
                                          });

            return true;
        }

    }
}