#pragma once

#include <cstdint>
#include <cstring>

#include "Core/Memory.hpp"
#include "Rendering/Pipeline/Rasterizer/Common.hpp"

#if !defined(IRAM_ATTR)
#if defined(ESP_PLATFORM) || defined(ESP32)
#include <esp_attr.h>
#else
#define IRAM_ATTR
#endif
#endif

#if defined(__GNUC__) || defined(__clang__)
#ifndef PIP3D_PREFETCH
#define PIP3D_PREFETCH(ptr) __builtin_prefetch((ptr), 1, 0)
#endif
#else
#ifndef PIP3D_PREFETCH
#define PIP3D_PREFETCH(ptr) ((void)0)
#endif
#endif

namespace pip3D
{
    template <uint16_t WIDTH, uint16_t HEIGHT>
    class ZBuffer
    {
    private:
        static constexpr size_t BUFFER_SIZE = WIDTH * HEIGHT;
        int16_t *buffer;
        static constexpr int16_t CLEAR_DEPTH = 0x7F7F;
        static constexpr int16_t SHADOW_FLAG = static_cast<int16_t>(0x8000);
        static constexpr float INV_MAX_DEPTH = 1.0f / 32767.0f;

    public:
        ZBuffer() : buffer(nullptr) {}
        ZBuffer(const ZBuffer &) = delete;
        ZBuffer &operator=(const ZBuffer &) = delete;

        __attribute__((warn_unused_result)) bool init()
        {
            if (buffer)
            {
                ::pip3D::MemUtils::freeData(buffer);
                buffer = nullptr;
            }

            buffer = static_cast<int16_t *>(::pip3D::MemUtils::allocAligned(
                BUFFER_SIZE * sizeof(int16_t), 32));

            if (!buffer)
            {
                return false;
            }

            clear();
            return true;
        }

        void IRAM_ATTR clear()
        {
            if (!buffer)
                return;

            static constexpr uint32_t FILL = 0x7F7F7F7Fu;
            uint32_t *p = reinterpret_cast<uint32_t *>(buffer);
            size_t n32 = BUFFER_SIZE / 2;

            size_t i = 0;
            for (; i + 8 <= n32; i += 8)
            {
                p[i + 0] = FILL;
                p[i + 1] = FILL;
                p[i + 2] = FILL;
                p[i + 3] = FILL;
                p[i + 4] = FILL;
                p[i + 5] = FILL;
                p[i + 6] = FILL;
                p[i + 7] = FILL;
            }
            for (; i < n32; ++i)
                p[i] = FILL;
        }

        __attribute__((always_inline)) inline bool IRAM_ATTR hasGeometry(uint16_t x, uint16_t y) const
        {
            const int16_t *row = buffer + static_cast<size_t>(y) * WIDTH;
            return (row[x] & 0x7FFF) != CLEAR_DEPTH;
        }

        __attribute__((always_inline)) inline bool IRAM_ATTR hasShadow(uint16_t x, uint16_t y) const
        {
            const int16_t *row = buffer + static_cast<size_t>(y) * WIDTH;
            return (row[x] & SHADOW_FLAG) != 0;
        }

        __attribute__((always_inline)) inline float IRAM_ATTR getDepth01(uint16_t x, uint16_t y) const
        {
            const int16_t *row = buffer + static_cast<size_t>(y) * WIDTH;
            const int16_t d = static_cast<int16_t>(row[x] & 0x7FFF);

            if (d == CLEAR_DEPTH)
                return 1.0f;

            return static_cast<float>(d) * INV_MAX_DEPTH;
        }

        __attribute__((always_inline)) inline int16_t IRAM_ATTR getRawDepth(uint16_t x, uint16_t y) const
        {
            const int16_t *row = buffer + static_cast<size_t>(y) * WIDTH;
            return static_cast<int16_t>(row[x] & 0x7FFF);
        }

        __attribute__((always_inline)) inline const int16_t *getBufferPtr() const
        {
            return buffer;
        }

        static __attribute__((always_inline)) inline int16_t clearDepthValue()
        {
            return CLEAR_DEPTH;
        }

        static __attribute__((always_inline)) inline int16_t shadowFlagMask()
        {
            return SHADOW_FLAG;
        }

