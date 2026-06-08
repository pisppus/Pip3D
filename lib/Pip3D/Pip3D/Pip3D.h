#pragma once

#include "Core/Platform.h"
#include "Core/Memory.h"
#include "Core/Color.h"
#include "Core/Viewport.h"
#include "Core/Diagnostics.h"
#include "Core/Events.h"
#include "Core/Resources.h"
#include "Core/Jobs.h"

#include "Debug/Flags.h"
#include "Debug/Logging.h"
#include "Debug/Gizmos.h"

#include "Math/Algebra.h"
#include "Math/Collision.h"

#include "Physics/Physics.h"

#include "Camera/Camera.h"
#include "Camera/Frustum.h"
#include "Camera/Timeline.h"

#include "Geometry/Mesh.h"
#include "Geometry/Instance.h"
#include "Geometry/Primitives.h"

#include "Input/Input.h"

#include "Rendering/Display/FrameBuffer.h"
#include "Rendering/Display/ZBuffer.h"
#include "Rendering/Display/Sky.h"
#include "Rendering/Renderer.h"

#include "Rendering/Pipeline/Rasterizer.h"
#include "Rendering/Pipeline/Shading.h"
#include "Rendering/Pipeline/Object.h"
#include "Rendering/Pipeline/Water.h"

#include "Rendering/Lighting/Lighting.h"
#include "Rendering/Lighting/Shadow.h"

#include "Rendering/Effects/Particles.h"

#include "Rendering/UI/Font.h"
#include "Rendering/UI/HUD.h"

#include "Scene/Node.h"
#include "Scene/Graph.h"
#include "Scene/Helper.h"
#include "Scene/Atmosphere.h"
#include "Scene/Character.h"

namespace pip3D
{
    inline constexpr uint8_t VERSION_MAJOR = 0;
    inline constexpr uint8_t VERSION_MINOR = 2;
    inline constexpr uint8_t VERSION_PATCH = 0;
    inline constexpr const char *VERSION = "0.2.0 - Alpha";

    inline const char *getVersion()
    {
        return VERSION;
    }

    inline Color RGB888(uint8_t r, uint8_t g, uint8_t b)
    {
        return Color::rgb(r, g, b);
    }

    inline Renderer &renderer()
    {
        static Renderer instance;
        return instance;
    }

    inline Renderer &begin3D(uint16_t width,
                             uint16_t height,
                             int8_t cs,
                             int8_t dc,
                             int8_t rst,
                             int8_t bl = -1,
                             uint32_t spi_freq = 80000000)
    {
        Renderer &r = renderer();
        DisplayConfig cfg(width, height, cs, dc, rst);
        cfg.bl = bl;
        cfg.spi_freq = spi_freq;

        if (r.init(cfg))
        {
            r.setSkyboxWithLighting(SKYBOX_DAY);
            r.setShadowsEnabled(true);
            r.setShadowPlaneY(0.0f);
        }

        return r;
    }

    inline Renderer &begin3D(int8_t cs,
                             int8_t dc,
                             int8_t rst,
                             int8_t bl = -1,
                             uint32_t spi_freq = 80000000)
    {
        return begin3D(320, 240, cs, dc, rst, bl, spi_freq);
    }
}