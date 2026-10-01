#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "Core/Platform.hpp"
#include "Core/Color.hpp"
#include "Core/Diagnostics.hpp"
#include "Math/Algebra.hpp"
#include "Rendering/Buffers/ZBuffer.hpp"
#include "Rendering/Environment/Sky.hpp"

namespace pip3D
{
    enum class FlareProfile : uint8_t
    {
        Orb,
        RingSoft,
        RingTight,
    };

    struct LensFlareElement
    {
        float axisPos;
        float radius;
        float alpha;
        FlareProfile profile;
        bool avoidSun;
        uint16_t color;
    };

    class LensFlare
    {
    public:
        LensFlare()
        {
            applySunPreset();
        }

        void setEnabled(bool on) { enabled = on; }
        bool isEnabled() const { return enabled; }

        void setIntensity(float value) { intensity = clamp(value, 0.0f, 2.0f); }
        float getIntensity() const { return intensity; }

        void setElements(const LensFlareElement *elems, size_t count)
        {
            if (!elems)
                count = 0;
            if (count > kMaxElements)
                count = kMaxElements;
            for (size_t i = 0; i < count; ++i)
                elements[i] = elems[i];
            elementCount = count;
        }

        size_t getElementCount() const { return elementCount; }
        const LensFlareElement *getElements() const { return elements; }

        void applySunPreset()
        {
            static const LensFlareElement kPreset[] = {
                {0.00f, 0.100f, 0.35f, FlareProfile::Orb, false, Color(255, 240, 218).rgb565},
                {1.00f, 0.105f, 0.30f, FlareProfile::RingSoft, true, Color(255, 255, 255).rgb565},
                {1.35f, 0.055f, 0.26f, FlareProfile::RingTight, true, Color(250, 252, 255).rgb565},
                {1.70f, 0.038f, 0.30f, FlareProfile::Orb, true, Color(255, 255, 255).rgb565},
                {2.10f, 0.080f, 0.22f, FlareProfile::RingSoft, true, Color(240, 247, 255).rgb565},
                {2.45f, 0.030f, 0.28f, FlareProfile::Orb, true, Color(226, 237, 255).rgb565},
                {2.85f, 0.045f, 0.16f, FlareProfile::RingTight, true, Color(255, 255, 255).rgb565},
            };
            setElements(kPreset, sizeof(kPreset) / sizeof(kPreset[0]));
            intensity = 1.0f;
        }

        float getVisibility() const { return visibility; }

        void notifySunLost()
        {
            const uint64_t nowUs = getSystemMicros();
            const float dt = elapsedSeconds(nowUs);
            lastUpdateUs = nowUs;
            stepVisibility(0.0f, dt);
        }

        void render(uint16_t *fb, ZBuffer &zBuf,
                    const Vector3 &sunScreen, float sunDiskRadius,
                    int bandTop, int bandBottom)
        {
            const uint64_t nowUs = getSystemMicros();
            const float dt = elapsedSeconds(nowUs);
            lastUpdateUs = nowUs;

            float target = edgeFade(sunScreen.x, sunScreen.y, sunDiskRadius);

            if (target > 0.0f &&
                sunScreen.y >= static_cast<float>(bandTop) &&
                sunScreen.y < static_cast<float>(bandBottom))
            {
                const float cov = sampleDiskCoverage(
                    zBuf, sunScreen, sunDiskRadius, bandTop, bandBottom);
                if (cov >= 0.0f)
                    diskCoverage = cov;
            }
            target *= diskCoverage;

            if (!enabled || intensity <= 0.0f)
                target = 0.0f;

            stepVisibility(target, dt);

            if (visibility <= 0.0f)
                return;

            const float cx = sunScreen.x;
            const float cy = sunScreen.y;
            const float axisX = static_cast<float>(SCREEN_WIDTH) * 0.5f - cx;
            const float axisY = static_cast<float>(SCREEN_HEIGHT) * 0.5f - cy;
            const float minDim = static_cast<float>(
                SCREEN_WIDTH < SCREEN_HEIGHT ? SCREEN_WIDTH : SCREEN_HEIGHT);
            const float master = clamp(visibility * intensity, 0.0f, 1.0f);
            const float axisLen = sqrtf(axisX * axisX + axisY * axisY);

            const float discR = sunDiskRadius * SunDisc::kRadiusScale;

            const float chainFade =
                clamp(0.5f + axisLen / (0.45f * minDim), 0.5f, 1.0f);

            for (size_t i = 0; i < elementCount; ++i)
            {
                const LensFlareElement &e = elements[i];
                const float ex = cx + axisX * e.axisPos;
                const float ey = cy + axisY * e.axisPos;
                const float er = e.radius * minDim;
                if (er < 2.0f)
                    continue;

                float a = e.alpha * master * chainFade;

                if (e.avoidSun)
                {

                    const float dxs = ex - cx;
                    const float dys = ey - cy;
                    const float dist = sqrtf(dxs * dxs + dys * dys);
                    const float inner = discR * 0.85f + er;
                    const float outer = discR * 2.0f + er;
                    a *= clamp((dist - inner) / (outer - inner), 0.0f, 1.0f);
                }

                const uint8_t aByte = static_cast<uint8_t>(clamp(a, 0.0f, 1.0f) * 255.0f + 0.5f);
                if (aByte == 0)
                    continue;

                SunDisc::drawRadialProfile(fb, ex, ey, er,
                                           kProfiles[static_cast<int>(e.profile)],
                                           e.color, aByte, bandTop, bandBottom);
            }
        }

