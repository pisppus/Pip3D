#include <Arduino.h>
#include <math.h>
#include "Pip3D.h"

using namespace pip3D;

static const int8_t TFT_CS_PIN = 10;
static const int8_t TFT_DC_PIN = 9;
static const int8_t TFT_RST_PIN = 8;
static const int8_t TFT_BL_PIN = 4;

static Plane *g_ground = nullptr;
static Cube *g_stackCubes[3] = {nullptr, nullptr, nullptr};
static Cube *g_impactCube = nullptr;
static Plane *g_lift = nullptr;
static Cube *g_pendulumCube = nullptr;

static PhysicsWorld g_physics;
static RigidBody g_groundBody;
static RigidBody g_stackBodies[3];
static RigidBody g_impactBody;
static RigidBody g_liftBody;
static RigidBody g_pendulumAnchorBody;
static RigidBody g_pendulumBody;

static DistanceConstraint *g_distanceConstraint = nullptr;
static PointConstraint *g_pendulumJoint = nullptr;

static uint32_t g_lastMs = 0;
static float g_time = 0.0f;
static float g_resetTimer = 0.0f;

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

static void resetImpactCube()
{
  const float cubeSize = 1.5f;
  const float cubeHalf = cubeSize * 0.5f;
  const float baseHeight = cubeHalf + cubeSize * 3.0f;
  Vector3 pos(1.5f, baseHeight + 1.0f, 0.8f);
  g_impactBody.setPosition(pos);
  g_impactBody.velocity = Vector3(0.0f, 0.0f, 0.0f);
  g_impactBody.angularVelocity = Vector3(0.0f, 0.0f, 0.0f);
  g_impactBody.acceleration = Vector3(0.0f, 0.0f, 0.0f);
  g_impactBody.isSleeping = false;
  g_impactBody.sleepTimer = 0.0f;
  g_impactBody.orientation = Quaternion::fromEuler(45.0f * DEG2RAD, 45.0f * DEG2RAD, 45.0f * DEG2RAD);
  g_impactBody.updateBoundsFromTransform();
}

static void initCamera(Renderer &r)
{
  Camera &cam = r.getCamera();
  cam.position = Vector3(0.0f, 4.0f, -8.0f);
  cam.target = Vector3(0.0f, 0.5f, 0.0f);
  cam.up = Vector3(0.0f, 1.0f, 0.0f);
  cam.setPerspective(60.0f, 0.1f, 80.0f);
  cam.markDirty();
}

static void initScene(Renderer &r)
{
  g_ground = new Plane(20.0f, 20.0f, 1, Color::fromRGB888(100, 100, 100));
  g_ground->setPosition(0.0f, 0.0f, 0.0f);

  const float cubeSize = 1.5f;
  const float cubeHalf = cubeSize * 0.5f;

  for (int i = 0; i < 3; ++i)
  {
    g_stackCubes[i] = new Cube(cubeSize, Color::fromRGB888(180, 140 + i * 30, 80));
    float y = cubeHalf + cubeSize * i;
    g_stackCubes[i]->setPosition(0.0f, y, 0.0f);
  }

  g_impactCube = new Cube(cubeSize, Color::fromRGB888(80, 200, 200));
  g_impactCube->setPosition(1.5f, cubeHalf + cubeSize * 3.0f + 1.0f, 0.8f);
  g_impactCube->setRotation(45.0f, 45.0f, 45.0f);

  g_physics.setFixedTimeStep(1.0f / 60.0f);

  g_groundBody.setBox(Vector3(20.0f, 0.5f, 20.0f));
  g_groundBody.setPosition(Vector3(0.0f, -0.25f, 0.0f));
  g_groundBody.setStatic(true);
  g_groundBody.setMaterial(PhysicsMaterial(0.8f, 0.0f));
  g_physics.addBody(&g_groundBody);

  for (int i = 0; i < 3; ++i)
  {
    g_stackBodies[i].setBox(Vector3(cubeSize, cubeSize, cubeSize));
    float y = cubeHalf + cubeSize * i;
    g_stackBodies[i].setPosition(Vector3(0.0f, y, 0.0f));
    g_stackBodies[i].setMaterial(PhysicsMaterial(0.6f, 0.0f));
    g_stackBodies[i].setCanSleep(true);
    g_stackBodies[i].wakeUp();
    g_physics.addBody(&g_stackBodies[i]);
  }

  g_impactBody.setBox(Vector3(cubeSize, cubeSize, cubeSize));
  g_impactBody.setMaterial(PhysicsMaterial(0.6f, 0.0f));
  g_impactBody.setCanSleep(true);
  resetImpactCube();
  g_physics.addBody(&g_impactBody);

  // Кинематический лифт (платформа)
  const float liftSize = 2.0f;
  const float liftHalfHeight = 0.25f;
  Vector3 liftPos(-3.0f, liftHalfHeight + 0.5f, 0.0f);

  g_lift = new Plane(liftSize, liftSize, 3, Color::fromRGB888(120, 120, 255));
  g_lift->setPosition(liftPos.x, liftPos.y, liftPos.z);

  g_liftBody.setBox(Vector3(liftSize, liftHalfHeight * 2.0f, liftSize));
  g_liftBody.setPosition(liftPos);
  g_liftBody.setKinematic(true);
  g_liftBody.setMaterial(PhysicsMaterial(0.8f, 0.0f));
  g_physics.addBody(&g_liftBody);

  // Маятник на PointConstraint (шарнир)
  const float pendulumSize = 1.0f;
  const float pendulumHalf = pendulumSize * 0.5f;
  Vector3 anchorPos(-6.0f, 3.0f, 0.0f);
  Vector3 bobPos = anchorPos + Vector3(0.0f, -2.0f, 0.0f);

  g_pendulumCube = new Cube(pendulumSize, Color::fromRGB888(255, 220, 120));
  g_pendulumCube->setPosition(bobPos.x, bobPos.y, bobPos.z);

  g_pendulumAnchorBody.setBox(Vector3(0.2f, 0.2f, 0.2f));
  g_pendulumAnchorBody.setPosition(anchorPos);
  g_pendulumAnchorBody.setStatic(true);
  g_pendulumAnchorBody.setMaterial(PhysicsMaterial(0.8f, 0.0f));
  g_physics.addBody(&g_pendulumAnchorBody);

  g_pendulumBody.setBox(Vector3(pendulumSize, pendulumSize, pendulumSize));
  g_pendulumBody.setPosition(bobPos);
  g_pendulumBody.setMaterial(PhysicsMaterial(0.6f, 0.0f));
  g_pendulumBody.setCanSleep(true);
  g_physics.addBody(&g_pendulumBody);

  Vector3 localAnchorStatic(0.0f, 0.0f, 0.0f);
  Vector3 localAnchorPendulum(0.0f, pendulumHalf, 0.0f);
  g_pendulumJoint = new PointConstraint(&g_pendulumAnchorBody,
                                        &g_pendulumBody,
                                        localAnchorStatic,
                                        localAnchorPendulum);
  g_physics.addConstraint(g_pendulumJoint);

  // DistanceConstraint между верхним кубом стека и ударным кубом
  Vector3 topLocal(0.0f, cubeHalf, 0.0f);
  Vector3 bottomLocal(0.0f, -cubeHalf, 0.0f);
  Vector3 worldTop = g_stackBodies[2].position + topLocal;
  Vector3 worldBottom = g_impactBody.position + bottomLocal;
  Vector3 diff = worldBottom - worldTop;
  float restLenSq = diff.lengthSquared();
  float restLen = (restLenSq > 1e-8f) ? sqrtf(restLenSq) : 0.0f;
  g_distanceConstraint = new DistanceConstraint(&g_stackBodies[2],
                                                &g_impactBody,
                                                topLocal,
                                                bottomLocal,
                                                restLen);
  g_physics.addConstraint(g_distanceConstraint);

  r.setSkyboxWithLighting(SKYBOX_DAY);
  r.setShadowsEnabled(true);
  r.setShadowPlaneY(0.0f);
  r.setBackfaceCullingEnabled(true);
}

