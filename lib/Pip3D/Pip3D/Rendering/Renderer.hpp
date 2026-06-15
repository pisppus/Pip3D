#pragma once

#include "Core/Platform.hpp"
#include "Core/Color.hpp"
#include "Core/Viewport.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Events.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Resources.hpp"
#include "Debug/Gizmos.hpp"
#include "Camera/Camera.hpp"
#include "Camera/Frustum.hpp"
#include "Geometry/Instance.hpp"
#include "Core/Jobs.hpp"
#include "Math/Algebra.hpp"
#include "Geometry/Mesh.hpp"
#include "Rendering/UI/Font.hpp"
#include "Rendering/Display/ZBuffer.hpp"
#include "Display/DirtyRegions.hpp"
#include "Lighting/Lighting.hpp"
#include "Lighting/Shadow.hpp"
#include "Pipeline/Rasterizer.hpp"
#include "Pipeline/Shading.hpp"
#include "Rendering/Display/Sky.hpp"
#if defined(PIP3D_PC)
#include <PipCore/Platforms/Desktop/Runtime.hpp>
#else
#include <PipCore/Platforms/Select.hpp>
#endif
#include "UI/HUD.hpp"
#include "Pipeline/Culling.hpp"
#include "Rendering/Pipeline/MeshDraw.hpp"
#include "Camera/Camera.hpp"
#include <vector>

#ifndef TFT_MOSI
#define TFT_MOSI 11
#endif

#ifndef TFT_SCLK
#define TFT_SCLK 12
#endif

#ifndef PIP3D_DISPLAY_ROTATION
#define PIP3D_DISPLAY_ROTATION 1
#endif

class PhysicsWorld;

namespace pip3D
{
    class Renderer
    {

    private:
    private:
        void updateReflectionBufferOnDemand()
        {
            if (!reflectionBuffer)
                return;

            const DisplayConfig &fbCfg = framebuffer.getConfig();
            int16_t bandY = currentBandOffsetY();
            const uint16_t *fb = framebuffer.getBuffer();
            if (!fb)
                return;

            int16_t startRefly = bandY / 2;
            int16_t endRefly = (bandY + fbCfg.height) / 2;

            for (int16_t refly = startRefly; refly < endRefly; ++refly)
            {
                int16_t fbY = refly * 2 - bandY;
                if (fbY < 0 || fbY >= fbCfg.height)
                    continue;

                uint16_t *dstRow = reflectionBuffer + refly * reflectionWidth;
                const uint16_t *srcRow = fb + fbY * fbCfg.width;

                int16_t reflx = 0;
                for (; reflx < reflectionWidth - 3; reflx += 4)
                {
                    dstRow[reflx] = srcRow[reflx * 2];
                    dstRow[reflx + 1] = srcRow[(reflx + 1) * 2];
                    dstRow[reflx + 2] = srcRow[(reflx + 2) * 2];
                    dstRow[reflx + 3] = srcRow[(reflx + 3) * 2];
                }
                for (; reflx < reflectionWidth; ++reflx)
                {
                    dstRow[reflx] = srcRow[reflx * 2];
                }
            }
        }
        uint16_t *reflectionBuffer = nullptr;
        uint16_t reflectionWidth = 0;
        uint16_t reflectionHeight = 0;

        void updateReflectionBuffer(int bandIndex)
        {
            if (!reflectionBuffer)
                return;

            const DisplayConfig &fbCfg = framebuffer.getConfig();
            int16_t bandY = static_cast<int16_t>(bandIndex * fbCfg.height);
            const uint16_t *fb = framebuffer.getBuffer();
            if (!fb)
                return;

            int16_t startRefly = bandY / 2;
            int16_t endRefly = (bandY + fbCfg.height) / 2;

            for (int16_t refly = startRefly; refly < endRefly; ++refly)
            {
                int16_t fbY = refly * 2 - bandY;
                if (fbY < 0 || fbY >= fbCfg.height)
                    continue;

                uint16_t *dstRow = reflectionBuffer + refly * reflectionWidth;
                const uint16_t *srcRow = fb + fbY * fbCfg.width;

                int16_t reflx = 0;
                for (; reflx < reflectionWidth - 3; reflx += 4)
                {
                    dstRow[reflx] = srcRow[reflx * 2];
                    dstRow[reflx + 1] = srcRow[(reflx + 1) * 2];
                    dstRow[reflx + 2] = srcRow[(reflx + 2) * 2];
                    dstRow[reflx + 3] = srcRow[(reflx + 3) * 2];
                }
                for (; reflx < reflectionWidth; ++reflx)
                {
                    dstRow[reflx] = srcRow[reflx * 2];
                }
            }
        }

        static constexpr int BAND_COUNT = SCREEN_BAND_COUNT;
        static constexpr int BAND_HEIGHT = SCREEN_BAND_HEIGHT;

        FrameBuffer framebuffer;
        ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer;

        static constexpr size_t MAX_QUEUE_ELEMENTS = 64;
        MeshInstance *shadowQueue[MAX_QUEUE_ELEMENTS];
        MeshInstance *opaqueQueue[MAX_QUEUE_ELEMENTS];
        size_t shadowQueueCount = 0;
        size_t opaqueQueueCount = 0;

        Mesh *meshShadowQueue[MAX_QUEUE_ELEMENTS];
        Mesh *meshOpaqueQueue[MAX_QUEUE_ELEMENTS];
        size_t meshShadowQueueCount = 0;
        size_t meshOpaqueQueueCount = 0;

        PhysicsWorld *physicsWorld = nullptr;
#if defined(PIP3D_PC)
        bool pcDisplayReady;
#else
        pipcore::Display *display;
#endif

        std::vector<Camera> cameras;
        int activeCameraIndex;

        Matrix4x4 viewMatrix;
        Matrix4x4 projMatrix;
        Matrix4x4 viewProjMatrix;
        bool viewProjMatrixDirty;

        Viewport viewport;
        Frustum frustum;

        std::vector<Light> lights;
        int activeLightCount;

        bool shadowsEnabled;
        bool backfaceCullingEnabled;
        bool occlusionCullingEnabled;

        ShadowSettings shadowSettings;

        PerformanceCounter perfCounter;

        ShadingMode shadingMode;

        uint32_t statsTrianglesTotal;
        uint32_t statsTrianglesBackfaceCulled;
        uint32_t statsInstancesTotal;
        uint32_t statsInstancesFrustumCulled;
        uint32_t statsInstancesOcclusionCulled;
        WorldInstanceDirtySlot worldInstanceDirty[MAX_WORLD_DIRTY_INSTANCES];

        int16_t worldDirtyMinX;
        int16_t worldDirtyMinY;
        int16_t worldDirtyMaxX;
        int16_t worldDirtyMaxY;
        int16_t lastWorldDirtyMinX;
        int16_t lastWorldDirtyMinY;
        int16_t lastWorldDirtyMaxX;
        int16_t lastWorldDirtyMaxY;
        bool hasWorldDirtyRegion;
        bool hasLastWorldDirtyRegion;

        int16_t hudDirtyMinX;
        int16_t hudDirtyMinY;
        int16_t hudDirtyMaxX;
        int16_t hudDirtyMaxY;
        bool hasHudDirtyRegion;

