#pragma once

#include <cstdint>

namespace pip3D
{

    struct Texture
    {
        const uint16_t *data;
        const uint16_t *mipData;
        uint8_t shift;
        uint8_t mipCount;

        PIP3D_FORCE_INLINE uint16_t mask() const noexcept
        {
            return static_cast<uint16_t>((1u << shift) - 1u);
        }

        PIP3D_FORCE_INLINE uint16_t dimPx() const noexcept
        {
            return static_cast<uint16_t>(1u << shift);
        }

        PIP3D_FORCE_INLINE float dimFlt() const noexcept
        {
            return static_cast<float>(1u << shift);
        }
    };
}