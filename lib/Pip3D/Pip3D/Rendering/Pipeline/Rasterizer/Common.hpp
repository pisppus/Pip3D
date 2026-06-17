#pragma once

#include <cstdint>

namespace pip3D
{
    namespace detail
    {
        alignas(16) static constexpr int32_t kBayerMatrix10Bit[4][4] = {
            {0, 512, 128, 640},
            {768, 256, 896, 384},
            {192, 704, 64, 576},
            {960, 448, 832, 320}};
    }

    namespace Rasterizer
    {
        struct ShadowParams
        {
            uint16_t *frameBuffer;
            int16_t *zbBase;
            int16_t width;
            int16_t height;
            int32_t dz_dx_fixed;
            int32_t dz_dy_fixed;
            uint32_t s_rb;
            uint32_t s_g;
            uint8_t alpha;
            bool softEdges;
            int startTopGlobal;
            int endBottomGlobal;
            int16_t offsetY;
        };

        struct SmoothParams
        {
            uint16_t *frameBuffer;
            int16_t *zbBase;
            int16_t width;
            int16_t height;
            int32_t dz_dx_fixed;
            int32_t dz_dy_fixed;
            int32_t dr_dx_fixed;
            int32_t dr_dy_fixed;
            int32_t dg_dx_fixed;
            int32_t dg_dy_fixed;
            int32_t db_dx_fixed;
            int32_t db_dy_fixed;
            int16_t shadowMask;
            int16_t invShadowMask;
        };
    }
}