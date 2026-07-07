#pragma once

#include "Core/Platform.hpp"
#include "Math/Algebra.hpp"
#include "Rendering/Lighting/Lighting.hpp"
#include "Rendering/Pipeline/Rasterizer/Common.hpp"

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

namespace pip3D
{

    enum ShadingMode : uint8_t
    {
        SHADING_FLAT = 0,
        SHADING_GOURAUD = 1
    };

    class Shading
    {
    public:
        static constexpr float DIFFUSE_STRENGTH = 1.20f;
        static constexpr float SPECULAR_STRENGTH = 0.35f;
        static constexpr float RIM_STRENGTH = 0.25f;
        static constexpr float DIFFUSE_WRAP = 0.18f;

        static constexpr float INV_DIFFUSE_WRAP = 1.0f / (1.0f + DIFFUSE_WRAP);
        static constexpr float HEMI_SCALE = 0.25f;
        static constexpr float AMBIENT_BASE = 0.03f;
        static constexpr float SATURATION = 1.30f;
        static constexpr float SATURATION_LUM_FACTOR = SATURATION - 1.0f;
        static constexpr float DIFFUSE_SCALE = DIFFUSE_STRENGTH * INV_DIFFUSE_WRAP;

        __attribute__((always_inline, hot)) static inline void IRAM_ATTR calculateLighting(
            const Vector3 &fragPos,
            const Vector3 &normal,
            const Vector3 &viewDir,
            const Light *lights,
            int lightCount,
            float baseR, float baseG, float baseB,
            float &outR, float &outG, float &outB,
            bool skipSpecularAndRim = false)
        {
            const float nx = normal.x;
            const float ny = normal.y;
            const float nz = normal.z;
            const float vdx = viewDir.x;
            const float vdy = viewDir.y;
            const float vdz = viewDir.z;

            const float ambientScale = Rasterizer::g_ambientScale;
            const float hemi = ny * HEMI_SCALE + AMBIENT_BASE;
            const float ambientTerm = ((hemi > 0.0f) ? (AMBIENT_BASE + hemi) : 0.0f) * ambientScale;

            outR = baseR * ambientTerm;
            outG = baseG * ambientTerm;
            outB = baseB * ambientTerm;

            float rimAmount = 0.0f;
            if (!skipSpecularAndRim)
            {
                float NdotV = nx * vdx + ny * vdy + nz * vdz;
                if (NdotV < 0.0f)
                    NdotV = 0.0f;
                float rim = 1.0f - NdotV;
                rimAmount = rim * rim * RIM_STRENGTH;
            }

            const float fpx = fragPos.x;
            const float fpy = fragPos.y;
            const float fpz = fragPos.z;

            for (int i = 0; i < lightCount; ++i)
            {
                const Light &light = lights[i];
                float ldx, ldy, ldz;
                float attenuation = 1.0f;

                if (likely(light.type == LIGHT_DIRECTIONAL))
                {
                    ldx = -light.direction.x;
                    ldy = -light.direction.y;
                    ldz = -light.direction.z;
                }
                else if (light.type == LIGHT_POINT)
                {
                    ldx = light.position.x - fpx;
                    ldy = light.position.y - fpy;
                    ldz = light.position.z - fpz;

                    const float distSq = ldx * ldx +
                                         ldy * ldy +
                                         ldz * ldz;

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
                            attenuation = FastMath::fastReciprocal(1.0f + distSq * light.invRangeSq);
                        }
                    }
                    else if (distSq > 1e-8f)
                    {
                        const float invDist = FastMath::fastInvSqrt(distSq);
                        ldx *= invDist;
                        ldy *= invDist;
                        ldz *= invDist;
                    }
                }
                else
                {
                    continue;
                }

                const float NdotL = nx * ldx + ny * ldy + nz * ldz;

                float diffuse = (NdotL + DIFFUSE_WRAP) * DIFFUSE_SCALE;
                if (diffuse < 0.0f)
                    diffuse = 0.0f;

                const float lightIntensityAtten = light.intensity * attenuation;
                diffuse *= lightIntensityAtten;

                float specular = 0.0f;
                if (!skipSpecularAndRim && NdotL > 0.0f && SPECULAR_STRENGTH > 0.0f)
                {
                    float hx = ldx + vdx;
                    float hy = ldy + vdy;
                    float hz = ldz + vdz;

                    const float hLenSq = hx * hx + hy * hy + hz * hz;
                    if (hLenSq > 1e-8f)
                    {
                        const float invHLen = FastMath::fastInvSqrt(hLenSq);
                        hx *= invHLen;
                        hy *= invHLen;
                        hz *= invHLen;

                        const float NdotH = nx * hx + ny * hy + nz * hz;
                        if (NdotH > 0.0f)
                        {
                            float spec = NdotH * NdotH;
                            spec *= spec;
                            spec *= spec;
                            spec *= spec;
                            specular = spec * SPECULAR_STRENGTH * lightIntensityAtten;
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

            outR += baseR * rimAmount * ambientScale;
            outG += baseG * rimAmount * ambientScale;
            outB += baseB * rimAmount * ambientScale;

            const float exposure = 1.15f * Rasterizer::g_exposureScale;
            outR *= exposure * FastMath::fastReciprocal(0.25f + outR);
            outG *= exposure * FastMath::fastReciprocal(0.25f + outG);
            outB *= exposure * FastMath::fastReciprocal(0.25f + outB);

            const float lum = outR * 0.299f + outG * 0.587f + outB * 0.114f;
            outR += (outR - lum) * SATURATION_LUM_FACTOR;
            outG += (outG - lum) * SATURATION_LUM_FACTOR;
            outB += (outB - lum) * SATURATION_LUM_FACTOR;

            if (outR < 0.0f)
                outR = 0.0f;
            else if (outR > 1.0f)
                outR = 1.0f;
            if (outG < 0.0f)
                outG = 0.0f;
            else if (outG > 1.0f)
                outG = 1.0f;
            if (outB < 0.0f)
                outB = 0.0f;
            else if (outB > 1.0f)
                outB = 1.0f;
        }
    };

}
