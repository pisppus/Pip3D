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
#include "Rendering/Pipeline/Telemetry.hpp"
#include "Lighting/Lighting.hpp"
#include "Lighting/Shadow.hpp"
#include "Pipeline/Rasterizer.hpp"
#include "Pipeline/Shading.hpp"
#include "Rendering/Display/Sky.hpp"
#include <PipCore/Display.hpp>
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
    struct Billboard;

    struct FlushJob
    {
        pipcore::Display *display;
        const uint16_t *pixels;
        int16_t x;
        int16_t y;
        int16_t w;
        int16_t h;
        int32_t stridePixels;
    };

    class Renderer
    {
    private:
        static constexpr int BAND_COUNT = SCREEN_BAND_COUNT;
        static constexpr int BAND_HEIGHT = SCREEN_BAND_HEIGHT;

        FrameBuffer framebuffer;
        ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer;
        uint16_t *reflectBuffer;
        uint16_t *reflectWriteBuffer;
        static constexpr uint16_t REFLECT_WIDTH = SCREEN_WIDTH / 4;
        static constexpr uint16_t REFLECT_HEIGHT = SCREEN_HEIGHT / 4;

        static constexpr size_t MAX_QUEUE_ELEMENTS = 64;
        MeshInstance *shadowQueue[MAX_QUEUE_ELEMENTS];
        MeshInstance *opaqueQueue[MAX_QUEUE_ELEMENTS];
        size_t shadowQueueCount = 0;
        size_t opaqueQueueCount = 0;

        Mesh *meshShadowQueue[MAX_QUEUE_ELEMENTS];
        Mesh *meshOpaqueQueue[MAX_QUEUE_ELEMENTS];
        size_t meshShadowQueueCount = 0;
        size_t meshOpaqueQueueCount = 0;

        MeshInstance *blobShadowQueue[MAX_QUEUE_ELEMENTS];
        Mesh *meshBlobShadowQueue[MAX_QUEUE_ELEMENTS];
        size_t blobShadowQueueCount = 0;
        size_t meshBlobShadowQueueCount = 0;

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

        float cachedHfovRad_ = 0.0f;
        bool hfovCacheValid_ = false;

        Viewport viewport;
        Frustum frustum;

        std::vector<Light> lights;
        int activeLightCount;

        bool shadowsEnabled;
        bool backfaceCullingEnabled;
        bool occlusionCullingEnabled;

        ShadowSettings shadowSettings;
        uint32_t shadowCacheGeneration;
        Color lastAutoShadowColor;
        PerformanceCounter perfCounter;
        ShadingMode shadingMode;

        uint32_t statsTrianglesTotal;
        uint32_t statsTrianglesBackfaceCulled;
        uint32_t statsInstancesTotal;
        uint32_t statsInstancesFrustumCulled;
        uint32_t statsInstancesOcclusionCulled;

        bool initialized;

        bool fogEnabled;
        Color fogColor;
        float fogNear;
        float fogFar;

        float ambientScale = 1.0f;
        float exposureScale = 1.0f;

        bool sunEnabled = true;
        bool sunVisible = false;
        Color sunColor = Color::WHITE;
        float sunIntensity = 1.0f;
        Vector3 sunWorldDir = Vector3(0.0f, 1.0f, 0.0f);

        bool shouldRenderShadowForBounds(const Vector3 &center, float radius) const;
        void drawWaterTriangleInternal(const Vector3 &v0, const Vector3 &v1, const Vector3 &v2, const Color &waterColor, uint8_t alphaByte, const DisplayConfig &cfg, uint16_t *frameBufferPtr);

        __attribute__((always_inline)) inline float ensureHfovCached()
        {
            if (likely(hfovCacheValid_))
                return cachedHfovRad_;

            const Camera &cam = cameras[activeCameraIndex];
            const float aspect = static_cast<float>(viewport.width) /
                                 static_cast<float>(viewport.height);
            cachedHfovRad_ = 2.0f * atanf(
                                        tanf(cam.fov * 0.5f * kDegToRad) * aspect);
            hfovCacheValid_ = true;
            return cachedHfovRad_;
        }

        static void flushJobFunc(void *userData);
        static constexpr int FLUSH_JOB_SLOTS = 2;
        FlushJob flushJobs[FLUSH_JOB_SLOTS];
        int flushJobNext = 0;

    public:
        Renderer();
        ~Renderer();

        bool init(const DisplayConfig &cfg);
        void beginFrame();
        void endFrame();
        void beginFrameBand(int bandIndex);
        void endFrameBand(int bandIndex);
        void drawSkyboxBackground();
        Vector3 project(const Vector3 &v);

        int createCamera();
        void setActiveCamera(int index);
        Camera &getCamera();

        void setLight(int index, const Light &light);
        Light *getLight(int index);
        const Light *getLights() const { return lights.data(); }
        int getActiveLightCount() const { return activeLightCount; }
        void clearLights();
        void setMainDirectionalLight(const Vector3 &direction, const Color &color, float intensity = 1.0f);
        void setLightPosition(const Vector3 &pos);
        void setLightDirection(const Vector3 &dir);
        void setLightTemperature(float kelvin);

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
        void invalidateSkyboxCache() { framebuffer.invalidateSkyboxCache(); }

        void setCloudsEnabled(bool enabled) { framebuffer.setCloudsEnabled(enabled); }
        void setCloudColor(const Color &c) { framebuffer.setCloudColor(c); }
        void setCloudAlpha(float a) { framebuffer.setCloudAlpha(a); }
        bool areCloudsEnabled() const { return framebuffer.areCloudsEnabled(); }
        void generateClouds(uint32_t seed = 0xC10Du, float coverage = 0.45f)
        {
            framebuffer.generateClouds(seed, coverage);
        }

        void setShadowOpacity(float opacity)
        {
            shadowSettings.shadowOpacity = clamp(opacity, 0.0f, 1.0f);
            ++shadowCacheGeneration;
        }
        void setShadowColor(const Color &color)
        {
            shadowSettings.shadowColor = color;
            shadowSettings.shadowColorAuto = false;
            ++shadowCacheGeneration;
        }
        void setShadowColorAuto()
        {
            shadowSettings.shadowColorAuto = true;
            ++shadowCacheGeneration;
        }
        void setShadowPlane(const Vector3 &normal, float distance)
        {
            shadowSettings.plane = ShadowProjector::ShadowPlane(normal, distance);
            ++shadowCacheGeneration;
        }
        void setShadowPlaneY(float y)
        {
            shadowSettings.plane = ShadowProjector::ShadowPlane(Vector3(0, 1, 0), -y);
            ++shadowCacheGeneration;
        }
        ShadowSettings &getShadowSettings() { return shadowSettings; }
        uint32_t getShadowCacheGeneration() const { return shadowCacheGeneration; }
        void invalidateShadowCache() { ++shadowCacheGeneration; }

        void setFogEnabled(bool enabled) { fogEnabled = enabled; }
        bool getFogEnabled() const { return fogEnabled; }

        void setAmbientScale(float s) { ambientScale = (s < 0.0f) ? 0.0f : (s > 2.0f) ? 2.0f
                                                                                      : s; }
        float getAmbientScale() const { return ambientScale; }
        void setExposureScale(float s) { exposureScale = (s < 0.05f) ? 0.05f : (s > 2.0f) ? 2.0f
                                                                                          : s; }
        float getExposureScale() const { return exposureScale; }

        void setMipmapsEnabled(bool enabled) { Rasterizer::g_mipmapsEnabled = enabled; }
        bool getMipmapsEnabled() const { return Rasterizer::g_mipmapsEnabled; }
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
        void drawMeshInstanceInternal(MeshInstance *instance, bool performFrustumCull);

        bool clipAndDrawNearTextured(const DrawTelemetryClipVert inVerts[3],
                                     float nearD,
                                     const Camera &camera,
                                     const Viewport &viewport,
                                     const Matrix4x4 &viewProjMatrix,
                                     FrameBuffer &framebuffer,
                                     ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer,
                                     const Texture &tex,
                                     const Mesh *meshForTelemetry,
                                     uint16_t faceIdxForTelemetry,
                                     uint32_t frameForTelemetry);

        void drawMeshInstance(MeshInstance *instance);
        void drawMeshInstance(MeshInstance *instance, ShadingMode mode);
        void drawMeshInstanceStatic(MeshInstance *instance);
        void drawInstances(InstanceManager &manager);
        void drawMeshShadow(Mesh *mesh);
        void drawMeshInstanceShadow(MeshInstance *instance);
        void drawMesh(Mesh *mesh);
        void drawMesh(Mesh *mesh, ShadingMode mode);
        void drawTriangle3D(const Vector3 &v0, const Vector3 &v1, const Vector3 &v2, uint16_t color);
        void drawBlobShadow(const Vector3 &position, float radius, float opacity);
        void drawWaterMesh(Mesh *mesh, float time);
        void drawWater(float yLevel, float size, Color color, float alpha, float time);
        static constexpr uint16_t reflectWidth() { return REFLECT_WIDTH; }
        static constexpr uint16_t reflectHeight() { return REFLECT_HEIGHT; }
        uint16_t *getReflectBuffer() const { return reflectBuffer; }
        void drawSunSprite(const Vector3 &worldPos, const Color &color, float glow, float sizeScale = 1.0f);
        void setSunEnabled(bool enabled) { sunEnabled = enabled; }
        bool isSunEnabled() const { return sunEnabled; }
        void updateSun(const Vector3 &dir, const Color &color, float intensity, bool visible)
        {
            sunWorldDir = dir;
            sunColor = color;
            sunIntensity = intensity;
            sunVisible = visible;
        }
        bool isSunVisible() const { return sunVisible; }
        void drawSky();
        void drawBillboard(const Billboard &bb);
        void drawText(int16_t x, int16_t y, const char *text, uint16_t color = 0xFFFF);
        void drawText(int16_t x, int16_t y, const char *text, Color color);
        void drawTextAdaptive(int16_t x, int16_t y, const char *text);
        uint16_t getAdaptiveTextColor(int16_t x, int16_t y, int16_t width = 40, int16_t height = 8);
        int16_t getTextWidth(const char *text);
    };
}