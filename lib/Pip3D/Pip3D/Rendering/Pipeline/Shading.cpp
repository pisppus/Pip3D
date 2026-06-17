#include "Shading.hpp"
#include "Rendering/Pipeline/Rasterizer.hpp"

namespace pip3D
{
    float Shading::GAMMA_LUT[256];
    bool Shading::lutInitialized = false;

    namespace Rasterizer
    {
        thread_local FogState g_fogState;
    }
}