        __attribute__((always_inline)) inline void IRAM_ATTR markShadow(uint16_t x, uint16_t y)
        {
            int16_t *row = buffer + static_cast<size_t>(y) * WIDTH;
            row[x] |= SHADOW_FLAG;
        }

        __attribute__((always_inline, hot)) inline void IRAM_ATTR testAndSetScanline(uint16_t y, uint16_t x_start, uint16_t x_end,
                                                                                     int32_t depthStart, int32_t depthStep,
                                                                                     uint16_t *frameBuffer, uint16_t color)
        {
            const uint16_t countTotal = x_end - x_start + 1;
            size_t index = y * WIDTH + x_start;
            int16_t *__restrict__ buf = buffer + index;
            uint16_t *__restrict__ fb = frameBuffer + index;

            int32_t depth = depthStart;
            uint16_t count = countTotal;

            if (Rasterizer::g_fogState.enabled)
            {
                uint32_t rb1 = color & 0xF81F;
                uint32_t g1 = color & 0x07E0;

                while (count > 0)
                {
                    int16_t d = static_cast<int16_t>(depth);
                    int16_t curr = *buf & 0x7FFF;
                    if (d < curr)
                    {
                        *buf = d;

                        float denom = Rasterizer::g_fogState.kVal - static_cast<float>(d);
                        if (unlikely(denom < 1.0f))
                            denom = 1.0f;

                        float z_eye = Rasterizer::g_fogState.knVal * FastMath::fastReciprocal(denom);

                        float fogF = (z_eye - Rasterizer::g_fogState.worldNear) * Rasterizer::g_fogState.worldScale32;
                        int32_t f_alpha = static_cast<int32_t>(fogF);

                        if (f_alpha <= 0)
                        {
                            *fb = color;
                        }
                        else if (f_alpha >= 32)
                        {
                            *fb = Rasterizer::g_fogState.color;
                        }
                        else
                        {
                            uint32_t inv_f_alpha = 32 - f_alpha;
                            uint32_t rb = ((rb1 * inv_f_alpha + Rasterizer::g_fogState.color_rb * f_alpha) >> 5) & 0xF81F;
                            uint32_t g = ((g1 * inv_f_alpha + Rasterizer::g_fogState.color_g * f_alpha) >> 5) & 0x07E0;
                            *fb = static_cast<uint16_t>(rb | g);
                        }
                    }
                    depth += depthStep;
                    ++buf;
                    ++fb;
                    --count;
                }
            }
            else
            {
                while (count >= 4)
                {
                    PIP3D_PREFETCH(buf + 16);
                    PIP3D_PREFETCH(fb + 16);

                    {
                        int16_t d = static_cast<int16_t>(depth);
                        int16_t curr = buf[0] & 0x7FFF;
                        if (d < curr)
                        {
                            buf[0] = d;
                            fb[0] = color;
                        }
                        depth += depthStep;
                    }
                    {
                        int16_t d = static_cast<int16_t>(depth);
                        int16_t curr = buf[1] & 0x7FFF;
                        if (d < curr)
                        {
                            buf[1] = d;
                            fb[1] = color;
                        }
                        depth += depthStep;
                    }
                    {
                        int16_t d = static_cast<int16_t>(depth);
                        int16_t curr = buf[2] & 0x7FFF;
                        if (d < curr)
                        {
                            buf[2] = d;
                            fb[2] = color;
                        }
                        depth += depthStep;
                    }
                    {
                        int16_t d = static_cast<int16_t>(depth);
                        int16_t curr = buf[3] & 0x7FFF;
                        if (d < curr)
                        {
                            buf[3] = d;
                            fb[3] = color;
                        }
                        depth += depthStep;
                    }
                    buf += 4;
                    fb += 4;
                    count -= 4;
                }
                while (count > 0)
                {
                    int16_t d = static_cast<int16_t>(depth);
                    int16_t curr = *buf & 0x7FFF;
                    if (d < curr)
                    {
                        *buf = d;
                        *fb = color;
                    }
                    depth += depthStep;
                    ++buf;
                    ++fb;
                    --count;
                }
            }
        }

        ~ZBuffer()
        {
            if (buffer)
                ::pip3D::MemUtils::freeData(buffer);
        }
    };
}