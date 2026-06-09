#pragma once

#include "Core/Platform.hpp"
#include "Core/Memory.hpp"
#include "Core/Color.hpp"
#include "Core/Viewport.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Events.hpp"
#include "Core/Resources.hpp"
#include "Core/Jobs.hpp"

#include "Debug/Flags.hpp"
#include "Debug/Logging.hpp"
#include "Debug/Gizmos.hpp"

#include "Math/Algebra.hpp"
#include "Math/Collision.hpp"

#include "Physics/Physics.hpp"

#include "Camera/Camera.hpp"
#include "Camera/Frustum.hpp"
#include "Camera/Timeline.hpp"

#include "Geometry/Mesh.hpp"
#include "Geometry/Instance.hpp"
#include "Geometry/Primitives.hpp"

#include "Input/Input.hpp"

#include "Rendering/Display/FrameBuffer.hpp"
#include "Rendering/Display/ZBuffer.hpp"
#include "Rendering/Display/Sky.hpp"
#include "Rendering/Renderer.hpp"

#include "Rendering/Pipeline/Rasterizer.hpp"
#include "Rendering/Pipeline/Shading.hpp"
#include "Rendering/Pipeline/Object.hpp"
#include "Rendering/Pipeline/Water.hpp"

#include "Rendering/Lighting/Lighting.hpp"
#include "Rendering/Lighting/Shadow.hpp"

#include "Rendering/Effects/Particles.hpp"

#include "Rendering/UI/Font.hpp"
#include "Rendering/UI/HUD.hpp"

#include "Scene/Node.hpp"
#include "Scene/Graph.hpp"
#include "Scene/Helper.hpp"
#include "Scene/Atmosphere.hpp"
#include "Scene/Character.hpp"

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