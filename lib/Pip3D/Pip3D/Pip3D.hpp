#pragma once

#include "Core/Platform.hpp"
#include "Core/Memory.hpp"
#include "Core/Color.hpp"
#include "Core/Diagnostics.hpp"
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

#include "Audio/SpatialAudio.hpp"
#include "Audio/Reverb.hpp"

#include "Geometry/Mesh.hpp"
#include "Geometry/Instance.hpp"
#include "Geometry/Primitives/Basic.hpp"
#include "Geometry/Primitives/Capsule.hpp"
#include "Geometry/Primitives/Helix.hpp"
#include "Geometry/Primitives/Sphere.hpp"
#include "Geometry/Primitives/Torus.hpp"
#include "Geometry/Primitives/TrefoilKnot.hpp"

#include "Rendering/Resources/Texture.hpp"
#include "Rendering/Buffers/FrameBuffer.hpp"
#include "Rendering/Buffers/ZBuffer.hpp"

#include "Rendering/Environment/Sky.hpp"
#include "Rendering/Environment/Clouds.hpp"
#include "Rendering/Environment/TimeOfDay.hpp"

#include "Rendering/Lighting/Lighting.hpp"
#include "Rendering/Lighting/Shadow.hpp"
#include "Rendering/Lighting/Deferred.hpp"

#include "Rendering/Pipeline/Shading.hpp"
#include "Rendering/Pipeline/Billboard.hpp"
#include "Rendering/Pipeline/Culling.hpp"
#include "Rendering/Pipeline/MeshDraw.hpp"
#include "Rendering/Pipeline/DrawCache.hpp"

#include "Rendering/Effects/Glow.hpp"
#include "Rendering/Effects/Particles.hpp"
#include "Rendering/UI/Font.hpp"
#include "Rendering/UI/HUD.hpp"

#include "Rendering/Renderer.hpp"

#include "Scene/Node.hpp"
#include "Scene/Graph.hpp"
#include "Scene/Helper.hpp"
#include "Scene/Character.hpp"

namespace pip3D
{
    inline constexpr uint8_t VERSION_MAJOR = 0;
    inline constexpr uint8_t VERSION_MINOR = 3;
    inline constexpr uint8_t VERSION_PATCH = 0;
    inline constexpr const char *VERSION = "0.3.0 - beta";

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

        cfg.mosi = TFT_MOSI;
        cfg.sclk = TFT_SCLK;
        cfg.rotation = PIP3D_DISPLAY_ROTATION;

        if (r.init(cfg))
        {
            r.setSkybox(DAY);
            r.setShadowsEnabled(true);
            r.setShadowPlaneY(0.0f);
        }

#if PIPCORE_ENABLE_AUDIO
        if (pipcore::Audio *a = pipcore::GetPlatform()->audio())
        {
            a->configure({});
            a->begin();
        }
#endif

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