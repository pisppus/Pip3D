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
            auto smoothstep = [](float e0, float e1, float x) -> float
            {
                float u = (x - e0) / (e1 - e0);
                if (u < 0.0f)
                    u = 0.0f;
                else if (u > 1.0f)
                    u = 1.0f;
                return u * u * (3.0f - 2.0f * u);
            };

            const float hour = t * 24.0f;
            constexpr float NIGHT_FLOOR = 0.12f;
            auto dayFactor = [&smoothstep](float h) -> float
            {
                if (h < 5.0f || h >= 20.5f)
                    return 0.0f;
                if (h < 7.0f)
                    return smoothstep(5.0f, 7.0f, h);
                if (h < 17.0f)
                    return 1.0f;
                return 1.0f - smoothstep(17.0f, 20.5f, h);
            };
            const float dayRaw = dayFactor(hour);
            const float day = NIGHT_FLOOR + (1.0f - NIGHT_FLOOR) * dayRaw;

            const float twilight = (dayRaw > 0.0f && dayRaw < 1.0f) ? (1.0f - fabsf(2.0f * dayRaw - 1.0f)) : 0.0f;
            const float twi = twilight * twilight;

            const float dayAngle = (t - 0.25f) * TWO_PI;
            const float elevation = sinf(dayAngle);

            static const Color
                nightTop = Color::rgb(8, 12, 28),
                nightHoriz = Color::rgb(20, 30, 55),
                nightGround = Color::rgb(4, 6, 14),
                dayTop = Color::rgb(70, 130, 220),
                dayHoriz = Color::rgb(180, 205, 235),
                dayGround = Color::rgb(95, 90, 80),
                twiHoriz = Color::rgb(255, 150, 70);

            Color top = lerpColor(nightTop, dayTop, day);
            Color ground = lerpColor(nightGround, dayGround, day);
            Color horizon = lerpColor(nightHoriz, dayHoriz, day);
            horizon = lerpColor(horizon, twiHoriz, twi * 0.85f);
            top = lerpColor(top, Color::rgb(120, 90, 130), twi * 0.30f);

            const float azimuth = t * TWO_PI;
            const float sx = cosf(azimuth) * 0.6f;
            const float sz = sinf(azimuth) * 0.6f;
            Vector3 dayDir(sx, -elevation, sz);
            Vector3 moonDir(sx * 0.5f, -0.45f, sz * 0.5f);
            dayDir.normalize();
            moonDir.normalize();
            const float wNight = 1.0f - day;
            Vector3 lightDir(
                dayDir.x * (1.0f - wNight) + moonDir.x * wNight,
                dayDir.y * (1.0f - wNight) + moonDir.y * wNight,
                dayDir.z * (1.0f - wNight) + moonDir.z * wNight);
            lightDir.normalize();

            static const Color moonLight = Color::rgb(120, 140, 190);
            Color sunColor = Color::fromTemperature(5500.0f + (2500.0f - 5500.0f) * (1.0f - day));
            Color lightColor = lerpColor(moonLight, sunColor, day);
            lightColor = lerpColor(lightColor, Color::rgb(255, 150, 80), twi * 0.6f);

            const float intensity = nightIntensity + (baseIntensity - nightIntensity) * day;

            const float ambientScale = 0.30f + 0.70f * dayRaw;
            const float exposureScale = 0.60f + 0.40f * dayRaw;

            static const Color cloudDay_ = Color::rgb(250, 250, 252);
            static const Color cloudNight_ = Color::rgb(55, 60, 85);
            Color cloudColor = lerpColor(cloudNight_, cloudDay_, day);
            cloudColor = lerpColor(cloudColor, Color::rgb(255, 200, 150), twi * 0.7f);
            const float cloudAlpha = 0.50f + 0.50f * dayRaw;

            out.top = top;
            out.horizon = horizon;
            out.ground = ground;
            out.sunColor = lightColor;
            out.sunDir = lightDir;
            out.intensity = intensity;
            out.cloudColor = cloudColor;
            out.cloudAlpha = cloudAlpha;
            out.ambientScale = ambientScale;
            out.exposureScale = exposureScale;
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