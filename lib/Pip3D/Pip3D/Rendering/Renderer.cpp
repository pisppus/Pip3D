#include "Renderer.hpp"
#include "Debug/Logging.hpp"
#include "Debug/Gizmos.hpp"
#include <cstring>

namespace pip3D
{
    Renderer::Renderer() : zBuffer(nullptr),
                           reflectBuffer(nullptr),
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
                           shadowCacheGeneration(1),
                           lastAutoShadowColor(Color::BLACK),
                           shadingMode(SHADING_FLAT),
                           statsTrianglesTotal(0),
                           statsTrianglesBackfaceCulled(0),
                           statsInstancesTotal(0),
                           statsInstancesFrustumCulled(0),
                           statsInstancesOcclusionCulled(0),
                           initialized(false),
                           fogEnabled(false),
                           fogColor(Color::rgb(140, 160, 175)),
                           fogNear(10.0f),
                           fogFar(80.0f)
    {
        lights[0].type = LIGHT_DIRECTIONAL;
        lights[0].direction = Vector3(-0.5f, -1.0f, -0.5f);
        lights[0].direction.normalize();
        lights[0].color = Color::WHITE;
        lights[0].intensity = 1.0f;
    }

    Renderer::~Renderer()
    {
        if (zBuffer)
            delete zBuffer;
        if (reflectBuffer)
        {
            MemUtils::freeData(reflectBuffer);
            reflectBuffer = nullptr;
        }
    }

    bool Renderer::init(const DisplayConfig &cfg)
    {
        initialized = false;
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
        const size_t reflectBytes = static_cast<size_t>(REFLECT_WIDTH) *
                                    static_cast<size_t>(REFLECT_HEIGHT) *
                                    sizeof(uint16_t);
        reflectBuffer = static_cast<uint16_t *>(MemUtils::allocData(reflectBytes, 16));
        if (reflectBuffer)
        {
            memset(reflectBuffer, 0, reflectBytes);
            LOGI(::pip3D::Debug::LOG_MODULE_RENDER,
                 "Renderer::init: reflection buffer %ux%u (%u bytes) — water SSR enabled",
                 static_cast<unsigned>(REFLECT_WIDTH),
                 static_cast<unsigned>(REFLECT_HEIGHT),
                 static_cast<unsigned>(reflectBytes));
        }
        else
        {
            LOGW(::pip3D::Debug::LOG_MODULE_RENDER,
                 "Renderer::init: reflection buffer OOM (%u bytes) — water SSR disabled (tint/sky fallback)",
                 static_cast<unsigned>(reflectBytes));
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

        const DisplayConfig &cfg = framebuffer.getConfig();
        framebuffer.endFrameRegion(0, currentBandOffsetY(), cfg.width, cfg.height);
        perfCounter.endFrame();
    }

    void IRAM_ATTR Renderer::beginFrameBand(int bandIndex)
    {
        if (!isInitialized())
            return;

        shadowQueueCount = 0;
        opaqueQueueCount = 0;
        meshShadowQueueCount = 0;
        meshOpaqueQueueCount = 0;
        blobShadowQueueCount = 0;
        meshBlobShadowQueueCount = 0;

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
                lights[i].warmCache();
            }

            CameraController::updateViewProjectionIfNeeded(cameras[activeCameraIndex],
                                                           viewport,
                                                           viewMatrix,
                                                           projMatrix,
                                                           viewProjMatrix,
                                                           frustum,
                                                           viewProjMatrixDirty);

            statsTrianglesTotal = 0;
            statsTrianglesBackfaceCulled = 0;
            statsInstancesTotal = 0;
            statsInstancesFrustumCulled = 0;
            statsInstancesOcclusionCulled = 0;

            const bool skyEnabled = framebuffer.isSkyboxEnabled();
            const Color skyHorizon = skyEnabled ? framebuffer.getSkybox().horizon
                                                : Color::rgb(40, 42, 50);

            if (shadowSettings.shadowColorAuto)
            {
                const Color autoColor = skyHorizon.darken(200);
                if (autoColor.rgb565 != lastAutoShadowColor.rgb565)
                {
                    lastAutoShadowColor = autoColor;
                    shadowSettings.shadowColor = autoColor;
                    ++shadowCacheGeneration;
                }
            }

            Rasterizer::g_fogState.enabled = fogEnabled;
            if (fogEnabled)
            {
                const Color activeFogColor = skyEnabled ? skyHorizon : fogColor;
                const uint16_t fogRGB = activeFogColor.rgb565;

                Rasterizer::g_fogState.color = fogRGB;
                Rasterizer::g_fogState.color_rb = fogRGB & 0xF81F;
                Rasterizer::g_fogState.color_g = fogRGB & 0x07E0;
                Rasterizer::g_fogState.worldNear = fogNear;

                const float worldRange = fogFar - fogNear;
                const float invWorldRange = (worldRange > 1e-4f) ? (1.0f / worldRange) : 0.0f;
                Rasterizer::g_fogState.worldScale = invWorldRange;
                Rasterizer::g_fogState.worldScale32 = 32.0f * invWorldRange;

                Rasterizer::g_fogState.color_r = static_cast<float>((fogRGB >> 11) & 0x1F) * (1.0f / 31.0f);
                Rasterizer::g_fogState.color_g_f = static_cast<float>((fogRGB >> 5) & 0x3F) * (1.0f / 63.0f);
                Rasterizer::g_fogState.color_b_f = static_cast<float>(fogRGB & 0x1F) * (1.0f / 31.0f);

                const Camera &cam = cameras[activeCameraIndex];
                const float camNear = cam.nearPlane;
                const float camFar = cam.farPlane;

                const float denomFarNear = camFar - camNear;
                const float safeDenom = (denomFarNear > 1e-4f) ? denomFarNear : 1.0f;
                const float k = 32638.0f * (camFar / safeDenom);
                Rasterizer::g_fogState.kVal = k;
                Rasterizer::g_fogState.knVal = k * camNear;
            }
        }