    private:
        static constexpr size_t kMaxElements = 12;

        static constexpr float kFadeInPerSec = 7.0f;
        static constexpr float kFadeOutPerSec = 26.0f;

        static constexpr uint8_t kProfileOrb[SunDisc::kLutSize] = {
            255, 251, 247, 243, 239, 235, 232, 228, 224, 220, 217, 213,
            209, 206, 202, 199, 195, 192, 188, 185, 182, 178, 175, 172,
            168, 165, 162, 159, 156, 153, 149, 146, 143, 140, 138, 135,
            132, 129, 126, 123, 121, 118, 115, 112, 110, 107, 105, 102,
            100, 97, 95, 92, 90, 88, 85, 83, 81, 78, 76, 74,
            72, 70, 68, 66, 64, 62, 60, 58, 56, 54, 52, 51,
            49, 47, 45, 44, 42, 40, 39, 37, 36, 34, 33, 32,
            30, 29, 27, 26, 25, 24, 22, 21, 20, 19, 18, 17,
            16, 15, 14, 13, 12, 11, 11, 10, 9, 8, 8, 7,
            6, 6, 5, 4, 4, 4, 3, 3, 2, 2, 2, 1,
            1, 1, 1, 0, 0, 0, 0, 0, 0};

        static constexpr uint8_t kProfileRingSoft[SunDisc::kLutSize] = {
            0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 2,
            2, 3, 4, 5, 6, 8, 9, 11, 14, 16, 19, 22,
            26, 30, 34, 39, 44, 49, 55, 61, 67, 73, 80, 87,
            94, 102, 109, 117, 124, 132, 140, 148, 155, 163, 171, 178,
            185, 192, 199, 205, 211, 217, 222, 227, 232, 236, 240, 243,
            246, 249, 251, 253, 254, 255, 255, 255, 254, 254, 252, 251,
            249, 236, 224, 212, 200, 189, 177, 166, 156, 146, 136, 127,
            118, 109, 101, 93, 86, 79, 72, 66, 60, 55, 50, 45,
            41, 37, 33, 29, 26, 23, 21, 18, 16, 14, 12, 11,
            9, 8, 7, 6, 5, 4, 3, 3, 2, 2, 1, 1,
            1, 1, 0, 0, 0, 0, 0, 0, 0};

        static constexpr uint8_t kProfileRingTight[SunDisc::kLutSize] = {
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 1, 1, 1, 2, 2, 3, 4, 5,
            7, 8, 10, 13, 15, 19, 22, 27, 31, 37, 43, 49,
            56, 64, 72, 81, 90, 99, 109, 119, 130, 140, 151, 162,
            172, 182, 192, 201, 210, 218, 226, 232, 238, 243, 248, 251,
            253, 244, 234, 224, 213, 202, 191, 179, 168, 156, 145, 134,
            124, 113, 104, 94, 85, 77, 69, 62, 55, 49, 43, 38,
            34, 29, 25, 22, 19, 16, 14, 12, 10, 8, 7, 6,
            5, 4, 3, 3, 2, 2, 1, 1, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0};

