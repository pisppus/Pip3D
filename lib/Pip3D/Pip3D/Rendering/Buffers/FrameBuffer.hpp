#pragma once

#include <cstddef>
#include <cstdint>

#include "Core/Platform.hpp"
#include "Core/Color.hpp"
#include "Math/Algebra.hpp"
#include "Rendering/Environment/Sky.hpp"
#include "Rendering/Environment/Clouds.hpp"

#if defined(PIP3D_PC)
#include <PipCore/Platforms/Desktop/Runtime.hpp>
#else
#include <PipCore/Display.hpp>
#endif

namespace pip3D
{
    class alignas(16) FrameBuffer
    {
    public:
        static constexpr uint16_t kWidth = SCREEN_WIDTH;
        static constexpr uint16_t kHeight = SCREEN_BAND_HEIGHT;
        static constexpr uint32_t kPixelCount = static_cast<uint32_t>(kWidth) * kHeight;
        static constexpr uint32_t kByteSize = kPixelCount * sizeof(uint16_t);
        static constexpr uint32_t kStride = kWidth;
        static constexpr size_t kDmaAlign = 64;

        struct SkyColor888
        {
            uint8_t r, g, b, _pad;
        };

        struct DitherOffset
        {
            uint8_t dR, dG, dB;
        };

    private:
        alignas(kDmaAlign) uint16_t storage[2][kPixelCount];
        alignas(4) SkyColor888 skyboxCache[SCREEN_HEIGHT];
        uint32_t baseClear32;
        Color clearColor;
        uint8_t activeSlot;
        bool useSkybox;
        bool cacheValid;

        DisplayConfig config;
#if defined(PIP3D_PC)
        bool displayReady;
#else
        pipcore::Display *display;
#endif

        Skybox skybox;
        CloudLayer clouds;

        static constexpr DitherOffset kDitherLut[64] = {
            {0 >> 3, 0 >> 4, 0 >> 3},
            {32 >> 3, 32 >> 4, 32 >> 3},
            {8 >> 3, 8 >> 4, 8 >> 3},
            {40 >> 3, 40 >> 4, 40 >> 3},
            {2 >> 3, 2 >> 4, 2 >> 3},
            {34 >> 3, 34 >> 4, 34 >> 3},
            {10 >> 3, 10 >> 4, 10 >> 3},
            {42 >> 3, 42 >> 4, 42 >> 3},

            {48 >> 3, 48 >> 4, 48 >> 3},
            {16 >> 3, 16 >> 4, 16 >> 3},
            {56 >> 3, 56 >> 4, 56 >> 3},
            {24 >> 3, 24 >> 4, 24 >> 3},
            {50 >> 3, 50 >> 4, 50 >> 3},
            {18 >> 3, 18 >> 4, 18 >> 3},
            {58 >> 3, 58 >> 4, 58 >> 3},
            {26 >> 3, 26 >> 4, 26 >> 3},

            {12 >> 3, 12 >> 4, 12 >> 3},
            {44 >> 3, 44 >> 4, 44 >> 3},
            {4 >> 3, 4 >> 4, 4 >> 3},
            {36 >> 3, 36 >> 4, 36 >> 3},
            {14 >> 3, 14 >> 4, 14 >> 3},
            {46 >> 3, 46 >> 4, 46 >> 3},
            {6 >> 3, 6 >> 4, 6 >> 3},
            {38 >> 3, 38 >> 4, 38 >> 3},

            {60 >> 3, 60 >> 4, 60 >> 3},
            {28 >> 3, 28 >> 4, 28 >> 3},
            {52 >> 3, 52 >> 4, 52 >> 3},
            {20 >> 3, 20 >> 4, 20 >> 3},
            {62 >> 3, 62 >> 4, 62 >> 3},
            {30 >> 3, 30 >> 4, 30 >> 3},
            {54 >> 3, 54 >> 4, 54 >> 3},
            {22 >> 3, 22 >> 4, 22 >> 3},

            {3 >> 3, 3 >> 4, 3 >> 3},
            {35 >> 3, 35 >> 4, 35 >> 3},
            {11 >> 3, 11 >> 4, 11 >> 3},
            {43 >> 3, 43 >> 4, 43 >> 3},
            {1 >> 3, 1 >> 4, 1 >> 3},
            {33 >> 3, 33 >> 4, 33 >> 3},
            {9 >> 3, 9 >> 4, 9 >> 3},
            {41 >> 3, 41 >> 4, 41 >> 3},

            {51 >> 3, 51 >> 4, 51 >> 3},
            {19 >> 3, 19 >> 4, 19 >> 3},
            {59 >> 3, 59 >> 4, 59 >> 3},
            {27 >> 3, 27 >> 4, 27 >> 3},
            {49 >> 3, 49 >> 4, 49 >> 3},
            {17 >> 3, 17 >> 4, 17 >> 3},
            {57 >> 3, 57 >> 4, 57 >> 3},
            {25 >> 3, 25 >> 4, 25 >> 3},

            {15 >> 3, 15 >> 4, 15 >> 3},
            {47 >> 3, 47 >> 4, 47 >> 3},
            {7 >> 3, 7 >> 4, 7 >> 3},
            {39 >> 3, 39 >> 4, 39 >> 3},
            {13 >> 3, 13 >> 4, 13 >> 3},
            {45 >> 3, 45 >> 4, 45 >> 3},
            {5 >> 3, 5 >> 4, 5 >> 3},
            {37 >> 3, 37 >> 4, 37 >> 3},

            {63 >> 3, 63 >> 4, 63 >> 3},
            {31 >> 3, 31 >> 4, 31 >> 3},
            {55 >> 3, 55 >> 4, 55 >> 3},
            {23 >> 3, 23 >> 4, 23 >> 3},
            {61 >> 3, 61 >> 4, 61 >> 3},
            {29 >> 3, 29 >> 4, 29 >> 3},
            {53 >> 3, 53 >> 4, 53 >> 3},
            {21 >> 3, 21 >> 4, 21 >> 3}};

