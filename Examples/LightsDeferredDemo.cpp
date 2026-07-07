#include <Arduino.h>
#include <algorithm>
#include <array>
#include <vector>
#include <math.h>
#include <stdio.h>
#include <string.h>

#if defined(PIP3D_PC)
#include <PipCore/Platforms/Desktop/Runtime.hpp>
#endif

#define TFT_MOSI 6
#define TFT_MISO -1
#define TFT_SCLK 5

#include "Pip3D.hpp"
#include "Rendering/Display/Textures/Barrier.hpp"
#include "Rendering/Display/Textures/Gravel.hpp"
#include "Rendering/Display/Textures/Sun.hpp"
#include "Geometry/Billboard.hpp"

using namespace pip3D;

static const int8_t TFT_CS_PIN = 7;
static const int8_t TFT_DC_PIN = 8;
static const int8_t TFT_RST_PIN = -1;
static const int8_t TFT_BL_PIN = -1;

static InstanceManager g_instances;
static BillboardManager g_billboards;

static Plane *g_floorMesh = nullptr;
static Plane *g_backWallMesh = nullptr;
static Cube *g_shrineMesh = nullptr;

static MeshInstance *g_floorInstance = nullptr;
static MeshInstance *g_backWallInstance = nullptr;
static MeshInstance *g_shrineInstance = nullptr;

struct Player
{
    Vector3 eyePos;
    float yawDeg = 0.0f;
    float pitchDeg = 0.0f;
    float moveSpeed = 4.5f;
    float eyeHeight = 1.7f;
};
static Player g_player;

static TimeOfDayController g_timeOfDay;

char g_serialLine[96];
uint8_t g_serialLen = 0;

static bool g_sunOn = true;

static void initMeshes()
{
    g_floorMesh = new Plane(40.0f, 40.0f, 4, Color::WHITE, 12.0f);
    g_floorMesh->setCastShadows(false);
    g_floorMesh->setSingleColorLighting(false);
    g_floorMesh->setTexture(&g_gravelTexture);

    g_backWallMesh = new Plane(16.0f, 8.0f, 2, Color::rgb(140, 140, 145));
    g_backWallMesh->setCastShadows(false);
    g_backWallMesh->setSingleColorLighting(false);

    g_shrineMesh = new Cube(1.6f, Color::WHITE);
    g_shrineMesh->setTexture(&g_barrierTexture);
    g_shrineMesh->setCastShadows(true);
}

static void buildScene()
{
    g_floorInstance = g_instances.create(g_floorMesh);
    g_floorInstance->setPosition(0.0f, 0.0f, 0.0f);
    g_floorInstance->setColor(Color::rgb(50, 50, 55));

    g_backWallInstance = g_instances.create(g_backWallMesh);
    g_backWallInstance->setPosition(0.0f, 4.0f, -10.0f);
    g_backWallInstance->setEuler(90.0f, 0.0f, 0.0f);
    g_backWallInstance->setColor(Color::rgb(40, 40, 45));

    g_shrineInstance = g_instances.create(g_shrineMesh);
    g_shrineInstance->setPosition(0.0f, 0.8f, -8.0f);
    g_shrineInstance->setColor(Color::rgb(140, 140, 140));
}

static void setupDemoLights(Renderer &r)
{
    r.clearPointLights();

    r.addPointLight(Vector3(0.0f, 2.4f, -9.7f), Color::rgb(255, 140, 45), 7.5f, 2.2f);
    r.addPointLight(Vector3(-3.5f, 0.2f, -7.0f), Color::rgb(0, 180, 220), 4.5f, 0.9f);
}

static void applyPlayerToCamera(Renderer &r)
{
    Camera &cam = r.getCamera();
    cam.position = g_player.eyePos;

    const float yr = g_player.yawDeg * kDegToRad;
    const float pr = g_player.pitchDeg * kDegToRad;
    const float cp = cosf(pr);
    const Vector3 fwd(sinf(yr) * cp, sinf(pr), cosf(yr) * cp);
    cam.target = g_player.eyePos + fwd;
    cam.up = Vector3(0.0f, 1.0f, 0.0f);
    cam.markDirty();
}

static bool parseCommand(const char *line, Renderer &r)
{
    if (line[0] == 0)
        return true;

    char cmd = line[0];
    if (line[1] != ':' && line[1] != 0)
        return false;
    const char *args = (line[1] == ':') ? line + 2 : line + 1;

    switch (cmd)
    {
    case 'M':
    {
        float fwdAmt, rightAmt, upAmt, yawDelta, pitchDelta;
        if (sscanf(args, "%f,%f,%f,%f,%f", &fwdAmt, &rightAmt, &upAmt, &yawDelta, &pitchDelta) == 5)
        {
            const float yr = g_player.yawDeg * kDegToRad;
            const Vector3 hFwd(sinf(yr), 0.0f, cosf(yr));
            const Vector3 hRight(-cosf(yr), 0.0f, sinf(yr));
            g_player.eyePos += hFwd * fwdAmt;
            g_player.eyePos += hRight * rightAmt;
            g_player.eyePos.y += upAmt;
            g_player.yawDeg += yawDelta;
            g_player.pitchDeg += pitchDelta;
            if (g_player.pitchDeg > 89.0f)
                g_player.pitchDeg = 89.0f;
            if (g_player.pitchDeg < -89.0f)
                g_player.pitchDeg = -89.0f;
        }
        return true;
    }
    case 'P':
    {
        float yaw, pitch, x, y, z;
        if (sscanf(args, "%f,%f,%f,%f,%f", &yaw, &pitch, &x, &y, &z) == 5)
        {
            g_player.yawDeg = yaw;
            g_player.pitchDeg = pitch;
            g_player.eyePos = Vector3(x, y, z);
        }
        return true;
    }
    case 'T':
    {
        float hour = strtof(args, nullptr);
        g_timeOfDay.setTime(hour, 0.0f);
        return true;
    }
    case 'S':
    {
        int v = atoi(args);
        g_sunOn = (v != 0);
        r.setSunEnabled(g_sunOn);
        return true;
    }
    case 'F':
    {
        float fov = strtof(args, nullptr);
        if (fov > 10.0f && fov < 130.0f)
            r.getCamera().setPerspective(fov, 0.1f, 100.0f);
        return true;
    }
    default:
        return false;
    }
}

