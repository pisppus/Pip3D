#pragma once

#include <cstdint>
#include <utility>

#include "Core/Platform.hpp"
#include "Math/Algebra.hpp"
#include "Rendering/Buffers/ZBuffer.hpp"
#include "Rendering/Lighting/Fog.hpp"
#include "Rendering/Pipeline/Shading.hpp"

namespace pip3D
{
    namespace Rasterizer
    {

        template <typename Shader, int N_ATTRS>
        struct alignas(16) InterpolatedParamsN
        {
            uint16_t *PIP3D_RESTRICT frameBuffer;
            uint16_t *PIP3D_RESTRICT zbBase;
            int32_t dz_dx_fixed;
            int32_t dz_dy_fixed;
            int32_t dz_dx2_fixed;
            int32_t dz_dx3_fixed;
            int32_t dz_dx4_fixed;
            alignas(16) int32_t da_dx_fixed[N_ATTRS];
            alignas(16) int32_t da_dy_fixed[N_ATTRS];
            alignas(16) int32_t da_dx2_fixed[N_ATTRS];
            alignas(16) int32_t da_dx3_fixed[N_ATTRS];
            alignas(16) int32_t da_dx4_fixed[N_ATTRS];
            int16_t width;
            int16_t height;
            Shader shader;
        };