        __attribute__((always_inline)) inline void ensureSkyboxCache()
        {
            if (cacheValid || !useSkybox || !skybox.enabled)
                return;

            for (int16_t y = 0; y < SCREEN_HEIGHT; ++y)
            {
                skybox.getColorAtY888(y, skyboxCache[y].r, skyboxCache[y].g, skyboxCache[y].b);
            }

            cacheValid = true;
        }

        __attribute__((always_inline)) inline bool readyForFlush() const
        {
#if defined(PIP3D_PC)
            return displayReady;
#else
            return display != nullptr;
#endif
        }

        static PIP3D_FORCE_INLINE constexpr uint16_t
        dither565(uint16_t base, uint8_t bayer) noexcept
        {
            if (bayer < 32u)
                return base;

            const uint16_t r5 = static_cast<uint16_t>((base >> 11) & 0x1Fu);
            const uint16_t g6 = static_cast<uint16_t>((base >> 5) & 0x3Fu);
            const uint16_t b5 = static_cast<uint16_t>(base & 0x1Fu);

            const uint16_t bumpR = static_cast<uint16_t>(base + ((r5 < 31u) ? 0x0800u : 0u));
            const uint16_t bumpB = static_cast<uint16_t>(base + ((b5 < 31u) ? 0x0001u : 0u));
            const uint16_t bumpG = static_cast<uint16_t>(base + ((g6 < 63u) ? 0x0020u : 0u));

            const uint8_t sel = static_cast<uint8_t>(bayer & 3u);
            return (sel == 0u)   ? bumpR
                   : (sel == 1u) ? bumpB
                                 : bumpG;
        }

        static PIP3D_FORCE_INLINE int32_t
        clampRow(int32_t v, int32_t maxY) noexcept
        {
            v = (v < 0) ? 0 : v;
            v = (v > maxY) ? maxY : v;
            return v;
        }