        bool cameraChangedThisFrame;

        bool debugShowDirtyRegions;
        bool initialized;

        bool shouldRenderShadowForBounds(const Vector3 &center, float radius) const
        {
            if (!shadowsEnabled || radius <= 0.0f)
                return false;

            const Camera &cam = cameras[activeCameraIndex];
            if (cam.projectionType != PERSPECTIVE)
                return true;

            Vector3 toCenter = center - cam.position;
            float distSq = toCenter.lengthSquared();
            if (distSq <= 1e-6f)
                return true;

            const float maxDistance = cam.farPlane + radius * 6.0f + 24.0f;
            if (distSq > maxDistance * maxDistance)
                return false;

            return true;
        }

        void drawSunDiscAtScreen(int16_t cx, int16_t cyFull, const Color &color, float glow, float sizeScale)
        {
            auto cfg = framebuffer.getConfig();
            uint16_t *fb = framebuffer.getBuffer();
            if (!fb)
            {
                return;
            }

            int16_t minDim = viewport.width < viewport.height ? viewport.width : viewport.height;
            if (minDim <= 0)
            {
                return;
            }

            if (sizeScale < 0.2f)
            {
                sizeScale = 0.2f;
            }
            if (sizeScale > 3.0f)
            {
                sizeScale = 3.0f;
            }

            float baseRadius = minDim * 0.018f * sizeScale;
            if (baseRadius < 1.0f)
            {
                return;
            }

            float extra = glow;
            if (extra < 0.0f)
            {
                extra = 0.0f;
            }
            if (extra > 1.0f)
            {
                extra = 1.0f;
            }

            int16_t radius = (int16_t)(baseRadius * (0.85f + extra * 0.35f));
            if (radius <= 0)
            {
                return;
            }

            const int16_t bandTop = currentBandOffsetY();
            const int16_t bandBottom = static_cast<int16_t>(bandTop + cfg.height);
            if (cyFull + radius < bandTop || cyFull - radius >= bandBottom)
            {
                return;
            }

            int16_t cy = static_cast<int16_t>(cyFull - bandTop);
            int r2 = radius * radius;

            for (int dy = -radius; dy <= radius; ++dy)
            {
                int yy = cy + dy;
                if (yy < 0 || yy >= cfg.height)
                {
                    continue;
                }

                for (int dx = -radius; dx <= radius; ++dx)
                {
                    int xx = cx + dx;
                    if (xx < 0 || xx >= cfg.width)
                    {
                        continue;
                    }

                    int d2 = dx * dx + dy * dy;
                    if (d2 <= r2)
                    {
                        fb[yy * cfg.width + xx] = color.rgb565;
                    }
                }
            }
        }

    public:
        Renderer() : zBuffer(nullptr),
#if defined(PIP3D_PC)
                     pcDisplayReady(false),
#else
                     display(nullptr),
#endif
                     cameras(1),
                     activeCameraIndex(0),
                     viewProjMatrixDirty(true),
                     lights(1),
                     activeLightCount(1),
                     shadowsEnabled(true),
                     backfaceCullingEnabled(true),
                     occlusionCullingEnabled(false),
                     shadingMode(SHADING_FLAT),
                     statsTrianglesTotal(0),
                     statsTrianglesBackfaceCulled(0),
                     statsInstancesTotal(0),
                     statsInstancesFrustumCulled(0),
                     statsInstancesOcclusionCulled(0),
                     initialized(false)
        {
            lights[0].type = LIGHT_DIRECTIONAL;
            lights[0].direction = Vector3(-0.5f, -1.0f, -0.5f);
            lights[0].direction.normalize();
            lights[0].color = Color::WHITE;
            lights[0].intensity = 1.0f;

            hasWorldDirtyRegion = false;
            hasLastWorldDirtyRegion = false;
            hasHudDirtyRegion = false;
            cameraChangedThisFrame = false;
            debugShowDirtyRegions = false;
            worldDirtyMinX = 0;
            worldDirtyMinY = 0;
            worldDirtyMaxX = 0;
            worldDirtyMaxY = 0;
            lastWorldDirtyMinX = 0;
            lastWorldDirtyMinY = 0;
            lastWorldDirtyMaxX = 0;
            lastWorldDirtyMaxY = 0;
            hudDirtyMinX = 0;
            hudDirtyMinY = 0;
            hudDirtyMaxX = 0;
            hudDirtyMaxY = 0;

            for (int i = 0; i < MAX_WORLD_DIRTY_INSTANCES; ++i)
            {
                worldInstanceDirty[i].instance = nullptr;
                worldInstanceDirty[i].hasCurrent = false;
                worldInstanceDirty[i].hasLast = false;
            }
        }

        void setPhysicsWorld(PhysicsWorld *world) { physicsWorld = world; }
        void draw(MeshInstance *instance)
        {
            if (unlikely(!instance || !instance->isVisible()))
                return;

            if (shadowsEnabled && instance->getMesh() && instance->getMesh()->getCastShadows())
            {
                if (shadowQueueCount < MAX_QUEUE_ELEMENTS)
                {
                    shadowQueue[shadowQueueCount++] = instance;
                }
            }

            if (opaqueQueueCount < MAX_QUEUE_ELEMENTS)
            {
                opaqueQueue[opaqueQueueCount++] = instance;
            }
        }

        void draw(Mesh *mesh)
        {
            if (unlikely(!mesh || !mesh->isVisible()))
                return;

            if (shadowsEnabled && mesh->getCastShadows())
            {
                if (meshShadowQueueCount < MAX_QUEUE_ELEMENTS)
                {
                    meshShadowQueue[meshShadowQueueCount++] = mesh;
                }
            }

            if (meshOpaqueQueueCount < MAX_QUEUE_ELEMENTS)
            {
                meshOpaqueQueue[meshOpaqueQueueCount++] = mesh;
            }
        }

        Renderer(const Renderer &) = delete;
        Renderer &operator=(const Renderer &) = delete;
        Renderer(Renderer &&) = delete;
        Renderer &operator=(Renderer &&) = delete;

        ~Renderer()
        {
            if (zBuffer)
                delete zBuffer;
            if (reflectionBuffer)
            {
                MemUtils::freeAligned(reflectionBuffer);
                reflectionBuffer = nullptr;
            }
        }

