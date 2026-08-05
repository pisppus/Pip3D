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

        __attribute__((hot)) void update(float deltaSeconds)
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

        static constexpr TimeKey kTimeKeys[9] = {
            // hour/256,        skyTop,                skyHorizon,                skyGround,                  sunColor,              cloudColor,             sunInt, cloudA, amb,   exp
            {   0,      Color::rgb(8,12,28),    Color::rgb(20,30,55),   Color::rgb(4,6,14),     Color::rgb(120,140,190), Color::rgb(55,60,85),    0.10f, 0.45f, 0.28f, 0.58f }, // deep night
            {  53,      Color::rgb(18,22,48),   Color::rgb(55,40,70),   Color::rgb(6,6,14),     Color::rgb(255,110,60),  Color::rgb(120,90,110),  0.25f, 0.55f, 0.42f, 0.68f }, // pre-dawn  (5h)
            {  69,      Color::rgb(60,80,130),  Color::rgb(255,150,90), Color::rgb(50,40,45),   Color::rgb(255,170,110), Color::rgb(255,210,180), 0.60f, 0.75f, 0.70f, 0.84f }, // sunrise  (6.5h)
            {  85,      Color::rgb(70,130,220), Color::rgb(180,205,235),Color::rgb(95,90,80),   Color::rgb(255,240,220), Color::rgb(250,250,252), 0.92f, 0.95f, 0.93f, 0.97f }, // morning  (8h)
            { 128,      Color::rgb(70,135,225), Color::rgb(190,210,240),Color::rgb(100,95,85),  Color::rgb(255,250,240), Color::rgb(250,250,252), 1.00f, 1.00f, 1.00f, 1.00f }, // noon     (12h)
            { 181,      Color::rgb(80,120,200), Color::rgb(200,200,215),Color::rgb(95,85,75),   Color::rgb(255,235,200), Color::rgb(252,245,235), 0.95f, 0.95f, 0.95f, 0.98f }, // afternoon(17h)
            { 203,      Color::rgb(90,70,130),  Color::rgb(255,130,60), Color::rgb(55,35,40),   Color::rgb(255,150,80),  Color::rgb(255,190,140), 0.55f, 0.70f, 0.68f, 0.82f }, // sunset   (19h)
            { 224,      Color::rgb(12,16,38),   Color::rgb(30,35,65),   Color::rgb(5,6,15),     Color::rgb(130,145,195), Color::rgb(60,65,90),    0.14f, 0.50f, 0.36f, 0.66f }, // night    (21h)
            { 256,      Color::rgb(8,12,28),    Color::rgb(20,30,55),   Color::rgb(4,6,14),     Color::rgb(120,140,190), Color::rgb(55,60,85),    0.10f, 0.45f, 0.28f, 0.58f }, // sentinel (== [0])
        };
        static constexpr int kTimeKeyCount = 8;

        static constexpr uint16_t kSunPushThresholdQ16 = 64;

        Renderer *renderer_ = nullptr;
        uint16_t timeQ16_ = 0;
        uint16_t lastSunPushedQ16_ = 0xFFFF;
        float dayLengthSeconds_ = 120.0f;
        bool autoAdvance_ = true;
        bool dirty_ = true;

        __attribute__((always_inline)) static inline void computeSunDir(uint16_t phaseQ8,
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

        __attribute__((hot)) void applyToRenderer()
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