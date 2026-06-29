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

            constexpr float TAU = 6.2831855f;
            float y = yawRad;
            if (y < 0.0f)
                y += TAU;
            if (y >= TAU)
                y -= TAU;

            const float hfovFrac = (hfovRad > 0.001f && hfovRad < TAU)
                                       ? (hfovRad / TAU)
                                       : 1.0f;

            constexpr float span = static_cast<float>(CLOUDS_W) *
                                   static_cast<float>(CLOUDS_REPEATS);
            constexpr float INV_TAU = 1.0f / TAU;
            constexpr float INV_PITCH = 1.0f / 0.7853982f;

            const float centerPano = -(y * span) * INV_TAU;
            const float stepPano = (hfovFrac * span) / static_cast<float>(WIDTH);
            const float startPano = centerPano - stepPano * (WIDTH / 2);

            const uint32_t stepFP = static_cast<uint32_t>(stepPano * 65536.0f);
            int32_t curFP = static_cast<int32_t>(startPano * 65536.0f);

            const float pitchShift = (-pitchRad * INV_PITCH * 0.5f) *
                                     static_cast<float>(CLOUDS_H);

            const uint16_t invCloudLineY = cloudLineY > 0 ? cloudLineY : 1u;
            const uint16_t *__restrict__ pano = panorama;

            int16_t rowMap[HEIGHT];
            const int32_t CLOUDS_H_I = static_cast<int32_t>(CLOUDS_H);
            const int32_t invY_I = static_cast<int32_t>(invCloudLineY);
            const int32_t pitchSh_I = static_cast<int32_t>(pitchShift);

            for (uint16_t yb = 0; yb < HEIGHT; ++yb)
            {
                const int16_t globalY = bandOffY + static_cast<int16_t>(yb);
                if (globalY < 0 || globalY >= static_cast<int16_t>(cloudLineY))
                {
                    rowMap[yb] = -1;
                    continue;
                }
                int32_t m = (static_cast<int32_t>(globalY) * CLOUDS_H_I) / invY_I;
                m += pitchSh_I;
                if (m < 0)
                    m = 0;
                if (m >= CLOUDS_H_I)
                    m = CLOUDS_H_I - 1;
                rowMap[yb] = static_cast<int16_t>(m);
            }

            uint16_t rowBuf[CLOUDS_W];

            for (uint16_t yb = 0; yb < HEIGHT; ++yb)
            {
                const int16_t maskRow = rowMap[yb];
                if (maskRow < 0)
                    continue;

                const uint16_t *__restrict__ panoRow =
                    pano + static_cast<size_t>(maskRow) * CLOUDS_W;

                memcpy(rowBuf, panoRow, CLOUDS_W * sizeof(uint16_t));

                uint16_t *__restrict__ fbRow = buf + static_cast<size_t>(yb) * WIDTH;

                int32_t mx = curFP;
                for (uint16_t x = 0; x < WIDTH; ++x)
                {
                    fbRow[x] = rowBuf[(mx >> 16) & CLOUDS_W_MASK];
                    mx += stepFP;
                }
            }
        }
    };
}