        bool init(const DisplayConfig &cfg)
        {
            initialized = false;
            Shading::initLUT();
            useDualCore(true);

            LOGI(::pip3D::Debug::LOG_MODULE_RENDER,
                 "Renderer::init: display %dx%d @ %dMHz (cs=%d, dc=%d, rst=%d, bl=%d)",
                 cfg.width,
                 cfg.height,
                 (int)(cfg.spi_freq / 1000000),
                 cfg.cs,
                 cfg.dc,
                 cfg.rst,
                 cfg.bl);

#if defined(PIP3D_PC)
            auto &runtime = pipcore::desktop::Runtime::instance();
            if (!runtime.configureDisplay(cfg.width, cfg.height) || !runtime.beginDisplay(0))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RENDER,
                     "Renderer::init: pc display init failed");
                initialized = false;
                return false;
            }
            pcDisplayReady = true;
#else
            auto *platform = pipcore::GetPlatform();
            if (!platform)
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RENDER,
                     "Renderer::init: failed to get pipcore platform");
                return false;
            }

            pipcore::DisplayConfig displayCfg;
            displayCfg.mosi = TFT_MOSI;
            displayCfg.sclk = TFT_SCLK;
            displayCfg.cs = cfg.cs;
            displayCfg.dc = cfg.dc;
            displayCfg.rst = cfg.rst;
            displayCfg.width = cfg.height;
            displayCfg.height = cfg.width;
            displayCfg.hz = cfg.spi_freq;
            displayCfg.order = 0;
            displayCfg.invert = true;
            displayCfg.swap = true;
            displayCfg.xOffset = 0;
            displayCfg.yOffset = 0;

            if (!platform->configDisplay(displayCfg) || !platform->beginDisplay(PIP3D_DISPLAY_ROTATION))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RENDER,
                     "Renderer::init: display init failed");
                initialized = false;
                return false;
            }

            display = platform->display();
            if (!display)
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RENDER,
                     "Renderer::init: platform returned null display");
                initialized = false;
                return false;
            }
