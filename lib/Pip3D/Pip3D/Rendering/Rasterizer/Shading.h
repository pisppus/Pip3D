#pragma once

#include "Core/Core.h"
#include "Math/Math.h"
#include "Rendering/Lighting/Lighting.h"

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

namespace pip3D
{

    class Shading
    {
    public:
        static constexpr float AMBIENT_LIGHT = 0.06f;
        static constexpr float DIFFUSE_STRENGTH = 1.20f;
        static constexpr float SPECULAR_STRENGTH = 0.35f;
        static constexpr float RIM_STRENGTH = 0.25f;
        static constexpr float HDR_EXPOSURE = 1.8f;
        static constexpr float DIFFUSE_WRAP = 0.18f;
        static constexpr float CONTRAST = 1.35f;
        static constexpr float SATURATION = 1.30f;

        static constexpr float INV_DIFFUSE_WRAP = 0.84745762711864406780f;
        static constexpr float HEMI_SCALE = 0.25f;
        static constexpr float AMBIENT_BASE = 0.03f;
        static constexpr float CONTRAST_OFFSET = -0.175f;
        static constexpr float INV_HDR_EXPOSURE = 0.55555555555555555556f;
        static constexpr float SATURATION_LUM_FACTOR = SATURATION - 1.0f;
        static constexpr float INV_255 = 0.00392156862745098039f;
        static constexpr float BAYER_MATRIX_4X4[4][4] = {
            {0.0f / 16.0f, 8.0f / 16.0f, 2.0f / 16.0f, 10.0f / 16.0f},
            {12.0f / 16.0f, 4.0f / 16.0f, 14.0f / 16.0f, 6.0f / 16.0f},
            {3.0f / 16.0f, 11.0f / 16.0f, 1.0f / 16.0f, 9.0f / 16.0f},
            {15.0f / 16.0f, 7.0f / 16.0f, 13.0f / 16.0f, 5.0f / 16.0f}};

        static float GAMMA_LUT[256];
        static bool lutInitialized;

        static void initLUT()
        {
            if (lutInitialized)
                return;

            for (int i = 0; i < 256; ++i)
            {
                GAMMA_LUT[i] = sqrtf(static_cast<float>(i) * INV_255);
            }

            lutInitialized = true;
        }

        __attribute__((always_inline)) static inline float fastSqrt01(float x)
        {
            if (x <= 0.0f)
                return 0.0f;
            if (x >= 1.0f)
                return 1.0f;

            const int idx = static_cast<int>(x * 255.0f);
            return GAMMA_LUT[idx];
        }