        template <typename Shader, int N_ATTRS>
        __attribute__((noinline, hot)) IRAM_ATTR static void fillInterpolatedHalfN(
            float xa0, float ya0,
            float xa1, float ya1,
            float xb0, float yb0,
            float xb1, float yb1,
            int endYExclusive,
            int32_t z_start_fixed_base,
            const int32_t *PIP3D_RESTRICT a_start_fixed_base,
            int clampStartY,
            const InterpolatedParamsN<Shader, N_ATTRS> &params) noexcept
        {
            const float dya = ya1 - ya0;
            const float dyb = yb1 - yb0;
            if (unlikely(fabsf(dya) < 1e-6f || fabsf(dyb) < 1e-6f))
                return;

            const float dx_dy_A = (xa1 - xa0) * FastMath::fastReciprocal(dya);
            const float dx_dy_B = (xb1 - xb0) * FastMath::fastReciprocal(dyb);

            const int clampEndY = (endYExclusive > params.height) ? params.height : endYExclusive;
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

            alignas(16) int32_t da_dx_local[N_ATTRS];
            alignas(16) int32_t da_dx2_local[N_ATTRS];
            alignas(16) int32_t da_dx3_local[N_ATTRS];
            alignas(16) int32_t da_dx4_local[N_ATTRS];
            alignas(16) int32_t da_dy_local[N_ATTRS];
            for (int i = 0; i < N_ATTRS; ++i)
            {
                da_dx_local[i] = params.da_dx_fixed[i];
                da_dx2_local[i] = params.da_dx2_fixed[i];
                da_dx3_local[i] = params.da_dx3_fixed[i];
                da_dx4_local[i] = params.da_dx4_fixed[i];
                da_dy_local[i] = params.da_dy_fixed[i];
            }

            int32_t z_row_fixed = z_start_fixed_base;
            alignas(16) int32_t a_row_fixed[N_ATTRS];
            for (int i = 0; i < N_ATTRS; ++i)
                a_row_fixed[i] = a_start_fixed_base[i];

            const int32_t dz_dx = params.dz_dx_fixed;
            const int32_t dz_dx2 = params.dz_dx2_fixed;
            const int32_t dz_dx3 = params.dz_dx3_fixed;
            const int32_t dz_dx4 = params.dz_dx4_fixed;
            const int32_t dz_dy = params.dz_dy_fixed;
            const Shader &shader = params.shader;

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
                    int32_t depth_fixed = z_row_fixed + dz_dx * xStart;
                    alignas(16) int32_t a_fixed[N_ATTRS];
                    for (int i = 0; i < N_ATTRS; ++i)
                        a_fixed[i] = a_row_fixed[i] + da_dx_local[i] * xStart;

                    uint16_t *PIP3D_RESTRICT zPtr =
                        params.zbBase + static_cast<size_t>(y) * params.width + xStart;
                    uint16_t *PIP3D_RESTRICT fbPtr =
                        params.frameBuffer + static_cast<size_t>(y) * params.width + xStart;

                    const int16_t *PIP3D_RESTRICT bayerRow = ::pip3D::detail::kBayerMatrix10Bit[y & 3];

                    PIP3D_PREFETCH_W(zPtr);
                    PIP3D_PREFETCH_W(fbPtr);

                    int16_t count = xEnd - xStart + 1;
                    int16_t xCur = xStart;

                    const int idx0 = xStart & 3;
                    const int32_t bayer0 = bayerRow[idx0];
                    const int32_t bayer1 = bayerRow[(idx0 + 1) & 3];
                    const int32_t bayer2 = bayerRow[(idx0 + 2) & 3];
                    const int32_t bayer3 = bayerRow[(idx0 + 3) & 3];

                    while (count >= 4)
                    {
                        PIP3D_PREFETCH_W(zPtr + 16);
                        PIP3D_PREFETCH_W(fbPtr + 16);

                        {
                            const uint16_t d0 = static_cast<uint16_t>(depth_fixed >> 14);
                            if (d0 > (zPtr[0] & Z_DEPTH_MASK))
                            {
                                zPtr[0] = d0;
                                fbPtr[0] = shader(a_fixed, bayer0);
                            }
                        }
                        {
                            const uint16_t d1 = static_cast<uint16_t>((depth_fixed + dz_dx) >> 14);
                            if (d1 > (zPtr[1] & Z_DEPTH_MASK))
                            {
                                alignas(16) int32_t a1[N_ATTRS];
                                for (int i = 0; i < N_ATTRS; ++i)
                                    a1[i] = a_fixed[i] + da_dx_local[i];
                                zPtr[1] = d1;
                                fbPtr[1] = shader(a1, bayer1);
                            }
                        }
                        {
                            const uint16_t d2 = static_cast<uint16_t>((depth_fixed + dz_dx2) >> 14);
                            if (d2 > (zPtr[2] & Z_DEPTH_MASK))
                            {
                                alignas(16) int32_t a2[N_ATTRS];
                                for (int i = 0; i < N_ATTRS; ++i)
                                    a2[i] = a_fixed[i] + da_dx2_local[i];
                                zPtr[2] = d2;
                                fbPtr[2] = shader(a2, bayer2);
                            }
                        }
                        {
                            const uint16_t d3 = static_cast<uint16_t>((depth_fixed + dz_dx3) >> 14);
                            if (d3 > (zPtr[3] & Z_DEPTH_MASK))
                            {
                                alignas(16) int32_t a3[N_ATTRS];
                                for (int i = 0; i < N_ATTRS; ++i)
                                    a3[i] = a_fixed[i] + da_dx3_local[i];
                                zPtr[3] = d3;
                                fbPtr[3] = shader(a3, bayer3);
                            }
                        }

                        depth_fixed += dz_dx4;
                        for (int i = 0; i < N_ATTRS; ++i)
                            a_fixed[i] += da_dx4_local[i];

                        zPtr += 4;
                        fbPtr += 4;
                        count -= 4;
                        xCur += 4;
                    }

                    while (count > 0)
                    {
                        const uint16_t d = static_cast<uint16_t>(depth_fixed >> 14);
                        if (d > (zPtr[0] & Z_DEPTH_MASK))
                        {
                            zPtr[0] = d;
                            const int32_t bayerValue = bayerRow[xCur & 3];
                            fbPtr[0] = shader(a_fixed, bayerValue);
                        }
                        depth_fixed += dz_dx;
                        for (int i = 0; i < N_ATTRS; ++i)
                            a_fixed[i] += da_dx_local[i];

                        ++zPtr;
                        ++fbPtr;
                        ++xCur;
                        --count;
                    }
                }

