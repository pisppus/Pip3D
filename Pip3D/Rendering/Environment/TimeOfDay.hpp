#pragma once

#include "Core/Platform.hpp"
#include "Rendering/Renderer.hpp"
#include "Sky.hpp"

namespace pip3D
{

    struct TimeOfDayConfig
    {
        float dayLengthSeconds;
        float startHour;
        bool autoAdvance;

        constexpr TimeOfDayConfig()
            : dayLengthSeconds(120.0f), startHour(10.0f), autoAdvance(true) {}
    };

    class TimeOfDayController
    {
    public:
        constexpr TimeOfDayController() = default;

        void init(Renderer *r, const TimeOfDayConfig &cfg)
        {
            renderer_ = r;
            dayLengthSeconds_ = cfg.dayLengthSeconds;
            autoAdvance_ = cfg.autoAdvance;
            lastSunPushedQ16_ = 0xFFFF;
            setTime(cfg.startHour);
        }

        void setTime(float hours)
        {
            float h = FastMath::fastFmod(hours, 24.0f);
            if (h < 0.0f)
                h += 24.0f;

            const uint16_t newTimeQ16 = static_cast<uint16_t>(h * (65536.0f / 24.0f) + 0.5f);
            if (newTimeQ16 != timeQ16_)
            {
                timeQ16_ = newTimeQ16;
                dirty_ = true;
                applyToRenderer();
            }
        }

        PIP3D_FORCE_INLINE float getTimeHours() const
        {
            return static_cast<float>(timeQ16_) * (24.0f / 65536.0f);
        }

        PIP3D_HOT void update(float deltaSeconds)
        {
            if (__builtin_expect(!renderer_, 0))
                return;

            if (autoAdvance_ && dayLengthSeconds_ > 0.0f && deltaSeconds > 0.0f)
            {

                const float frac = deltaSeconds / dayLengthSeconds_;
                const uint32_t step = static_cast<uint32_t>(frac * 65536.0f);
                if (step != 0)
                {

                    timeQ16_ = static_cast<uint16_t>(timeQ16_ + step);
                    dirty_ = true;
                }
            }

            if (__builtin_expect(dirty_, 1))
                applyToRenderer();
        }

    private:
        struct TimeKey
        {
            uint16_t hourQ8;
            Color skyTop;
            Color skyHorizon;
            Color skyGround;
            Color sunColor;
            Color cloudColor;
            float sunIntensity;
            float cloudAlpha;
            float ambientScale;
            float exposureScale;
        };

        static constexpr TimeKey kDeepNight{
            .hourQ8 = 0,
            .skyTop = Color::rgb(8, 12, 28),
            .skyHorizon = Color::rgb(20, 30, 55),
            .skyGround = Color::rgb(4, 6, 14),
            .sunColor = Color::rgb(120, 140, 190),
            .cloudColor = Color::rgb(55, 60, 85),
            .sunIntensity = 0.10f,
            .cloudAlpha = 0.45f,
            .ambientScale = 0.28f,
            .exposureScale = 0.58f};

        static constexpr TimeKey kPreDawn{
            .hourQ8 = 53,
            .skyTop = Color::rgb(18, 22, 48),
            .skyHorizon = Color::rgb(55, 40, 70),
            .skyGround = Color::rgb(6, 6, 14),
            .sunColor = Color::rgb(255, 110, 60),
            .cloudColor = Color::rgb(120, 90, 110),
            .sunIntensity = 0.25f,
            .cloudAlpha = 0.55f,
            .ambientScale = 0.42f,
            .exposureScale = 0.68f};

        static constexpr TimeKey kSunrise{
            .hourQ8 = 69,
            .skyTop = Color::rgb(60, 80, 130),
            .skyHorizon = Color::rgb(255, 150, 90),
            .skyGround = Color::rgb(50, 40, 45),
            .sunColor = Color::rgb(255, 170, 110),
            .cloudColor = Color::rgb(255, 210, 180),
            .sunIntensity = 0.60f,
            .cloudAlpha = 0.75f,
            .ambientScale = 0.70f,
            .exposureScale = 0.84f};

