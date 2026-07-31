#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "Core/Memory.hpp"
#include "Core/Platform.hpp"
#include "Math/Algebra.hpp"
#include "Rendering/Pipeline/Rasterizer/Common.hpp"

namespace pip3D
{

    template <uint16_t WIDTH, uint16_t HEIGHT>
    class ZBuffer
    {
    public:
        static constexpr size_t BUFFER_SIZE = static_cast<size_t>(WIDTH) * HEIGHT;
        static constexpr int16_t CLEAR_DEPTH = 0x7F7F;
        static constexpr int16_t DEPTH_MAX = 32638;
        static constexpr uint32_t CLEAR_PACK32 = 0x7F7F7F7Fu;

    private:
        alignas(32) int16_t storage[BUFFER_SIZE];

    public:
        ZBuffer() = default;
        ZBuffer(const ZBuffer &) = delete;
        ZBuffer &operator=(const ZBuffer &) = delete;

        __attribute__((always_inline)) inline bool init()
        {
            clear();
            return true;
        }

        __attribute__((always_inline, hot)) inline void IRAM_ATTR clear()
        {

            memset(storage, 0x7F, BUFFER_SIZE * sizeof(int16_t));
        }

        __attribute__((always_inline)) inline int16_t *data() { return storage; }
        __attribute__((always_inline)) inline const int16_t *data() const { return storage; }

        __attribute__((always_inline)) inline int16_t *getBufferPtr()
        {
            return storage;
        }
        __attribute__((always_inline)) inline const int16_t *getBufferPtr() const
        {
            return storage;
        }

        __attribute__((always_inline)) inline int16_t IRAM_ATTR
        getRawDepth(uint16_t x, uint16_t y) const
        {
            return static_cast<int16_t>(static_cast<uint16_t>(storage[static_cast<size_t>(y) * WIDTH + x]) & Z_DEPTH_MASK);
        }

        static __attribute__((always_inline)) inline int16_t clearDepthValue() { return CLEAR_DEPTH; }
        static __attribute__((always_inline)) inline int16_t shadowFlagMask() { return Z_SHADOW_FLAG; }

        __attribute__((always_inline, hot)) inline void IRAM_ATTR testAndSetScanline(
            uint16_t y,
            uint16_t x_start,
            uint16_t x_end,
            int32_t depthStart,
            int32_t depthStep,
            uint16_t *frameBuffer,
            uint16_t color)
        {
            const size_t index = static_cast<size_t>(y) * WIDTH + x_start;
            int16_t *__restrict__ buf = storage + index;
            uint16_t *__restrict__ fb = frameBuffer + index;

            if (Rasterizer::g_fogState.enabled && Rasterizer::g_fogLut.valid)
                Rasterizer::fillScanlineFog(buf, fb, x_end - x_start + 1, depthStart, depthStep, color);
            else
                Rasterizer::fillScanlinePlain(buf, fb, x_end - x_start + 1, depthStart, depthStep, color);
        }
    };
}
