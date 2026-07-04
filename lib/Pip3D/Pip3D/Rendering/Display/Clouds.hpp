#pragma once

#include "Core/Platform.hpp"
#include "Core/Color.hpp"
#include "Rendering/Display/CloudsMask.hpp"

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

    static constexpr float CLOUDS_SHADE_MIN = 0.72f;
    static constexpr float CLOUDS_SHADE_MAX = 1.07f;

    static_assert((CLOUDS_PANO_W & (CLOUDS_PANO_W - 1u)) == 0u,
                  "CLOUDS_PANO_W must be a power of two (mask & wrap)");
    static_assert(CLOUDS_PANO_REPEATS >= 1u, "CLOUDS_PANO_REPEATS must be >= 1");

    struct alignas(8) CloudLayer
    {
        const uint8_t *alphaData = g_cloudsAlpha;
        const uint8_t *shadeData = g_cloudsShade;
        const uint32_t *alphaOffset = g_cloudsAlphaOffset;
        const uint32_t *shadeOffset = g_cloudsShadeOffset;
        Color cloudColor = Color(CLOUDS_DEFAULT_COLOR);
        float cloudAlpha = 1.0f;
        uint16_t cloudLineY = 0;
        bool enabled = false;
        bool lutDirty = true;

        uint16_t shadeLut[256];

        CloudLayer() = default;
        ~CloudLayer() = default;

        CloudLayer(const CloudLayer &) = delete;
        CloudLayer &operator=(const CloudLayer &) = delete;

        PIP3D_FORCE_INLINE bool isReady() const noexcept { return enabled; }

        void free() noexcept {}
        bool reserveBuffer() noexcept { return true; }

        __attribute__((always_inline)) inline void buildLuts() noexcept
        {
            const uint32_t base = cloudColor.rgb565;
            const uint32_t r5 = (base >> 11) & 0x1Fu;
            const uint32_t g6 = (base >> 5) & 0x3Fu;
            const uint32_t b5 = base & 0x1Fu;

            for (uint32_t s8 = 0; s8 < 256u; ++s8)
            {
                const float fs = static_cast<float>(s8) * (1.0f / 255.0f);
                const float sf = CLOUDS_SHADE_MIN + fs * (CLOUDS_SHADE_MAX - CLOUDS_SHADE_MIN);

                uint32_t r = static_cast<uint32_t>(static_cast<float>(r5) * sf + 0.5f);
                uint32_t g = static_cast<uint32_t>(static_cast<float>(g6) * sf + 0.5f);
                uint32_t b = static_cast<uint32_t>(static_cast<float>(b5) * sf + 0.5f);
                if (r > 31u)
                    r = 31u;
                if (g > 63u)
                    g = 63u;
                if (b > 31u)
                    b = 31u;
                shadeLut[s8] = static_cast<uint16_t>((r << 11) | (g << 5) | b);
            }
            lutDirty = false;
        }

        void setCloudColor(Color c) noexcept
        {
            if (c.rgb565 != cloudColor.rgb565)
            {
                cloudColor = c;
                lutDirty = true;
            }
        }

        void setCloudAlpha(float a) noexcept
        {
            float clamped = (a < 0.0f) ? 0.0f : (a > 1.0f) ? 1.0f
                                                           : a;
            if (clamped != cloudAlpha)
                cloudAlpha = clamped;
        }

        void generatePanorama(uint32_t /*seed*/, float /*coverage*/,
                              uint16_t screenH, const Sky & /*sky*/) noexcept
        {
            cloudLineY = static_cast<uint16_t>((static_cast<uint32_t>(screenH) * 166u + 128u) >> 8);
            cloudColor = Color(CLOUDS_DEFAULT_COLOR);
            cloudAlpha = 1.0f;
            lutDirty = true;
        }

        static __attribute__((always_inline)) inline void IRAM_ATTR
        unpackRow(const uint8_t *__restrict__ src, uint8_t *__restrict__ dst, uint16_t rowW) noexcept
        {
            uint16_t written = 0;
            while (written < rowW)
            {
                const uint8_t c = *src++;
                if (c & 0x80u)
                {
                    uint8_t count = static_cast<uint8_t>((c & 0x7Fu) + 1u);
                    do
                    {
                        dst[written++] = *src++;
                    } while (--count);
                }
                else
                {
                    uint8_t count = static_cast<uint8_t>(c + 1u);
                    const uint8_t v = *src++;
                    do
                    {
                        dst[written++] = v;
                    } while (--count);
                }
            }
        }

        template <uint16_t WIDTH, uint16_t HEIGHT>
        __attribute__((always_inline, hot)) inline void IRAM_ATTR
        drawClouds(uint16_t *__restrict__ buf,
                   int16_t bandOffY,
                   float yawRad,
                   float pitchShiftRows,
                   float hfovRad) const
        {
            if (unlikely(!enabled))
                return;

            const int32_t shift = static_cast<int32_t>(pitchShiftRows);
            const int32_t cloudLineDynI =
                static_cast<int32_t>(cloudLineY) + shift;
            const int16_t cloudLineDyn =
                static_cast<int16_t>(cloudLineDynI > 0 ? cloudLineDynI : 0);

            if (bandOffY >= cloudLineDyn)
                return;

            if (unlikely(lutDirty))
                const_cast<CloudLayer *>(this)->buildLuts();

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

            const float centerPano = -(y * span) * INV_TAU;
            const float stepPano = (hfovFrac * span) / static_cast<float>(WIDTH);
            const float startPano = centerPano - stepPano * (WIDTH / 2);

            const uint32_t stepFP = static_cast<uint32_t>(stepPano * 65536.0f);
            int32_t curFP = static_cast<int32_t>(startPano * 65536.0f);

            const uint16_t invCloudLineY = cloudLineY > 0 ? cloudLineY : 1u;
            const uint8_t *__restrict__ aData = alphaData;
            const uint8_t *__restrict__ sData = shadeData;
            const uint32_t *__restrict__ aOff = alphaOffset;
            const uint32_t *__restrict__ sOff = shadeOffset;

            int16_t rowMap[HEIGHT];
            const int32_t CLOUDS_H_I = static_cast<int32_t>(CLOUDS_H);
            const int32_t invY_I = static_cast<int32_t>(invCloudLineY);

            for (uint16_t yb = 0; yb < HEIGHT; ++yb)
            {
                const int16_t globalY = bandOffY + static_cast<int16_t>(yb);
                if (globalY < 0 || globalY >= cloudLineDyn)
                {
                    rowMap[yb] = -1;
                    continue;
                }
                int32_t virtualY = static_cast<int32_t>(globalY) - shift;
                if (virtualY < 0)
                    virtualY = 0;
                int32_t m = (virtualY * CLOUDS_H_I) / invY_I;
                if (m < 0)
                    m = 0;
                if (m >= CLOUDS_H_I)
                    m = CLOUDS_H_I - 1;
                rowMap[yb] = static_cast<int16_t>(m);
            }

            uint8_t alphaBuf[CLOUDS_W];
            uint8_t shadeBuf[CLOUDS_W];
            int16_t lastDecodedRow = -1;

            const uint32_t alphaMulFP = static_cast<uint32_t>(cloudAlpha * 256.0f + 0.5f);

            for (uint16_t yb = 0; yb < HEIGHT; ++yb)
            {
                const int16_t maskRow = rowMap[yb];
                if (maskRow < 0)
                    continue;

                if (maskRow != lastDecodedRow)
                {
                    unpackRow(aData + aOff[maskRow], alphaBuf, CLOUDS_W);
                    unpackRow(sData + sOff[maskRow], shadeBuf, CLOUDS_W);
                    lastDecodedRow = maskRow;
                }

                uint16_t *__restrict__ fbRow = buf + static_cast<size_t>(yb) * WIDTH;

                int32_t mx = curFP;
                for (uint16_t x = 0; x < WIDTH; ++x)
                {
                    const uint16_t idx = static_cast<uint16_t>((mx >> 16) & CLOUDS_W_MASK);
                    const uint8_t a8 = alphaBuf[idx];
                    if (a8 != 0u)
                    {
                        const uint16_t shaded = shadeLut[shadeBuf[idx]];
                        uint16_t a = static_cast<uint16_t>((a8 * alphaMulFP) >> 8);
                        if (a >= 255u)
                            fbRow[x] = shaded;
                        else
                            fbRow[x] = Color(fbRow[x]).blend(Color(shaded), static_cast<uint8_t>(a)).rgb565;
                    }
                    mx += stepFP;
                }
            }
        }
    };
}
