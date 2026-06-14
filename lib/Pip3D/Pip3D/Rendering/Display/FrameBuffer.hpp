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

#include "ZBuffer.hpp"
#include "Rendering/Display/Sky.hpp"

namespace pip3D
{
    class __attribute__((aligned(16))) FrameBuffer
    {
    private:
        DisplayConfig config;
        Skybox skybox;
        uint16_t *buffer;
#if !defined(PIP3D_PC)
        pipcore::Display *display;
#endif
        uint16_t *skyboxColorCache;
        uint32_t totalPixels;
        uint32_t pixels32;
        Color clearColor;
        bool useSkybox;
        bool oddPixels;
        bool cacheValid;
#if defined(PIP3D_PC)
        bool displayReady;
#endif

        static constexpr size_t DMA_ALIGNMENT = 64;

    public:
        FrameBuffer() : config(),
                        skybox(),
                        buffer(nullptr),
#if !defined(PIP3D_PC)
                        display(nullptr),
#endif
                        skyboxColorCache(nullptr),
                        totalPixels(0),
                        pixels32(0),
                        clearColor(Color::BLACK),
                        useSkybox(true),
                        oddPixels(false),
                        cacheValid(false)
#if defined(PIP3D_PC)
                        ,
                        displayReady(false)
#endif
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
            if (buffer)
                return false;

            config = cfg;
#if defined(PIP3D_PC)
            displayReady = pcReady;
#else
            display = disp;
#endif

            totalPixels = static_cast<uint32_t>(config.width) * static_cast<uint32_t>(config.height);
            pixels32 = totalPixels >> 1;
            oddPixels = (totalPixels & 1u) != 0;

            size_t bufferSize = totalPixels * sizeof(uint16_t);
            bufferSize = (bufferSize + DMA_ALIGNMENT - 1) & ~(DMA_ALIGNMENT - 1);

            buffer = (uint16_t *)pip3D::MemUtils::allocAligned(bufferSize, DMA_ALIGNMENT, pipcore::AllocCaps::PreferInternal);