        template <uint16_t WIDTH, uint16_t HEIGHT>
        __attribute__((always_inline, hot)) PIP3D_FLATTEN inline void IRAM_ATTR
        fillBackgroundSky(float pitchShiftRows)
        {
            uint16_t *__restrict__ buf = storage[activeSlot];

            const int16_t bandOffY = g_bandOffsetY;
            const int16_t maxY16 = static_cast<int16_t>(SCREEN_HEIGHT - 1);
            const int32_t maxY32 = static_cast<int32_t>(maxY16);
            const int32_t shiftI = static_cast<int32_t>(lroundf(pitchShiftRows));

            constexpr uint16_t widthHalf = WIDTH >> 1;
            static_assert((WIDTH & 1u) == 0, "WIDTH must be even for packed-32 store path");
            static_assert((widthHalf & 3u) == 0,
                          "WIDTH/2 must be multiple of 4 (UNROLL=4 stores 8 px)");

            for (uint16_t y = 0; y < HEIGHT; ++y)
            {
                const int16_t globalY = static_cast<int16_t>(bandOffY + static_cast<int16_t>(y));

                const int32_t vy = clampRow(static_cast<int32_t>(globalY) - shiftI, maxY32);
                const SkyColor888 &c888 = skyboxCache[static_cast<uint32_t>(vy)];

                const uint32_t R8 = c888.r;
                const uint32_t G8 = c888.g;
                const uint32_t B8 = c888.b;

                const uint8_t brow = static_cast<uint8_t>(globalY & 7u);
                const DitherOffset *PIP3D_RESTRICT dRow = &kDitherLut[brow * 8u];

#define MAKE_PX(i) static_cast<uint16_t>( \
    (((R8 + dRow[i].dR) >> 3) << 11) |    \
    (((G8 + dRow[i].dG) >> 2) << 5) |     \
    ((B8 + dRow[i].dB) >> 3))

                const uint16_t p0 = MAKE_PX(0);
                const uint16_t p1 = MAKE_PX(1);
                const uint16_t p2 = MAKE_PX(2);
                const uint16_t p3 = MAKE_PX(3);
                const uint16_t p4 = MAKE_PX(4);
                const uint16_t p5 = MAKE_PX(5);
                const uint16_t p6 = MAKE_PX(6);
                const uint16_t p7 = MAKE_PX(7);
#undef MAKE_PX

                const uint32_t lut0 = (static_cast<uint32_t>(p1) << 16) | p0;
                const uint32_t lut1 = (static_cast<uint32_t>(p3) << 16) | p2;
                const uint32_t lut2 = (static_cast<uint32_t>(p5) << 16) | p4;
                const uint32_t lut3 = (static_cast<uint32_t>(p7) << 16) | p6;

                uint32_t *__restrict__ row32 =
                    reinterpret_cast<uint32_t *>(buf + static_cast<size_t>(y) * WIDTH);

                constexpr uint16_t UNROLL = 4;
                uint16_t x = 0;
                for (; x + UNROLL <= widthHalf; x += UNROLL)
                {
                    row32[x + 0] = lut0;
                    row32[x + 1] = lut1;
                    row32[x + 2] = lut2;
                    row32[x + 3] = lut3;
                }
                for (; x < widthHalf; ++x)
                {
                    const uint16_t idx2 = static_cast<uint16_t>(x & 3u);
                    row32[x] = (idx2 == 0u)   ? lut0
                               : (idx2 == 1u) ? lut1
                               : (idx2 == 2u) ? lut2
                                              : lut3;
                }
            }
        }

        template <uint16_t WIDTH, uint16_t HEIGHT>
        __attribute__((always_inline, hot)) PIP3D_FLATTEN inline void IRAM_ATTR
        fillBackgroundSolid() noexcept
        {
            uint16_t *__restrict__ buf = storage[activeSlot];
            const uint32_t fill = baseClear32;
            constexpr uint16_t widthHalf = WIDTH >> 1;
            constexpr uint16_t UNROLL = 8;

            static_assert((WIDTH & 1u) == 0, "WIDTH must be even");
            static_assert((widthHalf % UNROLL) == 0,
                          "WIDTH/2 must be multiple of UNROLL");

            for (uint16_t y = 0; y < HEIGHT; ++y)
            {
                uint32_t *__restrict__ row32 =
                    reinterpret_cast<uint32_t *>(buf + static_cast<size_t>(y) * WIDTH);

                for (uint16_t x = 0; x < widthHalf; x += UNROLL)
                {
                    row32[x + 0] = fill;
                    row32[x + 1] = fill;
                    row32[x + 2] = fill;
                    row32[x + 3] = fill;
                    row32[x + 4] = fill;
                    row32[x + 5] = fill;
                    row32[x + 6] = fill;
                    row32[x + 7] = fill;
                }
            }
        }

