#include <Arduino.h>
#include <math.h>
#include "Pip3D.h"

using namespace pip3D;

static const int8_t TFT_CS_PIN = 10;
static const int8_t TFT_DC_PIN = 9;
static const int8_t TFT_RST_PIN = 8;
static const int8_t TFT_BL_PIN = 4;

// Simple benchmark scene: platform + one cube and one sphere
static Plane *g_platform = nullptr;
static Cube *g_benchCube = nullptr;
static Sphere *g_benchSphere = nullptr;

// Simple physics world: cube falling into water
static PhysicsWorld g_physics;
static RigidBody g_groundBody;
static RigidBody g_cubeBody;
static const float g_waterSurfaceY = 0.5f;
static const float g_waterSize = 20.0f;

static uint32_t g_lastMs = 0;
static float g_time = 0.0f;
static Vector3 quatToEulerDegrees(const Quaternion &q)
{
  float ysqr = q.y * q.y;

  float t0 = 2.0f * (q.w * q.x + q.y * q.z);
  float t1 = 1.0f - 2.0f * (q.x * q.x + ysqr);
  float roll = atan2f(t0, t1);

  float t2 = 2.0f * (q.w * q.y - q.z * q.x);
  if (t2 > 1.0f)
    t2 = 1.0f;
  if (t2 < -1.0f)
    t2 = -1.0f;
  float pitch = asinf(t2);

  float t3 = 2.0f * (q.w * q.z + q.x * q.y);
  float t4 = 1.0f - 2.0f * (ysqr + q.z * q.z);
  float yaw = atan2f(t3, t4);

  return Vector3(roll * RAD2DEG, pitch * RAD2DEG, yaw * RAD2DEG);
}

static void initCamera(Renderer &r)
{
  Camera &cam = r.getCamera();
  cam.position = Vector3(0.0f, 7.0f, -8.0f);
  cam.target = Vector3(0.0f, 1.0f, 0.0f);
  cam.up = Vector3(0.0f, 1.0f, 0.0f);
  cam.setPerspective(60.0f, 0.1f, 80.0f);
  cam.markDirty();
}

static void initScene(Renderer &r)
{
  // Ground platform
  if (!g_platform)
    g_platform = new Plane(20.0f, 20.0f, 1, Color::fromRGB888(100, 100, 100));
  g_platform->setPosition(0.0f, 0.0f, 0.0f);

  // Benchmark cube (slightly larger and higher above ground)
  const float cubeSize = 1.2f;
  if (!g_benchCube)
    g_benchCube = new Cube(cubeSize, Color::fromRGB888(200, 160, 80));
  g_benchCube->setPosition(0.0f, cubeSize * 2.0f, 0.0f);
  g_benchCube->setRotation(0.0f, 0.0f, 0.0f);

  // Benchmark sphere (larger and slightly lifted)
  const float sphereRadius = 1.0f;
  if (!g_benchSphere)
    g_benchSphere = new Sphere(sphereRadius, Color::fromRGB888(160, 200, 255));
  g_benchSphere->setPosition(2.0f, sphereRadius * 1.2f, 0.0f);
  g_benchSphere->setRotation(0.0f, 0.0f, 0.0f);

  r.setSkyboxWithLighting(SKYBOX_DAY);
  r.setShadowsEnabled(true);
  r.setShadowPlaneY(0.0f);
  r.setBackfaceCullingEnabled(true);

  // Physics setup: ground + dynamic cube + water buoyancy zone
  g_physics.setFixedTimeStep(1.0f / 60.0f);

  // Static ground body slightly below Y=0 so its top matches the visual platform
  g_groundBody.setBox(Vector3(20.0f, 0.5f, 20.0f));
  g_groundBody.setPosition(Vector3(0.0f, -0.25f, 0.0f));
  g_groundBody.setStatic(true);
  g_groundBody.setMaterial(PhysicsMaterial(0.8f, 0.0f));
  g_physics.addBody(&g_groundBody);

  // Dynamic cube body that will fall into the water
  g_cubeBody.setBox(Vector3(cubeSize, cubeSize, cubeSize));
  g_cubeBody.setPosition(Vector3(0.0f, cubeSize * 3.0f, 0.0f));
  g_cubeBody.setMaterial(PhysicsMaterial(0.6f, 0.0f));
  g_cubeBody.setCanSleep(true);
  g_cubeBody.wakeUp();
  g_physics.addBody(&g_cubeBody);

  // Water volume for buoyancy: large pool centered at origin
  {
    AABB waterBounds(Vector3(-g_waterSize * 0.5f, -5.0f, -g_waterSize * 0.5f),
                     Vector3( g_waterSize * 0.5f,  5.0f,  g_waterSize * 0.5f));
    const float waterDensity = 1.5f;
    const float waterDragL = 2.0f;
    const float waterDragA = 2.0f;
    g_physics.addBuoyancyZone(BuoyancyZone(waterBounds, g_waterSurfaceY, waterDensity, waterDragL, waterDragA));
  }
}

static void updateScene(Renderer &r, float dt)
{
  g_time += dt;

  // Step physics world (cube falling into water)
  g_physics.updateFixed(dt);

  // Sync visual cube with physics body
  if (g_benchCube)
  {
    const Vector3 &p = g_cubeBody.position;
    g_benchCube->setPosition(p.x, p.y, p.z);
    Vector3 euler = quatToEulerDegrees(g_cubeBody.orientation);
    g_benchCube->setRotation(euler.x, euler.y, euler.z);
  }

  // Sphere rotates and slowly orbits around its initial position
  if (g_benchSphere)
  {
    float rotX = fmodf(g_time * 60.0f, 360.0f);
    float rotY = fmodf(g_time * 30.0f, 360.0f);
    g_benchSphere->setRotation(rotX, rotY, 0.0f);
  }

  Vector3 lightDir(-0.6f, -1.0f, -0.4f);
  lightDir.normalize();
  r.setMainDirectionalLight(lightDir, Color::WHITE, 1.2f);
}

static void renderScene(Renderer &r)
{
  if (g_platform)
  {
    r.drawMesh(g_platform);
    r.drawMeshShadow(g_platform);
  }

  if (g_benchCube)
  {
    r.drawMesh(g_benchCube);
    r.drawMeshShadow(g_benchCube);
  }

  if (g_benchSphere)
  {
    r.drawMesh(g_benchSphere);
    r.drawMeshShadow(g_benchSphere);
  }

  // Water surface at y ~= 0.5 spanning the platform
  r.drawWater(0.5f, 20.0f, Color::fromRGB888(40, 100, 180), 0.45f, g_time);
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

  Renderer &r = begin3D(320, 240, TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN, TFT_BL_PIN, 80000000);
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

  r.beginFrame();
  updateScene(r, dt);
  renderScene(r);
  drawHud(r);
  r.endFrame();
}
