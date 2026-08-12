#pragma once

#include <cstdint>

#include "Core/Platform.hpp"
#include "Math/Algebra.hpp"
#include "Rendering/Lighting/Lighting.hpp"
#include "Rendering/Lighting/Fog.hpp"

namespace pip3D
{

    enum ShadingMode : uint8_t
    {
        SHADING_FLAT = 0,
        SHADING_GOURAUD = 1,
        SHADING_PHONG = 2
    };

    struct ShadingParams
    {
        float diffuseStrength;
        float diffuseWrap;
        float specularStrength;
        float specularExponent;
        float rimStrength;
        float hemiScale;
        float ambientBase;
        float exposureCoeff;
        float toneKnee;
        float saturation;

        static constexpr ShadingParams flat() noexcept
        {
            return {1.05f, 0.20f,
                    0.00f, 64.0f,
                    0.00f,
                    0.20f, 0.04f,
                    1.00f, 0.35f,
                    1.10f};
        }

        static constexpr ShadingParams gouraud() noexcept
        {
            return {1.10f, 0.20f,
                    0.25f, 32.0f,
                    0.10f,
                    0.22f, 0.04f,
                    1.05f, 0.30f,
                    1.18f};
        }

        static constexpr ShadingParams phong() noexcept
        {
            return {1.10f, 0.18f,
                    0.45f, 48.0f,
                    0.18f,
                    0.25f, 0.04f,
                    1.10f, 0.28f,
                    1.22f};
        }

        static constexpr ShadingParams punchy() noexcept
        {
            return {1.30f, 0.10f,
                    0.60f, 96.0f,
                    0.12f,
                    0.10f, 0.02f,
                    1.20f, 0.20f,
                    1.30f};
        }

        static constexpr ShadingParams balanced() noexcept
        {
            return {1.10f, 0.20f,
                    0.35f, 48.0f,
                    0.12f,
                    0.22f, 0.04f,
                    1.05f, 0.30f,
                    1.18f};
        }
    };

    class Shading
    {
    public:
        static constexpr float SPEC_NDOTV_RAMP = 5.0f;
        static constexpr float DIFFUSE_SMOOTH_EPS = 0.10f;
        static constexpr float HEMI_SMOOTH_EPS = 0.05f;
        static constexpr float HALF_VECTOR_MIN_LEN_SQ = 0.04f;

        static constexpr int SPEC_LUT_SIZE = 256;

        alignas(16) inline static float specLUT[SPEC_LUT_SIZE] PIP3D_FAST_DATA;

        [[nodiscard]] static ShadingParams &params() noexcept { return g_params; }
        [[nodiscard]] static const ShadingParams &getParams() noexcept { return g_params; }

        __attribute__((cold)) static void setParams(const ShadingParams &p) noexcept
        {
            const bool needRebuild =
                !g_lutReady ||
                p.specularExponent != g_params.specularExponent ||
                p.specularStrength != g_params.specularStrength;
            g_params = p;
            if (needRebuild)
                rebuildSpecLUT();
        }

        __attribute__((cold)) static void applyPreset(const ShadingParams &preset) noexcept
        {
            setParams(preset);
        }

        __attribute__((cold)) static void rebuildSpecLUT() noexcept
        {
            const float exponent = g_params.specularExponent;
            const float strength = g_params.specularStrength;

            const float ecFactor = (exponent + 2.0f) *
                                   FastMath::fastReciprocal(2.0f * kPi);
            const float effective = strength * ecFactor;
            for (int i = 0; i < SPEC_LUT_SIZE; ++i)
            {
                const float nh = static_cast<float>(i) * (1.0f / 255.0f);
                specLUT[i] = powf(nh, exponent) * effective;
            }
            g_lutReady = true;
        }

        PIP3D_FORCE_INLINE static void IRAM_ATTR ensureSpecLUT() noexcept
        {
            if (likely(g_lutReady))
                return;
            rebuildSpecLUT();
        }

        __attribute__((noinline, hot)) static void IRAM_ATTR
        calculateLambert(const Vector3 &normal,
                         const Light *lights, int lightCount,
                         float baseR, float baseG, float baseB,
                         float &outR, float &outG, float &outB) noexcept
        {
            const float nx = normal.x;
            const float ny = normal.y;
            const float nz = normal.z;

            const ShadingParams &p = g_params;
            const float ambientScale = Rasterizer::g_ambientScale;

            const float hemiLinear = ny * p.hemiScale + 2.0f * p.ambientBase;
            const float ambientTerm =
                smoothPositive(hemiLinear, HEMI_SMOOTH_EPS) * ambientScale;

            const float invWrap = FastMath::fastReciprocal(1.0f + p.diffuseWrap);
            const float diffScale = p.diffuseStrength * invWrap;

            float diffR = ambientTerm;
            float diffG = ambientTerm;
            float diffB = ambientTerm;

            for (int i = 0; i < lightCount; ++i)
            {
                const Light &light = lights[i];
                if (likely(light.type == LIGHT_DIRECTIONAL))
                {

                    const float NdotL = nx * (-light.direction.x) +
                                        ny * (-light.direction.y) +
                                        nz * (-light.direction.z);

                    const float diff = smoothPositive(NdotL + p.diffuseWrap,
                                                      DIFFUSE_SMOOTH_EPS) *
                                       diffScale;
                    const float atten = light.intensity;
                    diffR += diff * atten * light.cachedR;
                    diffG += diff * atten * light.cachedG;
                    diffB += diff * atten * light.cachedB;
                }
            }

            outR = baseR * diffR;
            outG = baseG * diffG;
            outB = baseB * diffB;

            toneMap3(outR, outG, outB);
        }

