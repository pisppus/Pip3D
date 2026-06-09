#pragma once

#include "Rendering/Renderer.hpp"

namespace pip3D
{

    struct WaterRenderer
    {
        static void drawWater(Renderer &renderer,
                              float yLevel,
                              float size,
                              Color color,
                              float alpha,
                              float time)
        {
            renderer.drawWater(yLevel, size, color, alpha, time);
        }
    };

}

