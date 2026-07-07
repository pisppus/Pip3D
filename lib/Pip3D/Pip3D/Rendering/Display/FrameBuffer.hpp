#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "Core/Platform.hpp"
#include "Core/Viewport.hpp"
#include "Core/Memory.hpp"
#include "Core/Color.hpp"
#include "Math/Algebra.hpp"

#if defined(PIP3D_PC)
#include <PipCore/Platforms/Desktop/Runtime.hpp>
#else
#include <PipCore/Display.hpp>
#endif

#include "Rendering/Display/Sky.hpp"
#include "Rendering/Display/Clouds.hpp"

namespace pip3D
{
    class __attribute__((aligned(16))) FrameBuffer
    {
    private:
        Skybox skybox;
        CloudLayer clouds;
        uint16_t *buffer[2];
        uint8_t activeSlot;
        uint16_t *skyboxColorCache;
#if !defined(PIP3D_PC)
        pipcore::Display *display;
#endif
        DisplayConfig config;
        Color clearColor;
        bool useSkybox;
#if defined(PIP3D_PC)
        bool displayReady;
#endif
        bool cacheValid;

        static constexpr size_t DMA_ALIGNMENT = 64;
        static constexpr uint8_t bayer4x4[16] = {
            0, 8, 2, 10,
            12, 4, 14, 6,
            3, 11, 1, 9,
            15, 7, 13, 5};

        __attribute__((always_inline)) inline bool readyForFlush() const
        {
#if defined(PIP3D_PC)
            return buffer[0] && displayReady;
#else
            return buffer[0] && display;
#endif
        }

        __attribute__((always_inline)) inline void rebuildSkyboxCache()
        {
            if (!skyboxColorCache || !useSkybox || !skybox.enabled)
                return;

            for (int16_t y = 0; y < SCREEN_HEIGHT; ++y)
            {
                skyboxColorCache[y] = skybox.getColorAtY(y, SCREEN_HEIGHT).rgb565;
            }

            cacheValid = true;
        }

        __attribute__((always_inline)) inline void ensureSkyboxCache()
        {
            if (useSkybox && skybox.enabled && !cacheValid)
                rebuildSkyboxCache();
        }

#if defined(PIP3D_PC)
        __attribute__((always_inline)) inline void swapEndianInPlace(uint32_t count)
        {
            uint16_t *buf = buffer[activeSlot];
            uint32_t *fb32 = reinterpret_cast<uint32_t *>(buf);
            const uint32_t count32 = count >> 1;
            for (uint32_t i = 0; i < count32; ++i)
            {
                const uint32_t v = fb32[i];
                fb32[i] = ((v & 0x00FF00FFu) << 8) | ((v >> 8) & 0x00FF00FFu);
            }
            if (count & 1u)
                buf[count - 1] = __builtin_bswap16(buf[count - 1]);
        }
#endif

    public:
        FrameBuffer() : skybox(),
                        buffer{nullptr, nullptr},
                        activeSlot(0),
                        skyboxColorCache(nullptr),
#if !defined(PIP3D_PC)
                        display(nullptr),
#endif
                        config(),
                        clearColor(Color::BLACK),
                        useSkybox(true),
#if defined(PIP3D_PC)
                        displayReady(false),
#endif
                        cacheValid(false)
        {
            skybox.setPreset(DAY);
        }

        bool init(const DisplayConfig &cfg,
#if defined(PIP3D_PC)
                  bool pcReady
#else
                  pipcore::Display *disp
#endif
        )
        {
            if (buffer[0] || buffer[1])
                return false;

            config = cfg;
#if defined(PIP3D_PC)
            displayReady = pcReady;
#else
            display = disp;
#endif

            const uint32_t totalPixels =
                static_cast<uint32_t>(config.width) * static_cast<uint32_t>(config.height);

            size_t bufferSize = totalPixels * sizeof(uint16_t);
            bufferSize = (bufferSize + DMA_ALIGNMENT - 1) & ~(DMA_ALIGNMENT - 1);

            for (int slot = 0; slot < 2; ++slot)
            {
                buffer[slot] = static_cast<uint16_t *>(
                    MemUtils::allocAligned(bufferSize, DMA_ALIGNMENT, pipcore::AllocCaps::PreferInternal));
                if (!buffer[slot])
                {
                    LOGE(::pip3D::Debug::LOG_MODULE_RENDER,
                         "FrameBuffer::init failed: SRAM OOM (staging slot %d, %u bytes)!",
                         slot, (unsigned)bufferSize);
                    for (int s = 0; s < slot; ++s)
                    {
                        MemUtils::freeAligned(buffer[s]);
                        buffer[s] = nullptr;
                    }
                    return false;
                }
                memset(buffer[slot], 0, bufferSize);
            }
            activeSlot = 0;

            skyboxColorCache = static_cast<uint16_t *>(MemUtils::allocAligned(
                SCREEN_HEIGHT * sizeof(uint16_t), 4, pipcore::AllocCaps::PreferInternal));

            return (skyboxColorCache != nullptr);
        }