        static constexpr TimeKey kMorning{
            .hourQ8 = 85,
            .skyTop = Color::rgb(70, 130, 220),
            .skyHorizon = Color::rgb(180, 205, 235),
            .skyGround = Color::rgb(95, 90, 80),
            .sunColor = Color::rgb(255, 240, 220),
            .cloudColor = Color::rgb(250, 250, 252),
            .sunIntensity = 0.92f,
            .cloudAlpha = 0.95f,
            .ambientScale = 0.93f,
            .exposureScale = 0.97f};

        static constexpr TimeKey kNoon{
            .hourQ8 = 128,
            .skyTop = Color::rgb(70, 135, 225),
            .skyHorizon = Color::rgb(190, 210, 240),
            .skyGround = Color::rgb(100, 95, 85),
            .sunColor = Color::rgb(255, 250, 240),
            .cloudColor = Color::rgb(250, 250, 252),
            .sunIntensity = 1.00f,
            .cloudAlpha = 1.00f,
            .ambientScale = 1.00f,
            .exposureScale = 1.00f};

        static constexpr TimeKey kAfternoon{
            .hourQ8 = 181,
            .skyTop = Color::rgb(80, 120, 200),
            .skyHorizon = Color::rgb(200, 200, 215),
            .skyGround = Color::rgb(95, 85, 75),
            .sunColor = Color::rgb(255, 235, 200),
            .cloudColor = Color::rgb(252, 245, 235),
            .sunIntensity = 0.95f,
            .cloudAlpha = 0.95f,
            .ambientScale = 0.95f,
            .exposureScale = 0.98f};

        static constexpr TimeKey kSunset{
            .hourQ8 = 203,
            .skyTop = Color::rgb(90, 70, 130),
            .skyHorizon = Color::rgb(255, 130, 60),
            .skyGround = Color::rgb(55, 35, 40),
            .sunColor = Color::rgb(255, 150, 80),
            .cloudColor = Color::rgb(255, 190, 140),
            .sunIntensity = 0.55f,
            .cloudAlpha = 0.70f,
            .ambientScale = 0.68f,
            .exposureScale = 0.82f};

        static constexpr TimeKey kNight{
            .hourQ8 = 224,
            .skyTop = Color::rgb(12, 16, 38),
            .skyHorizon = Color::rgb(30, 35, 65),
            .skyGround = Color::rgb(5, 6, 15),
            .sunColor = Color::rgb(130, 145, 195),
            .cloudColor = Color::rgb(60, 65, 90),
            .sunIntensity = 0.14f,
            .cloudAlpha = 0.50f,
            .ambientScale = 0.36f,
            .exposureScale = 0.66f};

        static constexpr TimeKey kMidnightWrap = []()
        {
            TimeKey k = kDeepNight;
            k.hourQ8 = 256;
            return k;
        }();

        static constexpr TimeKey kTimeKeys[] = {
            kDeepNight,
            kPreDawn,
            kSunrise,
            kMorning,
            kNoon,
            kAfternoon,
            kSunset,
            kNight,
            kMidnightWrap,
        };
        static constexpr int kTimeKeyCount = 8;

        static constexpr uint16_t kSunPushThresholdQ16 = 64;

        Renderer *renderer_ = nullptr;
        uint16_t timeQ16_ = 0;
        uint16_t lastSunPushedQ16_ = 0xFFFF;
        float dayLengthSeconds_ = 120.0f;
        bool autoAdvance_ = true;
        bool dirty_ = true;

        PIP3D_ALWAYS_INLINE static inline void computeSunDir(uint16_t phaseQ8,
                                                             Vector3 &outDir,
                                                             float &outDayFactor)
        {

            const float azimuth = static_cast<float>(phaseQ8) * (kTwoPi / 65536.0f);
            float sa, ca;
            FastMath::fastSinCos(azimuth, sa, ca);

            outDir.x = ca * 0.6f;
            outDir.y = ca;
            outDir.z = sa * 0.6f;

            const float hourQ8f = static_cast<float>(phaseQ8) * (1.0f / 256.0f);
            float dayF;
            if (hourQ8f < 53.0f || hourQ8f >= 224.0f)
                dayF = 0.0f;
            else if (hourQ8f < 75.0f)
            {
                const float u = (hourQ8f - 53.0f) * (1.0f / 22.0f);
                dayF = u * u * (3.0f - 2.0f * u);
            }
            else if (hourQ8f < 203.0f)
                dayF = 1.0f;
            else
            {
                const float u = (224.0f - hourQ8f) * (1.0f / 21.0f);
                dayF = u * u * (3.0f - 2.0f * u);
            }
            outDayFactor = dayF;
        }