                leftX_fixed += dx_dy_A_fixed;
                rightX_fixed += dx_dy_B_fixed;
                z_row_fixed += dz_dy;
                for (int i = 0; i < N_ATTRS; ++i)
                    a_row_fixed[i] += da_dy_local[i];
            }
        }

        template <typename Shader, int N_ATTRS>
        PIP3D_FORCE_INLINE static void fillTriangleInterpolatedN(
            int16_t x0, int16_t y0, float z0, const float *a0,
            int16_t x1, int16_t y1, float z1, const float *a1,
            int16_t x2, int16_t y2, float z2, const float *a2,
            const float *scales,
            const Shader &shader,
            uint16_t *PIP3D_RESTRICT frameBuffer,
            ZBuffer *PIP3D_RESTRICT zBuffer,
            const DisplayConfig &config) noexcept
        {
            const int16_t width = config.width;
            const int16_t height = config.height;

            if (unlikely(!frameBuffer || !zBuffer))
                return;

            uint16_t *const PIP3D_RESTRICT zBufferData = zBuffer->data();
            if (unlikely(!zBufferData))
                return;

            if (y0 > y1)
            {
                std::swap(x0, x1);
                std::swap(y0, y1);
                std::swap(z0, z1);
                std::swap(a0, a1);
            }
            if (y1 > y2)
            {
                std::swap(x1, x2);
                std::swap(y1, y2);
                std::swap(z1, z2);
                std::swap(a1, a2);
            }
            if (y0 > y1)
            {
                std::swap(x0, x1);
                std::swap(y0, y1);
                std::swap(z0, z1);
                std::swap(a0, a1);
            }

            if (y0 == y2)
                return;
            if (unlikely(x0 == x1 && x1 == x2))
                return;

            const float dx02 = static_cast<float>(x0 - x2);
            const float dy12 = static_cast<float>(y1 - y2);
            const float dy02 = static_cast<float>(y0 - y2);
            const float dx12 = static_cast<float>(x1 - x2);
            const float det = dx02 * dy12 - dy02 * dx12;
            if (unlikely(fabsf(det) < 1e-6f))
                return;
            const float invDet = FastMath::fastReciprocal(det);

            constexpr float depthScale = 16384.0f;
            const float dz02 = z0 - z2;
            const float dz12 = z1 - z2;
            const float dz_dx = (dz02 * dy12 - dy02 * dz12) * invDet;
            const float dz_dy = (dx02 * dz12 - dz02 * dx12) * invDet;

            float da_dx[N_ATTRS];
            float da_dy[N_ATTRS];
            for (int i = 0; i < N_ATTRS; ++i)
            {
                const float da02 = a0[i] - a2[i];
                const float da12 = a1[i] - a2[i];
                da_dx[i] = (da02 * dy12 - dy02 * da12) * invDet;
                da_dy[i] = (dx02 * da12 - da02 * dx12) * invDet;
            }

            InterpolatedParamsN<Shader, N_ATTRS> params;
            params.frameBuffer = frameBuffer;
            params.zbBase = zBufferData;
            params.width = width;
            params.height = height;

            const int32_t dzx = static_cast<int32_t>(dz_dx * depthScale);
            params.dz_dx_fixed = dzx;
            params.dz_dy_fixed = static_cast<int32_t>(dz_dy * depthScale);
            params.dz_dx2_fixed = dzx * 2;
            params.dz_dx3_fixed = dzx * 3;
            params.dz_dx4_fixed = dzx * 4;

            for (int i = 0; i < N_ATTRS; ++i)
            {
                const int32_t dx = static_cast<int32_t>(da_dx[i] * scales[i]);
                const int32_t dy = static_cast<int32_t>(da_dy[i] * scales[i]);
                params.da_dx_fixed[i] = dx;
                params.da_dy_fixed[i] = dy;
                params.da_dx2_fixed[i] = dx * 2;
                params.da_dx3_fixed[i] = dx * 3;
                params.da_dx4_fixed[i] = dx * 4;
            }
            params.shader = shader;

            const int startTop = fastCeilNonNeg(static_cast<float>(y0) - 0.5f);
            const int endTopExclusive = fastCeilNonNeg(static_cast<float>(y1) - 0.5f);
            const int startBottom = fastCeilNonNeg(static_cast<float>(y1) - 0.5f);
            const int endBottomExclusive = fastCeilNonNeg(static_cast<float>(y2) - 0.5f);

            const int clampStartY_top = (startTop < 0) ? 0 : startTop;
            const int clampStartY_bottom = (startBottom < 0) ? 0 : startBottom;

            const bool runTop = (clampStartY_top < endTopExclusive) && (clampStartY_top < height);
            const bool runBottom = (clampStartY_bottom < endBottomExclusive) && (clampStartY_bottom < height);
            if (!runTop && !runBottom)
                return;

            const float z2_scaled = z2 * depthScale;
            const float dz_dx_scaled = dz_dx * depthScale;
            const float dz_dy_scaled = dz_dy * depthScale;

            float a2_scaled[N_ATTRS];
            float da_dx_scaled[N_ATTRS];
            float da_dy_scaled[N_ATTRS];
            for (int i = 0; i < N_ATTRS; ++i)
            {
                a2_scaled[i] = a2[i] * scales[i];
                da_dx_scaled[i] = da_dx[i] * scales[i];
                da_dy_scaled[i] = da_dy[i] * scales[i];
            }

            const float x2f = static_cast<float>(x2);
            const float halfPixel = 0.5f;

            if (runTop)
            {
                const float dy_init =
                    static_cast<float>(clampStartY_top) + halfPixel - static_cast<float>(y2);

                const float z_base = z2_scaled + dz_dy_scaled * dy_init - dz_dx_scaled * x2f + dz_dx_scaled * halfPixel;
                int32_t a_base[N_ATTRS];
                for (int i = 0; i < N_ATTRS; ++i)
                {
                    a_base[i] = static_cast<int32_t>(
                        a2_scaled[i] + da_dy_scaled[i] * dy_init - da_dx_scaled[i] * x2f + da_dx_scaled[i] * halfPixel);
                }

                fillInterpolatedHalfN<Shader, N_ATTRS>(
                    static_cast<float>(x0), static_cast<float>(y0),
                    static_cast<float>(x1), static_cast<float>(y1),
                    static_cast<float>(x0), static_cast<float>(y0),
                    static_cast<float>(x2), static_cast<float>(y2),
                    endTopExclusive,
                    static_cast<int32_t>(z_base),
                    a_base,
                    clampStartY_top,
                    params);
            }

            if (runBottom)
            {
                const float dy_init =
                    static_cast<float>(clampStartY_bottom) + halfPixel - static_cast<float>(y2);

                const float z_base = z2_scaled + dz_dy_scaled * dy_init - dz_dx_scaled * x2f + dz_dx_scaled * halfPixel;
                int32_t a_base[N_ATTRS];
                for (int i = 0; i < N_ATTRS; ++i)
                {
                    a_base[i] = static_cast<int32_t>(
                        a2_scaled[i] + da_dy_scaled[i] * dy_init - da_dx_scaled[i] * x2f + da_dx_scaled[i] * halfPixel);
                }

                fillInterpolatedHalfN<Shader, N_ATTRS>(
                    static_cast<float>(x1), static_cast<float>(y1),
                    static_cast<float>(x2), static_cast<float>(y2),
                    static_cast<float>(x0), static_cast<float>(y0),
                    static_cast<float>(x2), static_cast<float>(y2),
                    endBottomExclusive,
                    static_cast<int32_t>(z_base),
                    a_base,
                    clampStartY_bottom,
                    params);
            }
        }

        struct SmoothShader
        {
            PIP3D_FORCE_INLINE uint16_t operator()(
                const int32_t *PIP3D_RESTRICT attrs, int32_t bayer) const noexcept
            {

                const uint32_t ir = static_cast<uint32_t>(attrs[0] + bayer) >> 10;
                const uint32_t ig = static_cast<uint32_t>(attrs[1] + bayer) >> 10;
                const uint32_t ib = static_cast<uint32_t>(attrs[2] + bayer) >> 10;

                return static_cast<uint16_t>((ir << 11) | (ig << 5) | ib);
            }
        };

        PIP3D_FORCE_INLINE static void fillTriangleSmooth(
            int16_t x0, int16_t y0, float z0,
            int16_t x1, int16_t y1, float z1,
            int16_t x2, int16_t y2, float z2,
            float r0, float g0, float b0,
            float r1, float g1, float b1,
            float r2, float g2, float b2,
            uint16_t *PIP3D_RESTRICT frameBuffer,
            ZBuffer *PIP3D_RESTRICT zBuffer,
            const DisplayConfig &config) noexcept
        {
            constexpr int N = 3;
            const float a0[N] = {r0, g0, b0};
            const float a1[N] = {r1, g1, b1};
            const float a2[N] = {r2, g2, b2};
            constexpr float scales[N] = {31.0f * 1024.0f, 63.0f * 1024.0f, 31.0f * 1024.0f};

            SmoothShader shader;
            fillTriangleInterpolatedN<SmoothShader, N>(
                x0, y0, z0, a0,
                x1, y1, z1, a1,
                x2, y2, z2, a2,
                scales,
                shader,
                frameBuffer, zBuffer, config);
        }

        struct PhongShader
        {

            Vector3 camPos;
            const Light *lights;
            int lightCount;
            float baseR, baseG, baseB;

            float fogR = 0.0f, fogG = 0.0f, fogB = 0.0f;
            float fogWorldNear = 0.0f;
            float fogWorldScale = 0.0f;
            bool fogEnabled = false;

            float hemiScale = 0.22f;
            float ambientBase = 0.04f;
            float rimStrength = 0.0f;
            float diffuseWrap = 0.20f;
            float diffScale = 0.0f;
            bool enableSpec = false;
            bool enableRim = false;

            __attribute__((noinline, hot)) IRAM_ATTR uint16_t operator()(
                const int32_t *PIP3D_RESTRICT attrs, int32_t bayer) const noexcept
            {

                const float scale = FastMath::fastReciprocal(static_cast<float>(attrs[6]));

                const float px = attrs[0] * scale;
                const float py = attrs[1] * scale;
                const float pz = attrs[2] * scale;

                float nx = attrs[3] * scale;
                float ny = attrs[4] * scale;
                float nz = attrs[5] * scale;
                const float nLenSq = nx * nx + ny * ny + nz * nz;
                const float invNLen = (likely(nLenSq > 0.3f && nLenSq < 3.0f))
                                          ? (0.5f * (3.0f - nLenSq))
                                          : ((nLenSq > 1e-10f) ? FastMath::fastInvSqrt(nLenSq) : 0.0f);
                nx *= invNLen;
                ny *= invNLen;
                nz *= invNLen;

                const float vdx_raw = camPos.x - px;
                const float vdy_raw = camPos.y - py;
                const float vdz_raw = camPos.z - pz;
                const float vLenSq = vdx_raw * vdx_raw +
                                     vdy_raw * vdy_raw +
                                     vdz_raw * vdz_raw;
                const float invVLen = (vLenSq > 1e-8f)
                                          ? FastMath::fastInvSqrt(vLenSq)
                                          : 0.0f;

                const float vdx = vdx_raw * invVLen;
                const float vdy = vdy_raw * invVLen;
                const float vdz = vdz_raw * invVLen;
                const float dist = vLenSq * invVLen;

                const float NdotV_raw = nx * vdx + ny * vdy + nz * vdz;

                const float ambientScale = Rasterizer::g_ambientScale;
                const float hemiLinear = ny * hemiScale + 2.0f * ambientBase;
                const float ambientTerm =
                    Shading::smoothPositive(hemiLinear, Shading::HEMI_SMOOTH_EPS) * ambientScale;

                float r = baseR * ambientTerm;
                float g = baseG * ambientTerm;
                float b = baseB * ambientTerm;

                float rimAmount = 0.0f;
                if (enableRim)
                {
                    const float NdotV = (NdotV_raw < 0.0f) ? 0.0f : NdotV_raw;
                    const float rim = 1.0f - NdotV;
                    rimAmount = rim * rim * rimStrength;
                    r += baseR * rimAmount * ambientScale;
                    g += baseG * rimAmount * ambientScale;
                    b += baseB * rimAmount * ambientScale;
                }

                const float specNVFalloff = enableSpec
                                                ? Shading::specFalloffByNdotV(NdotV_raw)
                                                : 0.0f;
                const float *PIP3D_RESTRICT specLUT = Shading::specLUT;

                for (int i = 0; i < lightCount; ++i)
                {
                    const Light &light = lights[i];
                    float ldx, ldy, ldz;
                    float lightAtten;

                    if (likely(light.type == LIGHT_DIRECTIONAL))
                    {

                        ldx = -light.direction.x;
                        ldy = -light.direction.y;
                        ldz = -light.direction.z;
                        lightAtten = light.intensity;
                    }
                    else if (light.type == LIGHT_POINT)
                    {
                        ldx = light.position.x - px;
                        ldy = light.position.y - py;
                        ldz = light.position.z - pz;
                        const float distSq = ldx * ldx + ldy * ldy + ldz * ldz;
                        if (light.range > 0.0f)
                        {
                            if (distSq > light.rangeSq)
                                continue;
                            if (distSq > 1e-8f)
                            {
                                const float invDist = FastMath::fastInvSqrt(distSq);
                                ldx *= invDist;
                                ldy *= invDist;
                                ldz *= invDist;
                                lightAtten = light.intensity *
                                             FastMath::fastReciprocal(1.0f + distSq * light.invRangeSq);
                            }
                            else
                            {
                                lightAtten = light.intensity;
                            }
                        }
                        else if (distSq > 1e-8f)
                        {
                            const float invDist = FastMath::fastInvSqrt(distSq);
                            ldx *= invDist;
                            ldy *= invDist;
                            ldz *= invDist;
                            lightAtten = light.intensity;
                        }
                        else
                        {
                            lightAtten = light.intensity;
                        }
                    }
                    else
                    {
                        continue;
                    }

                    const float NdotL = nx * ldx + ny * ldy + nz * ldz;
                    if (NdotL <= -diffuseWrap)
                        continue;

                    float diffuse = Shading::smoothPositive(
                                        NdotL + diffuseWrap,
                                        Shading::DIFFUSE_SMOOTH_EPS) *
                                    diffScale;
                    diffuse *= lightAtten;

                    float spec = 0.0f;
                    if (enableSpec && NdotL > 0.0f && specNVFalloff > 0.0f)
                    {
                        const float hx = ldx + vdx;
                        const float hy = ldy + vdy;
                        const float hz = ldz + vdz;
                        const float hLenSq = hx * hx + hy * hy + hz * hz;
                        if (hLenSq > Shading::HALF_VECTOR_MIN_LEN_SQ)
                        {
                            const float invHLen = FastMath::fastInvSqrt(hLenSq);
                            const float NdotH = (nx * hx + ny * hy + nz * hz) * invHLen;
                            if (NdotH > 0.0f)
                            {

                                const uint8_t nhIdx = static_cast<uint8_t>(
                                    fminf(NdotH, 1.0f) * 255.0f);
                                spec = specLUT[nhIdx] * lightAtten * specNVFalloff;
                            }
                        }
                    }

                    const float lightR = light.cachedR;
                    const float lightG = light.cachedG;
                    const float lightB = light.cachedB;

                    r += (baseR * diffuse + spec) * lightR;
                    g += (baseG * diffuse + spec) * lightG;
                    b += (baseB * diffuse + spec) * lightB;
                }

                Shading::toneMap3(r, g, b);

                if (fogEnabled)
                {
                    float fogFactor = (dist - fogWorldNear) * fogWorldScale;
                    fogFactor = clamp(fogFactor, 0.0f, 1.0f);
                    const float invFog = 1.0f - fogFactor;
                    r = r * invFog + fogR * fogFactor;
                    g = g * invFog + fogG * fogFactor;
                    b = b * invFog + fogB * fogFactor;
                }

                const int32_t ir = clamp((static_cast<int32_t>(r * 31744.0f) + bayer) >> 10, 0, 31);
                const int32_t ig = clamp((static_cast<int32_t>(g * 64512.0f) + bayer) >> 10, 0, 63);
                const int32_t ib = clamp((static_cast<int32_t>(b * 31744.0f) + bayer) >> 10, 0, 31);

                return static_cast<uint16_t>((ir << 11) | (ig << 5) | ib);
            }
        };

        PIP3D_FORCE_INLINE static void fillTrianglePhong(
            int16_t x0, int16_t y0, float z0,
            int16_t x1, int16_t y1, float z1,
            int16_t x2, int16_t y2, float z2,
            float px0, float py0, float pz0,
            float px1, float py1, float pz1,
            float px2, float py2, float pz2,
            float nx0, float ny0, float nz0,
            float nx1, float ny1, float nz1,
            float nx2, float ny2, float nz2,
            float d0, float d1, float d2,
            float baseR, float baseG, float baseB,
            const Vector3 &camPos,
            const Light *lights, int lightCount,
            uint16_t *PIP3D_RESTRICT frameBuffer,
            ZBuffer *PIP3D_RESTRICT zBuffer,
            const DisplayConfig &config) noexcept
        {

            const ShadingParams &sp = Shading::getParams();

            const bool enableSpec = (sp.specularStrength > 0.0f);
            const bool enableRim = (sp.rimStrength > 0.0f);

            if (enableSpec)
                Shading::ensureSpecLUT();

            constexpr int N = 7;

            const float invW0 = (d0 > 1e-4f) ? FastMath::fastReciprocal(d0) : 1.0f;
            const float invW1 = (d1 > 1e-4f) ? FastMath::fastReciprocal(d1) : 1.0f;
            const float invW2 = (d2 > 1e-4f) ? FastMath::fastReciprocal(d2) : 1.0f;

            const float a0[N] = {
                px0 * invW0, py0 * invW0, pz0 * invW0,
                nx0 * invW0, ny0 * invW0, nz0 * invW0,
                invW0};
            const float a1[N] = {
                px1 * invW1, py1 * invW1, pz1 * invW1,
                nx1 * invW1, ny1 * invW1, nz1 * invW1,
                invW1};
            const float a2[N] = {
                px2 * invW2, py2 * invW2, pz2 * invW2,
                nx2 * invW2, ny2 * invW2, nz2 * invW2,
                invW2};

            constexpr float normalScale = 65536.0f;
            constexpr float scales[N] = {
                normalScale, normalScale, normalScale,
                normalScale, normalScale, normalScale,
                normalScale};

            PhongShader shader;
            shader.camPos = camPos;
            shader.lights = lights;
            shader.lightCount = lightCount;
            shader.baseR = baseR;
            shader.baseG = baseG;
            shader.baseB = baseB;

            const auto &fog = g_fogState;
            shader.fogEnabled = fog.enabled;
            if (fog.enabled)
            {
                const FogColorF fc = fogColorFloat();
                shader.fogR = fc.r;
                shader.fogG = fc.g;
                shader.fogB = fc.b;
                shader.fogWorldNear = fog.worldNear;
                shader.fogWorldScale = fog.worldScale;
            }

            shader.hemiScale = sp.hemiScale;
            shader.ambientBase = sp.ambientBase;
            shader.rimStrength = sp.rimStrength;
            shader.diffuseWrap = sp.diffuseWrap;

            shader.diffScale = sp.diffuseStrength *
                               FastMath::fastReciprocal(1.0f + sp.diffuseWrap);
            shader.enableSpec = enableSpec;
            shader.enableRim = enableRim;

            fillTriangleInterpolatedN<PhongShader, N>(
                x0, y0, z0, a0,
                x1, y1, z1, a1,
                x2, y2, z2, a2,
                scales,
                shader,
                frameBuffer, zBuffer, config);
        }
    }
}