#pragma once

#include <cstdint>
#include <cstring>

#include "Core/Platform.hpp"

namespace pip3D
{

    inline constexpr uint16_t Z_DEPTH_MASK = 0x7FFFu;
    inline constexpr uint16_t Z_SHADOW_FLAG = 0x8000u;
    inline constexpr uint16_t Z_DEPTH_MAX = 0x7FFEu;
    inline constexpr uint16_t Z_CLEAR_VALUE = 0x0000u;
    inline constexpr uint32_t Z_CLEAR_PACK32 = 0x00000000u;

    class ZBuffer
    {
    public:
        ZBuffer() = default;

        ZBuffer(const ZBuffer &) = delete;
        ZBuffer &operator=(const ZBuffer &) = delete;

        void clear();

        PIP3D_FORCE_INLINE uint16_t *data() { return storage; }
        PIP3D_FORCE_INLINE const uint16_t *data() const { return storage; }

        static constexpr uint32_t kPixelCount =
            static_cast<uint32_t>(SCREEN_WIDTH) * SCREEN_BAND_HEIGHT;
        static constexpr uint32_t kByteSize = kPixelCount * sizeof(uint16_t);
        static constexpr uint32_t kStride = SCREEN_WIDTH;

    private:
        alignas(PIP3D_CACHELINE_SIZE) uint16_t storage[kPixelCount];
    };

    inline void ZBuffer::clear()
    {
        std::memset(storage, 0, kByteSize);
    }
}
