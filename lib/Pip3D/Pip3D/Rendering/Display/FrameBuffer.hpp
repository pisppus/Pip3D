#pragma once

#include "Core/Platform.hpp"
#include "Core/Viewport.hpp"
#include "Core/Memory.hpp"
#if defined(PIP3D_PC)
#include <PipCore/Platforms/Desktop/Runtime.hpp>
#else
#include <PipCore/Display.hpp>
#endif
#include "ZBuffer.hpp"
#include "Rendering/Display/Sky.hpp"
#if defined(__GNUC__) || defined(__clang__)
#ifndef PIP3D_PREFETCH
#define PIP3D_PREFETCH(ptr) __builtin_prefetch((ptr), 0, 0)
#endif
#else
#ifndef PIP3D_PREFETCH
#define PIP3D_PREFETCH(ptr) ((void)0)
#endif
#endif

namespace pip3D
{
    class __attribute__((aligned(16))) FrameBuffer
    {
    private:
        uint16_t *buffer;
#if defined(PIP3D_PC)
        bool displayReady;
#else
        pipcore::Display *display;
#endif
        DisplayConfig config;
        Skybox skybox;
        bool useSkybox;
        Color clearColor;

        static constexpr size_t DMA_ALIGNMENT = 64;

        uint32_t totalPixels;
        uint32_t pixels32;
        bool oddPixels;

        uint16_t *skyboxColorCache;
        int16_t cachedScreenHeight;
        bool cacheValid;

        uint16_t colorLUT[2];

    public:
        FrameBuffer() : buffer(nullptr),
#if defined(PIP3D_PC)
                        displayReady(false),
#else
                        display(nullptr),
#endif
                        useSkybox(true),
                        clearColor(Color::BLACK), totalPixels(0), pixels32(0), oddPixels(false),
                        skyboxColorCache(nullptr), cachedScreenHeight(0), cacheValid(false)
        {
            skybox.setPreset(SKYBOX_DAY);
            colorLUT[0] = colorLUT[1] = 0;
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
            {
                LOGW(::pip3D::Debug::LOG_MODULE_RENDER,
                     "FrameBuffer::init called more than once (buffer already allocated)");
                return false;
            }

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
                LOGE(::pip3D::Debug::LOG_MODULE_RENDER,
                     "FrameBuffer::init failed: could not allocate %u bytes for %dx%d framebuffer",
                     static_cast<unsigned int>(bufferSize),
                     static_cast<int>(config.width),
                     static_cast<int>(config.height));
                return false;
            }

            memset(buffer, 0, bufferSize);

            skyboxColorCache = (uint16_t *)pip3D::MemUtils::allocAligned(SCREEN_HEIGHT * 2 * sizeof(uint16_t), 4, pipcore::AllocCaps::PreferInternal);

            if (!skyboxColorCache)
            {
                LOGW(::pip3D::Debug::LOG_MODULE_RENDER,
                     "FrameBuffer::init warning: could not allocate skybox cache, performance will be reduced");
            }

            LOGI(::pip3D::Debug::LOG_MODULE_RENDER,
                 "FrameBuffer::init OK: %dx%d, bufferSize=%u bytes",
                 static_cast<int>(config.width),
                 static_cast<int>(config.height),
                 static_cast<unsigned int>(bufferSize));
            return true;
        }

