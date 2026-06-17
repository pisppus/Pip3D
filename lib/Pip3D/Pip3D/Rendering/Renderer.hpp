#pragma once

#include "Core/Platform.hpp"
#include "Core/Color.hpp"
#include "Core/Viewport.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Events.hpp"
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

namespace pip3D
{
    class PhysicsWorld;

    class Renderer
    {
    private:
        void updateReflectionBufferOnDemand();
        void updateReflectionBuffer(int bandIndex);

        uint16_t *reflectionBuffer = nullptr;
        uint16_t reflectionWidth = 0;
        uint16_t reflectionHeight = 0;

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

        bool fogEnabled;
        Color fogColor;
        float fogNear;
        float fogFar;

        bool shouldRenderShadowForBounds(const Vector3 &center, float radius) const;
        void drawSunDiscAtScreen(int16_t cx, int16_t cyFull, const Color &color, float glow, float sizeScale);
        void drawWaterTriangleInternal(const Vector3 &v0, const Vector3 &v1, const Vector3 &v2, const Color &waterColor, uint8_t alphaByte, const Camera &cam, const DisplayConfig &cfg, uint16_t *frameBufferPtr);

        void addDirtyRect(MeshInstance *instance, int16_t x, int16_t y, int16_t w, int16_t h);
        void addHudDirtyRect(int16_t x, int16_t y, int16_t w, int16_t h);
        void addDirtyFromSphere(MeshInstance *instance, const Vector3 &c, float r);

    public:
        Renderer();
        ~Renderer();

        bool init(const DisplayConfig &cfg);
        void beginFrame();
        void endFrame();
        void endFrameRegion(int16_t x, int16_t y, int16_t w, int16_t h);
        void beginFrameBand(int bandIndex);
        void endFrameBand(int bandIndex);
        void drawSkyboxBackground();
        Vector3 project(const Vector3 &v);

        int createCamera();
        void setActiveCamera(int index);
        Camera &getCamera();
        Camera &getCamera(int index);

        void setLight(int index, const Light &light);
        int addLight(const Light &light);
        void removeLight(int index);
        Light *getLight(int index);
        void clearLights();
        void setMainDirectionalLight(const Vector3 &direction, const Color &color, float intensity = 1.0f);
        void setMainPointLight(const Vector3 &position, const Color &color, float intensity = 1.0f, float range = 10.0f);
        void setLightColor(const Color &color);
        void setLightPosition(const Vector3 &pos);
        void setLightDirection(const Vector3 &dir);
        void setLightTemperature(float kelvin);
        Color getLightColor() const;

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

        void setPhysicsWorld(PhysicsWorld *world) { physicsWorld = world; }
        const Viewport &getViewport() const { return viewport; }
        float getFPS() const { return perfCounter.getFPS(); }
        float getAverageFPS() const { return perfCounter.getAverageFPS(); }
        uint32_t getFrameTime() const { return perfCounter.getFrameTime(); }
        int getActiveCameraIndex() const { return activeCameraIndex; }
        int getCameraCount() const { return cameras.size(); }
        uint16_t *getFrameBuffer() const { return const_cast<uint16_t *>(framebuffer.getBuffer()); }
        ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *getZBuffer() const { return zBuffer; }
        const Frustum &getFrustum() const { return frustum; }

        uint32_t getStatsTrianglesTotal() const { return statsTrianglesTotal; }
        uint32_t getStatsTrianglesBackfaceCulled() const { return statsTrianglesBackfaceCulled; }
        uint32_t getStatsInstancesTotal() const { return statsInstancesTotal; }
        uint32_t getStatsInstancesFrustumCulled() const { return statsInstancesFrustumCulled; }
        uint32_t getStatsInstancesOcclusionCulled() const { return statsInstancesOcclusionCulled; }

        void setShadowsEnabled(bool enabled) { shadowsEnabled = enabled; }
        bool getShadowsEnabled() const { return shadowsEnabled; }
        void setBackfaceCullingEnabled(bool enabled) { backfaceCullingEnabled = enabled; }
        bool getBackfaceCullingEnabled() const { return backfaceCullingEnabled; }
        void setOcclusionCullingEnabled(bool enabled) { occlusionCullingEnabled = enabled; }
        bool getOcclusionCullingEnabled() const { return occlusionCullingEnabled; }
        void setDebugShowDirtyRegions(bool enabled) { debugShowDirtyRegions = enabled; }
        bool getDebugShowDirtyRegions() const { return debugShowDirtyRegions; }
        void setShadingMode(ShadingMode mode) { shadingMode = mode; }
        ShadingMode getShadingMode() const { return shadingMode; }

        void setLightType(LightType type)
        {
            if (activeLightCount == 0 || lights.empty())
                return;
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

        void setShadowOpacity(float opacity) { shadowSettings.shadowOpacity = clamp(opacity, 0.0f, 1.0f); }
        void setShadowColor(const Color &color) { shadowSettings.shadowColor = color; }
        void setShadowPlane(const Vector3 &normal, float distance) { shadowSettings.plane = ShadowProjector::ShadowPlane(normal, distance); }
        void setShadowPlaneY(float y) { shadowSettings.plane = ShadowProjector::ShadowPlane(Vector3(0, 1, 0), -y); }
        ShadowSettings &getShadowSettings() { return shadowSettings; }

        void setFogEnabled(bool enabled) { fogEnabled = enabled; }
        bool getFogEnabled() const { return fogEnabled; }
        void setFogColor(const Color &color) { fogColor = color; }
        Color getFogColor() const { return fogColor; }
        void setFogLimits(float nearDist, float farDist)
        {
            fogNear = nearDist;
            fogFar = farDist;
        }
        float getFogNear() const { return fogNear; }
        float getFogFar() const { return fogFar; }

        void draw(MeshInstance *instance);
        void draw(Mesh *mesh);
        void flushQueue();
        void drawMeshInstanceInternal(MeshInstance *instance, bool performFrustumCull, bool trackDirty);
        void drawMeshInstance(MeshInstance *instance);
        void drawMeshInstance(MeshInstance *instance, ShadingMode mode);
        void drawMeshInstanceStatic(MeshInstance *instance);
        void drawInstances(InstanceManager &manager);
        void drawMeshShadow(Mesh *mesh);
        void drawMeshInstanceShadow(MeshInstance *instance);
        void drawMesh(Mesh *mesh);
        void drawMesh(Mesh *mesh, ShadingMode mode);
        void drawTriangle3D(const Vector3 &v0, const Vector3 &v1, const Vector3 &v2, uint16_t color);

        void drawWaterMesh(Mesh *mesh, float time);
        void drawWater(float yLevel, float size, Color color, float alpha, float time);
        void drawSunSprite(const Vector3 &worldPos, const Color &color, float glow, float sizeScale = 1.0f);
        void drawSunSpriteDirectional(const Vector3 &sunDir, const Color &color, float glow, float sizeScale = 1.0f);
        void drawText(int16_t x, int16_t y, const char *text, uint16_t color = 0xFFFF);
        void drawText(int16_t x, int16_t y, const char *text, Color color);
        void drawTextAdaptive(int16_t x, int16_t y, const char *text);
        uint16_t getAdaptiveTextColor(int16_t x, int16_t y, int16_t width = 40, int16_t height = 8);
        int16_t getTextWidth(const char *text);
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