        __attribute__((always_inline, hot)) static inline void IRAM_ATTR
        calculateLighting(const Vector3 &fragPos,
                          const Vector3 &normal,
                          const Vector3 &viewDir,
                          const Light *lights, int lightCount,
                          float baseR, float baseG, float baseB,
                          float &outR, float &outG, float &outB) noexcept
        {
            const ShadingParams &p = g_params;

            const bool enableSpec = (p.specularStrength > 0.0f);
            const bool enableRim = (p.rimStrength > 0.0f);

            if (enableSpec)
                ensureSpecLUT();

            const float nx = normal.x;
            const float ny = normal.y;
            const float nz = normal.z;
            const float vdx = viewDir.x;
            const float vdy = viewDir.y;
            const float vdz = viewDir.z;

            const float ambientScale = Rasterizer::g_ambientScale;

            const float hemiLinear = ny * p.hemiScale + 2.0f * p.ambientBase;
            const float ambientTerm =
                smoothPositive(hemiLinear, HEMI_SMOOTH_EPS) * ambientScale;

            outR = baseR * ambientTerm;
            outG = baseG * ambientTerm;
            outB = baseB * ambientTerm;

            float rimAmount = 0.0f;
            float specNVFalloff = 0.0f;
            if (enableRim || enableSpec)
            {
                const float NdotV_raw = nx * vdx + ny * vdy + nz * vdz;
                if (enableRim)
                {
                    const float NdotV = NdotV_raw < 0.0f ? 0.0f : NdotV_raw;
                    const float rim = 1.0f - NdotV;
                    rimAmount = rim * rim * p.rimStrength;
                }
                if (enableSpec)
                    specNVFalloff = specFalloffByNdotV(NdotV_raw);
            }

            const float invWrap = FastMath::fastReciprocal(1.0f + p.diffuseWrap);
            const float diffScale = p.diffuseStrength * invWrap;

            const float fpx = fragPos.x;
            const float fpy = fragPos.y;
            const float fpz = fragPos.z;

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
                    ldx = light.position.x - fpx;
                    ldy = light.position.y - fpy;
                    ldz = light.position.z - fpz;

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
                                         FastMath::fastReciprocal(
                                             1.0f + distSq * light.invRangeSq);
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
                if (NdotL <= -p.diffuseWrap)
                    continue;

                float diffuse = smoothPositive(NdotL + p.diffuseWrap,
                                               DIFFUSE_SMOOTH_EPS) *
                                diffScale;
                diffuse *= lightAtten;

                float specular = 0.0f;
                if (enableSpec && NdotL > 0.0f && specNVFalloff > 0.0f)
                {
                    const float hx = ldx + vdx;
                    const float hy = ldy + vdy;
                    const float hz = ldz + vdz;
                    const float hLenSq = hx * hx + hy * hy + hz * hz;
                    if (hLenSq > HALF_VECTOR_MIN_LEN_SQ)
                    {
                        const float invHLen = FastMath::fastInvSqrt(hLenSq);
                        const float NdotH = (nx * hx + ny * hy + nz * hz) * invHLen;
                        if (NdotH > 0.0f)
                        {

                            const uint8_t nhIdx = static_cast<uint8_t>(
                                fminf(NdotH, 1.0f) * 255.0f);
                            specular = specLUT[nhIdx] * lightAtten * specNVFalloff;
                        }
                    }
                }

                const float lightR = light.cachedR;
                const float lightG = light.cachedG;
                const float lightB = light.cachedB;

                outR += (baseR * diffuse + specular) * lightR;
                outG += (baseG * diffuse + specular) * lightG;
                outB += (baseB * diffuse + specular) * lightB;
            }

            if (enableRim)
            {
                outR += baseR * rimAmount * ambientScale;
                outG += baseG * rimAmount * ambientScale;
                outB += baseB * rimAmount * ambientScale;
            }

            toneMap3(outR, outG, outB);
        }