        void beginFrame()
        {
            if (unlikely(!buffer))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RENDER,
                     "FrameBuffer::beginFrame called with null buffer");
                return;
            }
        }

    private:
        __attribute__((always_inline)) inline void rebuildSkyboxCache()
        {
            if (!skyboxColorCache || !useSkybox || !skybox.enabled)
                return;

            for (int16_t y = 0; y < SCREEN_HEIGHT; ++y)
            {
                Color lineColor = skybox.getColorAtY(y, SCREEN_HEIGHT);
                uint16_t color1 = lineColor.rgb565;

                uint16_t darker = color1;
#if !defined(PIP3D_PC)
                if (color1 != 0)
                {
                    const uint16_t r = (color1 >> 11);
                    const uint16_t g = (color1 >> 5) & 0x3F;
                    const uint16_t b = color1 & 0x1F;
                    darker = ((r ? r - 1 : 0) << 11) |
                             ((g ? g - 1 : 0) << 5) |
                             (b ? b - 1 : 0);
                }
#endif

                skyboxColorCache[y * 2] = color1;
                skyboxColorCache[y * 2 + 1] = darker;
            }

            cachedScreenHeight = SCREEN_HEIGHT;
            cacheValid = true;
        }

        __attribute__((always_inline)) inline void fastClear()
        {
            const uint16_t clearCol = clearColor.rgb565;
            const uint32_t clearColor32 = (clearCol << 16) | clearCol;
            uint32_t *fb32 = (uint32_t *)buffer;

            uint32_t i = 0;
            const uint32_t blocks8 = pixels32 >> 3;
            const uint32_t limit8 = blocks8 << 3;

            for (; i < limit8; i += 8)
            {
                fb32[i] = clearColor32;
                fb32[i + 1] = clearColor32;
                fb32[i + 2] = clearColor32;
                fb32[i + 3] = clearColor32;
                fb32[i + 4] = clearColor32;
                fb32[i + 5] = clearColor32;
                fb32[i + 6] = clearColor32;
                fb32[i + 7] = clearColor32;
            }

            for (; i < pixels32; i++)
            {
                fb32[i] = clearColor32;
            }

            if (oddPixels)
            {
                buffer[totalPixels - 1] = clearCol;
            }
        }

    public:
        template <uint16_t WIDTH, uint16_t HEIGHT>
        __attribute__((always_inline)) inline void drawSkyboxWhereEmpty(const ZBuffer<WIDTH, HEIGHT> &zbuf)
        {
            if (unlikely(!buffer))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RENDER,
                     "FrameBuffer::drawSkyboxWhereEmpty called with null buffer");
                return;
            }

            const int16_t *__restrict__ zb = zbuf.getBufferPtr();
            if (unlikely(!zb))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RENDER,
                     "FrameBuffer::drawSkyboxWhereEmpty called with null z-buffer");
                return;
            }

            const uint16_t fbWidth = config.width;
            const uint16_t fbHeight = config.height;

            if (unlikely(fbWidth != WIDTH || fbHeight != HEIGHT))
            {
                LOGW(::pip3D::Debug::LOG_MODULE_RENDER,
                     "FrameBuffer::drawSkyboxWhereEmpty size mismatch (fb=%ux%u, zb=%ux%u)",
                     static_cast<unsigned int>(fbWidth),
                     static_cast<unsigned int>(fbHeight),
                     static_cast<unsigned int>(WIDTH),
                     static_cast<unsigned int>(HEIGHT));
                return;
            }

            const int16_t clearDepth = ZBuffer<WIDTH, HEIGHT>::clearDepthValue();
            const int16_t invShadowMask = ~ZBuffer<WIDTH, HEIGHT>::shadowFlagMask();

            const bool shouldUseSkybox = useSkybox && skybox.enabled;

            if (shouldUseSkybox && skyboxColorCache && !cacheValid)
            {
                rebuildSkyboxCache();
            }

            const uint16_t baseClearColor = clearColor.rgb565;

            for (uint16_t y = 0; y < fbHeight; ++y)
            {
                const int16_t globalY = currentBandOffsetY() + static_cast<int16_t>(y);

                uint16_t *__restrict__ row = buffer + (static_cast<size_t>(y) * fbWidth);
                const int16_t *__restrict__ zbRow = zb + (static_cast<size_t>(y) * WIDTH);

                if (shouldUseSkybox && skyboxColorCache && cacheValid && globalY < SCREEN_HEIGHT)
                {
                    const uint16_t cacheIdx = globalY * 2;
                    const uint16_t yOdd = y & 1;
                    colorLUT[0] = skyboxColorCache[cacheIdx];
                    colorLUT[1] = skyboxColorCache[cacheIdx + (yOdd ? 1 : 0)];
                }
                else if (shouldUseSkybox && globalY < SCREEN_HEIGHT)
                {
                    Color lineColor = skybox.getColorAtY(globalY, SCREEN_HEIGHT);
                    const uint16_t color1 = lineColor.rgb565;

                    uint16_t darker = color1;
#if !defined(PIP3D_PC)
                    if (color1 != 0)
                    {
                        const uint16_t r = (color1 >> 11);
                        const uint16_t g = (color1 >> 5) & 0x3F;
                        const uint16_t b = color1 & 0x1F;
                        darker = ((r ? r - 1 : 0) << 11) |
                                 ((g ? g - 1 : 0) << 5) |
                                 (b ? b - 1 : 0);
                    }
#endif

                    const uint16_t yOdd = y & 1;
                    colorLUT[0] = color1;
                    colorLUT[1] = yOdd ? darker : color1;
                }
                else
                {
                    colorLUT[0] = colorLUT[1] = baseClearColor;
                }

                const uint32_t colorLUT32 = (static_cast<uint32_t>(colorLUT[1]) << 16) | colorLUT[0];
                const int32_t clearDepth32 = (static_cast<int32_t>(clearDepth) << 16) | clearDepth;
                const int32_t invShadowMask32 = (static_cast<int32_t>(invShadowMask) << 16) | invShadowMask;

                uint16_t x = 0;
                const uint16_t widthHalf = fbWidth >> 1;

                uint32_t *__restrict__ row32 = reinterpret_cast<uint32_t *>(row);
                const int32_t *__restrict__ zbRow32 = reinterpret_cast<const int32_t *>(zbRow);

                for (; x < widthHalf; ++x)
                {
                    PIP3D_PREFETCH(&zbRow32[x + 8]);

                    int32_t zb2 = zbRow32[x];
                    int32_t d2 = zb2 & invShadowMask32;

                    if (d2 == clearDepth32)
                    {
                        row32[x] = colorLUT32;
                    }
                    else
                    {
                        int16_t d0 = static_cast<int16_t>(zb2 & 0xFFFF) & invShadowMask;
                        if (d0 == clearDepth)
                        {
                            row[x * 2] = colorLUT[0];
                        }
                        int16_t d1 = static_cast<int16_t>(zb2 >> 16) & invShadowMask;
                        if (d1 == clearDepth)
                        {
                            row[x * 2 + 1] = colorLUT[1];
                        }
                    }
                }

                if (unlikely(fbWidth & 1u))
                {
                    const int16_t depthNoShadow = zbRow[fbWidth - 1] & invShadowMask;
                    if (depthNoShadow == clearDepth)
                    {
                        row[fbWidth - 1] = colorLUT[(fbWidth - 1) & 1u];
                    }
                }
            }
        }

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
                     (void *)buffer,
                     0);
                return;
            }
