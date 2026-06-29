#pragma once

#include "Core/Platform.hpp"
#include "Core/Color.hpp"
#include "Rendering/Display/Panorama.hpp"

#if !defined(IRAM_ATTR)
#if defined(ESP_PLATFORM) || defined(ESP32)
#include <esp_attr.h>
#else
#define IRAM_ATTR
#endif
#endif

namespace pip3D
{
    static constexpr uint16_t CLOUDS_W = CLOUDS_PANO_W;
    static constexpr uint16_t CLOUDS_W_MASK = CLOUDS_PANO_W - 1u;
    static constexpr uint16_t CLOUDS_H = CLOUDS_PANO_H;
    static constexpr uint16_t CLOUDS_REPEATS = CLOUDS_PANO_REPEATS;

    static_assert((CLOUDS_PANO_W & (CLOUDS_PANO_W - 1u)) == 0u,
                  "CLOUDS_PANO_W must be a power of two (mask & wrap)");
    static_assert(CLOUDS_PANO_REPEATS >= 1u, "CLOUDS_PANO_REPEATS must be >= 1");

    struct alignas(8) CloudLayer
    {
        const uint16_t *panorama = g_cloudsPanorama;
        Color color = Color::rgb(252, 253, 255);
        uint16_t cloudLineY = 0;
        bool enabled = false;

        CloudLayer() = default;
        ~CloudLayer() = default;

        CloudLayer(const CloudLayer &) = delete;
        CloudLayer &operator=(const CloudLayer &) = delete;

        PIP3D_FORCE_INLINE bool isReady() const noexcept { return enabled; }

        void free() noexcept {}
        bool reserveBuffer() noexcept { return true; }

        void generatePanorama(uint32_t /*seed*/, float /*coverage*/,
                              uint16_t screenH, const Sky & /*sky*/) noexcept
        {
            cloudLineY = static_cast<uint16_t>((static_cast<uint32_t>(screenH) * 166u + 128u) >> 8);
        }

        template <uint16_t WIDTH, uint16_t HEIGHT>
        __attribute__((always_inline, hot)) inline void IRAM_ATTR
        drawClouds(uint16_t *__restrict__ buf,
                   int16_t bandOffY,
                   float yawRad,
                   float pitchRad,
                   float hfovRad) const
        {
            if (unlikely(!enabled))
                return;

            float y = yawRad;
            while (y < 0.0f)
                y += 6.2831855f;
            while (y >= 6.2831855f)
                y -= 6.2831855f;

            constexpr float TAU = 6.2831855f;
            const float hfovFrac = (hfovRad > 0.001f && hfovRad < TAU)
                                       ? (hfovRad / TAU)
                                       : 1.0f;

            const float span = static_cast<float>(CLOUDS_W) * static_cast<float>(CLOUDS_REPEATS);
            const float centerPano = -(y * span) / TAU;
            const float stepPano = (hfovFrac * span) / static_cast<float>(WIDTH);
            const float startPano = centerPano - stepPano * (WIDTH * 0.5f);

            const uint32_t stepFP = static_cast<uint32_t>(stepPano * 65536.0f) & 0xFFFFFFFFu;
            int32_t curFP = static_cast<int32_t>(startPano * 65536.0f);

            const float pitchNorm = (pitchRad / 0.7853982f);
            const float pitchShift = (-pitchNorm * 0.5f) * static_cast<float>(CLOUDS_H);

            const uint16_t invCloudLineY = cloudLineY > 0 ? cloudLineY : 1u;
            const uint16_t *__restrict__ pano = panorama;

            for (uint16_t yb = 0; yb < HEIGHT; ++yb)
            {
                const int16_t globalY = bandOffY + static_cast<int16_t>(yb);
                if (globalY < 0 || globalY >= static_cast<int16_t>(cloudLineY))
                    continue;

                int32_t maskRow = static_cast<int32_t>(
                    (static_cast<int32_t>(globalY) * static_cast<int32_t>(CLOUDS_H)) / static_cast<int32_t>(invCloudLineY));
                maskRow += static_cast<int32_t>(pitchShift);
                if (maskRow < 0)
                    maskRow = 0;
                if (maskRow >= static_cast<int32_t>(CLOUDS_H))
                    maskRow = CLOUDS_H - 1;

                const uint16_t *__restrict__ panoRow =
                    pano + static_cast<size_t>(maskRow) * CLOUDS_W;

                uint16_t *__restrict__ fbRow = buf + static_cast<size_t>(yb) * WIDTH;

                int32_t mx = curFP;
                for (uint16_t x = 0; x < WIDTH; ++x)
                {
                    fbRow[x] = panoRow[(static_cast<uint16_t>(mx >> 16)) & CLOUDS_W_MASK];
                    mx += stepFP;
                }
            }
        }
    };
}