    public:
        FrameBuffer() noexcept
            : storage{},
              skyboxCache{},
              baseClear32(0u),
              clearColor(Color::BLACK),
              activeSlot(0),
              useSkybox(true),
              cacheValid(false),
              config(),
#if defined(PIP3D_PC)
              displayReady(false)
#else
              display(nullptr)
#endif
        {
            skybox.setPreset(DAY);
        }

        ~FrameBuffer() = default;

        FrameBuffer(const FrameBuffer &) = delete;
        FrameBuffer &operator=(const FrameBuffer &) = delete;

        bool init(const DisplayConfig &cfg
#if defined(PIP3D_PC)
                  ,
                  bool pcReady
#else
                  ,
                  pipcore::Display *disp
#endif
                  ) noexcept
        {
            if (cfg.width != kWidth || cfg.height != kHeight)
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RENDER,
                     "FrameBuffer::init: config %ux%u does not match static storage %ux%u",
                     (unsigned)cfg.width, (unsigned)cfg.height,
                     (unsigned)kWidth, (unsigned)kHeight);
                return false;
            }

            config = cfg;
#if defined(PIP3D_PC)
            displayReady = pcReady;
#else
            display = disp;
#endif

            baseClear32 = (static_cast<uint32_t>(clearColor.rgb565) << 16) | static_cast<uint32_t>(clearColor.rgb565);

            ensureSkyboxCache();

            return true;
        }

        __attribute__((always_inline)) inline uint16_t *getBuffer() noexcept
        {
            return storage[activeSlot];
        }

        __attribute__((always_inline)) inline const uint16_t *getBuffer() const noexcept
        {
            return storage[activeSlot];
        }

        __attribute__((always_inline)) inline uint16_t *getStagingBufferForFlush() noexcept
        {
            return storage[activeSlot];
        }

        __attribute__((always_inline)) inline void swapStagingSlot() noexcept
        {
            activeSlot = static_cast<uint8_t>(activeSlot ^ 1u);
        }

        __attribute__((always_inline)) inline uint8_t getActiveSlot() const noexcept
        {
            return activeSlot;
        }

        __attribute__((always_inline)) inline const DisplayConfig &getConfig() const noexcept
        {
            return config;
        }

        __attribute__((always_inline)) inline Skybox &getSkybox() noexcept { return skybox; }

        __attribute__((always_inline)) inline bool isSkyboxEnabled() const noexcept
        {
            return useSkybox;
        }

        __attribute__((always_inline)) inline void setSkyboxEnabled(bool enabled) noexcept
        {
            if (useSkybox != enabled)
            {
                useSkybox = enabled;
                cacheValid = false;
            }
        }

        __attribute__((always_inline)) inline void setSkyboxType(SkyboxType type) noexcept
        {
            skybox.setPreset(type);
            cacheValid = false;
        }

        __attribute__((always_inline)) inline void invalidateSkyboxCache() noexcept
        {
            cacheValid = false;
        }

        __attribute__((always_inline)) inline void setClearColor(Color color) noexcept
        {
            clearColor = color;
            baseClear32 = (static_cast<uint32_t>(color.rgb565) << 16) | static_cast<uint32_t>(color.rgb565);
        }

        __attribute__((always_inline)) inline CloudLayer &getClouds() noexcept { return clouds; }
        __attribute__((always_inline)) inline bool areCloudsEnabled() const noexcept { return clouds.enabled; }
        __attribute__((always_inline)) inline void setCloudsEnabled(bool e) noexcept { clouds.enabled = e; }
        __attribute__((always_inline)) inline void setCloudColor(Color c) noexcept { clouds.setCloudColor(c); }
        __attribute__((always_inline)) inline void setCloudAlpha(float a) noexcept { clouds.setCloudAlpha(a); }
        __attribute__((always_inline)) inline void setCloudHeight(float m) noexcept { clouds.setCloudHeight(m); }
        __attribute__((always_inline)) inline void setCloudScale(float m) noexcept { clouds.setCloudScale(m); }
        __attribute__((always_inline)) inline void setCloudDrift(float sx, float sz) noexcept { clouds.setDrift(sx, sz); }
        __attribute__((always_inline)) inline void updateClouds(float dt) noexcept { clouds.update(dt); }

        __attribute__((always_inline)) inline void
        generateClouds(uint32_t seed, float coverage) noexcept
        {
            clouds.generatePanorama(seed, coverage, SCREEN_HEIGHT, skybox);
        }

        template <uint16_t WIDTH, uint16_t HEIGHT>
        __attribute__((always_inline, hot)) inline void
        drawClouds(const Vector3 &camPos,
                   const Vector3 &fwd, const Vector3 &right, const Vector3 &up,
                   float vfovRad, float hfovRad) noexcept
        {
            clouds.drawClouds<WIDTH, HEIGHT>(storage[activeSlot], g_bandOffsetY,
                                             camPos, fwd, right, up, vfovRad, hfovRad);
        }

        __attribute__((always_inline)) inline void
        endFrameRegion(int16_t x, int16_t y, int16_t w, int16_t h) noexcept
        {
            if (unlikely(!readyForFlush()))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RENDER,
                     "FrameBuffer::endFrameRegion called with invalid state (buffer=%p)",
                     (void *)getBuffer());
                return;
            }

            if (w <= 0 || h <= 0)
                return;

            const int16_t bandTop = g_bandOffsetY;
            const int16_t bandBottom = static_cast<int16_t>(bandTop + config.height);

            const int16_t x0 = (x < 0) ? 0 : x;
            const int16_t y0 = (y < bandTop) ? bandTop : y;
            const int16_t x1 = (x + w > config.width) ? config.width
                                                      : static_cast<int16_t>(x + w);
            const int16_t y1 = (y + h > bandBottom) ? bandBottom
                                                    : static_cast<int16_t>(y + h);

            if (x1 <= x0 || y1 <= y0)
                return;

            const int16_t localY = static_cast<int16_t>(y0 - bandTop);
            if (localY < 0 || localY >= config.height)
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RENDER,
                     "FrameBuffer::endFrameRegion localY out of bounds (y=%d, localY=%d, bandOffset=%d, bandHeight=%d, fbHeight=%d)",
                     static_cast<int>(y),
                     static_cast<int>(localY),
                     static_cast<int>(g_bandOffsetY),
                     static_cast<int>(g_bandHeight),
                     static_cast<int>(config.height));
                return;
            }

            const int16_t clippedW = static_cast<int16_t>(x1 - x0);
            const int16_t clippedH = static_cast<int16_t>(y1 - y0);
            const uint16_t *region = getBuffer() + static_cast<size_t>(localY) * config.width + static_cast<size_t>(x0);