            if (!buffer)
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RENDER, "FrameBuffer::init failed: SRAM OOM!");
                return false;
            }

            memset(buffer, 0, bufferSize);

            skyboxColorCache = (uint16_t *)pip3D::MemUtils::allocAligned(
                SCREEN_HEIGHT * 2 * sizeof(uint16_t), 4, pipcore::AllocCaps::PreferInternal);

            return (skyboxColorCache != nullptr);
        }

        __attribute__((always_inline)) inline void beginFrame()
        {
            if (unlikely(!buffer))
                return;
        }

        __attribute__((always_inline)) inline void invalidateSkyboxCache()
        {
            cacheValid = false;
        }

        __attribute__((always_inline)) inline void ensureSkyboxCache()
        {
            if (useSkybox && skybox.enabled && skyboxColorCache && !cacheValid)
                rebuildSkyboxCache();
        }

        template <uint16_t WIDTH, uint16_t HEIGHT>
        __attribute__((always_inline)) inline void drawSkyboxWhereEmpty(const ZBuffer<WIDTH, HEIGHT> &zbuf)
        {
            if (unlikely(!buffer))
                return;

            const int16_t *__restrict__ zb = zbuf.getBufferPtr();
            if (unlikely(!zb))
                return;

            const int16_t clearDepth = ZBuffer<WIDTH, HEIGHT>::clearDepthValue();
            const int16_t invShadowMask = ~ZBuffer<WIDTH, HEIGHT>::shadowFlagMask();
            const int32_t clearDepth32 = (static_cast<int32_t>(clearDepth) << 16) | static_cast<uint16_t>(clearDepth);
            const int32_t invShadowMask32 = (static_cast<int32_t>(invShadowMask) << 16) | static_cast<uint16_t>(invShadowMask);

            const bool hasSkyboxCache = useSkybox && skybox.enabled && skyboxColorCache && cacheValid;
            const bool hasSkyboxRaw = useSkybox && skybox.enabled && !hasSkyboxCache;
            const uint16_t baseClear = clearColor.rgb565;
            const uint32_t baseClear32 = (static_cast<uint32_t>(baseClear) << 16) | baseClear;

            const int16_t bandOffY = currentBandOffsetY();

            constexpr uint16_t widthHalf = WIDTH >> 1;

            for (uint16_t y = 0; y < HEIGHT; ++y)
            {
                const int16_t globalY = bandOffY + static_cast<int16_t>(y);

                uint32_t colorLUT32;

                if (hasSkyboxCache & (globalY < SCREEN_HEIGHT))
                {
                    const uint16_t *c = skyboxColorCache + globalY * 2;
                    const uint16_t c0 = c[0];
                    const uint16_t c1 = (y & 1u) ? c[1] : c[0];
                    colorLUT32 = (static_cast<uint32_t>(c1) << 16) | c0;
                }
                else if (hasSkyboxRaw & (globalY < SCREEN_HEIGHT))
                {
                    const uint16_t color1 = skybox.getColorAtY(globalY, SCREEN_HEIGHT).rgb565;
                    uint16_t darker = color1;
#if !defined(PIP3D_PC)
                    if (color1 != 0)
                    {
                        const uint16_t r = color1 >> 11;
                        const uint16_t g = (color1 >> 5) & 0x3Fu;
                        const uint16_t b = color1 & 0x1Fu;
                        darker = (((r - (r != 0)) & 0x1Fu) << 11) |
                                 (((g - (g != 0)) & 0x3Fu) << 5) |
                                 ((b - (b != 0)) & 0x1Fu);
                    }
                    const uint16_t c1 = (y & 1u) ? darker : color1;
                    colorLUT32 = (static_cast<uint32_t>(c1) << 16) | color1;
#else
                    colorLUT32 = (static_cast<uint32_t>(color1) << 16) | color1;
#endif
                }
                else
                {
                    colorLUT32 = baseClear32;
                }

                uint32_t *__restrict__ row32 = reinterpret_cast<uint32_t *>(buffer + static_cast<size_t>(y) * WIDTH);
                const int32_t *__restrict__ zbRow32 = reinterpret_cast<const int32_t *>(zb + static_cast<size_t>(y) * WIDTH);

                for (uint16_t x = 0; x < widthHalf; ++x)
                {
                    PIP3D_PREFETCH_R(&zbRow32[x + 8]);
                    PIP3D_PREFETCH_W(&row32[x + 8]);

                    const int32_t zb2 = zbRow32[x];
                    const int32_t d2 = zb2 & invShadowMask32;

                    const int16_t d0 = static_cast<int16_t>(zb2) & invShadowMask;
                    const int16_t d1 = static_cast<int16_t>(zb2 >> 16) & invShadowMask;
                    const uint32_t mask0 = static_cast<uint32_t>(-static_cast<int32_t>(d0 == clearDepth)) & 0xFFFFu;
                    const uint32_t mask1 = static_cast<uint32_t>(-static_cast<int32_t>(d1 == clearDepth)) << 16;
                    const uint32_t mask32 = mask1 | mask0;

                    row32[x] = (colorLUT32 & mask32) | (row32[x] & ~mask32);
                }

                if (unlikely(WIDTH & 1u))
                {
                    const int16_t dLast = zb[static_cast<size_t>(y) * WIDTH + WIDTH - 1] & invShadowMask;
                    const uint32_t mLast = static_cast<uint32_t>(-static_cast<int32_t>(dLast == clearDepth));
                    uint16_t *pLast = buffer + static_cast<size_t>(y) * WIDTH + WIDTH - 1;
                    const uint16_t skyCol = static_cast<uint16_t>(((WIDTH - 1) & 1u) ? colorLUT32 >> 16 : colorLUT32);
                    *pLast = static_cast<uint16_t>((skyCol & mLast) | (*pLast & ~mLast));
                }
            }
        }

    private:
        __attribute__((always_inline)) inline void rebuildSkyboxCache()
        {
            if (!skyboxColorCache || !useSkybox || !skybox.enabled)
                return;

            for (int16_t y = 0; y < SCREEN_HEIGHT; ++y)
            {
                const uint16_t color1 = skybox.getColorAtY(y, SCREEN_HEIGHT).rgb565;
                uint16_t darker = color1;
#if !defined(PIP3D_PC)
                if (color1 != 0)
                {
                    const uint16_t r = color1 >> 11;
                    const uint16_t g = (color1 >> 5) & 0x3Fu;
                    const uint16_t b = color1 & 0x1Fu;
                    darker = (((r - (r != 0)) & 0x1Fu) << 11) |
                             (((g - (g != 0)) & 0x3Fu) << 5) |
                             ((b - (b != 0)) & 0x1Fu);
                }
#endif
                skyboxColorCache[y * 2] = color1;
                skyboxColorCache[y * 2 + 1] = darker;
            }

            cacheValid = true;
        }

        __attribute__((always_inline)) inline void fastClear()
        {
            const uint16_t c = clearColor.rgb565;
            const uint32_t c32 = (static_cast<uint32_t>(c) << 16) | c;
            uint32_t *fb32 = reinterpret_cast<uint32_t *>(buffer);

            uint32_t i = 0;
            const uint32_t limit8 = (pixels32 >> 3) << 3;

            for (; i < limit8; i += 8)
            {
                fb32[i] = c32;
                fb32[i + 1] = c32;
                fb32[i + 2] = c32;
                fb32[i + 3] = c32;
                fb32[i + 4] = c32;
                fb32[i + 5] = c32;
                fb32[i + 6] = c32;
                fb32[i + 7] = c32;
            }

            for (; i < pixels32; ++i)
                fb32[i] = c32;

            if (oddPixels)
                buffer[totalPixels - 1] = c;
        }

    public:
        __attribute__((always_inline)) inline void endFrame()
        {
            bool invalidState = !buffer;
#if defined(PIP3D_PC)
            invalidState = invalidState || !displayReady;
#else
            invalidState = invalidState || !display;
#endif
            if (unlikely(invalidState))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RENDER,
                     "FrameBuffer::endFrame called with invalid state (buffer=%p)",
                     (void *)buffer);
                return;
            }