static void updateScene(Renderer &r, float dt)
{
  g_time += dt;
  g_resetTimer += dt;

  g_physics.updateFixed(dt);

  for (int i = 0; i < 3; ++i)
  {
    if (g_stackCubes[i])
    {
      const Vector3 &p = g_stackBodies[i].position;
      g_stackCubes[i]->setPosition(p.x, p.y, p.z);
      Vector3 euler = quatToEulerDegrees(g_stackBodies[i].orientation);
      g_stackCubes[i]->setRotation(euler.x, euler.y, euler.z);
    }
  }

  if (g_impactCube)
  {
    const Vector3 &p = g_impactBody.position;
    g_impactCube->setPosition(p.x, p.y, p.z);
    Vector3 euler = quatToEulerDegrees(g_impactBody.orientation);
    g_impactCube->setRotation(euler.x, euler.y, euler.z);
  }

  if (g_pendulumCube)
  {
    const Vector3 &p = g_pendulumBody.position;
    g_pendulumCube->setPosition(p.x, p.y, p.z);
    Vector3 euler = quatToEulerDegrees(g_pendulumBody.orientation);
    g_pendulumCube->setRotation(euler.x, euler.y, euler.z);
  }

  // Движение лифта (кинематическое тело) по синусоиде
  if (g_lift)
  {
    const float liftBaseY = 0.75f;
    const float liftAmplitude = 1.0f;
    const float liftSpeed = 0.8f;

    float y = liftBaseY + sinf(g_time * liftSpeed) * liftAmplitude;

    Vector3 pos = g_liftBody.position;
    pos.y = y;
    g_liftBody.setPosition(pos);

    g_lift->setPosition(pos.x, pos.y, pos.z);
  }

  if (g_resetTimer >= 5.0f)
  {
    resetImpactCube();
    g_resetTimer = 0.0f;
  }

  Vector3 lightDir(-0.6f, -1.0f, -0.4f);
  lightDir.normalize();
  r.setMainDirectionalLight(lightDir, Color::WHITE, 1.2f);
}

static void renderScene(Renderer &r)
{
  if (g_ground)
    r.drawMesh(g_ground);
  for (int i = 0; i < 3; ++i)
  {
    if (g_stackCubes[i])
    {
      r.drawMesh(g_stackCubes[i]);
      r.drawMeshShadow(g_stackCubes[i]);
    }
  }
  if (g_impactCube)
  {
    r.drawMesh(g_impactCube);
    r.drawMeshShadow(g_impactCube);
  }

  if (g_pendulumCube)
  {
    r.drawMesh(g_pendulumCube);
    r.drawMeshShadow(g_pendulumCube);
  }

  if (g_lift)
  {
    r.drawMesh(g_lift);
    r.drawMeshShadow(g_lift);
  }

#if ENABLE_DEBUG_DRAW
  g_physics.debugDraw(r);
#endif
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
