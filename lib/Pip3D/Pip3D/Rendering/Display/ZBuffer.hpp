#pragma once

#include <cstdint>
#include <cstring>

#include "Core/Memory.hpp"
#include "Math/Algebra.hpp"
#include "Rendering/Pipeline/Rasterizer/Common.hpp"

namespace pip3D
{
    template <uint16_t WIDTH, uint16_t HEIGHT>
    class ZBuffer
    {
    private:
        static constexpr size_t BUFFER_SIZE = WIDTH * HEIGHT;
        static constexpr int16_t CLEAR_DEPTH = 0x7F7F;
        static constexpr int16_t SHADOW_FLAG = static_cast<int16_t>(0x8000);
        static constexpr uint16_t DEPTH_MASK = 0x7FFF;

        int16_t *buffer;

        __attribute__((always_inline, hot)) inline void IRAM_ATTR
        testAndSetPlain(int16_t *__restrict__ buf,
                        uint16_t *__restrict__ fb,
                        uint32_t count,
                        int32_t depthStart,
                        int32_t depthStep,
                        uint16_t color)
        {
            int32_t depth = depthStart;

            while (count >= 4)
            {
                PIP3D_PREFETCH_W(buf + 16);
                PIP3D_PREFETCH_W(fb + 16);

                const int16_t d0 = static_cast<int16_t>(depth);
                const int16_t d1 = static_cast<int16_t>(depth + depthStep);
                const int16_t d2 = static_cast<int16_t>(depth + depthStep * 2);
                const int16_t d3 = static_cast<int16_t>(depth + depthStep * 3);

                const int16_t c0 = buf[0] & DEPTH_MASK;
                const int16_t c1 = buf[1] & DEPTH_MASK;
                const int16_t c2 = buf[2] & DEPTH_MASK;
                const int16_t c3 = buf[3] & DEPTH_MASK;

                if (d0 < c0)
                {
                    buf[0] = d0;
                    fb[0] = color;
                }
                if (d1 < c1)
                {
                    buf[1] = d1;
                    fb[1] = color;
                }
                if (d2 < c2)
                {
                    buf[2] = d2;
                    fb[2] = color;
                }
                if (d3 < c3)
                {
                    buf[3] = d3;
                    fb[3] = color;
                }

                depth += depthStep * 4;
                buf += 4;
                fb += 4;
                count -= 4;
            }

            while (count > 0)
            {
                const int16_t d = static_cast<int16_t>(depth);
                const int16_t c = *buf & DEPTH_MASK;
                if (d < c)
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

        __attribute__((always_inline, hot)) inline void IRAM_ATTR
        testAndSetFog(int16_t *__restrict__ buf,
                      uint16_t *__restrict__ fb,
                      uint32_t count,
                      int32_t depthStart,
                      int32_t depthStep,
                      uint16_t color)
        {
            const Rasterizer::FogState &fog = Rasterizer::g_fogState;

            const uint32_t rb1 = color & 0xF81F;
            const uint32_t g1 = color & 0x07E0;
            const uint32_t fog_rb = fog.color_rb;
            const uint32_t fog_g = fog.color_g;
            const uint16_t fog_color = fog.color;
            const float kVal = fog.kVal;
            const float knVal = fog.knVal;
            const float worldNear = fog.worldNear;
            const float worldScale32 = fog.worldScale32;

            int32_t depth = depthStart;

            while (count > 0)
            {
                const int16_t d = static_cast<int16_t>(depth);
                const int16_t curr = *buf & DEPTH_MASK;
                if (d < curr)
                {
                    *buf = d;

                    float denom = kVal - static_cast<float>(d);
                    if (unlikely(denom < 1.0f))
                        denom = 1.0f;

                    const float z_eye = knVal * FastMath::fastReciprocal(denom);
                    const int32_t f_alpha = static_cast<int32_t>((z_eye - worldNear) * worldScale32);

                    if (f_alpha <= 0)
                    {
                        *fb = color;
                    }
                    else if (f_alpha >= 32)
                    {
                        *fb = fog_color;
                    }
                    else
                    {
                        const uint32_t inv_f = 32u - static_cast<uint32_t>(f_alpha);
                        const uint32_t f32 = static_cast<uint32_t>(f_alpha);
                        const uint32_t rb = ((rb1 * inv_f + fog_rb * f32) >> 5) & 0xF81F;
                        const uint32_t g = ((g1 * inv_f + fog_g * f32) >> 5) & 0x07E0;
                        *fb = static_cast<uint16_t>(rb | g);
                    }
                }
                depth += depthStep;
                ++buf;
                ++fb;
                --count;
            }
        }

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
                return false;

            clear();
            return true;
        }

        void IRAM_ATTR clear()
        {
            if (buffer)
                memset(buffer, 0x7F, BUFFER_SIZE * sizeof(int16_t));
        }

        __attribute__((always_inline)) inline int16_t IRAM_ATTR getRawDepth(uint16_t x, uint16_t y) const
        {
            return buffer[static_cast<size_t>(y) * WIDTH + x] & DEPTH_MASK;
        }

        __attribute__((always_inline)) inline const int16_t *getBufferPtr() const
        {
            return buffer;
        }

        static __attribute__((always_inline)) inline int16_t clearDepthValue() { return CLEAR_DEPTH; }
        static __attribute__((always_inline)) inline int16_t shadowFlagMask() { return SHADOW_FLAG; }

        __attribute__((always_inline, hot)) inline void IRAM_ATTR testAndSetScanline(uint16_t y,
                                                                                     uint16_t x_start,
                                                                                     uint16_t x_end,
                                                                                     int32_t depthStart,
                                                                                     int32_t depthStep,
                                                                                     uint16_t *frameBuffer,
                                                                                     uint16_t color)
        {
            const size_t index = static_cast<size_t>(y) * WIDTH + x_start;
            int16_t *__restrict__ buf = buffer + index;
            uint16_t *__restrict__ fb = frameBuffer + index;

            if (Rasterizer::g_fogState.enabled)
            {
                testAndSetFog(buf, fb, x_end - x_start + 1, depthStart, depthStep, color);
            }
            else
            {
                testAndSetPlain(buf, fb, x_end - x_start + 1, depthStart, depthStep, color);
            }
        }

        ~ZBuffer()
        {
            if (buffer)
                ::pip3D::MemUtils::freeData(buffer);
        }
    };
}