#if defined(PIP3D_PC)
            for (int16_t row = 0; row < clippedH; ++row)
            {
                uint16_t *line = const_cast<uint16_t *>(region) + static_cast<size_t>(row) * config.width;
                for (int16_t col = 0; col < clippedW; ++col)
                    line[col] = __builtin_bswap16(line[col]);
            }
            pipcore::desktop::Runtime::instance().writeRect565(
                x0, y0, clippedW, clippedH, region, config.width);
            for (int16_t row = 0; row < clippedH; ++row)
            {
                uint16_t *line = const_cast<uint16_t *>(region) + static_cast<size_t>(row) * config.width;
                for (int16_t col = 0; col < clippedW; ++col)
                    line[col] = __builtin_bswap16(line[col]);
            }
#else
            display->writeRect565(x0, y0, clippedW, clippedH, region, config.width);
#endif
        }

        template <uint16_t WIDTH, uint16_t HEIGHT>
        __attribute__((always_inline, hot)) PIP3D_FLATTEN inline void IRAM_ATTR
        fillBackground(float pitchShiftRows) noexcept
        {
            if (useSkybox && skybox.enabled)
            {
                ensureSkyboxCache();
                fillBackgroundSky<WIDTH, HEIGHT>(pitchShiftRows);
            }
            else
            {
                fillBackgroundSolid<WIDTH, HEIGHT>();
            }
        }
    };
}