        ~FrameBuffer()
        {
            for (int slot = 0; slot < 2; ++slot)
            {
                if (buffer[slot])
                {
                    MemUtils::freeAligned(buffer[slot]);
                    buffer[slot] = nullptr;
                }
            }
            activeSlot = 0;
            if (skyboxColorCache)
            {
                MemUtils::freeAligned(skyboxColorCache);
                skyboxColorCache = nullptr;
            }
            clouds.free();
        }

        FrameBuffer(const FrameBuffer &) = delete;
        FrameBuffer &operator=(const FrameBuffer &) = delete;

        __attribute__((always_inline)) inline uint16_t *getBuffer() { return buffer[activeSlot]; }
        __attribute__((always_inline)) inline const uint16_t *getBuffer() const { return buffer[activeSlot]; }
        __attribute__((always_inline)) inline uint16_t *getStagingBufferForFlush()
        {
            return buffer[activeSlot];
        }

        __attribute__((always_inline)) inline void swapStagingSlot()
        {
            activeSlot ^= 1u;
        }

        __attribute__((always_inline)) inline uint8_t getActiveSlot() const { return activeSlot; }

        __attribute__((always_inline)) inline const DisplayConfig &getConfig() const { return config; }

        __attribute__((always_inline)) inline Skybox &getSkybox() { return skybox; }
        __attribute__((always_inline)) inline const Skybox &getSkybox() const { return skybox; }
        __attribute__((always_inline)) inline bool isSkyboxEnabled() const { return useSkybox; }

        __attribute__((always_inline)) inline void setSkyboxEnabled(bool enabled)
        {
            if (useSkybox != enabled)
            {
                useSkybox = enabled;
                cacheValid = false;
            }
        }

        __attribute__((always_inline)) inline void setSkyboxType(SkyboxType type)
        {
            skybox.setPreset(type);
            cacheValid = false;
        }

        __attribute__((always_inline)) inline void setClearColor(Color color) { clearColor = color; }

        __attribute__((always_inline)) inline void invalidateSkyboxCache()
        {
            cacheValid = false;
        }

        __attribute__((always_inline)) inline CloudLayer &getClouds() { return clouds; }
        __attribute__((always_inline)) inline const CloudLayer &getClouds() const { return clouds; }
        __attribute__((always_inline)) inline bool areCloudsEnabled() const { return clouds.enabled; }

        __attribute__((always_inline)) inline void setCloudsEnabled(bool e) { clouds.enabled = e; }
        __attribute__((always_inline)) inline void setCloudColor(Color c) { clouds.setCloudColor(c); }
        __attribute__((always_inline)) inline void setCloudAlpha(float a) { clouds.setCloudAlpha(a); }

        __attribute__((always_inline)) inline void generateClouds(uint32_t seed, float coverage)
        {
            clouds.generatePanorama(seed, coverage, SCREEN_HEIGHT, skybox);
        }

        template <uint16_t WIDTH, uint16_t HEIGHT>
        __attribute__((always_inline, hot)) inline void
        drawClouds(float yawRad, float pitchShiftRows, float hfovRad)
        {
            clouds.drawClouds<WIDTH, HEIGHT>(buffer[activeSlot], currentBandOffsetY(),
                                             yawRad, pitchShiftRows, hfovRad);
        }

        __attribute__((always_inline)) inline void endFrame()
        {
            if (unlikely(!readyForFlush()))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RENDER,
                     "FrameBuffer::endFrame called with invalid state (buffer=%p)",
                     (void *)getBuffer());
                return;
            }

            const uint32_t count = static_cast<uint32_t>(config.width) * static_cast<uint32_t>(config.height);
#if defined(PIP3D_PC)
            swapEndianInPlace(count);
            pipcore::desktop::Runtime::instance().writeRect565(0, 0, config.width, config.height, getBuffer(), config.width);
            swapEndianInPlace(count);
#else
            display->writeRect565(0, 0, config.width, config.height, getBuffer(), config.width);
#endif
        }

        __attribute__((always_inline)) inline void endFrameRegion(int16_t x, int16_t y, int16_t w, int16_t h)
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

            const int16_t bandTop = currentBandOffsetY();
            const int16_t bandBottom = static_cast<int16_t>(bandTop + config.height);

            const int16_t x0 = (x < 0) ? 0 : x;
            const int16_t y0 = (y < bandTop) ? bandTop : y;
            const int16_t x1 = (x + w > config.width) ? config.width : static_cast<int16_t>(x + w);
            const int16_t y1 = (y + h > bandBottom) ? bandBottom : static_cast<int16_t>(y + h);

            if (x1 <= x0 || y1 <= y0)
                return;

            const int16_t localY = static_cast<int16_t>(y0 - bandTop);
            if (localY < 0 || localY >= config.height)
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RENDER,
                     "FrameBuffer::endFrameRegion localY out of bounds (y=%d, localY=%d, bandOffset=%d, bandHeight=%d, fbHeight=%d)",
                     static_cast<int>(y),
                     static_cast<int>(localY),
                     static_cast<int>(currentBandOffsetY()),
                     static_cast<int>(currentBandHeight()),
                     static_cast<int>(config.height));
                return;
            }

            const int16_t clippedW = static_cast<int16_t>(x1 - x0);
            const int16_t clippedH = static_cast<int16_t>(y1 - y0);
            const uint16_t *region = getBuffer() + static_cast<size_t>(localY) * config.width + x0;

