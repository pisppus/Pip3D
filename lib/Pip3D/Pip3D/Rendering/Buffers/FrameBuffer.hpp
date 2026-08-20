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

        struct alignas(4) SkyColor888
        {
            uint8_t r, g, b, _pad;
        };

        struct DitherStep
        {
            uint8_t dr, dg;
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
        static constexpr DitherStep kDither[64] = {
            {0, 0}, {4, 2}, {1, 0}, {5, 2}, {0, 0}, {4, 2}, {1, 0}, {5, 2},
            {6, 3}, {2, 1}, {7, 3}, {3, 1}, {6, 3}, {2, 1}, {7, 3}, {3, 1},
            {1, 0}, {5, 2}, {0, 0}, {4, 2}, {1, 0}, {5, 2}, {0, 0}, {4, 2},
            {7, 3}, {3, 1}, {6, 3}, {2, 1}, {7, 3}, {3, 1}, {6, 3}, {2, 1},
            {0, 0}, {4, 2}, {1, 0}, {5, 2}, {0, 0}, {4, 2}, {1, 0}, {5, 2},
            {6, 3}, {2, 1}, {7, 3}, {3, 1}, {6, 3}, {2, 1}, {7, 3}, {3, 1},
            {1, 0}, {5, 2}, {0, 0}, {4, 2}, {1, 0}, {5, 2}, {0, 0}, {4, 2},
            {7, 3}, {3, 1}, {6, 3}, {2, 1}, {7, 3}, {3, 1}, {6, 3}, {2, 1}
        };

        static PIP3D_FORCE_INLINE constexpr uint16_t
        pack565Dithered(uint32_t r8, uint32_t g8, uint32_t b8,
                        uint8_t dr, uint8_t dg) noexcept
        {
            uint32_t r = (r8 + dr) >> 3;
            uint32_t g = (g8 + dg) >> 2;
            uint32_t b = (b8 + dr) >> 3;
            if (r > 31u)
                r = 31u;
            if (g > 63u)
                g = 63u;
            if (b > 31u)
                b = 31u;
            return static_cast<uint16_t>((r << 11) | (g << 5) | b);
        }

        static PIP3D_FORCE_INLINE int32_t
        clampRow(int32_t v, int32_t maxY) noexcept
        {
            v = (v < 0) ? 0 : v;
            v = (v > maxY) ? maxY : v;
            return v;
        }

        __attribute__((always_inline)) inline void updateBaseClear32() noexcept
        {
            baseClear32 = (static_cast<uint32_t>(clearColor.rgb565) << 16) | static_cast<uint32_t>(clearColor.rgb565);
        }

        __attribute__((always_inline)) inline void ensureSkyboxCache() noexcept
        {
            if (cacheValid || !useSkybox || !skybox.enabled)
                return;

            for (int16_t y = 0; y < SCREEN_HEIGHT; ++y)
                skybox.getColorAtY888(y, skyboxCache[y].r,
                                      skyboxCache[y].g,
                                      skyboxCache[y].b);

            cacheValid = true;
        }

        __attribute__((always_inline)) inline bool readyForFlush() const noexcept
        {
#if defined(PIP3D_PC)
            return displayReady;
#else
            return display != nullptr;
#endif
        }

        template <uint16_t WIDTH, uint16_t HEIGHT>
        __attribute__((always_inline, hot)) PIP3D_FLATTEN inline void IRAM_ATTR
        fillBackgroundSky(float pitchShiftRows) noexcept
        {
            uint16_t *__restrict__ buf = storage[activeSlot];

            const int16_t bandOffY = g_bandOffsetY;
            const int32_t maxY32 = static_cast<int32_t>(SCREEN_HEIGHT - 1);
            const int32_t shiftI = static_cast<int32_t>(lroundf(pitchShiftRows));

            constexpr uint16_t widthHalf = WIDTH >> 1;
            static_assert((WIDTH & 1u) == 0, "WIDTH must be even");
            static_assert((widthHalf & 3u) == 0, "WIDTH/2 must be multiple of 4");

            for (uint16_t y = 0; y < HEIGHT; ++y)
            {
                const int16_t globalY = static_cast<int16_t>(bandOffY + y);
                const int32_t vy = clampRow(static_cast<int32_t>(globalY) - shiftI, maxY32);
                const SkyColor888 &c888 = skyboxCache[static_cast<uint32_t>(vy)];

                const uint32_t R8 = c888.r;
                const uint32_t G8 = c888.g;
                const uint32_t B8 = c888.b;

                const DitherStep *PIP3D_RESTRICT dRow = &kDither[(globalY & 7u) * 8u];

                const uint16_t p0 = pack565Dithered(R8, G8, B8, dRow[0].dr, dRow[0].dg);
                const uint16_t p1 = pack565Dithered(R8, G8, B8, dRow[1].dr, dRow[1].dg);
                const uint16_t p2 = pack565Dithered(R8, G8, B8, dRow[2].dr, dRow[2].dg);
                const uint16_t p3 = pack565Dithered(R8, G8, B8, dRow[3].dr, dRow[3].dg);
                const uint16_t p4 = pack565Dithered(R8, G8, B8, dRow[4].dr, dRow[4].dg);
                const uint16_t p5 = pack565Dithered(R8, G8, B8, dRow[5].dr, dRow[5].dg);
                const uint16_t p6 = pack565Dithered(R8, G8, B8, dRow[6].dr, dRow[6].dg);
                const uint16_t p7 = pack565Dithered(R8, G8, B8, dRow[7].dr, dRow[7].dg);

                const uint32_t lut0 = (static_cast<uint32_t>(p1) << 16) | p0;
                const uint32_t lut1 = (static_cast<uint32_t>(p3) << 16) | p2;
                const uint32_t lut2 = (static_cast<uint32_t>(p5) << 16) | p4;
                const uint32_t lut3 = (static_cast<uint32_t>(p7) << 16) | p6;

                uint32_t *__restrict__ row32 =
                    reinterpret_cast<uint32_t *>(buf + static_cast<size_t>(y) * WIDTH);

                constexpr uint16_t UNROLL = 4;
                for (uint16_t x = 0; x < widthHalf; x += UNROLL)
                {
                    row32[x + 0] = lut0;
                    row32[x + 1] = lut1;
                    row32[x + 2] = lut2;
                    row32[x + 3] = lut3;
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
                     "FB init: cfg %ux%u != static %ux%u",
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

            updateBaseClear32();
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
            activeSlot ^= 1u;
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
            updateBaseClear32();
        }

        __attribute__((always_inline)) inline CloudLayer &getClouds() noexcept { return clouds; }
        __attribute__((always_inline)) inline bool areCloudsEnabled() const noexcept { return clouds.isReady(); }
        __attribute__((always_inline)) inline void setCloudsEnabled(bool e) noexcept { clouds.setEnabled(e); }
        __attribute__((always_inline)) inline void setCloudColor(Color c) noexcept { clouds.setCloudColor(c); }
        __attribute__((always_inline)) inline void setCloudAlpha(float a) noexcept { clouds.setCloudAlpha(a); }
        __attribute__((always_inline)) inline void setCloudHeight(float m) noexcept { clouds.setCloudHeight(m); }
        __attribute__((always_inline)) inline void setCloudScale(float m) noexcept { clouds.setCloudScale(m); }
        __attribute__((always_inline)) inline void setCloudDriftAngle(float angleDeg, float speedMps) noexcept { clouds.setDriftAngle(angleDeg, speedMps); }
        __attribute__((always_inline)) inline void updateClouds(float dt) noexcept { clouds.update(dt); }

        __attribute__((always_inline)) inline void
        generateClouds(uint32_t seed, float coverage) noexcept
        {
            (void)seed;
            (void)coverage;
            clouds.reset();
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

        template <uint16_t WIDTH, uint16_t HEIGHT>
        __attribute__((always_inline, hot)) inline void
        drawCloudsZTested(const Vector3 &camPos,
                          const Vector3 &fwd, const Vector3 &right, const Vector3 &up,
                          float vfovRad, float hfovRad,
                          const uint16_t *zBuf) noexcept
        {
            clouds.drawCloudsZTested<WIDTH, HEIGHT>(storage[activeSlot], zBuf,
                                                    g_bandOffsetY,
                                                    camPos, fwd, right, up,
                                                    vfovRad, hfovRad);
        }

        __attribute__((always_inline)) inline void
        endFrameRegion(int16_t x, int16_t y, int16_t w, int16_t h) noexcept
        {
            if (unlikely(!readyForFlush()))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RENDER,
                     "FB endFrameRegion: invalid state buf=%p", (void *)getBuffer());
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
                     "FB endFrameRegion: localY OOB y=%d ly=%d band=%d fbH=%d",
                     (int)y, (int)localY, (int)g_bandOffsetY, (int)config.height);
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