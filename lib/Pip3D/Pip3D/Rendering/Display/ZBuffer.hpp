#pragma once

#include <cstddef>
#include <cstdint>

namespace pip3D
{

    inline constexpr int16_t Z_SHADOW_FLAG = static_cast<int16_t>(0x8000);
    inline constexpr uint16_t Z_DEPTH_MASK = 0x7FFFu;

    template <uint16_t WIDTH, uint16_t HEIGHT>
    class ZBuffer
    {
    public:
        static constexpr int16_t CLEAR_DEPTH = 0x7FFF;
        static constexpr int16_t DEPTH_MAX = 0x7FFE;
        static constexpr uint32_t CLEAR_PACK32 = 0x7FFF7FFFu;

    private:
        static constexpr size_t BUFFER_SIZE = static_cast<size_t>(WIDTH) * HEIGHT;

        alignas(32) int16_t storage[BUFFER_SIZE];

    public:
        ZBuffer() { clear(); }
        ZBuffer(const ZBuffer &) = delete;
        ZBuffer &operator=(const ZBuffer &) = delete;

        inline void clear()
        {
            uint32_t *p = reinterpret_cast<uint32_t *>(storage);
            uint32_t n = BUFFER_SIZE / 2;

            while (n >= 8)
            {
                p[0] = p[1] = p[2] = p[3] =
                    p[4] = p[5] = p[6] = p[7] = CLEAR_PACK32;
                p += 8;
                n -= 8;
            }
            while (n--)
                *p++ = CLEAR_PACK32;
        }

        __attribute__((always_inline)) int16_t *data() { return storage; }
        __attribute__((always_inline)) const int16_t *data() const { return storage; }

        __attribute__((always_inline)) int16_t
        getRawDepth(uint16_t x, uint16_t y) const
        {
            return static_cast<int16_t>(
                static_cast<uint16_t>(storage[static_cast<size_t>(y) * WIDTH + x]) & Z_DEPTH_MASK);
        }
    };
}