#if defined(PIP3D_PC)
            const uint32_t count = static_cast<uint32_t>(config.width) * static_cast<uint32_t>(config.height);
            uint32_t *fb32 = reinterpret_cast<uint32_t *>(buffer);
            const uint32_t count32 = count >> 1;
            for (uint32_t i = 0; i < count32; ++i)
            {
                const uint32_t v = fb32[i];
                fb32[i] = ((v & 0x00FF00FFu) << 8) | ((v >> 8) & 0x00FF00FFu);
            }
            if (count & 1u)
                buffer[count - 1] = __builtin_bswap16(buffer[count - 1]);

            pipcore::desktop::Runtime::instance().writeRect565(0, 0, config.width, config.height, buffer, config.width);

            for (uint32_t i = 0; i < count32; ++i)
            {
                const uint32_t v = fb32[i];
                fb32[i] = ((v & 0x00FF00FFu) << 8) | ((v >> 8) & 0x00FF00FFu);
            }
            if (count & 1u)
                buffer[count - 1] = __builtin_bswap16(buffer[count - 1]);
#else
            display->writeRect565(0, 0, config.width, config.height, buffer, config.width);
#endif
        }

        __attribute__((always_inline)) inline void endFrameRegion(int16_t x, int16_t y, int16_t w, int16_t h)
        {
            bool invalidState = !buffer;
#if defined(PIP3D_PC)
            invalidState = invalidState || !displayReady;
#else
            invalidState = invalidState || !display;
#endif
            if (unlikely(invalidState))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RENDER,
                     "FrameBuffer::endFrameRegion called with invalid state (buffer=%p)",
                     (void *)buffer);
                return;
            }

            if (w <= 0 || h <= 0)
                return;

            const int16_t bandTop = currentBandOffsetY();
            const int16_t bandBottom = static_cast<int16_t>(bandTop + config.height);

            int16_t x0 = x < 0 ? 0 : x;
            int16_t y0 = y < bandTop ? bandTop : y;
            int16_t x1 = (x + w) > config.width ? config.width : static_cast<int16_t>(x + w);
            int16_t y1 = (y + h) > bandBottom ? bandBottom : static_cast<int16_t>(y + h);

            if (x1 <= x0 || y1 <= y0)
                return;

            int16_t localY = static_cast<int16_t>(y0 - bandTop);

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
            const uint16_t *region = buffer + static_cast<size_t>(localY) * config.width + x0;

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

        __attribute__((always_inline)) inline uint16_t *getBuffer() { return buffer; }
        __attribute__((always_inline)) inline const uint16_t *getBuffer() const { return buffer; }
        __attribute__((always_inline)) inline const DisplayConfig &getConfig() const { return config; }

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

        __attribute__((always_inline)) inline Skybox &getSkybox() { return skybox; }
        __attribute__((always_inline)) inline const Skybox &getSkybox() const { return skybox; }
        __attribute__((always_inline)) inline bool isSkyboxEnabled() const { return useSkybox; }

        ~FrameBuffer()
        {
            if (buffer)
            {
                pip3D::MemUtils::freeAligned(buffer);
                buffer = nullptr;
            }
            if (skyboxColorCache)
            {
                pip3D::MemUtils::freeAligned(skyboxColorCache);
                skyboxColorCache = nullptr;
            }
        }
    };
}