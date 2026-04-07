#pragma once

#include "Core/Core.h"
#include "Core/Camera.h"
#include "Core/Frustum.h"
#include "Core/Instance.h"
#include "Core/Jobs.h"
#include "Core/Debug/DebugConfig.h"
#include "Core/Debug/Logging.h"
#include "Core/Debug/DebugDraw.h"

#include "Math/Math.h"
#include "Math/Collision.h"

#include "Physics/Physics.h"

#include "Geometry/Mesh.h"
#include "Geometry/PrimitiveShapes.h"

#include "Rendering/Display/ZBuffer.h"
#include "Rendering/Lighting/Lighting.h"
#include "Rendering/Lighting/Shadow.h"
#include "Rendering/Rasterizer/Rasterizer.h"
#include "Rendering/Rasterizer/Shading.h"
#include "Rendering/Display/FrameBuffer.h"
#include "Rendering/Renderer.h"

#include "Graphics/Font.h"

#include "Scene/SceneHelper.h"
#include "Utils/ObjectUtils.h"
#include "Utils/CameraUtils.h"
#include "Utils/CameraTimeline.h"
#include "Utils/DayNightCycle.h"
#include "Utils/TimeUtils.h"
#include "Utils/FX.h"
#include "Input/Input.h"
#include "Scene/SceneNode.h"
#include "Scene/SceneGraph.h"
#include "Scene/CharacterController.h"

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

