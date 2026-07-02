#pragma once

#include "Core/Platform.hpp"
#include "Rendering/Renderer.hpp"
#include "Rendering/Display/Sky.hpp"

namespace pip3D
{

    struct TimeOfDayConfig
    {
        float dayLengthSeconds;
        float startHour;
        float baseIntensity;
        float nightIntensity;
        bool autoAdvance;

        TimeOfDayConfig()
            : dayLengthSeconds(120.0f), startHour(10.0f), baseIntensity(1.0f), nightIntensity(0.05f), autoAdvance(true) {}
    };

    class TimeOfDayController
    {
    public:
        TimeOfDayController(Renderer *r = nullptr)
            : renderer(r), timeMinutes(600.0f), dayLengthSeconds(120.0f), baseIntensity(1.0f), nightIntensity(0.05f), autoAdvance(true) {}

        void init(Renderer *r, const TimeOfDayConfig &cfg)
        {
            renderer = r;
            dayLengthSeconds = cfg.dayLengthSeconds;
            baseIntensity = cfg.baseIntensity;
            nightIntensity = cfg.nightIntensity;
            autoAdvance = cfg.autoAdvance;
            setTime(cfg.startHour, 0.0f);
        }

        void setRenderer(Renderer *r) { renderer = r; }

        void setDayLengthSeconds(float seconds)
        {
            dayLengthSeconds = seconds;
        }

        void setAutoAdvance(bool enabled) { autoAdvance = enabled; }

        void setBaseIntensity(float intensity)
        {
            baseIntensity = intensity;
        }

        void setNightIntensity(float intensity)
        {
            nightIntensity = intensity;
        }

        void setTime(float hours, float minutes = 0.0f)
        {
            float h = hours;
            while (h < 0.0f)
                h += 24.0f;
            while (h >= 24.0f)
                h -= 24.0f;
            float m = clamp(minutes, 0.0f, 59.999f);
            timeMinutes = h * 60.0f + m;
            applyToRenderer();
        }

        float getTimeHours() const
        {
            return timeMinutes / 60.0f;
        }

        float getTime01() const
        {
            return timeMinutes / 1440.0f;
        }

        __attribute__((hot)) void update(float deltaSeconds)
        {
            if (!renderer)
                return;

            if (autoAdvance && dayLengthSeconds > 0.0f && deltaSeconds > 0.0f)
            {
                float dayFrac = deltaSeconds / dayLengthSeconds;
                timeMinutes += 1440.0f * dayFrac;
                if (timeMinutes >= 1440.0f || timeMinutes < 0.0f)
                {
                    timeMinutes = fmodf(timeMinutes, 1440.0f);
                    if (timeMinutes < 0.0f)
                        timeMinutes += 1440.0f;
                }
            }
            applyToRenderer();
        }

    private:
        struct SkyState
        {
            Color top;
            Color horizon;
            Color ground;
            Color sunColor;
            Vector3 sunDir;
            float intensity;
            Color cloudColor;
            float cloudAlpha;
            float ambientScale;
            float exposureScale;
        };

        Renderer *renderer;
        float timeMinutes;
        float dayLengthSeconds;
        float baseIntensity;
        float nightIntensity;
        bool autoAdvance;

        struct TimeKey
        {
            float hour;
            Color skyTop;
            Color skyHorizon;
            Color skyGround;
            Color sunColor;
            float sunIntensity;
            Color cloudColor;
            float cloudAlpha;
            float ambientScale;
            float exposureScale;
        };

        static constexpr TimeKey kTimeKeys[8] = {
            // hour, skyTop,                skyHorizon,           skyGround,            sunColor,             sunInt, cloudColor,           cloudA, ambient, exposure
            { 0.0f, {8, 12, 28},            {20, 30, 55},         {4, 6, 14},           {120, 140, 190},      0.10f,  {55, 60, 85},         0.45f,  0.28f,   0.58f}, // deep night
            { 5.0f, {18, 22, 48},           {55, 40, 70},         {6, 6, 14},           {255, 110, 60},       0.25f,  {120, 90, 110},       0.55f,  0.42f,   0.68f}, // pre-dawn
            { 6.5f, {60, 80, 130},          {255, 150, 90},       {50, 40, 45},         {255, 170, 110},      0.60f,  {255, 210, 180},      0.75f,  0.70f,   0.84f}, // sunrise / golden
            { 8.0f, {70, 130, 220},         {180, 205, 235},      {95, 90, 80},         {255, 240, 220},      0.92f,  {250, 250, 252},      0.95f,  0.93f,   0.97f}, // morning
            {12.0f, {70, 135, 225},         {190, 210, 240},      {100, 95, 85},        {255, 250, 240},      1.00f,  {250, 250, 252},      1.00f,  1.00f,   1.00f}, // noon
            {17.0f, {80, 120, 200},         {200, 200, 215},      {95, 85, 75},         {255, 235, 200},      0.95f,  {252, 245, 235},      0.95f,  0.95f,   0.98f}, // late afternoon
            {19.0f, {90, 70, 130},          {255, 130, 60},       {55, 35, 40},         {255, 150, 80},       0.55f,  {255, 190, 140},      0.70f,  0.68f,   0.82f}, // sunset / golden
            {21.0f, {12, 16, 38},           {30, 35, 65},         {5, 6, 15},           {130, 145, 195},      0.14f,  {60, 65, 90},         0.50f,  0.36f,   0.66f}, // night onset
        };
        static constexpr int kTimeKeyCount = 8;