#if defined(PIP3D_PC)
            const uint32_t count = config.width * config.height;
            for (uint32_t i = 0; i < count; ++i)
            {
                buffer[i] = __builtin_bswap16(buffer[i]);
            }

            pipcore::desktop::Runtime::instance().writeRect565(0, 0, config.width, config.height, buffer, config.width);
            for (uint32_t i = 0; i < count; ++i)
            {
                buffer[i] = __builtin_bswap16(buffer[i]);
            }
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
                     (void *)buffer,
                     0);
                return;
            }

            if (w <= 0 || h <= 0)
            {
                return;
            }

            int16_t bandTop = currentBandOffsetY();
            int16_t bandBottom = static_cast<int16_t>(bandTop + config.height);

            int16_t clippedX0 = x;
            int16_t clippedY0 = y;
            int16_t clippedX1 = static_cast<int16_t>(x + w);
            int16_t clippedY1 = static_cast<int16_t>(y + h);

            if (clippedX0 < 0)
                clippedX0 = 0;
            if (clippedY0 < bandTop)
                clippedY0 = bandTop;
            if (clippedX1 > config.width)
                clippedX1 = config.width;
            if (clippedY1 > bandBottom)
                clippedY1 = bandBottom;

            if (clippedX1 <= clippedX0 || clippedY1 <= clippedY0)
            {
                return;
            }

            int16_t localY = clippedY0;
            if (currentBandHeight() == config.height || bandTop != 0)
            {
                localY = static_cast<int16_t>(clippedY0 - bandTop);
            }

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

            const int16_t clippedW = static_cast<int16_t>(clippedX1 - clippedX0);
            const int16_t clippedH = static_cast<int16_t>(clippedY1 - clippedY0);
            const uint16_t *region = buffer + static_cast<size_t>(localY) * config.width + clippedX0;
#if defined(PIP3D_PC)
            for (int16_t row = 0; row < clippedH; ++row)
            {
                uint16_t *line = const_cast<uint16_t *>(region) + (static_cast<size_t>(row) * config.width);
                for (int16_t col = 0; col < clippedW; ++col)
                {
                    line[col] = __builtin_bswap16(line[col]);
                }
            }

            pipcore::desktop::Runtime::instance().writeRect565(clippedX0, clippedY0, clippedW, clippedH, region, config.width);
            for (int16_t row = 0; row < clippedH; ++row)
            {
                uint16_t *line = const_cast<uint16_t *>(region) + (static_cast<size_t>(row) * config.width);
                for (int16_t col = 0; col < clippedW; ++col)
                {
                    line[col] = __builtin_bswap16(line[col]);
                }
            }
#else
            display->writeRect565(clippedX0, clippedY0, clippedW, clippedH, region, config.width);
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