#if defined(PIP3D_PC)
            for (int16_t row = 0; row < clippedH; ++row)
            {
                uint16_t *line = const_cast<uint16_t *>(region) + static_cast<size_t>(row) * config.width;
                for (int16_t col = 0; col < clippedW; ++col)
                    line[col] = __builtin_bswap16(line[col]);
            }
            pipcore::desktop::Runtime::instance().writeRect565(x0, y0, clippedW, clippedH, region, config.width);
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
        __attribute__((always_inline, hot)) inline void
        fillBackground(float pitchShiftRows)
        {
            uint16_t *__restrict__ buf = buffer[activeSlot];
            if (unlikely(!buf))
                return;

            const bool skyActive = useSkybox && skybox.enabled;
            ensureSkyboxCache();

            const uint16_t baseClear = clearColor.rgb565;
            const uint32_t baseClear32 = (static_cast<uint32_t>(baseClear) << 16) | baseClear;

            const int16_t bandOffY = currentBandOffsetY();
            constexpr uint16_t widthHalf = WIDTH >> 1;
            constexpr int16_t maxY = static_cast<int16_t>(SCREEN_HEIGHT) - 1;
            const int32_t shiftI = static_cast<int32_t>(pitchShiftRows);

            for (uint16_t y = 0; y < HEIGHT; ++y)
            {
                const int16_t globalY = bandOffY + static_cast<int16_t>(y);

                uint32_t lutEven;
                uint32_t lutOdd;
                if (skyActive)
                {
                    int32_t vy = static_cast<int32_t>(globalY) - shiftI;
                    if (vy < 0)
                        vy = 0;
                    else if (vy > maxY)
                        vy = maxY;
                    const uint16_t base = skyboxColorCache[static_cast<size_t>(vy)];
                    const uint8_t brow = static_cast<uint8_t>(globalY & 3u);
                    const uint8_t t0 = bayer4x4[brow * 4 + 0];
                    const uint8_t t1 = bayer4x4[brow * 4 + 1];
                    const uint8_t t2 = bayer4x4[brow * 4 + 2];
                    const uint8_t t3 = bayer4x4[brow * 4 + 3];

                    const Color baseColor(base);
                    const uint16_t d0 = baseColor.dither(t0).rgb565;
                    const uint16_t d1 = baseColor.dither(t1).rgb565;
                    const uint16_t d2 = baseColor.dither(t2).rgb565;
                    const uint16_t d3 = baseColor.dither(t3).rgb565;
                    lutEven = (static_cast<uint32_t>(d1) << 16) | d0;
                    lutOdd = (static_cast<uint32_t>(d3) << 16) | d2;
                }
                else
                {
                    lutEven = baseClear32;
                    lutOdd = baseClear32;
                }

                uint32_t *__restrict__ row32 =
                    reinterpret_cast<uint32_t *>(buf + static_cast<size_t>(y) * WIDTH);

                constexpr uint16_t widthHalf = WIDTH >> 1;
                uint16_t x = 0;

                constexpr uint16_t UNROLL = 4;
                for (; x + UNROLL <= widthHalf; x += UNROLL)
                {
                    row32[x + 0] = lutEven;
                    row32[x + 1] = lutOdd;
                    row32[x + 2] = lutEven;
                    row32[x + 3] = lutOdd;
                }
                for (; x < widthHalf; ++x)
                {
                    row32[x] = (x & 1u) ? lutOdd : lutEven;
                }

                if constexpr (WIDTH & 1u)
                {
                    const uint8_t tLast = bayer4x4[(static_cast<uint8_t>(globalY) & 3u) * 4 +
                                                   ((WIDTH - 1) & 3u)];
                    int32_t vyL = static_cast<int32_t>(globalY) - shiftI;
                    if (vyL < 0)
                        vyL = 0;
                    else if (vyL > maxY)
                        vyL = maxY;
                    const uint16_t lastColor = skyActive ? Color(skyboxColorCache[static_cast<size_t>(vyL)]).dither(tLast).rgb565
                                                         : baseClear;
                    buf[static_cast<size_t>(y) * WIDTH + WIDTH - 1] = lastColor;
                }
            }
        }
    };
}