        __attribute__((noinline, hot)) static void IRAM_ATTR
        calculateFaceLighting(const Vector3 &v0, const Vector3 &v1, const Vector3 &v2,
                              const Vector3 &camPos,
                              const Light *lights, int lightCount,
                              float baseR, float baseG, float baseB,
                              float &outR, float &outG, float &outB) noexcept
        {

            const float e1x = v1.x - v0.x, e1y = v1.y - v0.y, e1z = v1.z - v0.z;
            const float e2x = v2.x - v0.x, e2y = v2.y - v0.y, e2z = v2.z - v0.z;
            const float nx = e1y * e2z - e1z * e2y;
            const float ny = e1z * e2x - e1x * e2z;
            const float nz = e1x * e2y - e1y * e2x;

            const float nLenSq = nx * nx + ny * ny + nz * nz;
            const float nInvLen = (nLenSq > 1e-12f)
                                      ? FastMath::fastInvSqrt(nLenSq)
                                      : 0.0f;
            const Vector3 normal(nx * nInvLen, ny * nInvLen, nz * nInvLen);

            const float dx = camPos.x - v0.x;
            const float dy = camPos.y - v0.y;
            const float dz = camPos.z - v0.z;
            const float lenSq = dx * dx + dy * dy + dz * dz;
            const float invLen = (lenSq > 1e-8f) ? FastMath::fastInvSqrt(lenSq) : 0.0f;

            calculateLambert(normal, lights, lightCount,
                             baseR, baseG, baseB,
                             outR, outG, outB);

            const float dist = lenSq * invLen;
            applyFog(dist, outR, outG, outB, outR, outG, outB);
        }

        __attribute__((always_inline, hot)) static inline void IRAM_ATTR
        calculateVertexLightingGouraud(const Vector3 &vertexPos,
                                       const Vector3 &normal,
                                       const Vector3 &camPos,
                                       const Light *lights, int lightCount,
                                       float baseR, float baseG, float baseB,
                                       float &outR, float &outG, float &outB) noexcept
        {
            const float dx = camPos.x - vertexPos.x;
            const float dy = camPos.y - vertexPos.y;
            const float dz = camPos.z - vertexPos.z;
            const float distSq = dx * dx + dy * dy + dz * dz;
            const float invLen = (distSq > 1e-8f) ? FastMath::fastInvSqrt(distSq) : 0.0f;
            const Vector3 viewDir(dx * invLen, dy * invLen, dz * invLen);

            calculateLighting(vertexPos, normal, viewDir,
                              lights, lightCount,
                              baseR, baseG, baseB,
                              outR, outG, outB);

            if (Rasterizer::g_fogState.enabled)
            {
                const float dist = distSq * invLen;
                applyFog(dist, outR, outG, outB, outR, outG, outB);
            }
        }

        PIP3D_FORCE_INLINE static void IRAM_ATTR
        applyFog(float dist,
                 float inR, float inG, float inB,
                 float &outR, float &outG, float &outB) noexcept
        {
            const auto &fog = Rasterizer::g_fogState;
            if (!fog.enabled)
            {
                outR = inR;
                outG = inG;
                outB = inB;
                return;
            }

            float fogFactor = (dist - fog.worldNear) * fog.worldScale;
            fogFactor = clamp(fogFactor, 0.0f, 1.0f);
            const float invFog = 1.0f - fogFactor;

            const Rasterizer::FogColorF fc = Rasterizer::fogColorFloat();
            outR = inR * invFog + fc.r * fogFactor;
            outG = inG * invFog + fc.g * fogFactor;
            outB = inB * invFog + fc.b * fogFactor;
        }

        PIP3D_FORCE_INLINE static float IRAM_ATTR
        smoothPositive(float x, float eps) noexcept
        {
            if (x <= 0.0f)
                return 0.0f;
            if (x < eps)
            {
                const float u = x * FastMath::fastReciprocal(eps);
                return u * u * (2.0f - u) * eps;
            }
            return x;
        }

        PIP3D_FORCE_INLINE static float IRAM_ATTR
        smoothstep01(float t) noexcept
        {
            if (t <= 0.0f)
                return 0.0f;
            if (t >= 1.0f)
                return 1.0f;
            return t * t * (3.0f - 2.0f * t);
        }

        PIP3D_FORCE_INLINE static float IRAM_ATTR
        specFalloffByNdotV(float NdotV_raw) noexcept
        {
            return smoothstep01(NdotV_raw * SPEC_NDOTV_RAMP);
        }

        PIP3D_FORCE_INLINE static void IRAM_ATTR
        toneMap3(float &r, float &g, float &b) noexcept
        {
            const ShadingParams &p = g_params;
            const float exposure = p.exposureCoeff * Rasterizer::g_exposureScale;
            const float knee = p.toneKnee;
            const float satLumFactor = p.saturation - 1.0f;

            r = (r * exposure) * FastMath::fastReciprocal(knee + r);
            g = (g * exposure) * FastMath::fastReciprocal(knee + g);
            b = (b * exposure) * FastMath::fastReciprocal(knee + b);

            const float lum = r * 0.299f + g * 0.587f + b * 0.114f;
            r += (r - lum) * satLumFactor;
            g += (g - lum) * satLumFactor;
            b += (b - lum) * satLumFactor;

            r = clamp(r, 0.0f, 1.0f);
            g = clamp(g, 0.0f, 1.0f);
            b = clamp(b, 0.0f, 1.0f);
        }

    private:
        alignas(16) inline static ShadingParams g_params PIP3D_FAST_DATA =
            ShadingParams::balanced();

        inline static bool g_lutReady = false;
    };

}