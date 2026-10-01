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
#include "Rendering/Pipeline/Rasterizer/Textured.hpp"
#include "Rendering/Resources/Texture.hpp"

namespace pip3D
{
    namespace Rasterizer
    {
        template <bool kTextured>
        PIP3D_HOT inline bool IRAM_ATTR fillTriangleLMCore(float x0, float y0, float z0,
                                                           float x1, float y1, float z1,
                                                           float x2, float y2, float z2,
                                                           float u0, float v0,
                                                           float u1, float v1,
                                                           float u2, float v2,
                                                           float mu0, float mv0,
                                                           float mu1, float mv1,
                                                           float mu2, float mv2,
                                                           float w0, float w1, float w2,
                                                           const Texture *tex,
                                                           uint16_t solidColor,
                                                           const LMPaletteAtlas &lmAtlas,
                                                           uint16_t *frameBuffer,
                                                           ZBuffer *zBuffer,
                                                           const DisplayConfig &config)
        {
            const int16_t width = config.width;
            const int16_t height = config.height;
            if (unlikely(!frameBuffer || !zBuffer))
                return true;
            if (unlikely(!lmAtlas.valid()))
                return true;
            if (y0 > y1)
            {
                std::swap(x0, x1);
                std::swap(y0, y1);
                std::swap(z0, z1);
                if constexpr (kTextured)
                {
                    std::swap(u0, u1);
                    std::swap(v0, v1);
                }
                std::swap(mu0, mu1);
                std::swap(mv0, mv1);
                std::swap(w0, w1);
            }
            if (y1 > y2)
            {
                std::swap(x1, x2);
                std::swap(y1, y2);
                std::swap(z1, z2);
                if constexpr (kTextured)
                {
                    std::swap(u1, u2);
                    std::swap(v1, v2);
                }
                std::swap(mu1, mu2);
                std::swap(mv1, mv2);
                std::swap(w1, w2);
            }
            if (y0 > y1)
            {
                std::swap(x0, x1);
                std::swap(y0, y1);
                std::swap(z0, z1);
                if constexpr (kTextured)
                {
                    std::swap(u0, u1);
                    std::swap(v0, v1);
                }
                std::swap(mu0, mu1);
                std::swap(mv0, mv1);
                std::swap(w0, w1);
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

            uint32_t texShiftU = 0, texMaskU = 0, texMaskV = 0;
            const uint16_t *texData = nullptr;
            const uint16_t *mipData = nullptr;
            uint8_t maxMipLevel = 0;
            bool hasMipmaps = false;
            const uint32_t *mipOffsets = nullptr;
            const uint8_t *mipShiftUArr = nullptr;
            const uint8_t *mipShiftVArr = nullptr;

            if constexpr (kTextured)
            {
                const Texture &t = *tex;
                const float texW = t.widthFlt();
                const float texH = t.heightFlt();
                u0 *= texW;
                u1 *= texW;
                u2 *= texW;
                v0 *= texH;
                v1 *= texH;
                v2 *= texH;

                texData = t.data;
                texShiftU = t.shiftU;
                texMaskU = t.maskU();
                texMaskV = t.maskV();
                mipData = t.mipData;
                maxMipLevel = t.mipCount;
                hasMipmaps = (maxMipLevel > 0 && mipData != nullptr && g_mipmapsEnabled);
                const detail::MipTable &mip = detail::getMipTables(t);
                mipOffsets = mip.offsets;
                mipShiftUArr = mip.shiftU;
                mipShiftVArr = mip.shiftV;
            }

            const float q0 = FastMath::fastReciprocal(w0);
            const float q1 = FastMath::fastReciprocal(w1);
            const float q2 = FastMath::fastReciprocal(w2);
            const float mu_over_z0 = mu0 * q0;
            const float mu_over_z1 = mu1 * q1;
            const float mu_over_z2 = mu2 * q2;
            const float mv_over_z0 = mv0 * q0;
            const float mv_over_z1 = mv1 * q1;
            const float mv_over_z2 = mv2 * q2;
            float u_over_z2 = 0.0f, v_over_z2 = 0.0f;
            float du_over_z_dx = 0.0f, du_over_z_dy = 0.0f;
            float dv_over_z_dx = 0.0f, dv_over_z_dy = 0.0f;

            if constexpr (kTextured)
            {
                const float u_over_z0 = u0 * q0;
                const float u_over_z1 = u1 * q1;
                u_over_z2 = u2 * q2;
                const float v_over_z0 = v0 * q0;
                const float v_over_z1 = v1 * q1;
                v_over_z2 = v2 * q2;

                const float du_over_z02 = u_over_z0 - u_over_z2;
                const float du_over_z12 = u_over_z1 - u_over_z2;
                du_over_z_dx = (du_over_z02 * dy12 - dy02 * du_over_z12) * invDet;
                du_over_z_dy = (dx02 * du_over_z12 - du_over_z02 * dx12) * invDet;
                const float dv_over_z02 = v_over_z0 - v_over_z2;
                const float dv_over_z12 = v_over_z1 - v_over_z2;
                dv_over_z_dx = (dv_over_z02 * dy12 - dy02 * dv_over_z12) * invDet;
                dv_over_z_dy = (dx02 * dv_over_z12 - dv_over_z02 * dx12) * invDet;
            }

            const float dq02 = q0 - q2;
            const float dq12 = q1 - q2;
            const float dq_dx = (dq02 * dy12 - dy02 * dq12) * invDet;
            const float dq_dy = (dx02 * dq12 - dq02 * dx12) * invDet;

            const float dmu_over_z02 = mu_over_z0 - mu_over_z2;
            const float dmu_over_z12 = mu_over_z1 - mu_over_z2;
            const float dmu_over_z_dx = (dmu_over_z02 * dy12 - dy02 * dmu_over_z12) * invDet;
            const float dmu_over_z_dy = (dx02 * dmu_over_z12 - dmu_over_z02 * dx12) * invDet;
            const float dmv_over_z02 = mv_over_z0 - mv_over_z2;
            const float dmv_over_z12 = mv_over_z1 - mv_over_z2;
            const float dmv_over_z_dx = (dmv_over_z02 * dy12 - dy02 * dmv_over_z12) * invDet;
            const float dmv_over_z_dy = (dx02 * dmv_over_z12 - dmv_over_z02 * dx12) * invDet;

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
            const int32_t z_step = static_cast<int32_t>(dz_dx * 16384.0f);

            uint32_t sr = 0, sg = 0, sb = 0;
            if constexpr (!kTextured)
            {
                sr = (solidColor >> 11) & 0x1F;
                sg = (solidColor >> 5) & 0x3F;
                sb = solidColor & 0x1F;
            }

            const int32_t lmW = static_cast<int32_t>(lmAtlas.width);
            const int32_t lmH = static_cast<int32_t>(lmAtlas.height);
            const int32_t lmMaxX = lmW - 1;
            const int32_t lmMaxY = lmH - 1;
            const LMSampling lmMode = static_cast<LMSampling>(g_bakedState.lmSampling);

            const int32_t lmOvershoot = (lmMode != LMSampling::Nearest) ? 1 : 0;
            ::pip3D::detail::LMTexelMemo lmMemo{};
            lmMemo.reset(lmAtlas);

            auto drawSpanSubdivided = [&](int y, int16_t x_start, int16_t x_end)
            {
                int16_t count = x_end - x_start + 1;
                if (count <= 0)
                    return;
                const float dy_factor = (static_cast<float>(y) + 0.5f - y2);
                const float z_dy_term = z2 + dy_factor * dz_dy;
                float q = q2 + dy_factor * dq_dy + ((static_cast<float>(x_start) + 0.5f) - x2) * dq_dx;
                float mu_over_z = mu_over_z2 + dy_factor * dmu_over_z_dy + ((static_cast<float>(x_start) + 0.5f) - x2) * dmu_over_z_dx;
                float mv_over_z = mv_over_z2 + dy_factor * dmv_over_z_dy + ((static_cast<float>(x_start) + 0.5f) - x2) * dmv_over_z_dx;
                float u_over_z = 0.0f, v_over_z = 0.0f;
                if constexpr (kTextured)
                {
                    u_over_z = u_over_z2 + dy_factor * du_over_z_dy + ((static_cast<float>(x_start) + 0.5f) - x2) * du_over_z_dx;
                    v_over_z = v_over_z2 + dy_factor * dv_over_z_dy + ((static_cast<float>(x_start) + 0.5f) - x2) * dv_over_z_dx;
                }

                float w_start = FastMath::fastReciprocal(q);
                float mu = mu_over_z * w_start;
                float mv = mv_over_z * w_start;
                float u = 0.0f, v = 0.0f;
                if constexpr (kTextured)
                {
                    u = u_over_z * w_start;
                    v = v_over_z * w_start;
                }

                int16_t cur_x = x_start;
                size_t index = static_cast<size_t>(y) * width + cur_x;
                uint16_t *__restrict__ zb = zbBase + index;
                uint16_t *__restrict__ fb = frameBuffer + index;

                int32_t cacheCellX = 0;
                int32_t cacheCellY = 0;
                bool cacheCellValid = false;
                uint32_t cacheLR = 0, cacheLG = 0, cacheLB = 0;

                while (count > 0)
                {
                    const int16_t step = (count > 16) ? 16 : count;
                    const float next_q = q + step * dq_dx;
                    const float next_mu_over_z = mu_over_z + step * dmu_over_z_dx;
                    const float next_mv_over_z = mv_over_z + step * dmv_over_z_dx;
                    float next_u_over_z = 0.0f, next_v_over_z = 0.0f;
                    if constexpr (kTextured)
                    {
                        next_u_over_z = u_over_z + step * du_over_z_dx;
                        next_v_over_z = v_over_z + step * dv_over_z_dx;
                    }
                    const float next_w = FastMath::fastReciprocal(next_q);
                    const float next_mu = next_mu_over_z * next_w;
                    const float next_mv = next_mv_over_z * next_w;
                    float next_u = 0.0f, next_v = 0.0f;
                    if constexpr (kTextured)
                    {
                        next_u = next_u_over_z * next_w;
                        next_v = next_v_over_z * next_w;
                    }
                    const float inv_step = likely(step == 16) ? 0.0625f : FastMath::fastReciprocal(static_cast<float>(step));

                    int32_t u_fixed = 0, v_fixed = 0, du_fixed = 0, dv_fixed = 0;
                    if constexpr (kTextured)
                    {
                        u_fixed = static_cast<int32_t>(u * 65536.0f);
                        v_fixed = static_cast<int32_t>(v * 65536.0f);
                        const float du = (next_u - u) * inv_step;
                        const float dv = (next_v - v) * inv_step;
                        du_fixed = static_cast<int32_t>(du * 65536.0f);
                        dv_fixed = static_cast<int32_t>(dv * 65536.0f);
                    }

                    int32_t lmX_fixed = static_cast<int32_t>(mu * 65536.0f) * lmW;
                    int32_t lmY_fixed = static_cast<int32_t>(mv * 65536.0f) * lmH;
                    const int32_t lmStepX = static_cast<int32_t>((static_cast<int32_t>(next_mu * 65536.0f) * lmW - lmX_fixed) * inv_step);
                    const int32_t lmStepY = static_cast<int32_t>((static_cast<int32_t>(next_mv * 65536.0f) * lmH - lmY_fixed) * inv_step);

                    const float z_scaled_start = z_dy_term + (static_cast<float>(cur_x) + 0.5f - x2) * dz_dx;
                    int32_t z_val = static_cast<int32_t>(z_scaled_start * 16384.0f);

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
                    if constexpr (kTextured)
                    {
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
                                    const int32_t intLodClamped = (intLod > static_cast<int32_t>(detail::MAX_MIP_TABLE)) ? static_cast<int32_t>(detail::MAX_MIP_TABLE) : intLod;
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
                    }

                    PIP3D_PREFETCH_W(zb + 16);
                    PIP3D_PREFETCH_R(fb + 16);
                    if constexpr (kTextured)
                        PIP3D_PREFETCH_R(chunkTexData);

                    const int16_t baseY = static_cast<int16_t>(y);

                    auto fetchTexel = [&](int16_t px_x) -> uint16_t
                    {
                        uint16_t texColor = 0;
                        if constexpr (kTextured)
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
                            texColor = td[(tv << tsh) | tu];
                        }
                        return texColor;
                    };

                    bool uniformChunk = false;
                    uint32_t uniR = 0, uniG = 0, uniB = 0;
                    {
                        const int32_t lmX0 = lmX_fixed >> 16;
                        const int32_t lmY0 = lmY_fixed >> 16;
                        const int32_t lmXe = ((lmX_fixed + (step - 1) * lmStepX) >> 16);
                        const int32_t lmYe = ((lmY_fixed + (step - 1) * lmStepY) >> 16);
                        const int32_t lmXlo = (lmX0 < lmXe) ? lmX0 : lmXe;
                        const int32_t lmXhi = (lmX0 < lmXe) ? lmXe : lmX0;
                        const int32_t lmYlo = (lmY0 < lmYe) ? lmY0 : lmYe;
                        const int32_t lmYhi = (lmY0 < lmYe) ? lmYe : lmY0;
                        if (lmXlo >= 0 && lmYlo >= 0 &&
                            lmXhi + lmOvershoot < lmW && lmYhi + lmOvershoot < lmH)
                        {
                            const int32_t bx0 = lmXlo >> 3, bx1 = (lmXhi + lmOvershoot) >> 3;
                            const int32_t by0 = lmYlo >> 3, by1 = (lmYhi + lmOvershoot) >> 3;
                            if (bx0 == bx1 && by0 == by1)
                            {
                                uint32_t r, g, b;
                                if (::pip3D::detail::lmBlockSolidColor(lmAtlas, bx0, by0, r, g, b))
                                {
                                    uniformChunk = true;
                                    uniR = r;
                                    uniG = g;
                                    uniB = b;
                                }
                            }
                            else if ((bx1 - bx0) <= 4 && (by1 - by0) <= 4)
                            {
                                uint32_t r, g, b;
                                if (::pip3D::detail::lmRectSolidColor(lmAtlas, bx0, bx1, by0, by1, r, g, b))
                                {
                                    uniformChunk = true;
                                    uniR = r;
                                    uniG = g;
                                    uniB = b;
                                }
                            }
                        }
                    }

                    if (uniformChunk)
                    {
                        uint16_t fastLitColor = 0;
                        if constexpr (!kTextured)
                        {
                            fastLitColor = static_cast<uint16_t>(
                                ((sr * uniR + 16) >> 5) << 11 |
                                ((sg * uniG + 32) >> 6) << 5 |
                                ((sb * uniB + 16) >> 5));
                        }
                        for (int16_t i = 0; i < step; ++i)
                        {
                            const uint16_t d = static_cast<uint16_t>(z_val >> 14);
                            const uint16_t curr = *zb & Z_DEPTH_MASK;
                            if (d > curr)
                            {
                                *zb = d;
                                uint16_t litColor = fastLitColor;
                                if constexpr (kTextured)
                                {
                                    const uint16_t texColor = fetchTexel(cur_x + i);
                                    const uint32_t tr = (texColor >> 11) & 0x1F;
                                    const uint32_t tg = (texColor >> 5) & 0x3F;
                                    const uint32_t tb = texColor & 0x1F;

                                    litColor = static_cast<uint16_t>(
                                        (((tr * uniR + 16) >> 5) << 11) |
                                        (((tg * uniG + 32) >> 6) << 5) |
                                        ((tb * uniB + 16) >> 5));
                                }
                                if (fogEnabled)
                                    *fb = foggedColor(litColor, d, fogColorRb, fogColorG, fogColor);
                                else
                                    *fb = litColor;
                            }
                            z_val += z_step;
                            if constexpr (kTextured)
                            {
                                u_fixed += du_fixed;
                                v_fixed += dv_fixed;
                            }
                            ++zb;
                            ++fb;
                        }
                    }
                    else if (lmMode == LMSampling::Dithered)
                    {
                        for (int16_t i = 0; i < step; ++i)
                        {
                            const uint16_t d = static_cast<uint16_t>(z_val >> 14);
                            const uint16_t curr = *zb & Z_DEPTH_MASK;
                            if (d > curr)
                            {
                                *zb = d;
                                uint32_t tr, tg, tb;
                                if constexpr (kTextured)
                                {
                                    const uint16_t texColor = fetchTexel(cur_x + i);
                                    tr = (texColor >> 11) & 0x1F;
                                    tg = (texColor >> 5) & 0x3F;
                                    tb = texColor & 0x1F;
                                }
                                else
                                {
                                    tr = sr;
                                    tg = sg;
                                    tb = sb;
                                }

                                const int16_t px_x = cur_x + i;
                                const int32_t sx = (lmX_fixed >> 16) +
                                                   (((static_cast<uint32_t>(lmX_fixed >> 8) & 0xFFu) >
                                                     static_cast<uint32_t>(::pip3D::detail::kBayerMatrix10Bit[baseY & 3][px_x & 3] >> 2))
                                                        ? 1
                                                        : 0);
                                const int32_t sy = (lmY_fixed >> 16) +
                                                   (((static_cast<uint32_t>(lmY_fixed >> 8) & 0xFFu) >
                                                     static_cast<uint32_t>(::pip3D::detail::kBayerMatrix10Bit[(baseY + 2) & 3][px_x & 3] >> 2))
                                                        ? 1
                                                        : 0);
                                uint32_t lr, lg, lb;
                                if (!cacheCellValid || sx != cacheCellX || sy != cacheCellY)
                                {
                                    ::pip3D::detail::lmSampleNearest(lmMemo, sx << 16, sy << 16, lmMaxX, lmMaxY, lr, lg, lb);
                                    cacheCellX = sx;
                                    cacheCellY = sy;
                                    cacheLR = lr;
                                    cacheLG = lg;
                                    cacheLB = lb;
                                    cacheCellValid = true;
                                }
                                else
                                {
                                    lr = cacheLR;
                                    lg = cacheLG;
                                    lb = cacheLB;
                                }
                                const uint16_t litColor = static_cast<uint16_t>(
                                    (((tr * lr + 16) >> 5) << 11) |
                                    (((tg * lg + 32) >> 6) << 5) |
                                    ((tb * lb + 16) >> 5));
                                if (fogEnabled)
                                    *fb = foggedColor(litColor, d, fogColorRb, fogColorG, fogColor);
                                else
                                    *fb = litColor;
                            }
                            z_val += z_step;
                            if constexpr (kTextured)
                            {
                                u_fixed += du_fixed;
                                v_fixed += dv_fixed;
                            }
                            lmX_fixed += lmStepX;
                            lmY_fixed += lmStepY;
                            ++zb;
                            ++fb;
                        }
                    }
                    else
                    {

                        for (int16_t i = 0; i < step; ++i)
                        {
                            const uint16_t d = static_cast<uint16_t>(z_val >> 14);
                            const uint16_t curr = *zb & Z_DEPTH_MASK;
                            if (d > curr)
                            {
                                *zb = d;
                                uint32_t tr, tg, tb;
                                if constexpr (kTextured)
                                {
                                    const uint16_t texColor = fetchTexel(cur_x + i);
                                    tr = (texColor >> 11) & 0x1F;
                                    tg = (texColor >> 5) & 0x3F;
                                    tb = texColor & 0x1F;
                                }
                                else
                                {
                                    tr = sr;
                                    tg = sg;
                                    tb = sb;
                                }
                                uint32_t lr, lg, lb;
                                const int32_t tx = lmX_fixed >> 16;
                                const int32_t ty = lmY_fixed >> 16;
                                if (!cacheCellValid || tx != cacheCellX || ty != cacheCellY)
                                {
                                    ::pip3D::detail::lmSampleNearest(lmMemo, lmX_fixed, lmY_fixed, lmMaxX, lmMaxY, lr, lg, lb);
                                    cacheCellX = tx;
                                    cacheCellY = ty;
                                    cacheLR = lr;
                                    cacheLG = lg;
                                    cacheLB = lb;
                                    cacheCellValid = true;
                                }
                                else
                                {
                                    lr = cacheLR;
                                    lg = cacheLG;
                                    lb = cacheLB;
                                }
                                const uint16_t litColor = static_cast<uint16_t>(
                                    (((tr * lr + 16) >> 5) << 11) |
                                    (((tg * lg + 32) >> 6) << 5) |
                                    ((tb * lb + 16) >> 5));
                                if (fogEnabled)
                                    *fb = foggedColor(litColor, d, fogColorRb, fogColorG, fogColor);
                                else
                                    *fb = litColor;
                            }
                            z_val += z_step;
                            if constexpr (kTextured)
                            {
                                u_fixed += du_fixed;
                                v_fixed += dv_fixed;
                            }
                            lmX_fixed += lmStepX;
                            lmY_fixed += lmStepY;
                            ++zb;
                            ++fb;
                        }
                    }

                    q = next_q;
                    u_over_z = next_u_over_z;
                    v_over_z = next_v_over_z;
                    mu_over_z = next_mu_over_z;
                    mv_over_z = next_mv_over_z;
                    u = next_u;
                    v = next_v;
                    mu = next_mu;
                    mv = next_mv;

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

        PIP3D_HOT inline bool IRAM_ATTR fillTriangleTexturedLM(float x0, float y0, float z0,
                                                               float x1, float y1, float z1,
                                                               float x2, float y2, float z2,
                                                               float u0, float v0,
                                                               float u1, float v1,
                                                               float u2, float v2,
                                                               float mu0, float mv0,
                                                               float mu1, float mv1,
                                                               float mu2, float mv2,
                                                               float w0, float w1, float w2,
                                                               const Texture &tex,
                                                               const LMPaletteAtlas &lmAtlas,
                                                               uint16_t *frameBuffer,
                                                               ZBuffer *zBuffer,
                                                               const DisplayConfig &config)
        {
            return fillTriangleLMCore<true>(x0, y0, z0, x1, y1, z1, x2, y2, z2,
                                            u0, v0, u1, v1, u2, v2,
                                            mu0, mv0, mu1, mv1, mu2, mv2,
                                            w0, w1, w2,
                                            &tex, 0, lmAtlas, frameBuffer, zBuffer, config);
        }

        PIP3D_HOT inline bool IRAM_ATTR
        fillTriangleSolidLM(float x0, float y0, float z0,
                            float x1, float y1, float z1,
                            float x2, float y2, float z2,
                            float mu0, float mv0,
                            float mu1, float mv1,
                            float mu2, float mv2,
                            float w0, float w1, float w2,
                            uint16_t solidColor,
                            const LMPaletteAtlas &lmAtlas,
                            uint16_t *frameBuffer,
                            ZBuffer *zBuffer,
                            const DisplayConfig &config) noexcept
        {
            return fillTriangleLMCore<false>(x0, y0, z0, x1, y1, z1, x2, y2, z2,
                                             0, 0, 0, 0, 0, 0,
                                             mu0, mv0, mu1, mv1, mu2, mv2,
                                             w0, w1, w2,
                                             nullptr, solidColor, lmAtlas,
                                             frameBuffer, zBuffer, config);
        }

    }
}
