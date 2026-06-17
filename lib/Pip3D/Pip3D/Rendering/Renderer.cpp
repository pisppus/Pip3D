#include "Renderer.hpp"
#include "Physics/Physics.hpp"
#include "Debug/Logging.hpp"
#include "Debug/Gizmos.hpp"

namespace pip3D
{
    Renderer::Renderer() : zBuffer(nullptr),
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

    Renderer::~Renderer()
    {
        if (zBuffer)
            delete zBuffer;
        if (reflectionBuffer)
        {
            MemUtils::freeAligned(reflectionBuffer);
            reflectionBuffer = nullptr;
        }
    }

    bool Renderer::init(const DisplayConfig &cfg)
    {
        initialized = false;
        Shading::initLUT();
        useDualCore(true);

        LOGI(::pip3D::Debug::LOG_MODULE_RENDER,
             "Renderer::init: display %dx%d @ %dMHz (cs=%d, dc=%d, mosi=%d, sclk=%d, rst=%d, bl=%d, rotation=%u)",
             cfg.width,
             cfg.height,
             (int)(cfg.spi_freq / 1000000),
             cfg.cs,
             cfg.dc,
             cfg.mosi,
             cfg.sclk,
             cfg.rst,
             cfg.bl,
             cfg.rotation);

#if defined(PIP3D_PC)
        auto &runtime = pipcore::desktop::Runtime::instance();
        if (!runtime.configureDisplay(cfg.width, cfg.height) || !runtime.beginDisplay(0))
        {
            LOGE(::pip3D::Debug::LOG_MODULE_RENDER, "Renderer::init: pc display init failed");
            initialized = false;
            return false;
        }
        pcDisplayReady = true;
#else
        auto *platform = pipcore::GetPlatform();
        if (!platform)
        {
            LOGE(::pip3D::Debug::LOG_MODULE_RENDER, "Renderer::init: failed to get pipcore platform");
            return false;
        }

        pipcore::DisplayConfig displayCfg;
        displayCfg.mosi = cfg.mosi;
        displayCfg.sclk = cfg.sclk;
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

        if (!platform->configDisplay(displayCfg) || !platform->beginDisplay(cfg.rotation))
        {
            LOGE(::pip3D::Debug::LOG_MODULE_RENDER, "Renderer::init: display init failed");
            initialized = false;
            return false;
        }

        display = platform->display();
        if (!display)
        {
            LOGE(::pip3D::Debug::LOG_MODULE_RENDER, "Renderer::init: platform returned null display");
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
            LOGE(::pip3D::Debug::LOG_MODULE_RENDER, "Renderer::init: FrameBuffer::init failed for %dx%d", cfg.width, cfg.height);
            initialized = false;
            return false;
        }

        zBuffer = new ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT>();
        if (!zBuffer || !zBuffer->init())
        {
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
            LOGI(::pip3D::Debug::LOG_MODULE_RENDER, "Renderer::init: SSPR reflection buffer allocated (%dx%d)", (int)reflectionWidth, (int)reflectionHeight);
        }

        LOGI(::pip3D::Debug::LOG_MODULE_RENDER, "Renderer::init OK: viewport %dx%d", cfg.width, cfg.height);

        initialized = true;
        return true;
    }

    void Renderer::beginFrame()
    {
        if (!isInitialized())
            return;
        beginFrameBand(0);
    }

    void Renderer::endFrame()
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

    void Renderer::endFrameRegion(int16_t x, int16_t y, int16_t w, int16_t h)
    {
        framebuffer.endFrameRegion(x, y, w, h);
        perfCounter.endFrame();
    }

    void Renderer::beginFrameBand(int bandIndex)
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

    void Renderer::endFrameBand(int bandIndex)
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

            static uint32_t diagnosticFrameCount = 0;
            diagnosticFrameCount++;
            if ((diagnosticFrameCount & 31u) == 0u)
            {
                uint16_t *buf = framebuffer.getBuffer();
                uint16_t p0 = buf ? buf[0] : 0xFFFF;
                uint16_t p160 = buf ? buf[160] : 0xFFFF;
                uint16_t p319 = buf ? buf[319] : 0xFFFF;

                LOGI(::pip3D::Debug::LOG_MODULE_RENDER,
                     "Renderer: Frame %lu. FPS: %.1f | Triangles: %lu | Pixel Dump: [0]=0x%04X, [160]=0x%04X, [319]=0x%04X",
                     (unsigned long)diagnosticFrameCount,
                     (double)perfCounter.getFPS(),
                     (unsigned long)statsTrianglesTotal,
                     p0, p160, p319);
            }
        }
    }

    void Renderer::drawSkyboxBackground()
    {
        framebuffer.drawSkyboxWhereEmpty(*zBuffer);
    }

    Vector3 Renderer::project(const Vector3 &v)
    {
        return CameraController::project(v, viewProjMatrix, viewport);
    }

    int Renderer::createCamera()
    {
        cameras.push_back(Camera());
        return cameras.size() - 1;
    }

    void Renderer::setActiveCamera(int index)
    {
        if (index >= 0 && index < (int)cameras.size())
        {
            activeCameraIndex = index;
            viewProjMatrixDirty = true;
        }
    }

    Camera &Renderer::getCamera()
    {
        return cameras[activeCameraIndex];
    }

    Camera &Renderer::getCamera(int index)
    {
        if (index >= 0 && index < (int)cameras.size())
            return cameras[index];
        return cameras[activeCameraIndex];
    }

    void Renderer::setLight(int index, const Light &light)
    {
        if (index < 0)
            return;

        if (index >= static_cast<int>(lights.size()))
        {
            lights.resize(index + 1);
        }

        lights[index] = light;
        lights[index].colorCacheDirty = true;

        if (index + 1 > activeLightCount)
            activeLightCount = index + 1;
    }

    int Renderer::addLight(const Light &light)
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

    void Renderer::removeLight(int index)
    {
        if (index < 0 || index >= activeLightCount)
            return;

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

    Light *Renderer::getLight(int index)
    {
        if (index < 0 || index >= activeLightCount)
            return nullptr;
        return &lights[index];
    }

    void Renderer::clearLights()
    {
        activeLightCount = 0;
    }

    void Renderer::setMainDirectionalLight(const Vector3 &direction, const Color &color, float intensity)
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

    void Renderer::setMainPointLight(const Vector3 &position, const Color &color, float intensity, float range)
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

    void Renderer::setLightColor(const Color &color)
    {
        if (activeLightCount == 0 || lights.empty())
            return;
        lights[0].color = color;
        lights[0].colorCacheDirty = true;
    }

    void Renderer::setLightPosition(const Vector3 &pos)
    {
        if (activeLightCount == 0 || lights.empty())
            return;
        lights[0].position = pos;
    }

    void Renderer::setLightDirection(const Vector3 &dir)
    {
        if (activeLightCount == 0 || lights.empty())
            return;
        lights[0].direction = dir;
        lights[0].direction.normalize();
    }

    void Renderer::setLightTemperature(float kelvin)
    {
        if (activeLightCount == 0 || lights.empty())
            return;
        Color color = Color::fromTemperature(kelvin);
        lights[0].color = color;
        lights[0].colorCacheDirty = true;
    }

    Color Renderer::getLightColor() const
    {
        if (activeLightCount == 0 || lights.empty())
            return Color::WHITE;
        return lights[0].color;
    }

    void Renderer::updateReflectionBufferOnDemand()
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

    void Renderer::updateReflectionBuffer(int bandIndex)
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
}