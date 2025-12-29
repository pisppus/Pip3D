#include <Arduino.h>
#include <math.h>

#define TFT_MOSI 11
#define TFT_MISO -1
#define TFT_SCLK 12

#include "Pip3D.h"

using namespace pip3D;

static const int8_t TFT_CS_PIN = 10;
static const int8_t TFT_DC_PIN = 9;
static const int8_t TFT_RST_PIN = -1;
static const int8_t TFT_BL_PIN = 4;

// Simple demo scene: ground plane + rotating cube and sphere
static Plane *g_ground = nullptr;
static Cube *g_cube = nullptr;
static Sphere *g_sphere = nullptr;

static uint32_t g_lastMs = 0;
static float g_time = 0.0f;

static void initCamera(Renderer &r)
{
  Camera &cam = r.getCamera();
  cam.position = Vector3(0.0f, 6.0f, -10.0f);
  cam.target = Vector3(0.0f, 1.5f, 0.0f);
  cam.up = Vector3(0.0f, 1.0f, 0.0f);
  cam.setPerspective(60.0f, 0.1f, 80.0f);
  cam.markDirty();
}

static void initScene(Renderer &r)
{
  // Ground plane
  if (!g_ground)
    g_ground = new Plane(20.0f, 20.0f, 1, Color::fromRGB888(100, 100, 100));
  g_ground->setPosition(0.0f, 0.0f, 0.0f);

  // Rotating cube at the center
  const float cubeSize = 1.5f;
  if (!g_cube)
    g_cube = new Cube(cubeSize, Color::fromRGB888(220, 180, 80));
  g_cube->setPosition(-1.5f, cubeSize * 1.2f, 0.0f);
  g_cube->setRotation(0.0f, 0.0f, 0.0f);

  // Rotating sphere orbiting around the cube
  const float sphereRadius = 1.0f;
  if (!g_sphere)
    g_sphere = new Sphere(sphereRadius, Color::fromRGB888(150, 200, 255));
  g_sphere->setPosition(2.0f, sphereRadius * 1.5f, 0.0f);
  g_sphere->setRotation(0.0f, 0.0f, 0.0f);

  r.setSkyboxWithLighting(SKYBOX_DAY);
  r.setShadowsEnabled(true);
  r.setShadowPlaneY(0.0f);
  r.setBackfaceCullingEnabled(true);
}

static void updateScene(Renderer &r, float dt)
{
  (void)r;
  g_time += dt;

  // Cube spins around its own axes
  if (g_cube)
  {
    float rotY = fmodf(g_time * 45.0f, 360.0f);
    float rotX = fmodf(g_time * 30.0f, 360.0f);
    g_cube->setRotation(rotX, rotY, 0.0f);
  }

  // Sphere orbits around the origin and spins a bit
  if (g_sphere)
  {
    float orbitRadius = 3.0f;
    float orbitSpeed = 0.7f;
    float angle = g_time * orbitSpeed;
    float x = cosf(angle) * orbitRadius;
    float z = sinf(angle) * orbitRadius;
    g_sphere->setPosition(x, 1.5f, z);

    float rotY = fmodf(g_time * 60.0f, 360.0f);
    g_sphere->setRotation(0.0f, rotY, 0.0f);
  }

  // Static main directional light
  Vector3 lightDir(-0.6f, -1.0f, -0.4f);
  lightDir.normalize();
  r.setMainDirectionalLight(lightDir, Color::WHITE, 1.2f);
}

static void renderWorld(Renderer &r)
{
  if (g_ground)
  {
    r.drawMesh(g_ground);
    r.drawMeshShadow(g_ground);
  }

  if (g_cube)
  {
    r.drawMesh(g_cube);
    r.drawMeshShadow(g_cube);
  }

  if (g_sphere)
  {
    r.drawMesh(g_sphere);
    r.drawMeshShadow(g_sphere);
  }
}

static void drawHud(Renderer &r)
{
  char buf[64];
  uint16_t y = 4;
  uint16_t line = 9;

  float fps = r.getFPS();
  float avgFps = r.getAverageFPS();

  snprintf(buf, sizeof(buf), "FPS: %.1f AVG: %.1f", fps, avgFps);
  r.drawText(4, y, buf, Color::fromRGB888(255, 255, 0));
  y += line;

  uint32_t frameTimeUs = r.getFrameTime();
  uint32_t frameTimeMs = frameTimeUs / 1000;
  snprintf(buf, sizeof(buf), "FT: %lums", (unsigned long)frameTimeMs);
  r.drawText(4, y, buf, Color::fromRGB888(255, 255, 0));
  y += line;

  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t maxHeap = ESP.getMaxAllocHeap();
  snprintf(buf, sizeof(buf), "RAM: %lu/%lu", (unsigned long)freeHeap, (unsigned long)maxHeap);
  r.drawText(4, y, buf, Color::fromRGB888(255, 255, 0));
  y += line;

  uint32_t triTotal = r.getStatsTrianglesTotal();
  uint32_t triBack = r.getStatsTrianglesBackfaceCulled();
  uint32_t instTotal = r.getStatsInstancesTotal();
  uint32_t instFrustum = r.getStatsInstancesFrustumCulled();
  uint32_t instOcc = r.getStatsInstancesOcclusionCulled();

  uint32_t triVisible = triTotal - triBack;
  uint32_t instVisible = instTotal - instFrustum - instOcc;

  snprintf(buf, sizeof(buf), "TRI: %lu V:%lu C:%lu", (unsigned long)triTotal, (unsigned long)triVisible, (unsigned long)triBack);
  r.drawText(4, y, buf, Color::fromRGB888(255, 255, 0));
  y += line;

  snprintf(buf, sizeof(buf), "INST: %lu V:%lu F:%lu O:%lu", (unsigned long)instTotal, (unsigned long)instVisible, (unsigned long)instFrustum, (unsigned long)instOcc);
  r.drawText(4, y, buf, Color::fromRGB888(255, 255, 0));
}

void setup()
{
  Serial.begin(115200);

  Renderer &r = begin3D(480, 320, TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN, TFT_BL_PIN, 60000000);
  (void)r;

  initCamera(renderer());
  initScene(renderer());

  g_lastMs = millis();
  g_time = 0.0f;
}

void loop()
{
  uint32_t now = millis();
  uint32_t dtMs = now - g_lastMs;
  g_lastMs = now;

  float dt = dtMs * 0.001f;
  if (dt > 0.1f)
    dt = 0.1f;

  Renderer &r = renderer();

  r.getCamera().markDirty();

  // Update simple demo scene once per frame.
  updateScene(r, dt);

  // Render in bands, reusing the depth buffer for each band.
  for (int band = 0; band < SCREEN_BAND_COUNT; ++band)
  {
    r.beginFrameBand(band);

    // 1) Opaque world geometry (fills ZBuffer for current band)
    renderWorld(r);
    // 2) Skybox only where Z is empty (behind geometry in this band)
    r.drawSkyboxBackground();
    // 3) HUD on top of everything (only needs to be drawn once, in the first band)
    if (band == 0)
    {
      drawHud(r);
    }

    r.endFrameBand(band);
  }
}