static void pollSerial(Renderer &r)
{
    while (Serial.available())
    {
        int c = Serial.read();
        if (c < 0)
            break;
        if (c == '\r')
            continue;
        if (c == '\n')
        {
            g_serialLine[g_serialLen] = 0;
            parseCommand(g_serialLine, r);
            g_serialLen = 0;
        }
        else
        {
            if (g_serialLen < sizeof(g_serialLine) - 1)
                g_serialLine[g_serialLen++] = (char)c;
        }
    }
}

static void renderWorld(Renderer &r)
{
    r.setShadingMode(SHADING_FLAT);
    if (g_floorInstance)
        r.drawMeshInstanceStatic(g_floorInstance);
    if (g_backWallInstance)
        r.drawMeshInstanceStatic(g_backWallInstance);
    if (g_shrineInstance)
        r.draw(g_shrineInstance);
}

static void drawHud(Renderer &r)
{
    r.drawText(12, 12, "PIP3D 1X1 3D-DEFERRED DEMO", Color::WHITE);
    r.drawText(12, 22, "Cozy Dynamic Wall Lamp & Cool Floor Orbit Light", Color::rgb(0, 220, 255));

    char buf[80];
    snprintf(buf, sizeof(buf), "FPS: %.2f | Frame: %u ms",
             r.getFPS(), r.getFrameTime() / 1000);
    r.drawText(12, 35, buf, Color::WHITE);

    const Camera &cam = r.getCamera();
    snprintf(buf, sizeof(buf), "POS %5.1f %5.1f %5.1f | YAW %+6.1f PIT %+5.1f",
             cam.position.x, cam.position.y, cam.position.z,
             g_player.yawDeg, g_player.pitchDeg);
    r.drawText(12, 48, buf, Color::rgb(180, 220, 255));
}

void setup()
{
    Serial.begin(115200);

    Renderer &r = begin3D(SCREEN_WIDTH, SCREEN_HEIGHT,
                          TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN, TFT_BL_PIN,
                          80000000);

    if (!r.isInitialized())
    {
        for (;;)
            ;
    }

    initMeshes();
    buildScene();

    r.setBackfaceCullingEnabled(true);

    setupDemoLights(r);

    r.setDeferredLightingEnabled(true);

    r.setShadowsEnabled(true);
    r.setShadowPlaneY(0.0f);
    r.setShadowOpacity(0.65f);
    r.setShadowColorAuto();

    r.setSkyboxEnabled(true);
    r.setClearColor(Color::fromRGB888(10, 15, 30));

    r.setFogEnabled(false);

    r.generateClouds(0, 0.0f);
    r.setCloudsEnabled(false);

    TimeOfDayConfig todCfg;
    todCfg.dayLengthSeconds = 120.0f;
    todCfg.startHour = 19.8f;
    todCfg.autoAdvance = false;
    g_timeOfDay.init(&r, todCfg);

    r.setSunEnabled(true);
    g_sunOn = true;

    g_player.eyePos = Vector3(0.0f, g_player.eyeHeight, 4.0f);
    g_player.yawDeg = 180.0f;
    g_player.pitchDeg = -8.0f;

    Camera &cam = r.getCamera();
    cam.setPerspective(70.0f, 0.1f, 100.0f);
    cam.up = Vector3(0.0f, 1.0f, 0.0f);
    applyPlayerToCamera(r);
}

void loop()
{
    Renderer &r = renderer();
    if (!r.isInitialized())
        return;

    static uint32_t lastTick = 0;
    const uint32_t now = millis();
    const float dt = (lastTick == 0) ? 0.016f : fminf((now - lastTick) * 0.001f, 0.1f);
    lastTick = now;

    pollSerial(r);
    applyPlayerToCamera(r);

    g_timeOfDay.update(dt);

    static float animTime = 0.0f;
    animTime += dt;

    float amberX = sinf(animTime * 1.3f) * 4.8f;
    float amberY = 2.4f + cosf(animTime * 1.9f) * 0.4f;
    r.setPointLightPosition(0, Vector3(amberX, amberY, -9.7f));

    float tealRadius = 2.4f;
    float tealX = cosf(animTime * 1.1f) * tealRadius;
    float tealZ = -8.0f + sinf(animTime * 1.1f) * tealRadius;
    float tealY = 0.3f + sinf(animTime * 2.3f) * 0.15f;
    r.setPointLightPosition(1, Vector3(tealX, tealY, tealZ));

    for (int band = 0; band < SCREEN_BAND_COUNT; ++band)
    {
        r.beginFrameBand(band);
        r.drawSkyboxBackground();
        r.drawSky();

        renderWorld(r);
        r.flushQueue();

        r.applyDeferredLighting();

        g_billboards.render(r);

        drawHud(r);
        r.endFrameBand(band);
    }
}