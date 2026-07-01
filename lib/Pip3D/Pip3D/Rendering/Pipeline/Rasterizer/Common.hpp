#pragma once

#include <cstdint>

namespace pip3D
{
    namespace detail
    {
        alignas(16) static constexpr int16_t kBayerMatrix10Bit[4][4] = {
            {0, 512, 128, 640},
            {768, 256, 896, 384},
            {192, 704, 64, 576},
            {960, 448, 832, 320}};
    }

    namespace Rasterizer
    {
        struct alignas(16) FogState
        {
            float worldNear = 0.0f;
            float worldScale = 0.0f;
            float worldScale32 = 0.0f;
            float kVal = 0.0f;
            float knVal = 0.0f;
            float color_r = 0.0f;
            float color_g_f = 0.0f;
            float color_b_f = 0.0f;
            uint32_t color_rb = 0;
            uint32_t color_g = 0;
            uint16_t color = 0;
            bool enabled = false;
        };

        inline FogState g_fogState{};
        inline bool g_mipmapsEnabled = true;
        inline float g_ambientScale = 1.0f;
        inline float g_exposureScale = 1.0f;

        struct alignas(16) PlanarParams
        {
            uint16_t *frameBuffer;
            int16_t *zbBase;
            int32_t dz_dx_fixed;
            int32_t dz_dy_fixed;
            uint32_t s_rb;
            uint32_t s_g;
            uint8_t alpha;
            bool softEdges;
            int startTopGlobal;
            int endBottomGlobal;
            int16_t width;
            int16_t height;
            int16_t offsetY;
        };

        struct alignas(16) SmoothParams
        {
            uint16_t *frameBuffer;
            int16_t *zbBase;
            int32_t dz_dx_fixed;
            int32_t dz_dy_fixed;
            int32_t dr_dx_fixed;
            int32_t dr_dy_fixed;
            int32_t dg_dx_fixed;
            int32_t dg_dy_fixed;
            int32_t db_dx_fixed;
            int32_t db_dy_fixed;
            int16_t width;
            int16_t height;
        };
    }
}