        if (zBuffer)
            zBuffer->clear();

#if ENABLE_DEBUG_DRAW
        ::pip3D::Debug::Gizmos::beginFrame();
#endif
    }

    void IRAM_ATTR Renderer::endFrameBand(int bandIndex)
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
        if (reflectBuffer)
        {
            const uint16_t *src = framebuffer.getBuffer();
            if (src)
            {
                const int16_t srcW = fbCfg.width;
                const int16_t srcH = fbCfg.height;
                const int16_t halfW = srcW >> 1;
                const int16_t halfH = srcH >> 1;
                const int16_t dstYBase = bandY >> 1;

                const int16_t rowsToCopy = (dstYBase + halfH <= REFLECT_HEIGHT)
                                               ? halfH
                                               : static_cast<int16_t>(REFLECT_HEIGHT - dstYBase);
                if (rowsToCopy > 0)
                {
                    const uint32_t *__restrict__ src32 = reinterpret_cast<const uint32_t *>(src);
                    uint16_t *__restrict__ dstBase = reflectBuffer +
                                                     static_cast<size_t>(dstYBase) * halfW;

                    for (int16_t dy = 0; dy < rowsToCopy; ++dy)
                    {
                        const int16_t y = dy << 1;
                        const uint32_t *row0 = src32 + static_cast<size_t>(y) * halfW;
                        const uint32_t *row1 = row0 + halfW;
                        uint16_t *__restrict__ dst = dstBase + static_cast<size_t>(dy) * halfW;

                        for (int16_t x = 0; x < halfW; ++x)
                        {
                            const uint32_t p0 = row0[x];
                            const uint32_t p1 = row1[x];
                            const uint16_t a = static_cast<uint16_t>(p0);
                            const uint16_t b = static_cast<uint16_t>(p0 >> 16);
                            const uint16_t c = static_cast<uint16_t>(p1);
                            const uint16_t d = static_cast<uint16_t>(p1 >> 16);

                            const uint32_t r = ((a >> 11) & 0x1F) + ((b >> 11) & 0x1F) +
                                               ((c >> 11) & 0x1F) + ((d >> 11) & 0x1F);
                            const uint32_t g = ((a >> 5) & 0x3F) + ((b >> 5) & 0x3F) +
                                               ((c >> 5) & 0x3F) + ((d >> 5) & 0x3F);
                            const uint32_t bb = (a & 0x1F) + (b & 0x1F) +
                                                (c & 0x1F) + (d & 0x1F);
                            dst[x] = static_cast<uint16_t>(((r >> 2) << 11) |
                                                           ((g >> 2) << 5) |
                                                           (bb >> 2));
                        }
                    }
                }
            }
        }

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
        framebuffer.fillBackground<SCREEN_WIDTH, SCREEN_BAND_HEIGHT>();
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
        ++shadowCacheGeneration;

        if (index + 1 > activeLightCount)
            activeLightCount = index + 1;
    }

    Light *Renderer::getLight(int index)
    {
        if (index < 0 || index >= activeLightCount)
            return nullptr;
        return &lights[index];
    }

    void Renderer::clearLights()
    {
        ++shadowCacheGeneration;
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
        ++shadowCacheGeneration;
    }

    void Renderer::setLightPosition(const Vector3 &pos)
    {
        if (activeLightCount == 0 || lights.empty())
            return;
        lights[0].position = pos;
        ++shadowCacheGeneration;
    }

    void Renderer::setLightDirection(const Vector3 &dir)
    {
        if (activeLightCount == 0 || lights.empty())
            return;
        lights[0].direction = dir;
        lights[0].direction.normalize();
        ++shadowCacheGeneration;
    }

    void Renderer::setLightTemperature(float kelvin)
    {
        if (activeLightCount == 0 || lights.empty())
            return;
        Color color = Color::fromTemperature(kelvin);
        lights[0].color = color;
        lights[0].colorCacheDirty = true;
    }
}