        static constexpr const uint8_t *kProfiles[3] = {
            kProfileOrb, kProfileRingSoft, kProfileRingTight};

        LensFlareElement elements[kMaxElements] = {};
        size_t elementCount = 0;
        float intensity = 1.0f;
        bool enabled = true;
        float visibility = 0.0f;
        float diskCoverage = 1.0f;
        uint64_t lastUpdateUs = 0;

        float elapsedSeconds(uint64_t nowUs)
        {
            if (lastUpdateUs == 0)
                return 1.0f / 60.0f;
            const float dt = static_cast<float>(nowUs - lastUpdateUs) * 1e-6f;
            return clamp(dt, 0.0f, 0.1f);
        }

        void stepVisibility(float target, float dt)
        {
            const float rate = (target < visibility) ? kFadeOutPerSec : kFadeInPerSec;
            visibility += (target - visibility) * (1.0f - expf(-dt * rate));
            if (target <= 0.0f && visibility < 0.015f)
                visibility = 0.0f;
        }

        static float edgeFade(float x, float y, float sunDiskRadius)
        {
            const float dxE = static_cast<float>(SCREEN_WIDTH - 1) - x;
            const float dyE = static_cast<float>(SCREEN_HEIGHT - 1) - y;
            float d = x < dxE ? x : dxE;
            const float dy2 = y < dyE ? y : dyE;
            if (dy2 < d)
                d = dy2;

            const float fadeRange = sunDiskRadius * 1.5f + 8.0f;
            return clamp(1.0f + d / fadeRange, 0.0f, 1.0f);
        }

        static constexpr int kTapCount = 9;

        static float sampleDiskCoverage(ZBuffer &zBuf, const Vector3 &sunScreen,
                                        float sunDiskRadius,
                                        int bandTop, int bandBottom)
        {
            if (sunDiskRadius < 1.0f)
                sunDiskRadius = 1.0f;

            const int cx = static_cast<int>(sunScreen.x + 0.5f);
            const int cy = static_cast<int>(sunScreen.y + 0.5f);
            const uint16_t sunDepth = static_cast<uint16_t>(sunScreen.z);

            const uint16_t depthEps =
                static_cast<uint16_t>(static_cast<float>(sunDepth) * 0.03125f) + 1;

            static constexpr float kTapK = 0.70710678f;
            const float rx = sunDiskRadius * 0.55f;
            const float rd = sunDiskRadius * 0.85f;
            const float offsets[kTapCount][2] = {
                {0.0f, 0.0f},
                {rx, 0.0f},
                {-rx, 0.0f},
                {0.0f, rx},
                {0.0f, -rx},
                {rd * kTapK, rd * kTapK},
                {-rd * kTapK, rd * kTapK},
                {rd * kTapK, -rd * kTapK},
                {-rd * kTapK, -rd * kTapK},
            };

            const uint16_t *z = zBuf.data();
            int checked = 0;
            int occluded = 0;
            for (int i = 0; i < kTapCount; ++i)
            {
                const int sx = cx + static_cast<int>(offsets[i][0] + 0.5f);
                const int sy = cy + static_cast<int>(offsets[i][1] + 0.5f);
                if (sx < 0 || sx >= SCREEN_WIDTH)
                    continue;
                const int ly = sy - bandTop;
                if (ly < 0 || ly >= SCREEN_BAND_HEIGHT)
                    continue;
                const uint16_t d = z[static_cast<size_t>(ly) * SCREEN_WIDTH + sx] & Z_DEPTH_MASK;
                ++checked;
                if (d != Z_CLEAR_VALUE && d > sunDepth + depthEps)
                    ++occluded;
            }

            if (checked == 0)
                return -1.0f;
            return 1.0f - static_cast<float>(occluded) / static_cast<float>(checked);
        }
    };
}