#endif

            DisplayConfig fbCfg = cfg;
            fbCfg.height = cfg.height / BAND_COUNT;

            if (!framebuffer.init(fbCfg,
#if defined(PIP3D_PC)
                                  pcDisplayReady
#else
                                  display
#endif
                                  ))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RENDER,
                     "Renderer::init: FrameBuffer::init failed for %dx%d",
                     cfg.width,
                     cfg.height);
                initialized = false;
                return false;
            }

            zBuffer = new ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT>();
            if (!zBuffer || !zBuffer->init())
            {
                if (!zBuffer)
                {
                    LOGE(::pip3D::Debug::LOG_MODULE_RENDER,
                         "Renderer::init: failed to allocate ZBuffer");
                }
                else
                {
                    LOGE(::pip3D::Debug::LOG_MODULE_RENDER,
                         "Renderer::init: ZBuffer::init failed");
                }
                if (zBuffer)
                {
                    delete zBuffer;
                    zBuffer = nullptr;
                }
#if !defined(PIP3D_PC)
                display = nullptr;
#endif
                initialized = false;
                return false;
            }

            viewport = Viewport(0, 0, cfg.width, cfg.height);

            if (reflectionBuffer)
            {
                MemUtils::freeAligned(reflectionBuffer);
                reflectionBuffer = nullptr;
            }

            reflectionWidth = cfg.width / 2;
            reflectionHeight = cfg.height / 2;
            size_t reflSize = reflectionWidth * reflectionHeight * sizeof(uint16_t);

            reflectionBuffer = (uint16_t *)MemUtils::allocAligned(reflSize, 16, pipcore::AllocCaps::PreferInternal);
            if (reflectionBuffer)
            {
                memset(reflectionBuffer, 0, reflSize);
                LOGI(::pip3D::Debug::LOG_MODULE_RENDER,
                     "Renderer::init: SSPR reflection buffer allocated (%dx%d, %d bytes)",
                     (int)reflectionWidth, (int)reflectionHeight, (int)reflSize);
            }
            else
            {
                LOGW(::pip3D::Debug::LOG_MODULE_RENDER,
                     "Renderer::init: SSPR reflection buffer allocation failed (OOM)!");
            }

            LOGI(::pip3D::Debug::LOG_MODULE_RENDER,
                 "Renderer::init OK: viewport %dx%d",
                 cfg.width,
                 cfg.height);

            initialized = true;
            return true;
        }

        bool isInitialized() const
        {
            return initialized &&
#if defined(PIP3D_PC)
                   pcDisplayReady &&
#else
                   display != nullptr &&
#endif
                   zBuffer != nullptr;
        }

        void beginFrame()
        {
            if (!isInitialized())
                return;
            beginFrameBand(0);
        }

        void endFrame()
        {
            if (!isInitialized())
                return;
#if ENABLE_DEBUG_DRAW
            ::pip3D::Debug::Gizmos::render(*this);
#endif

            framebuffer.endFrameRegion(0, currentBandOffsetY(),
                                       framebuffer.getConfig().width,
                                       framebuffer.getConfig().height);
            perfCounter.endFrame();
        }

        void endFrameRegion(int16_t x, int16_t y, int16_t w, int16_t h)
        {
            framebuffer.endFrameRegion(x, y, w, h);
            perfCounter.endFrame();
        }

        void flushQueue()
        {
            for (size_t i = 0; i < shadowQueueCount; ++i)
            {
                drawMeshInstanceShadow(shadowQueue[i]);
            }

            for (size_t i = 0; i < meshShadowQueueCount; ++i)
            {
                drawMeshShadow(meshShadowQueue[i]);
            }

            for (size_t i = 0; i < opaqueQueueCount; ++i)
            {
                drawMeshInstanceInternal(opaqueQueue[i], false, true);
            }

            for (size_t i = 0; i < meshOpaqueQueueCount; ++i)
            {
                drawMesh(meshOpaqueQueue[i]);
            }
        }

        void beginFrameBand(int bandIndex)
        {
            if (!isInitialized())
                return;

            shadowQueueCount = 0;
            opaqueQueueCount = 0;

            meshShadowQueueCount = 0;
            meshOpaqueQueueCount = 0;

            if (bandIndex < 0)
                bandIndex = 0;
            if (bandIndex >= BAND_COUNT)
                bandIndex = BAND_COUNT - 1;

            int16_t bandTop = static_cast<int16_t>(bandIndex * BAND_HEIGHT);
            currentBandOffsetY() = bandTop;
            currentBandHeight() = BAND_HEIGHT;

            if (bandIndex == 0)
            {
                currentFrameStamp()++;
                perfCounter.begin();

                for (int i = 0; i < activeLightCount; ++i)
                {
                    float r, g, b;
                    lights[i].getCachedRGB(r, g, b);
                }

                hasWorldDirtyRegion = false;
                hasLastWorldDirtyRegion = false;
                hasHudDirtyRegion = false;
                cameraChangedThisFrame = false;

                CameraController::updateViewProjectionIfNeeded(cameras[activeCameraIndex],
                                                               viewport,
                                                               viewMatrix,
                                                               projMatrix,
                                                               viewProjMatrix,
                                                               frustum,
                                                               viewProjMatrixDirty,
                                                               cameraChangedThisFrame);

                statsTrianglesTotal = 0;
                statsTrianglesBackfaceCulled = 0;
                statsInstancesTotal = 0;
                statsInstancesFrustumCulled = 0;
                statsInstancesOcclusionCulled = 0;
            }

            framebuffer.beginFrame();
            if (zBuffer)
                zBuffer->clear();

#if ENABLE_DEBUG_DRAW
            ::pip3D::Debug::Gizmos::beginFrame();
#endif
        }

        void endFrameBand(int bandIndex)
        {
            if (!isInitialized())
                return;
            if (bandIndex < 0)
                bandIndex = 0;
            if (bandIndex >= BAND_COUNT)
                bandIndex = BAND_COUNT - 1;

            const DisplayConfig &fbCfg = framebuffer.getConfig();
            int16_t bandY = static_cast<int16_t>(bandIndex * fbCfg.height);

            framebuffer.endFrameRegion(0, bandY, fbCfg.width, fbCfg.height);

            if (bandIndex == BAND_COUNT - 1)
            {
                perfCounter.endFrame();
            }
        }

        void drawSkyboxBackground()
        {
            framebuffer.drawSkyboxWhereEmpty(*zBuffer);
        }

        Vector3 project(const Vector3 &v)
        {
            return CameraController::project(v, viewProjMatrix, viewport);
        }

        void drawSunSprite(const Vector3 &worldPos, const Color &color, float glow, float sizeScale = 1.0f)
        {
            Vector3 p = project(worldPos);
            if (cameras[activeCameraIndex].projectionType == PERSPECTIVE && p.z <= 0.0f)
            {
                return;
            }
            drawSunDiscAtScreen((int16_t)p.x, (int16_t)p.y, color, glow, sizeScale);
        }

        void drawSunSpriteDirectional(const Vector3 &sunDir, const Color &color, float glow, float sizeScale = 1.0f)
        {
            const Camera &cam = cameras[activeCameraIndex];
            Vector3 dir = sunDir;
            const float lenSq = dir.lengthSquared();
            if (lenSq <= 1e-6f)
            {
                return;
            }
            dir *= (1.0f / sqrtf(lenSq));

            const float forwardDot = dir.dot(cam.forward());
            if (forwardDot <= 0.001f)
            {
                return;
            }

            const float aspect = viewport.height > 0 ? (float)viewport.width / (float)viewport.height : 1.0f;
            const float tanHalfFov = tanf(cam.fov * kDegToRad * 0.5f);
            if (tanHalfFov <= 1e-6f)
            {
                return;
            }

            const float rightDot = dir.dot(cam.right());
            const float upDot = dir.dot(cam.upVec());
            const float ndcX = rightDot / (forwardDot * tanHalfFov * aspect);
            const float ndcY = upDot / (forwardDot * tanHalfFov);
            if (ndcX < -1.2f || ndcX > 1.2f || ndcY < -1.2f || ndcY > 1.2f)
            {
                return;
            }

            const int16_t screenX = static_cast<int16_t>((ndcX + 1.0f) * 0.5f * viewport.width);
            const int16_t screenY = static_cast<int16_t>((1.0f - ndcY) * 0.5f * viewport.height);
            drawSunDiscAtScreen(screenX, screenY, color, glow, sizeScale);
        }

        void drawWater(float yLevel, float size, Color color, float alpha, float time)
        {
            uint16_t *fb = framebuffer.getBuffer();
            if (!fb || !zBuffer)
            {
                return;
            }

            if (alpha <= 0.0f)
            {
                return;
            }
            if (alpha > 1.0f)
            {
                alpha = 1.0f;
            }

            const uint8_t alphaByte = static_cast<uint8_t>(alpha * COLOR_BYTE_MAX_F);
            const DisplayConfig &cfg = framebuffer.getConfig();

            Camera &cam = cameras[activeCameraIndex];

            addDirtyRect(nullptr, 0, 0, viewport.width, viewport.height);

            const Vector3 center(0.0f, yLevel, 0.0f);
            const float radius = size * 0.75f;
            if (!frustum.sphere(center, radius))
            {
                return;
            }

            const int GRID = 32;
            const float half = size * 0.5f;
            const float step = size / static_cast<float>(GRID);

            const float freq = 0.6f;
            const float amp = size * 0.02f;

            float sinTerms[GRID + 1];
            float cosTerms[GRID + 1];
            for (int i = 0; i <= GRID; ++i)
            {
                float val = -half + step * static_cast<float>(i);
                sinTerms[i] = FastMath::fastSin(val * freq + time) * amp;
                cosTerms[i] = FastMath::fastCos(val * freq + time) * amp;
            }

            for (int iz = 0; iz < GRID; ++iz)
            {
                float z0 = -half + step * static_cast<float>(iz);
                float z1 = z0 + step;

                for (int ix = 0; ix < GRID; ++ix)
                {
                    float x0 = -half + step * static_cast<float>(ix);
                    float x1 = x0 + step;

                    Vector3 v00(x0, yLevel + sinTerms[ix] + cosTerms[iz], z0);
                    Vector3 v10(x1, yLevel + sinTerms[ix + 1] + cosTerms[iz], z0);
                    Vector3 v01(x0, yLevel + sinTerms[ix] + cosTerms[iz + 1], z1);
                    Vector3 v11(x1, yLevel + sinTerms[ix + 1] + cosTerms[iz + 1], z1);

                    drawWaterTriangleInternal(v00, v10, v11, color, alphaByte, cam, cfg, fb);
                    drawWaterTriangleInternal(v00, v11, v01, color, alphaByte, cam, cfg, fb);
                }
            }
        }

        void drawTriangle3D(const Vector3 &v0, const Vector3 &v1, const Vector3 &v2, uint16_t color)
        {
            MeshRenderer::drawTriangle3D(v0, v1, v2, color,
                                         cameras[activeCameraIndex],
                                         viewport,
                                         viewProjMatrix,
                                         framebuffer,
                                         zBuffer,
                                         lights.data(),
                                         activeLightCount,
                                         backfaceCullingEnabled,
                                         statsTrianglesTotal,
                                         statsTrianglesBackfaceCulled);
        }

        Camera &getCamera() { return cameras[activeCameraIndex]; }
        Camera &getCamera(int index)
        {
            if (index >= 0 && index < (int)cameras.size())
                return cameras[index];
            return cameras[activeCameraIndex];
        }
        const Viewport &getViewport() const { return viewport; }
        float getFPS() const { return perfCounter.getFPS(); }
        float getAverageFPS() const { return perfCounter.getAverageFPS(); }
        uint32_t getFrameTime() const { return perfCounter.getFrameTime(); }
        int getActiveCameraIndex() const { return activeCameraIndex; }
        int getCameraCount() const { return cameras.size(); }
        uint16_t *getFrameBuffer() const { return const_cast<uint16_t *>(framebuffer.getBuffer()); }
        const Frustum &getFrustum() const { return frustum; }

        uint32_t getStatsTrianglesTotal() const { return statsTrianglesTotal; }
        uint32_t getStatsTrianglesBackfaceCulled() const { return statsTrianglesBackfaceCulled; }
        uint32_t getStatsInstancesTotal() const { return statsInstancesTotal; }
        uint32_t getStatsInstancesFrustumCulled() const { return statsInstancesFrustumCulled; }
        uint32_t getStatsInstancesOcclusionCulled() const { return statsInstancesOcclusionCulled; }

        int createCamera()
        {
            cameras.push_back(Camera());
            return cameras.size() - 1;
        }

        void setActiveCamera(int index)
        {
            if (index >= 0 && index < (int)cameras.size())
            {
                activeCameraIndex = index;
                viewProjMatrixDirty = true;
            }
        }

        void setLight(int index, const Light &light)
        {
            if (index < 0)
            {
                LOGW(::pip3D::Debug::LOG_MODULE_RENDER,
                     "Renderer::setLight: negative index %d (activeLightCount=%d)",
                     index,
                     activeLightCount);
                return;
            }

            if (index >= static_cast<int>(lights.size()))
            {
                lights.resize(index + 1);
            }

            lights[index] = light;
            lights[index].colorCacheDirty = true;

            if (index + 1 > activeLightCount)
                activeLightCount = index + 1;
        }

        int addLight(const Light &light)
        {
            if (activeLightCount < static_cast<int>(lights.size()))
            {
                lights[activeLightCount] = light;
            }
            else
            {
                lights.push_back(light);
            }

            lights[activeLightCount].colorCacheDirty = true;
            return activeLightCount++;
        }

        void removeLight(int index)
        {
            if (index < 0 || index >= activeLightCount)
            {
                LOGW(::pip3D::Debug::LOG_MODULE_RENDER,
                     "Renderer::removeLight: index %d out of range (activeLightCount=%d)",
                     index,
                     activeLightCount);
                return;
            }

            if (index == activeLightCount - 1)
            {
                activeLightCount--;
                return;
            }

            for (int i = index; i < activeLightCount - 1; i++)
            {
                lights[i] = lights[i + 1];
            }

            activeLightCount--;
        }

        Light *getLight(int index)
        {
            if (index < 0 || index >= activeLightCount)
            {
                LOGW(::pip3D::Debug::LOG_MODULE_RENDER,
                     "Renderer::getLight: index %d out of range (activeLightCount=%d)",
                     index,
                     activeLightCount);
                return nullptr;
            }
            return &lights[index];
        }

        void clearLights()
        {
            activeLightCount = 0;
        }

        int getLightCount() const
        {
            return activeLightCount;
        }

        void setMainDirectionalLight(const Vector3 &direction, const Color &color, float intensity = 1.0f)
        {
            if (activeLightCount == 0)
                activeLightCount = 1;
            if (lights.empty())
                lights.resize(1);
            lights[0].type = LIGHT_DIRECTIONAL;
            lights[0].direction = direction;
            lights[0].direction.normalize();
            lights[0].color = color;
            lights[0].intensity = intensity;
            lights[0].colorCacheDirty = true;
        }

        void setMainPointLight(const Vector3 &position, const Color &color, float intensity = 1.0f, float range = 10.0f)
        {
            if (activeLightCount == 0)
                activeLightCount = 1;
            if (lights.empty())
                lights.resize(1);
            lights[0].type = LIGHT_POINT;
            lights[0].position = position;
            lights[0].color = color;
            lights[0].intensity = intensity;
            lights[0].setRange(range);
            lights[0].colorCacheDirty = true;
        }

        void setLightColor(const Color &color)
        {
            if (activeLightCount == 0 || lights.empty())
            {
                LOGW(::pip3D::Debug::LOG_MODULE_RENDER,
                     "Renderer::setLightColor called with no active lights (activeLightCount=%d, lightsEmpty=%d)",
                     activeLightCount,
                     lights.empty() ? 1 : 0);
                return;
            }
            lights[0].color = color;
            lights[0].colorCacheDirty = true;
        }

        void setLightPosition(const Vector3 &pos)
        {
            if (activeLightCount == 0 || lights.empty())
            {
                LOGW(::pip3D::Debug::LOG_MODULE_RENDER,
                     "Renderer::setLightPosition called with no active lights (activeLightCount=%d, lightsEmpty=%d)",
                     activeLightCount,
                     lights.empty() ? 1 : 0);
                return;
            }
            lights[0].position = pos;
        }

        void setLightDirection(const Vector3 &dir)
        {
            if (activeLightCount == 0 || lights.empty())
            {
                LOGW(::pip3D::Debug::LOG_MODULE_RENDER,
                     "Renderer::setLightDirection called with no active lights (activeLightCount=%d, lightsEmpty=%d)",
                     activeLightCount,
                     lights.empty() ? 1 : 0);
                return;
            }
            lights[0].direction = dir;
            lights[0].direction.normalize();
        }

        void setLightTemperature(float kelvin)
        {
            if (activeLightCount == 0 || lights.empty())
            {
                LOGW(::pip3D::Debug::LOG_MODULE_RENDER,
                     "Renderer::setLightTemperature called with no active lights (activeLightCount=%d, lightsEmpty=%d)",
                     activeLightCount,
                     lights.empty() ? 1 : 0);
                return;
            }
            Color color = Color::fromTemperature(kelvin);
            lights[0].color = color;
            lights[0].colorCacheDirty = true;
        }

        Color getLightColor() const
        {
            if (activeLightCount == 0 || lights.empty())
            {
                LOGW(::pip3D::Debug::LOG_MODULE_RENDER,
                     "Renderer::getLightColor called with no active lights (activeLightCount=%d, lightsEmpty=%d)",
                     activeLightCount,
                     lights.empty() ? 1 : 0);
                return Color::WHITE;
            }
            return lights[0].color;
        }

        void setShadowsEnabled(bool enabled)
        {
            shadowsEnabled = enabled;
        }
        bool getShadowsEnabled() const { return shadowsEnabled; }
        void setBackfaceCullingEnabled(bool enabled) { backfaceCullingEnabled = enabled; }
        bool getBackfaceCullingEnabled() const { return backfaceCullingEnabled; }

        void setOcclusionCullingEnabled(bool enabled) { occlusionCullingEnabled = enabled; }
        bool getOcclusionCullingEnabled() const { return occlusionCullingEnabled; }

        void setDebugShowDirtyRegions(bool enabled) { debugShowDirtyRegions = enabled; }
        bool getDebugShowDirtyRegions() const { return debugShowDirtyRegions; }

        void setShadingMode(ShadingMode mode)
        {
            shadingMode = mode;
        }

        ShadingMode getShadingMode() const { return shadingMode; }

        void setLightType(LightType type)
        {
            if (activeLightCount == 0 || lights.empty())
            {
                LOGW(::pip3D::Debug::LOG_MODULE_RENDER,
                     "Renderer::setLightType called with no active lights (activeLightCount=%d, lightsEmpty=%d)",
                     activeLightCount,
                     lights.empty() ? 1 : 0);
                return;
            }
            lights[0].type = type;
        }

        void setSkyboxEnabled(bool enabled) { framebuffer.setSkyboxEnabled(enabled); }
        void setSkyboxType(SkyboxType type) { framebuffer.setSkyboxType(type); }
        void setSkybox(SkyboxType type)
        {
            framebuffer.setSkyboxType(type);
            float temp = framebuffer.getSkybox().getLightTemp();
            setLightTemperature(temp);
        }
        void setClearColor(Color color) { framebuffer.setClearColor(color); }
        Skybox &getSkybox() { return framebuffer.getSkybox(); }
        bool isSkyboxEnabled() const { return framebuffer.isSkyboxEnabled(); }

        void drawText(int16_t x, int16_t y, const char *text, uint16_t color = 0xFFFF)
        {
            HudRenderer::drawText(framebuffer, x, y, text, color);

            int16_t w = HudRenderer::getTextWidth(text);
            addHudDirtyRect(x, y, w, 8);
        }

        void drawText(int16_t x, int16_t y, const char *text, Color color)
        {
            drawText(x, y, text, color.rgb565);
        }

        void drawTextAdaptive(int16_t x, int16_t y, const char *text)
        {
            uint16_t color = getAdaptiveTextColor(x, y);
            drawText(x, y, text, color);
        }

        uint16_t getAdaptiveTextColor(int16_t x, int16_t y, int16_t width = 40, int16_t height = 8)
        {
            return HudRenderer::getAdaptiveTextColor(framebuffer, viewport, x, y, width, height);
        }

        int16_t getTextWidth(const char *text)
        {
            return HudRenderer::getTextWidth(text);
        }

        void drawMesh(Mesh *mesh)
        {
            MeshRenderer::drawMesh(mesh,
                                   cameras[activeCameraIndex],
                                   viewport,
                                   frustum,
                                   viewProjMatrix,
                                   framebuffer,
                                   zBuffer,
                                   lights.data(),
                                   activeLightCount,
                                   backfaceCullingEnabled,
                                   statsTrianglesTotal,
                                   statsTrianglesBackfaceCulled,
                                   shadingMode);
        }

        void drawMesh(Mesh *mesh, ShadingMode mode)
        {
            ShadingMode prev = shadingMode;
            shadingMode = mode;
            drawMesh(mesh);
            shadingMode = prev;
        }

        void drawWaterMesh(Mesh *mesh, float time)
        {
            if (!mesh)
                return;

            updateReflectionBufferOnDemand();

            MeshRenderer::drawWaterMesh(mesh,
                                        cameras[activeCameraIndex],
                                        viewport,
                                        frustum,
                                        viewProjMatrix,
                                        framebuffer,
                                        zBuffer,
                                        time,
                                        reflectionBuffer,
                                        reflectionWidth,
                                        reflectionHeight);
        }

    public:
        void drawMeshInstanceInternal(MeshInstance *instance, bool performFrustumCull, bool trackDirty)
        {
            if (!instance || !instance->isVisible())
                return;

            Mesh *mesh = instance->getMesh();
            if (!mesh)
                return;

            Vector3 center = instance->center();
            float radius = instance->radius();

            if (performFrustumCull)
            {
                if (!frustum.sphere(center, radius))
                {
                    statsInstancesFrustumCulled++;
                    return;
                }
            }

            const Camera &cam = cameras[activeCameraIndex];
            if (cam.projectionType == PERSPECTIVE)
            {
                Vector3 toCenter = center - cam.position;
                float distForward = toCenter.dot(cam.forward());
                if (distForward > cam.nearPlane)
                {
                    static float cachedFov = -1.0f;
                    static float cachedProjScale = 1.0f;
                    if (unlikely(cam.fov != cachedFov))
                    {
                        cachedFov = cam.fov;
                        float s, c;
                        FastMath::fastSinCos(cam.fov * 0.5f * kDegToRad, s, c);
                        cachedProjScale = c * FastMath::fastReciprocal(s);
                    }

                    const float invDist = FastMath::fastReciprocal(distForward);
                    const float radiusPixels = radius * cachedProjScale * invDist * (static_cast<float>(viewport.height) * 0.5f);

                    if (radiusPixels < 1.0f)
                    {
                        statsInstancesTotal++;
                        return;
                    }
                }
            }

            statsInstancesTotal++;

            const DisplayConfig &framebufferConfig = framebuffer.getConfig();
            if (occlusionCullingEnabled &&
                Culling::isInstanceOccluded(center, radius,
                                            cam,
                                            viewport,
                                            viewProjMatrix,
                                            zBuffer,
                                            framebufferConfig))
            {
                statsInstancesOcclusionCulled++;
                return;
            }

            if (trackDirty)
            {
                addDirtyFromSphere(instance, center, radius);
            }

            const uint16_t instColor565 = instance->color().rgb565;
            float baseR, baseG, baseB;
            MeshRenderer::decodeColorToFloat(instColor565, baseR, baseG, baseB);

            bool useUniformColor = mesh->getSingleColorLighting();
            uint16_t uniformColor = 0;
            const Light *const activeLights = lights.data();
            const Matrix4x4 &worldTransform = instance->transform();

            if (useUniformColor)
            {
                Vector3 localNormal = mesh->numVertices() > 0 ? mesh->vert(0).normal.get() : Vector3(0.0f, 1.0f, 0.0f);
                Vector3 worldNormal = worldTransform.transformNormal(localNormal);
                Vector3 viewDir = cam.position - center;
                viewDir.normalize();

                float finalR, finalG, finalB;
                Shading::calculateLighting(center, worldNormal, viewDir,
                                           activeLights, activeLightCount,
                                           baseR, baseG, baseB,
                                           finalR, finalG, finalB,
                                           true);
                uniformColor = Shading::quantizeColor(finalR, finalG, finalB);
            }

            const uint16_t vertexCountUsed = mesh->numVertices();
            const uint16_t faceCount = mesh->numFaces();
            
            bool useFallbackPath = false;
            Vector3 *worldVerts = nullptr;
            Vector3 *screenVerts = nullptr;

            if (instance->ensureProjectionCache(vertexCountUsed))
            {
                worldVerts = instance->getCachedWorldVertices();
                screenVerts = instance->getCachedScreenVertices();
            }
            else
            {
                useFallbackPath = true;
            }

            const Vector3 *localVerts = nullptr;
            if (mesh->ensureDecodedVertexCache())
                localVerts = mesh->getCachedLocalVertices();

            const uint32_t frameStamp = currentFrameStamp();
            const int16_t bandTop = currentBandOffsetY();
            const int16_t bandBottom = static_cast<int16_t>(bandTop + currentBandHeight());
            const float viewportWidth = static_cast<float>(viewport.width);
            const float viewportHalfWidth = viewportWidth * 0.5f;
            const float viewportHalfHeight = static_cast<float>(viewport.height) * 0.5f;

            if (!useFallbackPath && instance->getCachedProjectionFrameStamp() != frameStamp)
            {
                for (uint16_t i = 0; i < vertexCountUsed; ++i)
                {
                    Vector3 local = localVerts ? localVerts[i] : mesh->decodePosition(mesh->vert(i));
                    Vector3 world = worldTransform.transformNoDiv(local);
                    worldVerts[i] = world;
                    screenVerts[i] = CameraController::project(world, viewProjMatrix,
                                                               viewportHalfWidth, viewportHalfHeight,
                                                               viewport.x, viewport.y);
                }

                instance->setCachedProjectionFrameStamp(frameStamp);
            }

            thread_local static std::vector<Vector3> vertexColors;
            if (shadingMode == SHADING_GOURAUD && !useUniformColor)
            {
                if (vertexColors.size() < vertexCountUsed)
                    vertexColors.resize(vertexCountUsed);

                const Vector3 camPos = cam.position;
                for (uint16_t vi = 0; vi < vertexCountUsed; ++vi)
                {
                    Vector3 localNormal = mesh->vert(vi).normal.get();
                    Vector3 worldNormal = worldTransform.transformNormal(localNormal);
                    Vector3 v;
                    if (!useFallbackPath)
                    {
                        v = worldVerts[vi];
                    }
                    else
                    {
                        Vector3 local = localVerts ? localVerts[vi] : mesh->decodePosition(mesh->vert(vi));
                        v = worldTransform.transformNoDiv(local);
                    }
                    Vector3 viewDir = camPos - v;
                    viewDir.normalize();

                    float r, g, b;
                    Shading::calculateLighting(v, worldNormal, viewDir,
                                               activeLights, activeLightCount,
                                               baseR, baseG, baseB,
                                               r, g, b);
                    vertexColors[vi] = Vector3(r, g, b);
                }
            }

            for (uint16_t i = 0; i < faceCount; ++i)
            {
                const Face &face = mesh->face(i);
                
                Vector3 v0, v1, v2;
                Vector3 p0, p1, p2;

                if (!useFallbackPath)
                {
                    v0 = worldVerts[face.v0];
                    v1 = worldVerts[face.v1];
                    v2 = worldVerts[face.v2];

                    p0 = screenVerts[face.v0];
                    p1 = screenVerts[face.v1];
                    p2 = screenVerts[face.v2];
                }
                else
                {
                    Vector3 local0 = localVerts ? localVerts[face.v0] : mesh->decodePosition(mesh->vert(face.v0));
                    Vector3 local1 = localVerts ? localVerts[face.v1] : mesh->decodePosition(mesh->vert(face.v1));
                    Vector3 local2 = localVerts ? localVerts[face.v2] : mesh->decodePosition(mesh->vert(face.v2));

                    v0 = worldTransform.transformNoDiv(local0);
                    v1 = worldTransform.transformNoDiv(local1);
                    v2 = worldTransform.transformNoDiv(local2);

                    p0 = CameraController::project(v0, viewProjMatrix, viewportHalfWidth, viewportHalfHeight, viewport.x, viewport.y);
                    p1 = CameraController::project(v1, viewProjMatrix, viewportHalfWidth, viewportHalfHeight, viewport.x, viewport.y);
                    p2 = CameraController::project(v2, viewProjMatrix, viewportHalfWidth, viewportHalfHeight, viewport.x, viewport.y);
                }

                float d0 = (v0 - cam.position).dot(cam.forward());
                float d1 = (v1 - cam.position).dot(cam.forward());
                float d2 = (v2 - cam.position).dot(cam.forward());

                if (d0 < cam.nearPlane && d1 < cam.nearPlane && d2 < cam.nearPlane)
                    continue;

                bool partiallyClipped = (d0 < cam.nearPlane || d1 < cam.nearPlane || d2 < cam.nearPlane);

                if (!partiallyClipped)
                {
                    float minY = fminf(p0.y, fminf(p1.y, p2.y));
                    float maxY = fmaxf(p0.y, fmaxf(p1.y, p2.y));
                    if (maxY < bandTop || minY >= bandBottom)
                        continue;

                    float minX = fminf(p0.x, fminf(p1.x, p2.x));
                    float maxX = fmaxf(p0.x, fmaxf(p1.x, p2.x));
                    if (maxX < 0.0f || minX >= viewportWidth)
                        continue;
                }

                statsTrianglesTotal++;
                if (backfaceCullingEnabled)
                {
                    Vector3 faceNormal = (v1 - v0).cross(v2 - v0);
                    float normalLenSq = faceNormal.lengthSquared();
                    if (normalLenSq <= 1e-10f)
                    {
                        statsTrianglesBackfaceCulled++;
                        continue;
                    }

                    float facing = faceNormal.dot(cam.position - v0);
                    if (facing <= 0.0f)
                    {
                        statsTrianglesBackfaceCulled++;
                        continue;
                    }
                }

                if (shadingMode == SHADING_GOURAUD && !useUniformColor && !partiallyClipped)
                {
                    const Vector3 &c0 = vertexColors[face.v0];
                    const Vector3 &c1 = vertexColors[face.v1];
                    const Vector3 &c2 = vertexColors[face.v2];

                    Vector3 lp0 = p0;
                    Vector3 lp1 = p1;
                    Vector3 lp2 = p2;
                    lp0.y -= (float)bandTop;
                    lp1.y -= (float)bandTop;
                    lp2.y -= (float)bandTop;

                    Rasterizer::fillTriangleSmooth(
                        (int16_t)lp0.x, (int16_t)lp0.y, lp0.z,
                        (int16_t)lp1.x, (int16_t)lp1.y, lp1.z,
                        (int16_t)lp2.x, (int16_t)lp2.y, lp2.z,
                        c0.x, c0.y, c0.z,
                        c1.x, c1.y, c1.z,
                        c2.x, c2.y, c2.z,
                        framebuffer.getBuffer(),
                        zBuffer,
                        framebufferConfig);
                    continue;
                }

                MeshRenderer::drawTriangle3D_Preprojected(v0, v1, v2,
                                                          p0, p1, p2,
                                                          instColor565,
                                                          cam,
                                                          viewport,
                                                          viewProjMatrix,
                                                          framebuffer,
                                                          zBuffer,
                                                          activeLights,
                                                          activeLightCount,
                                                          backfaceCullingEnabled,
                                                          statsTrianglesTotal,
                                                          statsTrianglesBackfaceCulled,
                                                          useUniformColor,
                                                          uniformColor);
            }
        }

        void drawMeshInstance(MeshInstance *instance)
        {
            drawMeshInstanceInternal(instance, true, true);
        }

        void drawMeshInstance(MeshInstance *instance, ShadingMode mode)
        {
            ShadingMode prev = shadingMode;
            shadingMode = mode;
            drawMeshInstance(instance);
            shadingMode = prev;
        }

        void drawMeshInstanceStatic(MeshInstance *instance)
        {
            drawMeshInstanceInternal(instance, true, false);
        }

        void drawInstances(InstanceManager &manager)
        {
            static std::vector<MeshInstance *> visibleInstances;
            visibleInstances.reserve(manager.count());
            manager.cull(frustum, visibleInstances);

            manager.sort(cameras[activeCameraIndex].position, visibleInstances);

            for (auto *instance : visibleInstances)
            {
                drawMeshInstanceInternal(instance, false, true);
            }
        }

        void drawMeshShadow(Mesh *mesh)
        {
            if (!mesh || !mesh->getCastShadows())
                return;
            if (!shouldRenderShadowForBounds(mesh->center(), mesh->radius()))
                return;

            ShadowRenderer::drawMeshShadow(mesh,
                                           shadowsEnabled,
                                           shadowSettings,
                                           cameras[activeCameraIndex],
                                           lights.data(),
                                           activeLightCount,
                                           viewProjMatrix,
                                           viewport,
                                           framebuffer,
                                           zBuffer,
                                           backfaceCullingEnabled);
        }
        void drawMeshInstanceShadow(MeshInstance *instance)
        {
            if (!instance || !instance->isVisible())
                return;
            Mesh *mesh = instance->getMesh();
            if (!mesh || !mesh->getCastShadows())
                return;
            if (!shouldRenderShadowForBounds(instance->center(), instance->radius()))
                return;

            ShadowSettings activeSettings = shadowSettings;
            if (physicsWorld)
            {
                RaycastHit hit;
                Ray downRay(instance->pos(), Vector3(0.0f, -1.0f, 0.0f));
                if (physicsWorld->raycast(downRay, hit, 30.0f))
                {
                    Vector3 hitPointWithOffset = hit.point + hit.normal * activeSettings.shadowOffset;
                    activeSettings.plane = ShadowProjector::ShadowPlane::fromPointAndNormal(hitPointWithOffset, hit.normal);
                }
            }

            ShadowRenderer::drawMeshInstanceShadow(instance,
                                                   shadowsEnabled,
                                                   activeSettings,
                                                   cameras[activeCameraIndex],
                                                   lights.data(),
                                                   activeLightCount,
                                                   viewProjMatrix,
                                                   viewport,
                                                   framebuffer,
                                                   zBuffer,
                                                   backfaceCullingEnabled);
        }
        void setShadowOpacity(float opacity)
        {
            shadowSettings.shadowOpacity = clamp(opacity, 0.0f, 1.0f);
        }

        void setShadowColor(const Color &color)
        {
            shadowSettings.shadowColor = color;
        }

        void setShadowPlane(const Vector3 &normal, float distance)
        {
            shadowSettings.plane = ShadowProjector::ShadowPlane(normal, distance);
        }

        void setShadowPlaneY(float y)
        {
            shadowSettings.plane = ShadowProjector::ShadowPlane(Vector3(0, 1, 0), -y);
        }

        ShadowSettings &getShadowSettings() { return shadowSettings; }

    private:
        __attribute__((always_inline)) inline void addDirtyRect(MeshInstance *instance, int16_t x, int16_t y, int16_t w, int16_t h)
        {
            DirtyRegionHelper::addDirtyRect(instance, x, y, w, h,
                                            viewport,
                                            worldInstanceDirty,
                                            worldDirtyMinX,
                                            worldDirtyMinY,
                                            worldDirtyMaxX,
                                            worldDirtyMaxY,
                                            hasWorldDirtyRegion);
        }

        void addHudDirtyRect(int16_t x, int16_t y, int16_t w, int16_t h)
        {
            DirtyRegionHelper::addHudDirtyRect(x, y, w, h,
                                               viewport,
                                               hudDirtyMinX,
                                               hudDirtyMinY,
                                               hudDirtyMaxX,
                                               hudDirtyMaxY,
                                               hasHudDirtyRegion);
        }

        void addDirtyFromSphere(MeshInstance *instance, const Vector3 &c, float r)
        {
            if (r <= 0.0f)
                return;

            Vector3 pc = project(c);
            Vector3 px = project(Vector3(c.x + r, c.y, c.z));
            Vector3 py = project(Vector3(c.x, c.y + r, c.z));
            Vector3 pz = project(Vector3(c.x, c.y, c.z + r));

            float dx = fabsf(px.x - pc.x);
            float dy = fabsf(px.y - pc.y);

            float t = fabsf(py.x - pc.x);
            if (t > dx)
                dx = t;
            t = fabsf(pz.x - pc.x);
            if (t > dx)
                dx = t;

            t = fabsf(py.y - pc.y);
            if (t > dy)
                dy = t;
            t = fabsf(pz.y - pc.y);
            if (t > dy)
                dy = t;

            float rScr = dx > dy ? dx : dy;

            int16_t x0 = (int16_t)(pc.x - rScr);
            int16_t y0 = (int16_t)(pc.y - rScr);
            int16_t x1 = (int16_t)(pc.x + rScr + 1.0f);
            int16_t y1 = (int16_t)(pc.y + rScr + 1.0f);

            addDirtyRect(instance, x0, y0, x1 - x0, y1 - y0);
        }

        __attribute__((always_inline)) inline void drawWaterTriangleInternal(const Vector3 &v0,
                                                                             const Vector3 &v1,
                                                                             const Vector3 &v2,
                                                                             const Color &waterColor,
                                                                             uint8_t alphaByte,
                                                                             const Camera &cam,
                                                                             const DisplayConfig &cfg,
                                                                             uint16_t *frameBufferPtr)
        {
            Vector3 p0 = CameraController::project(v0, viewProjMatrix, viewport);
            Vector3 p1 = CameraController::project(v1, viewProjMatrix, viewport);
            Vector3 p2 = CameraController::project(v2, viewProjMatrix, viewport);

            float x0 = p0.x, y0 = p0.y, z0 = p0.z;
            float x1 = p1.x, y1 = p1.y, z1 = p1.z;
            float x2 = p2.x, y2 = p2.y, z2 = p2.z;

            float minXf = fminf(x0, fminf(x1, x2));
            float maxXf = fmaxf(x0, fmaxf(x1, x2));
            float minYf = fminf(y0, fminf(y1, y2));
            float maxYf = fmaxf(y0, fmaxf(y1, y2));

            int16_t minX = static_cast<int16_t>(floorf(minXf));
            int16_t maxX = static_cast<int16_t>(ceilf(maxXf));
            int16_t minY = static_cast<int16_t>(floorf(minYf));
            int16_t maxY = static_cast<int16_t>(ceilf(maxYf));

            if (maxX < 0 || maxY < 0 || minX >= (int16_t)SCREEN_WIDTH || minY >= (int16_t)SCREEN_HEIGHT)
            {
                return;
            }

            if (minX < 0)
                minX = 0;
            if (minY < 0)
                minY = 0;
            if (maxX >= (int16_t)SCREEN_WIDTH)
                maxX = (int16_t)SCREEN_WIDTH - 1;
            if (maxY >= (int16_t)SCREEN_HEIGHT)
                maxY = (int16_t)SCREEN_HEIGHT - 1;

            int16_t bandTop = currentBandOffsetY();
            int16_t bandH = currentBandHeight();
            int16_t bandBottom = static_cast<int16_t>(bandTop + bandH);

            if (maxY < bandTop || minY >= bandBottom)
            {
                return;
            }

            if (minY < bandTop)
                minY = bandTop;
            if (maxY >= bandBottom)
                maxY = static_cast<int16_t>(bandBottom - 1);

            float denom = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2);
            if (fabsf(denom) < 1e-6f)
            {
                return;
            }
            float invDenom = 1.0f / denom;

            for (int16_t y = minY; y <= maxY; ++y)
            {
                float py = static_cast<float>(y) + 0.5f;
                int16_t yLocal = static_cast<int16_t>(y - bandTop);

                for (int16_t x = minX; x <= maxX; ++x)
                {
                    float px = static_cast<float>(x) + 0.5f;

                    float w0 = ((y1 - y2) * (px - x2) + (x2 - x1) * (py - y2)) * invDenom;
                    float w1 = ((y2 - y0) * (px - x2) + (x0 - x2) * (py - y2)) * invDenom;
                    float w2 = 1.0f - w0 - w1;

                    if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f)
                        continue;

                    uint16_t &dst = frameBufferPtr[yLocal * cfg.width + x];
                    Color bg(dst);
                    dst = bg.blend(waterColor, alphaByte).rgb565;
                }
            }
        }
    };

    class MultiCameraHelper
    {
    public:
        static int createIsometricCamera(Renderer &renderer, float distance)
        {
            int idx = renderer.createCamera();
            Camera &c = renderer.getCamera(idx);
            c.setOrtho(distance, distance, 0.1f, 100.0f);

            float angle = 0.785398f;
            float dist = distance * 1.5f;
            c.position = Vector3(dist * cosf(angle), dist * 0.7f, dist * sinf(angle));
            c.target = Vector3(0.0f, 0.0f, 0.0f);
            c.markDirty();
            return idx;
        }
    };
}