        void computeSkyState(float t, SkyState &out) const
        {
            if (t < 0.0f)
                t = 0.0f;
            if (t > 1.0f)
                t = 1.0f;

            auto lerpColor = [](Color c1, Color c2, float k) -> Color
            {
                return c1.blend(c2, static_cast<uint8_t>(clamp(k, 0.0f, 1.0f) * 255.0f));
            };
            auto lerpF = [](float a, float b, float k) -> float
            {
                return a + (b - a) * k;
            };
            auto smoothstep = [](float e0, float e1, float x) -> float
            {
                float u = (x - e0) / (e1 - e0);
                if (u < 0.0f) u = 0.0f;
                else if (u > 1.0f) u = 1.0f;
                return u * u * (3.0f - 2.0f * u);
            };

            const float hour = t * 24.0f;
            int i0 = kTimeKeyCount - 1;
            for (int i = 0; i < kTimeKeyCount - 1; ++i)
            {
                if (hour >= kTimeKeys[i].hour && hour < kTimeKeys[i + 1].hour)
                {
                    i0 = i;
                    break;
                }
            }
            const TimeKey &kA = kTimeKeys[i0];
            const TimeKey &kB = (i0 + 1 < kTimeKeyCount) ? kTimeKeys[i0 + 1] : kTimeKeys[0];

            float span = (i0 + 1 < kTimeKeyCount) ? (kB.hour - kA.hour) : ((24.0f + kTimeKeys[0].hour) - kA.hour);
            float local = (hour - kA.hour) / span;
            float k = smoothstep(0.0f, 1.0f, local);

            out.top        = lerpColor(kA.skyTop,     kB.skyTop,     k);
            out.horizon    = lerpColor(kA.skyHorizon, kB.skyHorizon, k);
            out.ground     = lerpColor(kA.skyGround,  kB.skyGround,  k);
            out.sunColor   = lerpColor(kA.sunColor,   kB.sunColor,   k);
            out.cloudColor = lerpColor(kA.cloudColor, kB.cloudColor, k);
            out.cloudAlpha   = lerpF(kA.cloudAlpha,   kB.cloudAlpha,   k);
            out.ambientScale = lerpF(kA.ambientScale, kB.ambientScale, k);
            out.exposureScale= lerpF(kA.exposureScale,kB.exposureScale,k);

            const float keyIntensity = lerpF(kA.sunIntensity, kB.sunIntensity, k);
            out.intensity = nightIntensity + (baseIntensity - nightIntensity) * keyIntensity;

            const float dayAngle = (t - 0.25f) * TWO_PI;
            const float elevation = sinf(dayAngle);
            float dayRaw;
            if (hour < 5.0f || hour >= 21.0f)
                dayRaw = 0.0f;
            else if (hour < 7.0f)
                dayRaw = smoothstep(5.0f, 7.0f, hour);
            else if (hour < 19.0f)
                dayRaw = 1.0f;
            else
                dayRaw = 1.0f - smoothstep(19.0f, 21.0f, hour);
            const float azimuth = t * TWO_PI;
            const float sx = cosf(azimuth) * 0.6f;
            const float sz = sinf(azimuth) * 0.6f;
            Vector3 dayDir(sx, -elevation, sz);
            Vector3 moonDir(sx * 0.5f, -0.45f, sz * 0.5f);
            dayDir.normalize();
            moonDir.normalize();
            const float wNight = 1.0f - dayRaw;
            out.sunDir = Vector3(
                dayDir.x * (1.0f - wNight) + moonDir.x * wNight,
                dayDir.y * (1.0f - wNight) + moonDir.y * wNight,
                dayDir.z * (1.0f - wNight) + moonDir.z * wNight);
            out.sunDir.normalize();
        }

        void applySkyStateToRenderer(const SkyState &state)
        {
            if (!renderer)
                return;

            Skybox &sky = renderer->getSkybox();
            sky.setCustom(state.top, state.horizon, state.ground);
            renderer->invalidateSkyboxCache();
            renderer->setMainDirectionalLight(state.sunDir, state.sunColor, state.intensity);
            renderer->setCloudColor(state.cloudColor);
            renderer->setCloudAlpha(state.cloudAlpha);
            renderer->setAmbientScale(state.ambientScale);
            renderer->setExposureScale(state.exposureScale);
        }

        __attribute__((hot)) void applyToRenderer()
        {
            if (!renderer)
                return;

            float t = timeMinutes / 1440.0f;
            SkyState state;
            computeSkyState(t, state);
            applySkyStateToRenderer(state);
        }
    };

}