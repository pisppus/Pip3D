#pragma once

#if !defined(PIP3D_PC)
#include <Arduino.h>
#endif
#include "Core/Memory.hpp"
#include "Debug/Logging.hpp"

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
        static constexpr int16_t MAX_DEPTH = 32767;
        static constexpr int16_t CLEAR_DEPTH = static_cast<int16_t>(0x7F7F);
        static constexpr int16_t SHADOW_FLAG = static_cast<int16_t>(0x8000);
        static constexpr float INV_MAX_DEPTH = 0.00003051850947599719f;

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

            buffer = static_cast<int16_t *>(::pip3D::MemUtils::allocData(BUFFER_SIZE * sizeof(int16_t)));

            if (!buffer)
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RENDER,
                     "ZBuffer::init failed: could not allocate %u bytes for %ux%u buffer",
                     static_cast<unsigned int>(BUFFER_SIZE * sizeof(int16_t)),
                     static_cast<unsigned int>(WIDTH),
                     static_cast<unsigned int>(HEIGHT));
                return false;
            }

            clear();

            LOGI(::pip3D::Debug::LOG_MODULE_RENDER,
                 "ZBuffer::init OK: %ux%u (buffer=%p)",
                 static_cast<unsigned int>(WIDTH),
                 static_cast<unsigned int>(HEIGHT),
                 static_cast<void *>(buffer));
            return true;
        }

        void clear()
        {
            if (buffer)
            {
                memset(buffer, 0x7F, BUFFER_SIZE * sizeof(int16_t));
            }
        }

        __attribute__((always_inline)) inline bool hasGeometry(uint16_t x, uint16_t y) const
        {
            if (unlikely(x >= WIDTH || y >= HEIGHT || !buffer))
            {
                LOGW(::pip3D::Debug::LOG_MODULE_RENDER,
                     "ZBuffer::hasGeometry invalid call (x=%u, y=%u, WIDTH=%u, HEIGHT=%u, buffer=%p)",
                     static_cast<unsigned int>(x),
                     static_cast<unsigned int>(y),
                     static_cast<unsigned int>(WIDTH),
                     static_cast<unsigned int>(HEIGHT),
                     static_cast<const void *>(buffer));
                return false;
            }

            const int16_t *row = buffer + static_cast<size_t>(y) * WIDTH;
            const int16_t d = row[x];
            return (d & ~SHADOW_FLAG) != CLEAR_DEPTH;
        }

        __attribute__((always_inline)) inline bool hasShadow(uint16_t x, uint16_t y) const
        {
            if (unlikely(x >= WIDTH || y >= HEIGHT || !buffer))
            {
                LOGW(::pip3D::Debug::LOG_MODULE_RENDER,
                     "ZBuffer::hasShadow invalid call (x=%u, y=%u, WIDTH=%u, HEIGHT=%u, buffer=%p)",
                     static_cast<unsigned int>(x),
                     static_cast<unsigned int>(y),
                     static_cast<unsigned int>(WIDTH),
                     static_cast<unsigned int>(HEIGHT),
                     static_cast<const void *>(buffer));
                return false;
            }

            const int16_t *row = buffer + static_cast<size_t>(y) * WIDTH;
            const int16_t d = row[x];
            return (d & SHADOW_FLAG) != 0;
        }

        __attribute__((always_inline)) inline float getDepth01(uint16_t x, uint16_t y) const
        {
            if (unlikely(x >= WIDTH || y >= HEIGHT || !buffer))
            {
                return 1.0f;
            }

            const int16_t *row = buffer + static_cast<size_t>(y) * WIDTH;
            const int16_t stored = row[x];
            const int16_t d = static_cast<int16_t>(stored & ~SHADOW_FLAG);

            if (d == CLEAR_DEPTH)
            {
                return 1.0f;
            }

            return static_cast<float>(d) * INV_MAX_DEPTH;
        }

        __attribute__((always_inline)) inline int16_t getRawDepth(uint16_t x, uint16_t y) const
        {
            if (unlikely(x >= WIDTH || y >= HEIGHT || !buffer))
            {
                return CLEAR_DEPTH;
            }

            const int16_t *row = buffer + static_cast<size_t>(y) * WIDTH;
            const int16_t stored = row[x];
            return static_cast<int16_t>(stored & ~SHADOW_FLAG);
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

        __attribute__((always_inline)) inline void markShadow(uint16_t x, uint16_t y)
        {
            if (unlikely(x >= WIDTH || y >= HEIGHT || !buffer))
            {
                LOGW(::pip3D::Debug::LOG_MODULE_RENDER,
                     "ZBuffer::markShadow invalid call (x=%u, y=%u, WIDTH=%u, HEIGHT=%u, buffer=%p)",
                     static_cast<unsigned int>(x),
                     static_cast<unsigned int>(y),
                     static_cast<unsigned int>(WIDTH),
                     static_cast<unsigned int>(HEIGHT),
                     static_cast<void *>(buffer));
                return;
            }

            int16_t *row = buffer + static_cast<size_t>(y) * WIDTH;
            row[x] |= SHADOW_FLAG;
        }

        __attribute__((always_inline, hot)) inline void testAndSetScanline(uint16_t y, uint16_t x_start, uint16_t x_end,
                                                                           int32_t depthStart, int32_t depthStep,
                                                                           uint16_t *frameBuffer, uint16_t color)
        {
            if (unlikely(y >= HEIGHT))
                return;
            if (unlikely(x_end >= WIDTH))
                x_end = WIDTH - 1;
            if (unlikely(x_start >= WIDTH))
                return;

            const uint16_t countTotal = x_end - x_start + 1;
            if (unlikely(countTotal == 0))
                return;

            size_t index = y * WIDTH + x_start;
            int16_t *__restrict__ buf = buffer + index;
            uint16_t *__restrict__ fb = frameBuffer + index;

            int32_t depth = depthStart;
            uint16_t count = countTotal;

            // Развернутый цикл по 4 пикселя (полностью безветвленный)
            while (count >= 4)
            {
                PIP3D_PREFETCH(buf + 16);
                PIP3D_PREFETCH(fb + 16);

                // Пиксель 0
                int16_t d0 = static_cast<int16_t>(depth);
                int16_t curr0 = buf[0] & ~SHADOW_FLAG;
                int16_t mask0 = (d0 - curr0) >> 15; // 0xFFFF если d0 < curr0, иначе 0x0000
                buf[0] = (d0 & mask0) | (buf[0] & ~mask0);
                fb[0] = (color & mask0) | (fb[0] & ~mask0);
                depth += depthStep;

                // Пиксель 1
                int16_t d1 = static_cast<int16_t>(depth);
                int16_t curr1 = buf[1] & ~SHADOW_FLAG;
                int16_t mask1 = (d1 - curr1) >> 15;
                buf[1] = (d1 & mask1) | (buf[1] & ~mask1);
                fb[1] = (color & mask1) | (fb[1] & ~mask1);
                depth += depthStep;

                // Пиксель 2
                int16_t d2 = static_cast<int16_t>(depth);
                int16_t curr2 = buf[2] & ~SHADOW_FLAG;
                int16_t mask2 = (d2 - curr2) >> 15;
                buf[2] = (d2 & mask2) | (buf[2] & ~mask2);
                fb[2] = (color & mask2) | (fb[2] & ~mask2);
                depth += depthStep;

                // Пиксель 3
                int16_t d3 = static_cast<int16_t>(depth);
                int16_t curr3 = buf[3] & ~SHADOW_FLAG;
                int16_t mask3 = (d3 - curr3) >> 15;
                buf[3] = (d3 & mask3) | (buf[3] & ~mask3);
                fb[3] = (color & mask3) | (fb[3] & ~mask3);
                depth += depthStep;

                buf += 4;
                fb += 4;
                count -= 4;
            }

            // Остаточные пиксели (также безветвленные)
            while (count > 0)
            {
                int16_t d = static_cast<int16_t>(depth);
                int16_t curr = *buf & ~SHADOW_FLAG;
                int16_t mask = (d - curr) >> 15;
                *buf = (d & mask) | (*buf & ~mask);
                *fb = (color & mask) | (*fb & ~mask);

                depth += depthStep;
                ++buf;
                ++fb;
                --count;
            }
        }

        ~ZBuffer()
        {
            if (buffer)
                ::pip3D::MemUtils::freeData(buffer);
        }
    };

}