        __attribute__((always_inline, hot)) static inline void IRAM_ATTR calculateLighting(
            const Vector3 &fragPos,
            const Vector3 &normal,
            const Vector3 &viewDir,
            const Light *lights,
            int lightCount,
            float baseR, float baseG, float baseB,
            float &outR, float &outG, float &outB)
        {
            const float hemi = normal.y * HEMI_SCALE + AMBIENT_BASE;
            const float ambientTerm = AMBIENT_BASE + hemi;

            outR = baseR * ambientTerm;
            outG = baseG * ambientTerm;
            outB = baseB * ambientTerm;

            float NdotV = normal.dot(viewDir);
            NdotV = (NdotV < 0.0f) ? 0.0f : NdotV;
            float rim = 1.0f - NdotV;
            rim *= rim;
            const float rimAmount = rim * RIM_STRENGTH;

            for (int i = 0; i < lightCount; ++i)
            {
                const Light &light = lights[i];
                Vector3 lightDir;
                float attenuation = 1.0f;

                if (likely(light.type == LIGHT_DIRECTIONAL))
                {
                    lightDir.x = -light.direction.x;
                    lightDir.y = -light.direction.y;
                    lightDir.z = -light.direction.z;
                }
                else if (light.type == LIGHT_POINT)
                {
                    lightDir.x = light.position.x - fragPos.x;
                    lightDir.y = light.position.y - fragPos.y;
                    lightDir.z = light.position.z - fragPos.z;

                    if (light.range > 0.0f)
                    {
                        const float distSq = lightDir.x * lightDir.x +
                                             lightDir.y * lightDir.y +
                                             lightDir.z * lightDir.z;

                        if (distSq > light.rangeSq)
                            continue;

                        if (distSq > 1e-8f)
                        {
                            const float invDist = FastMath::fastInvSqrt(distSq);
                            lightDir.x *= invDist;
                            lightDir.y *= invDist;
                            lightDir.z *= invDist;
                            attenuation = 1.0f / (1.0f + distSq * light.invRangeSq);
                        }
                    }
                    else
                    {
                        lightDir.normalize();
                    }
                }
                else
                {
                    continue;
                }

                const float NdotL = normal.x * lightDir.x +
                                    normal.y * lightDir.y +
                                    normal.z * lightDir.z;

                float wrappedNdotL = (NdotL + DIFFUSE_WRAP) * INV_DIFFUSE_WRAP;
                wrappedNdotL = (wrappedNdotL < 0.0f) ? 0.0f : wrappedNdotL;

                const float lightIntensityAtten = light.intensity * attenuation;
                const float diffuse = wrappedNdotL * DIFFUSE_STRENGTH * lightIntensityAtten;

                float specular = 0.0f;
                if (NdotL > 0.0f && SPECULAR_STRENGTH > 0.0f)
                {
                    float hx = lightDir.x + viewDir.x;
                    float hy = lightDir.y + viewDir.y;
                    float hz = lightDir.z + viewDir.z;

                    const float hLenSq = hx * hx + hy * hy + hz * hz;
                    if (hLenSq > 1e-8f)
                    {
                        const float invHLen = FastMath::fastInvSqrt(hLenSq);
                        hx *= invHLen;
                        hy *= invHLen;
                        hz *= invHLen;

                        const float NdotH = normal.x * hx + normal.y * hy + normal.z * hz;
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

                float lightR;
                float lightG;
                float lightB;
                light.getCachedRGB(lightR, lightG, lightB);

                const float diffuseR = baseR * diffuse;
                const float diffuseG = baseG * diffuse;
                const float diffuseB = baseB * diffuse;

                outR += (diffuseR + specular) * lightR;
                outG += (diffuseG + specular) * lightG;
                outB += (diffuseB + specular) * lightB;
            }

            outR += baseR * rimAmount;
            outG += baseG * rimAmount;
            outB += baseB * rimAmount;

            outR = outR / (INV_HDR_EXPOSURE + outR);
            outG = outG / (INV_HDR_EXPOSURE + outG);
            outB = outB / (INV_HDR_EXPOSURE + outB);

            outR = fastSqrt01(outR);
            outG = fastSqrt01(outG);
            outB = fastSqrt01(outB);

            const float lum = outR * 0.299f + outG * 0.587f + outB * 0.114f;
            outR += (outR - lum) * SATURATION_LUM_FACTOR;
            outG += (outG - lum) * SATURATION_LUM_FACTOR;
            outB += (outB - lum) * SATURATION_LUM_FACTOR;

            outR = outR * CONTRAST + CONTRAST_OFFSET;
            outG = outG * CONTRAST + CONTRAST_OFFSET;
            outB = outB * CONTRAST + CONTRAST_OFFSET;

            outR = clamp(outR, 0.0f, 1.0f);
            outG = clamp(outG, 0.0f, 1.0f);
            outB = clamp(outB, 0.0f, 1.0f);
        }

        __attribute__((always_inline)) static inline uint16_t quantizeColor(float r, float g, float b)
        {
            const int ir = static_cast<int>(r * 31.0f + 0.5f);
            const int ig = static_cast<int>(g * 63.0f + 0.5f);
            const int ib = static_cast<int>(b * 31.0f + 0.5f);

            const uint16_t rc = (ir > 31) ? 31 : ((ir < 0) ? 0 : ir);
            const uint16_t gc = (ig > 63) ? 63 : ((ig < 0) ? 0 : ig);
            const uint16_t bc = (ib > 31) ? 31 : ((ib < 0) ? 0 : ib);

            return static_cast<uint16_t>((rc << 11) | (gc << 5) | bc);
        }

        __attribute__((always_inline, hot)) static inline uint16_t IRAM_ATTR applyDithering(float r, float g, float b, int16_t x, int16_t y)
        {
            r *= 31.0f;
            g *= 63.0f;
            b *= 31.0f;

            const float bayerValue = BAYER_MATRIX_4X4[y & 3][x & 3];
            const float ditherRB = bayerValue * 0.5f;
            const float ditherG = bayerValue * 0.25f;

            const int ir = static_cast<int>(r + ditherRB);
            const int ig = static_cast<int>(g + ditherG);
            const int ib = static_cast<int>(b + ditherRB);

            const uint16_t rc = (ir > 31) ? 31 : ((ir < 0) ? 0 : ir);
            const uint16_t gc = (ig > 63) ? 63 : ((ig < 0) ? 0 : ig);
            const uint16_t bc = (ib > 31) ? 31 : ((ib < 0) ? 0 : ib);

            return static_cast<uint16_t>((rc << 11) | (gc << 5) | bc);
        }
    };

}