        PIP3D_HOT void applyToRenderer()
        {
            dirty_ = false;
            if (!renderer_)
                return;

            const uint16_t hourQ8 = timeQ16_ >> 8;
            int i0 = 0;
#pragma GCC unroll 8
            for (int i = 0; i < kTimeKeyCount; ++i)
            {
                if (hourQ8 >= kTimeKeys[i].hourQ8 && hourQ8 < kTimeKeys[i + 1].hourQ8)
                {
                    i0 = i;
                    break;
                }
            }
            const TimeKey &kA = kTimeKeys[i0];
            const TimeKey &kB = kTimeKeys[i0 + 1];

            const float hourQ8f = static_cast<float>(timeQ16_) * (1.0f / 256.0f);
            const float spanQ8f = static_cast<float>(kB.hourQ8 - kA.hourQ8);
            const float local = (spanQ8f <= 0.0f) ? 0.0f
                                                  : (hourQ8f - static_cast<float>(kA.hourQ8)) / spanQ8f;
            const float k = local * local * (3.0f - 2.0f * local);

            const uint8_t k8 = static_cast<uint8_t>(k * 255.0f + 0.5f);

            const Color top = kA.skyTop.blend(kB.skyTop, k8);
            const Color horizon = kA.skyHorizon.blend(kB.skyHorizon, k8);
            const Color ground = kA.skyGround.blend(kB.skyGround, k8);
            const Color sunColor = kA.sunColor.blend(kB.sunColor, k8);
            const Color cloudCol = kA.cloudColor.blend(kB.cloudColor, k8);

            const float cloudAlpha = kA.cloudAlpha + (kB.cloudAlpha - kA.cloudAlpha) * k;
            const float ambientScale = kA.ambientScale + (kB.ambientScale - kA.ambientScale) * k;
            const float exposureScale = kA.exposureScale + (kB.exposureScale - kA.exposureScale) * k;
            const float keyIntensity = kA.sunIntensity + (kB.sunIntensity - kA.sunIntensity) * k;

            const float sunIntensity = 0.05f + 0.95f * keyIntensity;

            Skybox &sky = renderer_->getSkybox();
            sky.setCustom(top, horizon, ground);
            renderer_->invalidateSkyboxCache();
            renderer_->setCloudColor(cloudCol);
            renderer_->setCloudAlpha(cloudAlpha);
            renderer_->setAmbientScale(ambientScale);
            renderer_->setExposureScale(exposureScale);

            Vector3 sunDir;
            float dayFactor;
            computeSunDir(timeQ16_, sunDir, dayFactor);

            const float lenSq = sunDir.x * sunDir.x + sunDir.y * sunDir.y + sunDir.z * sunDir.z;
            const float invLen = FastMath::fastInvSqrt(lenSq);
            sunDir.x *= invLen;
            sunDir.y *= invLen;
            sunDir.z *= invLen;

            const float sunSpriteAlpha = dayFactor;
            const bool sunVisible = (dayFactor > 0.01f);

            uint16_t sunDelta = static_cast<uint16_t>(timeQ16_ - lastSunPushedQ16_);

            if (sunDelta > 32768u)
                sunDelta = static_cast<uint16_t>(65536u - sunDelta);

            if (sunDelta >= kSunPushThresholdQ16 || lastSunPushedQ16_ == 0xFFFF)
            {
                lastSunPushedQ16_ = timeQ16_;

                renderer_->setMainDirectionalLight(sunDir, sunColor, sunIntensity);

                renderer_->updateSun(-sunDir, sunColor, sunSpriteAlpha, sunVisible);
            }
            else
            {

                renderer_->updateSun(-sunDir, sunColor, sunSpriteAlpha, sunVisible);
            }
        }
    };
}