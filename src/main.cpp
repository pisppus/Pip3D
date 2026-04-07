#include <Arduino.h>
#include <algorithm>
#include <array>
#include <math.h>
#include <stdio.h>
#if defined(PIP3D_PC)
#include <PipCore/Platforms/Desktop/Runtime.hpp>
#endif

#define TFT_MOSI 11
#define TFT_MISO -1
#define TFT_SCLK 12

#include "Pip3D.h"

using namespace pip3D;

static const int8_t TFT_CS_PIN = 10;
static const int8_t TFT_DC_PIN = 9;
static const int8_t TFT_RST_PIN = -1;
static const int8_t TFT_BL_PIN = -1;

static constexpr float DEMO_DURATION = 42.0f;
static constexpr float WATER_LEVEL = 0.0f;
static constexpr float CITY_LENGTH = 118.0f;
static constexpr float CAMERA_FAR = 240.0f;
static constexpr float SUN_ORBIT_RADIUS = 520.0f;
static constexpr float SUN_SCENE_CENTER_Y = 18.0f;

static constexpr size_t CITY_BUILDING_COUNT = 20;
static constexpr size_t WINDOW_BAND_COUNT = 2;
static constexpr size_t CLOUD_CLUSTER_COUNT = 6;
static constexpr size_t CLOUD_PUFF_COUNT = 3;
static constexpr size_t PROMENADE_LIGHT_COUNT = 16;
static constexpr size_t BRIDGE_LIGHT_COUNT = 8;
static constexpr size_t BRIDGE_PART_COUNT = 12;
static constexpr size_t LAND_PART_COUNT = 8;
static constexpr size_t ROAD_PART_COUNT = 12;

enum class PlanePartId : size_t
{
  BODY = 0,
  NOSE,
  COCKPIT,
  WING_L,
  WING_R,
  WINGLET_L,
  WINGLET_R,
  ENGINE_L,
  ENGINE_R,
  ENGINE_INTAKE_L,
  ENGINE_INTAKE_R,
  TAIL_FIN,
  TAIL_L,
  TAIL_R,
  NAV_L,
  NAV_R,
  NAV_TAIL,
  BEACON_TOP,
  BEACON_BOTTOM,
  PART_COUNT
};

struct RigPart
{
  Node *node = nullptr;
  MeshInstance *instance = nullptr;
};

struct BuildingTower
{
  MeshInstance *base = nullptr;
  MeshInstance *mid = nullptr;
  MeshInstance *crown = nullptr;
  std::array<MeshInstance *, WINDOW_BAND_COUNT> bands{};
  Vector3 anchor;
  float width = 0.0f;
  float depth = 0.0f;
  float height = 0.0f;
  Color baseDay = Color::WHITE;
  Color baseNight = Color::WHITE;
  Color accentOff = Color::WHITE;
  Color accentOn = Color::WHITE;
  bool leftSide = false;
};

struct LampPost
{
  MeshInstance *pole = nullptr;
  MeshInstance *glow = nullptr;
  Vector3 anchor;
  Color glowOff = Color::WHITE;
  Color glowOn = Color::WHITE;
};

struct CloudCluster
{
  std::array<MeshInstance *, CLOUD_PUFF_COUNT> puffs{};
  Vector3 origin;
  float scale = 1.0f;
  float drift = 0.0f;
};

struct PlaneState
{
  Vector3 position;
  Vector3 forward;
  Vector3 right;
  Vector3 up;
  float speed = 0.0f;
  float bank = 0.0f;
};

struct SunState
{
  Vector3 direction;
  Vector3 worldPos;
  Color color = Color::WHITE;
  float glow = 0.0f;
  float size = 1.0f;
  float visibility = 0.0f;
};

static InstanceManager g_instances;
static FXSystem g_fx;

static Cube *g_cubeMesh = nullptr;
static Sphere *g_sphereMesh = nullptr;
static Cylinder *g_cylinderMesh = nullptr;
static Cone *g_coneMesh = nullptr;
static Capsule *g_capsuleMesh = nullptr;

static std::array<BuildingTower, CITY_BUILDING_COUNT> g_buildings{};
static std::array<LampPost, PROMENADE_LIGHT_COUNT> g_lamps{};
static std::array<CloudCluster, CLOUD_CLUSTER_COUNT> g_clouds{};
static std::array<MeshInstance *, BRIDGE_LIGHT_COUNT> g_bridgeLights{};
static std::array<MeshInstance *, BRIDGE_PART_COUNT> g_bridgeParts{};
static std::array<MeshInstance *, LAND_PART_COUNT> g_landParts{};
static std::array<MeshInstance *, ROAD_PART_COUNT> g_roadParts{};
static std::array<RigPart, static_cast<size_t>(PlanePartId::PART_COUNT)> g_planeParts{};

static Node *g_planeRoot = nullptr;
static Node *g_planeBody = nullptr;
static Node *g_planeWingL = nullptr;
static Node *g_planeWingR = nullptr;
static Node *g_planeTail = nullptr;
static Node *g_planeEngineL = nullptr;
static Node *g_planeEngineR = nullptr;
static Node *g_planeExhaustL = nullptr;
static Node *g_planeExhaustR = nullptr;

static ParticleEmitter *g_trailLeft = nullptr;
static ParticleEmitter *g_trailRight = nullptr;

static PlaneState g_planeState{};
static SunState g_sunState{};

static uint32_t g_lastMs = 0;
static float g_demoTime = 0.0f;

#if defined(PIP3D_PC)
struct FreeCameraState
{
  Vector3 position;
  float yaw = 0.0f;
  float pitch = 0.0f;
  bool enabled = true;
};

static FreeCameraState g_freeCamera{};
static bool g_tabWasDown = false;
#endif

static constexpr size_t toIndex(PlanePartId id)
{
  return static_cast<size_t>(id);
}

static float saturate(float v)
{
  return pip3D::clamp(v, 0.0f, 1.0f);
}

static float ease(float t)
{
  t = saturate(t);
  return t * t * (3.0f - 2.0f * t);
}

static float smoother(float t)
{
  t = saturate(t);
  return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

static Vector3 lerpVec(const Vector3 &a, const Vector3 &b, float t)
{
  t = saturate(t);
  return a + (b - a) * t;
}

static float sectionT(float t, float a, float b)
{
  if (b <= a)
    return 0.0f;
  return saturate((t - a) / (b - a));
}

static float hash01(int seed)
{
  float x = sinf(seed * 12.9898f + 78.233f) * 43758.5453f;
  return x - floorf(x);
}

static Color mixColor565(const Color &a, const Color &b, float t)
{
  t = saturate(t);
  const uint16_t av = a.rgb565;
  const uint16_t bv = b.rgb565;
  const int ar = (av >> 11) & 31;
  const int ag = (av >> 5) & 63;
  const int ab = av & 31;
  const int br = (bv >> 11) & 31;
  const int bg = (bv >> 5) & 63;
  const int bb = bv & 31;
  const int rr = static_cast<int>(ar + (br - ar) * t);
  const int rg = static_cast<int>(ag + (bg - ag) * t);
  const int rb = static_cast<int>(ab + (bb - ab) * t);
  return Color(static_cast<uint16_t>((rr << 11) | (rg << 5) | rb));
}

static const char *getSectionName(float t)
{
  if (t < 10.0f)
    return "CITY DESCENT";
  if (t < 22.0f)
    return "AVENUE RUN";
  if (t < 32.0f)
    return "TOWER SLALOM";
  return "NIGHT CLIMB";
}

static void safeNormalize(Vector3 &v, const Vector3 &fallback)
{
  float lenSq = v.lengthSquared();
  if (lenSq <= 1e-6f)
  {
    v = fallback;
    return;
  }
  v *= (1.0f / sqrtf(lenSq));
}

static Quaternion quaternionFromBasis(const Matrix4x4 &m)
{
  const float trace = m.m[0] + m.m[5] + m.m[10];
  if (trace > 0.0f)
  {
    float s = sqrtf(trace + 1.0f) * 2.0f;
    Quaternion q((m.m[9] - m.m[6]) / s, (m.m[2] - m.m[8]) / s, (m.m[4] - m.m[1]) / s, 0.25f * s);
    q.normalize();
    return q;
  }
  if (m.m[0] > m.m[5] && m.m[0] > m.m[10])
  {
    float s = sqrtf(1.0f + m.m[0] - m.m[5] - m.m[10]) * 2.0f;
    Quaternion q(0.25f * s, (m.m[1] + m.m[4]) / s, (m.m[2] + m.m[8]) / s, (m.m[9] - m.m[6]) / s);
    q.normalize();
    return q;
  }
  if (m.m[5] > m.m[10])
  {
    float s = sqrtf(1.0f + m.m[5] - m.m[0] - m.m[10]) * 2.0f;
    Quaternion q((m.m[1] + m.m[4]) / s, 0.25f * s, (m.m[6] + m.m[9]) / s, (m.m[2] - m.m[8]) / s);
    q.normalize();
    return q;
  }
  float s = sqrtf(1.0f + m.m[10] - m.m[0] - m.m[5]) * 2.0f;
  Quaternion q((m.m[2] + m.m[8]) / s, (m.m[6] + m.m[9]) / s, 0.25f * s, (m.m[4] - m.m[1]) / s);
  q.normalize();
  return q;
}

static void updateCamera(Renderer &r, float t);

#if defined(PIP3D_PC)
static void syncFreeCameraFromView(const Camera &cam)
{
  g_freeCamera.position = cam.position;
  Vector3 forward = cam.target - cam.position;
  safeNormalize(forward, Vector3(0.0f, 0.0f, 1.0f));
  g_freeCamera.yaw = atan2f(forward.x, forward.z) * RAD_TO_DEG;
  const float horizontal = sqrtf(forward.x * forward.x + forward.z * forward.z);
  g_freeCamera.pitch = atan2f(forward.y, horizontal) * RAD_TO_DEG;
}

static Vector3 freeCameraForward()
{
  const float yaw = g_freeCamera.yaw * DEG2RAD;
  const float pitch = g_freeCamera.pitch * DEG2RAD;
  const float cp = cosf(pitch);
  return Vector3(sinf(yaw) * cp, sinf(pitch), cosf(yaw) * cp);
}

static void updateFreeCamera(Renderer &r, float dt)
{
  auto &runtime = pipcore::desktop::Runtime::instance();
  const bool tabDown = runtime.keyDown(VK_TAB);
  if (tabDown && !g_tabWasDown)
  {
    g_freeCamera.enabled = !g_freeCamera.enabled;
    if (g_freeCamera.enabled)
    {
      syncFreeCameraFromView(r.getCamera());
      runtime.setMouseCapture(true);
    }
    else
    {
      runtime.setMouseCapture(false);
    }
  }
  g_tabWasDown = tabDown;

  if (!g_freeCamera.enabled)
  {
    updateCamera(r, g_demoTime);
    return;
  }

  if (!runtime.isMouseCaptured() && runtime.mouseButtonDown(0))
  {
    runtime.setMouseCapture(true);
  }

  int32_t mouseDx = 0;
  int32_t mouseDy = 0;
  runtime.consumeMouseDelta(mouseDx, mouseDy);
  g_freeCamera.yaw += static_cast<float>(mouseDx) * 0.11f;
  g_freeCamera.pitch -= static_cast<float>(mouseDy) * 0.09f;
  g_freeCamera.pitch = pip3D::clamp(g_freeCamera.pitch, -85.0f, 85.0f);

  Vector3 forward = freeCameraForward();
  Vector3 planarForward(forward.x, 0.0f, forward.z);
  safeNormalize(planarForward, Vector3(0.0f, 0.0f, 1.0f));
  Vector3 planarRight(planarForward.z, 0.0f, -planarForward.x);

  Vector3 move(0.0f, 0.0f, 0.0f);
  if (runtime.keyDown('W'))
    move += planarForward;
  if (runtime.keyDown('S'))
    move -= planarForward;
  if (runtime.keyDown('D'))
    move += planarRight;
  if (runtime.keyDown('A'))
    move -= planarRight;
  if (runtime.keyDown('E'))
    move.y += 1.0f;
  if (runtime.keyDown('Q'))
    move.y -= 1.0f;

  const float lenSq = move.lengthSquared();
  if (lenSq > 1e-6f)
  {
    move *= (1.0f / sqrtf(lenSq));
    const float baseSpeed = runtime.keyDown(VK_SHIFT) ? 21.0f : 10.5f;
    g_freeCamera.position += move * (baseSpeed * dt);
  }

  if (g_freeCamera.position.y < 1.65f)
    g_freeCamera.position.y = 1.65f;

  Camera &cam = r.getCamera();
  cam.position = g_freeCamera.position;
  cam.target = g_freeCamera.position + forward * 10.0f;
  cam.up = Vector3(0.0f, 1.0f, 0.0f);
  cam.setPerspective(74.0f, 0.1f, CAMERA_FAR + 80.0f);
  cam.markDirty();
}
#endif

static void syncNodeToInstance(Node *node, MeshInstance *instance)
{
  if (!node || !instance)
    return;
  const Matrix4x4 &world = node->getWorldTransform();
  Vector3 axisX(world.m[0], world.m[1], world.m[2]);
  Vector3 axisY(world.m[4], world.m[5], world.m[6]);
  Vector3 axisZ(world.m[8], world.m[9], world.m[10]);
  const float scaleX = axisX.length();
  const float scaleY = axisY.length();
  const float scaleZ = axisZ.length();
  if (scaleX > 1e-6f)
    axisX *= (1.0f / scaleX);
  if (scaleY > 1e-6f)
    axisY *= (1.0f / scaleY);
  if (scaleZ > 1e-6f)
    axisZ *= (1.0f / scaleZ);
  Matrix4x4 basis;
  basis.identity();
  basis.m[0] = axisX.x;
  basis.m[1] = axisX.y;
  basis.m[2] = axisX.z;
  basis.m[4] = axisY.x;
  basis.m[5] = axisY.y;
  basis.m[6] = axisY.z;
  basis.m[8] = axisZ.x;
  basis.m[9] = axisZ.y;
  basis.m[10] = axisZ.z;
  instance->setPosition(world.m[12], world.m[13], world.m[14]);
  instance->setRotation(quaternionFromBasis(basis));
  instance->setScale(scaleX, scaleY, scaleZ);
}

static void initMeshes()
{
  if (!g_cubeMesh)
    g_cubeMesh = new Cube(1.0f, Color::WHITE);
  if (!g_sphereMesh)
    g_sphereMesh = new Sphere(0.65f, 16, 12, Color::WHITE);
  if (!g_cylinderMesh)
    g_cylinderMesh = new Cylinder(0.55f, 1.4f, 16, Color::WHITE);
  if (!g_coneMesh)
    g_coneMesh = new Cone(0.6f, 1.3f, 16, Color::WHITE);
  if (!g_capsuleMesh)
    g_capsuleMesh = new Capsule(0.55f, 2.6f, 16, 7, Color::WHITE);
}

static void setupPlanePart(PlanePartId id, Node *parent, Mesh *mesh, const Color &color, const Vector3 &position, const Vector3 &scale, const Vector3 &rotation = Vector3(0.0f, 0.0f, 0.0f))
{
  Node *node = new Node();
  parent->addChild(node);
  node->setPosition(position);
  node->setRotation(rotation);
  node->setScale(scale);
  MeshInstance *instance = g_instances.create(mesh);
  instance->setColor(color);
  g_planeParts[toIndex(id)].node = node;
  g_planeParts[toIndex(id)].instance = instance;
}

static void initPlaneRig()
{
  if (g_planeRoot)
    return;
  g_planeRoot = new Node("PlaneRoot");
  g_planeBody = new Node("PlaneBody");
  g_planeWingL = new Node("WingL");
  g_planeWingR = new Node("WingR");
  g_planeTail = new Node("Tail");
  g_planeEngineL = new Node("EngineL");
  g_planeEngineR = new Node("EngineR");
  g_planeExhaustL = new Node("ExhaustL");
  g_planeExhaustR = new Node("ExhaustR");
  g_planeRoot->addChild(g_planeBody);
  g_planeBody->addChild(g_planeWingL);
  g_planeBody->addChild(g_planeWingR);
  g_planeBody->addChild(g_planeTail);
  g_planeBody->addChild(g_planeEngineL);
  g_planeBody->addChild(g_planeEngineR);
  g_planeEngineL->addChild(g_planeExhaustL);
  g_planeEngineR->addChild(g_planeExhaustR);

  setupPlanePart(PlanePartId::BODY, g_planeBody, g_cubeMesh, Color::fromRGB888(214, 220, 230), Vector3(0.0f, 0.0f, -0.20f), Vector3(0.92f, 0.62f, 6.40f));
  setupPlanePart(PlanePartId::NOSE, g_planeBody, g_coneMesh, Color::fromRGB888(238, 242, 246), Vector3(0.0f, 0.0f, 4.42f), Vector3(0.38f, 1.55f, 0.38f), Vector3(90.0f, 0.0f, 0.0f));
  setupPlanePart(PlanePartId::COCKPIT, g_planeBody, g_cubeMesh, Color::fromRGB888(96, 154, 214), Vector3(0.0f, 0.34f, 2.46f), Vector3(0.42f, 0.22f, 1.32f), Vector3(18.0f, 0.0f, 0.0f));
  setupPlanePart(PlanePartId::WING_L, g_planeWingL, g_cubeMesh, Color::fromRGB888(188, 196, 210), Vector3(-2.75f, -0.02f, 0.10f), Vector3(4.85f, 0.08f, 1.12f), Vector3(0.0f, -12.0f, 16.0f));
  setupPlanePart(PlanePartId::WING_R, g_planeWingR, g_cubeMesh, Color::fromRGB888(188, 196, 210), Vector3(2.75f, -0.02f, 0.10f), Vector3(4.85f, 0.08f, 1.12f), Vector3(0.0f, 12.0f, -16.0f));
  setupPlanePart(PlanePartId::WINGLET_L, g_planeWingL, g_cubeMesh, Color::fromRGB888(226, 230, 238), Vector3(-4.70f, 0.42f, -0.08f), Vector3(0.16f, 0.86f, 0.26f), Vector3(-8.0f, 0.0f, -18.0f));
  setupPlanePart(PlanePartId::WINGLET_R, g_planeWingR, g_cubeMesh, Color::fromRGB888(226, 230, 238), Vector3(4.70f, 0.42f, -0.08f), Vector3(0.16f, 0.86f, 0.26f), Vector3(-8.0f, 0.0f, 18.0f));

  g_planeEngineL->setPosition(-0.52f, -0.12f, -2.65f);
  g_planeEngineR->setPosition(0.52f, -0.12f, -2.65f);
  g_planeExhaustL->setPosition(-0.08f, 0.0f, -1.12f);
  g_planeExhaustR->setPosition(0.08f, 0.0f, -1.12f);

  setupPlanePart(PlanePartId::ENGINE_L, g_planeEngineL, g_cubeMesh, Color::fromRGB888(46, 52, 62), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.34f, 0.34f, 1.66f));
  setupPlanePart(PlanePartId::ENGINE_R, g_planeEngineR, g_cubeMesh, Color::fromRGB888(46, 52, 62), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.34f, 0.34f, 1.66f));
  setupPlanePart(PlanePartId::ENGINE_INTAKE_L, g_planeEngineL, g_cubeMesh, Color::fromRGB888(92, 106, 126), Vector3(0.0f, 0.0f, 0.72f), Vector3(0.22f, 0.16f, 0.10f));
  setupPlanePart(PlanePartId::ENGINE_INTAKE_R, g_planeEngineR, g_cubeMesh, Color::fromRGB888(92, 106, 126), Vector3(0.0f, 0.0f, 0.72f), Vector3(0.22f, 0.16f, 0.10f));
  setupPlanePart(PlanePartId::TAIL_FIN, g_planeTail, g_cubeMesh, Color::fromRGB888(210, 216, 226), Vector3(0.0f, 1.00f, -3.85f), Vector3(0.16f, 1.18f, 0.70f), Vector3(-6.0f, 0.0f, 0.0f));
  setupPlanePart(PlanePartId::TAIL_L, g_planeTail, g_cubeMesh, Color::fromRGB888(186, 194, 206), Vector3(-1.30f, 0.18f, -3.22f), Vector3(1.56f, 0.07f, 0.54f), Vector3(0.0f, -18.0f, 14.0f));
  setupPlanePart(PlanePartId::TAIL_R, g_planeTail, g_cubeMesh, Color::fromRGB888(186, 194, 206), Vector3(1.30f, 0.18f, -3.22f), Vector3(1.56f, 0.07f, 0.54f), Vector3(0.0f, 18.0f, -14.0f));
  setupPlanePart(PlanePartId::NAV_L, g_planeWingL, g_cubeMesh, Color::fromRGB888(255, 72, 72), Vector3(-4.86f, 0.12f, 0.32f), Vector3(0.10f, 0.10f, 0.10f));
  setupPlanePart(PlanePartId::NAV_R, g_planeWingR, g_cubeMesh, Color::fromRGB888(82, 255, 172), Vector3(4.86f, 0.12f, 0.32f), Vector3(0.10f, 0.10f, 0.10f));
  setupPlanePart(PlanePartId::NAV_TAIL, g_planeTail, g_cubeMesh, Color::fromRGB888(248, 248, 255), Vector3(0.0f, 0.64f, -4.44f), Vector3(0.08f, 0.08f, 0.08f));
  setupPlanePart(PlanePartId::BEACON_TOP, g_planeBody, g_cubeMesh, Color::fromRGB888(255, 92, 92), Vector3(0.0f, 0.50f, -0.55f), Vector3(0.06f, 0.06f, 0.06f));
  setupPlanePart(PlanePartId::BEACON_BOTTOM, g_planeBody, g_cubeMesh, Color::fromRGB888(255, 110, 110), Vector3(0.0f, -0.40f, -0.40f), Vector3(0.06f, 0.06f, 0.06f));

  ParticleEmitterConfig trailCfg;
  trailCfg.maxParticles = 56;
  trailCfg.emitRate = 22.0f;
  trailCfg.minLifetime = 0.45f;
  trailCfg.maxLifetime = 0.85f;
  trailCfg.initialSpeed = 0.12f;
  trailCfg.spread = 0.18f;
  trailCfg.acceleration = Vector3(0.0f, 0.05f, 0.0f);
  trailCfg.startColor = Color::fromRGB888(208, 220, 242);
  trailCfg.endColor = Color::fromRGB888(92, 118, 178);
  trailCfg.startSize = 1.8f;
  trailCfg.endSize = 0.6f;
  trailCfg.looping = true;
  trailCfg.additive = true;
  g_trailLeft = g_fx.createEmitter(trailCfg);
  g_trailRight = g_fx.createEmitter(trailCfg);
}

static void initDistrictGeometry()
{
  const Color slabColor = Color::fromRGB888(74, 80, 92);
  const Color sidewalkColor = Color::fromRGB888(118, 122, 132);
  const Color roadColor = Color::fromRGB888(34, 36, 42);
  const Color medianColor = Color::fromRGB888(88, 94, 102);

  g_landParts[0] = g_instances.create(g_cubeMesh);
  g_landParts[0]->setPosition(0.0f, -0.28f, 0.0f);
  g_landParts[0]->setScale(38.0f, 0.20f, CITY_LENGTH + 8.0f);
  g_landParts[0]->setColor(slabColor);

  g_landParts[1] = g_instances.create(g_cubeMesh);
  g_landParts[1]->setPosition(-8.4f, 0.040f, 0.0f);
  g_landParts[1]->setScale(4.2f, 0.028f, CITY_LENGTH);
  g_landParts[1]->setColor(sidewalkColor);

  g_landParts[2] = g_instances.create(g_cubeMesh);
  g_landParts[2]->setPosition(8.4f, 0.040f, 0.0f);
  g_landParts[2]->setScale(4.2f, 0.028f, CITY_LENGTH);
  g_landParts[2]->setColor(sidewalkColor);

  g_landParts[3] = g_instances.create(g_cubeMesh);
  g_landParts[3]->setPosition(0.0f, 0.046f, 0.0f);
  g_landParts[3]->setScale(1.2f, 0.022f, CITY_LENGTH - 16.0f);
  g_landParts[3]->setColor(medianColor);

  g_landParts[4] = g_instances.create(g_cubeMesh);
  g_landParts[4]->setPosition(0.0f, 0.034f, -43.0f);
  g_landParts[4]->setScale(10.0f, 0.028f, 6.2f);
  g_landParts[4]->setColor(sidewalkColor);

  g_landParts[5] = g_instances.create(g_cubeMesh);
  g_landParts[5]->setPosition(0.0f, 0.034f, 39.0f);
  g_landParts[5]->setScale(10.0f, 0.028f, 7.0f);
  g_landParts[5]->setColor(sidewalkColor);

  g_landParts[6] = g_instances.create(g_cubeMesh);
  g_landParts[6]->setPosition(-14.4f, 0.052f, 0.0f);
  g_landParts[6]->setScale(5.0f, 0.024f, CITY_LENGTH - 12.0f);
  g_landParts[6]->setColor(Color::fromRGB888(92, 96, 106));

  g_landParts[7] = g_instances.create(g_cubeMesh);
  g_landParts[7]->setPosition(14.4f, 0.052f, 0.0f);
  g_landParts[7]->setScale(5.0f, 0.024f, CITY_LENGTH - 12.0f);
  g_landParts[7]->setColor(Color::fromRGB888(92, 96, 106));

  g_roadParts[0] = g_instances.create(g_cubeMesh);
  g_roadParts[0]->setPosition(-2.4f, 0.026f, 0.0f);
  g_roadParts[0]->setScale(3.8f, 0.012f, CITY_LENGTH);
  g_roadParts[0]->setColor(roadColor);

  g_roadParts[1] = g_instances.create(g_cubeMesh);
  g_roadParts[1]->setPosition(2.4f, 0.026f, 0.0f);
  g_roadParts[1]->setScale(3.8f, 0.012f, CITY_LENGTH);
  g_roadParts[1]->setColor(roadColor);

  for (size_t i = 0; i < 10; ++i)
  {
    g_roadParts[i + 2] = g_instances.create(g_cubeMesh);
    float z = -49.0f + static_cast<float>(i) * 10.8f;
    g_roadParts[i + 2]->setPosition(0.0f, 0.037f, z);
    g_roadParts[i + 2]->setScale(0.18f, 0.005f, 2.7f);
    g_roadParts[i + 2]->setColor(Color::fromRGB888(228, 210, 150));
  }
}

static void initBridge()
{
  g_bridgeParts[0] = g_instances.create(g_cubeMesh);
  g_bridgeParts[0]->setPosition(0.0f, 0.32f, 6.0f);
  g_bridgeParts[0]->setScale(18.0f, 0.16f, 3.0f);
  g_bridgeParts[0]->setColor(Color::fromRGB888(138, 146, 160));
  g_bridgeParts[1] = g_instances.create(g_cubeMesh);
  g_bridgeParts[1]->setPosition(-4.4f, 3.2f, 6.0f);
  g_bridgeParts[1]->setScale(0.34f, 6.4f, 0.44f);
  g_bridgeParts[1]->setColor(Color::fromRGB888(156, 166, 180));
  g_bridgeParts[2] = g_instances.create(g_cubeMesh);
  g_bridgeParts[2]->setPosition(4.4f, 3.2f, 6.0f);
  g_bridgeParts[2]->setScale(0.34f, 6.4f, 0.44f);
  g_bridgeParts[2]->setColor(Color::fromRGB888(156, 166, 180));
  g_bridgeParts[3] = g_instances.create(g_cubeMesh);
  g_bridgeParts[3]->setPosition(-6.6f, 1.62f, 6.0f);
  g_bridgeParts[3]->setScale(4.2f, 0.08f, 0.14f);
  g_bridgeParts[3]->setEuler(0.0f, 0.0f, 26.0f);
  g_bridgeParts[3]->setColor(Color::fromRGB888(188, 198, 214));
  g_bridgeParts[4] = g_instances.create(g_cubeMesh);
  g_bridgeParts[4]->setPosition(-2.2f, 1.62f, 6.0f);
  g_bridgeParts[4]->setScale(4.2f, 0.08f, 0.14f);
  g_bridgeParts[4]->setEuler(0.0f, 0.0f, -26.0f);
  g_bridgeParts[4]->setColor(Color::fromRGB888(188, 198, 214));
  g_bridgeParts[5] = g_instances.create(g_cubeMesh);
  g_bridgeParts[5]->setPosition(2.2f, 1.62f, 6.0f);
  g_bridgeParts[5]->setScale(4.2f, 0.08f, 0.14f);
  g_bridgeParts[5]->setEuler(0.0f, 0.0f, 26.0f);
  g_bridgeParts[5]->setColor(Color::fromRGB888(188, 198, 214));
  g_bridgeParts[6] = g_instances.create(g_cubeMesh);
  g_bridgeParts[6]->setPosition(6.6f, 1.62f, 6.0f);
  g_bridgeParts[6]->setScale(4.2f, 0.08f, 0.14f);
  g_bridgeParts[6]->setEuler(0.0f, 0.0f, -26.0f);
  g_bridgeParts[6]->setColor(Color::fromRGB888(188, 198, 214));
  g_bridgeParts[7] = g_instances.create(g_cubeMesh);
  g_bridgeParts[7]->setPosition(-8.6f, 0.68f, 6.0f);
  g_bridgeParts[7]->setScale(0.16f, 1.36f, 0.16f);
  g_bridgeParts[7]->setColor(Color::fromRGB888(88, 96, 112));
  g_bridgeParts[8] = g_instances.create(g_cubeMesh);
  g_bridgeParts[8]->setPosition(8.6f, 0.68f, 6.0f);
  g_bridgeParts[8]->setScale(0.16f, 1.36f, 0.16f);
  g_bridgeParts[8]->setColor(Color::fromRGB888(88, 96, 112));
  g_bridgeParts[9] = g_instances.create(g_sphereMesh);
  g_bridgeParts[9]->setPosition(-4.4f, 6.55f, 6.0f);
  g_bridgeParts[9]->setScale(0.24f);
  g_bridgeParts[9]->setColor(Color::fromRGB888(255, 110, 110));
  g_bridgeParts[10] = g_instances.create(g_sphereMesh);
  g_bridgeParts[10]->setPosition(4.4f, 6.55f, 6.0f);
  g_bridgeParts[10]->setScale(0.24f);
  g_bridgeParts[10]->setColor(Color::fromRGB888(255, 110, 110));
  g_bridgeParts[11] = g_instances.create(g_cubeMesh);
  g_bridgeParts[11]->setPosition(0.0f, 1.16f, 6.0f);
  g_bridgeParts[11]->setScale(15.2f, 0.10f, 0.16f);
  g_bridgeParts[11]->setColor(Color::fromRGB888(174, 182, 194));
  for (size_t i = 0; i < BRIDGE_LIGHT_COUNT; ++i)
  {
    g_bridgeLights[i] = g_instances.create(g_sphereMesh);
    float x = -7.0f + static_cast<float>(i) * 2.0f;
    g_bridgeLights[i]->setPosition(x, 0.98f, 7.25f);
    g_bridgeLights[i]->setScale(0.12f);
    g_bridgeLights[i]->setColor(Color::fromRGB888(255, 216, 140));
  }
}

static void initBuildings()
{
  for (size_t i = 0; i < CITY_BUILDING_COUNT; ++i)
  {
    const bool left = i < CITY_BUILDING_COUNT / 2;
    const int slot = left ? static_cast<int>(i) : static_cast<int>(i - CITY_BUILDING_COUNT / 2);
    const int seed = static_cast<int>(i) * 13 + 7;
    float z = -48.0f + slot * 9.4f + (hash01(seed + 2) - 0.5f) * 2.2f;
    float focus = 1.0f - saturate(fabsf(z) / 50.0f);
    float xBase = 7.0f + hash01(seed + 5) * 2.6f + focus * 1.1f;
    float x = left ? -xBase : xBase;
    float width = 2.4f + hash01(seed + 11) * 1.8f + focus * 0.9f;
    float depth = 2.6f + hash01(seed + 19) * 1.4f;
    float height = 11.0f + hash01(seed + 23) * 12.0f + focus * 8.0f;
    bool hero = (focus > 0.72f) || slot == 0 || slot == 9;
    if (hero)
      height += 4.0f + hash01(seed + 31) * 5.5f;

    Color concrete = mixColor565(Color::fromRGB888(72, 84, 102), Color::fromRGB888(122, 126, 140), hash01(seed + 43));
    Color glassNight = mixColor565(Color::fromRGB888(54, 84, 126), Color::fromRGB888(84, 116, 160), hash01(seed + 47));
    Color accentWarm = mixColor565(Color::fromRGB888(145, 82, 38), Color::fromRGB888(255, 210, 128), hash01(seed + 53));
    Color accentCool = mixColor565(Color::fromRGB888(72, 132, 190), Color::fromRGB888(165, 220, 255), hash01(seed + 59));

    BuildingTower &tower = g_buildings[i];
    tower.anchor = Vector3(x, 0.0f, z);
    tower.width = width;
    tower.depth = depth;
    tower.height = height;
    tower.leftSide = left;
    tower.baseDay = concrete;
    tower.baseNight = glassNight;
    tower.accentOff = (i % 3 == 0) ? accentCool : accentWarm;
    tower.accentOn = (i % 3 == 0) ? Color::fromRGB888(180, 245, 255) : Color::fromRGB888(255, 226, 165);

    tower.base = g_instances.create(g_cubeMesh);
    tower.base->setPosition(x, height * 0.5f, z);
    tower.base->setScale(width, height, depth);
    tower.base->setColor(concrete);
    float midHeight = hero ? (3.2f + hash01(seed + 61) * 4.8f) : (1.8f + hash01(seed + 67) * 2.1f);
    tower.mid = g_instances.create(g_cubeMesh);
    tower.mid->setPosition(x + (left ? 0.22f : -0.22f), height + midHeight * 0.5f, z + (hash01(seed + 71) - 0.5f) * 0.6f);
    tower.mid->setScale(width * 0.70f, midHeight, depth * 0.68f);
    tower.mid->setColor(mixColor565(concrete, glassNight, 0.55f));
    tower.crown = g_instances.create((hero && (seed % 3 == 0)) ? static_cast<Mesh *>(g_coneMesh) : static_cast<Mesh *>(g_cubeMesh));
    tower.crown->setPosition(x, height + midHeight + (hero ? 1.2f : 0.7f), z);
    tower.crown->setScale(hero ? Vector3(0.18f, 1.6f, 0.18f) : Vector3(0.42f, 0.42f, 0.42f));
    tower.crown->setColor(tower.accentOff);
    float side = left ? 1.0f : -1.0f;
    for (size_t band = 0; band < WINDOW_BAND_COUNT; ++band)
    {
      tower.bands[band] = g_instances.create(g_cubeMesh);
      float y = height * (0.34f + 0.28f * static_cast<float>(band));
      float zOffset = (band == 0 ? -depth * 0.12f : depth * 0.10f);
      tower.bands[band]->setPosition(x + side * (width * 0.5f + 0.10f), y, z + zOffset);
      tower.bands[band]->setScale(0.12f, height * 0.12f, depth * 0.56f);
      tower.bands[band]->setColor(tower.accentOff);
    }
  }
}

static void initLamps()
{
  for (size_t i = 0; i < PROMENADE_LIGHT_COUNT; ++i)
  {
    const bool left = i < PROMENADE_LIGHT_COUNT / 2;
    const int slot = left ? static_cast<int>(i) : static_cast<int>(i - PROMENADE_LIGHT_COUNT / 2);
    float z = -47.0f + slot * 11.8f;
    float x = left ? -5.7f : 5.7f;
    LampPost &lamp = g_lamps[i];
    lamp.anchor = Vector3(x, 0.0f, z);
    lamp.glowOff = left ? Color::fromRGB888(90, 160, 210) : Color::fromRGB888(120, 108, 88);
    lamp.glowOn = left ? Color::fromRGB888(144, 238, 255) : Color::fromRGB888(255, 214, 146);
    lamp.pole = g_instances.create(g_cubeMesh);
    lamp.pole->setPosition(x, 1.0f, z);
    lamp.pole->setScale(0.08f, 2.0f, 0.08f);
    lamp.pole->setColor(Color::fromRGB888(62, 68, 82));
    lamp.glow = g_instances.create(g_cubeMesh);
    lamp.glow->setPosition(x, 2.08f, z);
    lamp.glow->setScale(0.13f, 0.13f, 0.13f);
    lamp.glow->setColor(lamp.glowOff);
  }
}

static void initClouds()
{
  for (size_t i = 0; i < CLOUD_CLUSTER_COUNT; ++i)
  {
    CloudCluster &cluster = g_clouds[i];
    cluster.origin = Vector3(-30.0f + hash01(static_cast<int>(i) * 17 + 3) * 60.0f, 16.0f + hash01(static_cast<int>(i) * 17 + 9) * 11.0f, -44.0f + hash01(static_cast<int>(i) * 17 + 15) * 88.0f);
    cluster.scale = 1.6f + hash01(static_cast<int>(i) * 17 + 21) * 2.2f;
    cluster.drift = 0.35f + hash01(static_cast<int>(i) * 17 + 27) * 0.9f;
    for (size_t puff = 0; puff < CLOUD_PUFF_COUNT; ++puff)
    {
      cluster.puffs[puff] = g_instances.create(g_sphereMesh);
      cluster.puffs[puff]->setScale(cluster.scale * (0.86f + 0.20f * static_cast<float>(puff)));
      cluster.puffs[puff]->setColor(Color::fromRGB888(238, 226, 214));
    }
  }
}

static void initScene(Renderer &r)
{
  initMeshes();
  initDistrictGeometry();
  initBuildings();
  initLamps();
  initPlaneRig();
  r.setSkyboxWithLighting(SKYBOX_SUNSET);
  r.setShadowsEnabled(true);
  r.setShadowPlaneY(WATER_LEVEL);
  r.setShadowOpacity(0.34f);
  r.setShadowColor(Color::fromRGB888(18, 24, 38));
  r.getShadowSettings().shadowOffset = 0.010f;
  r.setBackfaceCullingEnabled(true);
  r.setOcclusionCullingEnabled(false);
}

static Vector3 samplePlanePosition(float t)
{
  while (t < 0.0f)
    t += DEMO_DURATION;
  while (t >= DEMO_DURATION)
    t -= DEMO_DURATION;
  float u = t / DEMO_DURATION;
  float z = -56.0f + 118.0f * u + sinf(u * TWO_PI * 0.6f - 0.4f) * 2.2f;
  float x = sinf(u * TWO_PI * 1.08f - 0.35f) * 4.8f + sinf(u * TWO_PI * 2.75f + 0.6f) * 1.4f;
  float climb = smoother(sectionT(t, 30.0f, DEMO_DURATION)) * 5.5f;
  float y = 8.2f + sinf(u * TWO_PI * 0.82f + 0.6f) * 1.0f + sinf(u * TWO_PI * 2.15f - 0.4f) * 0.55f + climb;
  return Vector3(x, y, z);
}

static void evaluatePlaneState(float t, PlaneState &state)
{
  const float sampleStep = 0.06f;
  Vector3 p0 = samplePlanePosition(t - sampleStep);
  Vector3 p1 = samplePlanePosition(t);
  Vector3 p2 = samplePlanePosition(t + sampleStep);
  Vector3 forward = p2 - p0;
  safeNormalize(forward, Vector3(0.0f, 0.0f, 1.0f));
  Vector3 right = Vector3(0.0f, 1.0f, 0.0f).cross(forward);
  safeNormalize(right, Vector3(1.0f, 0.0f, 0.0f));
  Vector3 up = forward.cross(right);
  safeNormalize(up, Vector3(0.0f, 1.0f, 0.0f));
  Vector3 vel = (p2 - p0) * (0.5f / sampleStep);
  float lateralTurn = (p2.x - p0.x) * 2.6f;
  state.position = p1;
  state.forward = forward;
  state.right = right;
  state.up = up;
  state.speed = vel.length();
  state.bank = pip3D::clamp(-lateralTurn * 7.5f - forward.y * 10.0f, -38.0f, 38.0f);
}

static void updatePlaneRig(float t)
{
  evaluatePlaneState(t, g_planeState);
  const float yaw = atan2f(g_planeState.forward.x, g_planeState.forward.z) * RAD_TO_DEG;
  const float horiz = sqrtf(g_planeState.forward.x * g_planeState.forward.x + g_planeState.forward.z * g_planeState.forward.z);
  const float pitch = atan2f(g_planeState.forward.y, horiz) * RAD_TO_DEG;
  g_planeRoot->setPosition(g_planeState.position);
  g_planeRoot->setRotation(pitch, yaw, g_planeState.bank);
  const float wingFlex = sinf(t * 3.4f) * 1.0f + g_planeState.bank * 0.08f;
  g_planeBody->setPosition(0.0f, sinf(t * 4.6f) * 0.08f, 0.0f);
  g_planeBody->setRotation(1.2f * sinf(t * 2.4f), 0.0f, 0.0f);
  g_planeWingL->setRotation(0.0f, 0.0f, 2.0f + wingFlex);
  g_planeWingR->setRotation(0.0f, 0.0f, -2.0f - wingFlex);
  g_planeTail->setRotation(-1.0f + sinf(t * 2.0f) * 0.8f, 0.0f, 0.0f);
  g_planeEngineL->setRotation(0.0f, 0.0f, -4.0f - wingFlex * 0.55f);
  g_planeEngineR->setRotation(0.0f, 0.0f, 4.0f + wingFlex * 0.55f);

  float blink = (fmodf(t * 3.4f, 1.0f) < 0.18f) ? 1.0f : 0.15f;
  float beacon = (fmodf(t * 1.8f, 1.0f) < 0.12f) ? 1.0f : 0.10f;
  float duskMix = smoother(sectionT(t, 24.0f, DEMO_DURATION));
  g_planeParts[toIndex(PlanePartId::NAV_L)].instance->setColor(mixColor565(Color::fromRGB888(120, 30, 30), Color::fromRGB888(255, 92, 92), blink));
  g_planeParts[toIndex(PlanePartId::NAV_R)].instance->setColor(mixColor565(Color::fromRGB888(20, 72, 42), Color::fromRGB888(92, 255, 188), blink));
  g_planeParts[toIndex(PlanePartId::NAV_TAIL)].instance->setColor(mixColor565(Color::fromRGB888(76, 82, 108), Color::fromRGB888(255, 255, 255), 0.2f + 0.8f * blink));
  g_planeParts[toIndex(PlanePartId::BEACON_TOP)].instance->setColor(mixColor565(Color::fromRGB888(72, 20, 20), Color::fromRGB888(255, 110, 110), beacon));
  g_planeParts[toIndex(PlanePartId::BEACON_BOTTOM)].instance->setColor(mixColor565(Color::fromRGB888(72, 20, 20), Color::fromRGB888(255, 110, 110), beacon));
  Color fuselageCool = mixColor565(Color::fromRGB888(232, 235, 242), Color::fromRGB888(176, 198, 230), duskMix * 0.45f);
  g_planeParts[toIndex(PlanePartId::BODY)].instance->setColor(fuselageCool);
  g_planeParts[toIndex(PlanePartId::NOSE)].instance->setColor(mixColor565(fuselageCool, Color::fromRGB888(244, 244, 250), 0.25f));
  for (const RigPart &part : g_planeParts)
    syncNodeToInstance(part.node, part.instance);
  if (g_trailLeft && g_trailRight)
  {
    Vector3 trailVelocity = g_planeState.forward * (-g_planeState.speed * 0.16f);
    g_trailLeft->setPosition(g_planeExhaustL->getWorldPosition());
    g_trailRight->setPosition(g_planeExhaustR->getWorldPosition());
    g_trailLeft->setVelocityOffset(trailVelocity);
    g_trailRight->setVelocityOffset(trailVelocity);
  }
}

static void updateClouds(float t)
{
  Color cloudColor = mixColor565(Color::fromRGB888(238, 224, 208), Color::fromRGB888(196, 154, 184), smoother(sectionT(t, 18.0f, DEMO_DURATION)));
  cloudColor = mixColor565(cloudColor, Color::fromRGB888(118, 138, 188), smoother(sectionT(t, 32.0f, DEMO_DURATION)));
  for (size_t i = 0; i < CLOUD_CLUSTER_COUNT; ++i)
  {
    CloudCluster &cluster = g_clouds[i];
    float driftX = cluster.origin.x + t * cluster.drift * 0.9f;
    while (driftX > 38.0f)
      driftX -= 76.0f;
    for (size_t puff = 0; puff < CLOUD_PUFF_COUNT; ++puff)
    {
      float dir = static_cast<float>(puff) - 1.0f;
      Vector3 pos(driftX + dir * cluster.scale * 1.55f, cluster.origin.y + sinf(t * 0.35f + cluster.drift + dir) * 0.28f, cluster.origin.z + cosf(t * 0.22f + cluster.drift * 2.0f + dir) * 1.7f);
      cluster.puffs[puff]->setPosition(pos);
      cluster.puffs[puff]->setColor(cloudColor);
    }
  }
}

static void updateCityMaterials(float t)
{
  const float duskMix = smoother(sectionT(t, 18.0f, DEMO_DURATION));
  const float nightMix = smoother(sectionT(t, 30.0f, DEMO_DURATION));
  for (BuildingTower &tower : g_buildings)
  {
    tower.base->setColor(mixColor565(tower.baseDay, tower.baseNight, duskMix * 0.86f));
    tower.mid->setColor(mixColor565(tower.baseDay, tower.baseNight, 0.45f + duskMix * 0.50f));
    tower.crown->setColor(mixColor565(tower.baseNight, tower.accentOn, 0.18f + nightMix * 0.55f));
    for (size_t band = 0; band < WINDOW_BAND_COUNT; ++band)
    {
      float flicker = 0.84f + 0.16f * sinf(t * (2.4f + band * 0.5f) + tower.anchor.z * 0.07f + static_cast<float>(band));
      tower.bands[band]->setColor(mixColor565(tower.accentOff, tower.accentOn, pip3D::clamp((0.10f + 0.90f * duskMix) * flicker, 0.0f, 1.0f)));
    }
  }
  if (g_landParts[0])
    g_landParts[0]->setColor(mixColor565(Color::fromRGB888(80, 92, 108), Color::fromRGB888(38, 48, 66), duskMix));
  if (g_landParts[1])
    g_landParts[1]->setColor(mixColor565(Color::fromRGB888(80, 92, 108), Color::fromRGB888(38, 48, 66), duskMix));
  if (g_landParts[2])
    g_landParts[2]->setColor(mixColor565(Color::fromRGB888(118, 122, 132), Color::fromRGB888(60, 66, 78), duskMix));
  if (g_landParts[3])
    g_landParts[3]->setColor(mixColor565(Color::fromRGB888(118, 122, 132), Color::fromRGB888(60, 66, 78), duskMix));
  for (size_t i = 4; i < LAND_PART_COUNT; ++i)
    if (g_landParts[i])
      g_landParts[i]->setColor(mixColor565(Color::fromRGB888(124, 126, 134), Color::fromRGB888(62, 70, 84), duskMix));
  for (MeshInstance *road : g_roadParts)
    if (road)
      road->setColor(mixColor565(Color::fromRGB888(44, 46, 52), Color::fromRGB888(22, 24, 32), duskMix));
  for (size_t i = 2; i < ROAD_PART_COUNT; ++i)
    if (g_roadParts[i])
      g_roadParts[i]->setColor(mixColor565(Color::fromRGB888(174, 160, 110), Color::fromRGB888(255, 228, 164), nightMix * (0.82f + 0.18f * sinf(t * 2.0f + static_cast<float>(i)))));
  for (MeshInstance *part : g_bridgeParts)
    if (part)
      part->setColor(mixColor565(Color::fromRGB888(140, 148, 164), Color::fromRGB888(82, 94, 118), duskMix));
  if (g_bridgeParts[9])
    g_bridgeParts[9]->setColor(mixColor565(Color::fromRGB888(80, 24, 24), Color::fromRGB888(255, 110, 110), 0.16f + 0.84f * nightMix));
  if (g_bridgeParts[10])
    g_bridgeParts[10]->setColor(mixColor565(Color::fromRGB888(80, 24, 24), Color::fromRGB888(255, 110, 110), 0.16f + 0.84f * nightMix));
  for (size_t i = 0; i < BRIDGE_LIGHT_COUNT; ++i)
    if (g_bridgeLights[i])
      g_bridgeLights[i]->setColor(mixColor565(Color::fromRGB888(110, 92, 46), Color::fromRGB888(255, 222, 148), nightMix * (0.78f + 0.22f * sinf(t * 3.6f + static_cast<float>(i) * 0.6f))));
  for (size_t i = 0; i < PROMENADE_LIGHT_COUNT; ++i)
    g_lamps[i].glow->setColor(mixColor565(g_lamps[i].glowOff, g_lamps[i].glowOn, pip3D::clamp(nightMix * (0.85f + 0.15f * sinf(t * 2.6f + static_cast<float>(i) * 0.4f)), 0.0f, 1.0f)));
}

static void updateLightingAndSky(Renderer &r, float t)
{
  const float u = t / DEMO_DURATION;
  const float sunsetMix = smoother(sectionT(t, 6.0f, 26.0f));
  const float nightMix = smoother(sectionT(t, 28.0f, DEMO_DURATION));
  Color top = mixColor565(Color::fromRGB888(120, 188, 255), Color::fromRGB888(250, 142, 116), sunsetMix);
  top = mixColor565(top, Color::fromRGB888(22, 34, 76), nightMix);
  Color horizon = mixColor565(Color::fromRGB888(212, 228, 255), Color::fromRGB888(255, 188, 132), sunsetMix);
  horizon = mixColor565(horizon, Color::fromRGB888(72, 94, 154), nightMix);
  Color ground = mixColor565(Color::fromRGB888(146, 168, 188), Color::fromRGB888(116, 86, 96), sunsetMix);
  ground = mixColor565(ground, Color::fromRGB888(16, 20, 34), nightMix);
  r.getSkybox().setCustom(top, horizon, ground);

  const float sunAzimuth = (-0.26f + u * 0.58f) * PI;
  const float sunElevation = 0.72f - u * 0.98f;
  Vector3 sunDirToSky(cosf(sunAzimuth) * 0.72f, sunElevation, sinf(sunAzimuth) * 0.58f);
  safeNormalize(sunDirToSky, Vector3(-0.32f, 0.78f, 0.18f));
  Vector3 lightDir = sunDirToSky * -1.0f;
  safeNormalize(lightDir, Vector3(-0.42f, -0.88f, -0.24f));

  const float daylight = saturate((sunDirToSky.y + 0.18f) / 0.90f);
  const float sunDiscVisible = saturate((sunDirToSky.y - 0.10f) / 0.22f) * (1.0f - nightMix);
  Color sunColor = mixColor565(Color::fromRGB888(255, 246, 220), Color::fromRGB888(255, 168, 118), sunsetMix);
  sunColor = mixColor565(sunColor, Color::fromRGB888(232, 164, 124), 1.0f - daylight);
  r.clearLights();
  r.setMainDirectionalLight(lightDir, sunColor, 0.22f + 1.10f * daylight);

  Light towerLight;
  towerLight.type = LIGHT_POINT;
  towerLight.position = Vector3(-14.8f, 18.0f, 2.0f);
  towerLight.color = Color::fromRGB888(120, 220, 255);
  towerLight.intensity = 0.12f + 0.70f * nightMix;
  towerLight.setRange(22.0f);
  towerLight.colorCacheDirty = true;
  r.setLight(1, towerLight);

  Light bridgeLight;
  bridgeLight.type = LIGHT_POINT;
  bridgeLight.position = Vector3(0.0f, 2.3f, 6.0f);
  bridgeLight.color = Color::fromRGB888(255, 214, 152);
  bridgeLight.intensity = 0.10f + 0.54f * nightMix;
  bridgeLight.setRange(18.0f);
  bridgeLight.colorCacheDirty = true;
  r.setLight(2, bridgeLight);

  Light planeLight;
  planeLight.type = LIGHT_POINT;
  planeLight.position = g_planeState.position + g_planeState.forward * 1.6f;
  planeLight.color = Color::fromRGB888(208, 236, 255);
  planeLight.intensity = 0.10f + 0.45f * nightMix;
  planeLight.setRange(10.0f);
  planeLight.colorCacheDirty = true;
  r.setLight(3, planeLight);

  g_sunState.direction = sunDirToSky;
  g_sunState.worldPos = Vector3(0.0f, SUN_SCENE_CENTER_Y, 0.0f) + sunDirToSky * SUN_ORBIT_RADIUS;
  g_sunState.color = sunColor;
  g_sunState.visibility = sunDiscVisible;
  g_sunState.glow = 0.05f + 0.14f * sunDiscVisible;
  g_sunState.size = 0.52f + 0.10f * sunDiscVisible;
}

static void updateCamera(Renderer &r, float t)
{
  Camera &cam = r.getCamera();
  const Vector3 aim = g_planeState.position + g_planeState.forward * 8.0f + Vector3(0.0f, 0.3f, 0.0f);
  if (t < 10.0f)
  {
    float u = ease(sectionT(t, 0.0f, 10.0f));
    cam.position = g_planeState.position - g_planeState.forward * (24.0f - 6.0f * u) - g_planeState.right * (15.0f - 4.0f * u) + Vector3(-6.0f, 9.5f - 2.2f * u, -6.0f + 8.0f * u);
    cam.target = lerpVec(Vector3(-6.0f, 4.5f, 6.0f), aim, 0.78f + 0.22f * u);
    cam.setPerspective(62.0f - 6.0f * u, 0.1f, CAMERA_FAR);
  }
  else if (t < 22.0f)
  {
    float u = ease(sectionT(t, 10.0f, 22.0f));
    cam.position = g_planeState.position - g_planeState.forward * (10.0f - 1.0f * u) + g_planeState.right * (5.5f - 4.0f * u) + g_planeState.up * (1.8f + 0.4f * sinf(t * 1.7f));
    cam.target = g_planeState.position + g_planeState.forward * (18.0f - 3.0f * u) + g_planeState.right * 0.7f;
    cam.setPerspective(52.0f, 0.1f, CAMERA_FAR);
  }
  else if (t < 32.0f)
  {
    float u = ease(sectionT(t, 22.0f, 32.0f));
    cam.position = g_planeState.position + g_planeState.forward * (7.0f - 3.0f * u) + g_planeState.right * (6.4f - 11.0f * u) + g_planeState.up * (2.6f - 0.8f * u);
    cam.target = g_planeState.position + g_planeState.forward * 8.0f + g_planeState.up * 0.6f;
    cam.setPerspective(48.0f - 2.0f * sinf(u * PI), 0.1f, CAMERA_FAR);
  }
  else
  {
    float u = ease(sectionT(t, 32.0f, DEMO_DURATION));
    float orbit = 0.6f + u * 1.4f;
    Vector3 orbitOffset = g_planeState.right * (cosf(orbit) * 13.0f) - g_planeState.forward * (sinf(orbit) * 11.5f) + Vector3(0.0f, 5.6f - 0.8f * u, 0.0f);
    cam.position = g_planeState.position + orbitOffset;
    cam.target = g_planeState.position + g_planeState.forward * 20.0f + Vector3(0.0f, 2.6f, 0.0f);
    cam.setPerspective(56.0f + 4.0f * u, 0.1f, CAMERA_FAR);
  }
  cam.up = Vector3(0.0f, 1.0f, 0.0f);
  cam.markDirty();
}

static void updateScene(Renderer &r, float dt)
{
  g_demoTime += dt;
  if (g_demoTime >= DEMO_DURATION)
    g_demoTime = fmodf(g_demoTime, DEMO_DURATION);
  updatePlaneRig(g_demoTime);
  g_fx.update(dt);
  updateCityMaterials(g_demoTime);
  updateLightingAndSky(r, g_demoTime);
#if defined(PIP3D_PC)
  updateFreeCamera(r, dt);
#else
  updateCamera(r, g_demoTime);
#endif
}

static void renderWorld(Renderer &r)
{
  g_fx.render(r);
  const std::vector<MeshInstance *> &all = g_instances.all();
  for (MeshInstance *inst : all)
  {
    if (!inst || !inst->isVisible())
      continue;
    r.drawMeshInstance(inst);
    r.drawMeshInstanceShadow(inst);
  }
}

static void drawHud(Renderer &r)
{
#if defined(PIP3D_PC)
  if (g_freeCamera.enabled)
  {
    r.drawText(12, 12, "FREE LOOK", Color::fromRGB888(238, 244, 255));
    r.drawText(12, 22, "WASD move  mouse look  shift sprint  Q/E up/down", Color::fromRGB888(184, 210, 244));
    r.drawText(12, 32, "TAB cinematic toggle", Color::fromRGB888(152, 184, 226));
    if (!pipcore::desktop::Runtime::instance().isMouseCaptured())
      r.drawText(12, 42, "Left click to capture mouse  ESC release", Color::fromRGB888(255, 214, 156));
    return;
  }
  r.drawText(12, 12, "CINEMATIC", Color::fromRGB888(236, 242, 255));
  r.drawText(12, 22, "TAB free look", Color::fromRGB888(168, 206, 255));
#endif

  const float intro = 1.0f - smoother(sectionT(g_demoTime, 2.2f, 7.4f));
  const float finale = smoother(sectionT(g_demoTime, DEMO_DURATION - 5.5f, DEMO_DURATION));
  if (intro > 0.05f)
  {
    r.drawText(12, 14, "SKYLINE PASS", mixColor565(Color::fromRGB888(70, 84, 108), Color::fromRGB888(248, 238, 220), intro));
    r.drawText(12, 24, "city flight cinematic", mixColor565(Color::fromRGB888(40, 60, 92), Color::fromRGB888(168, 224, 255), intro));
  }
  if (finale > 0.08f)
  {
    r.drawText(12, SCREEN_HEIGHT - 24, "BLUE HOUR ARRIVAL", mixColor565(Color::fromRGB888(36, 52, 80), Color::fromRGB888(228, 236, 255), finale));
    r.drawText(12, SCREEN_HEIGHT - 14, getSectionName(g_demoTime), mixColor565(Color::fromRGB888(36, 52, 80), Color::fromRGB888(156, 202, 255), finale));
  }
}

void setup()
{
  Serial.begin(115200);
  Renderer &r = begin3D(SCREEN_WIDTH, SCREEN_HEIGHT, TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN, TFT_BL_PIN, 60000000);
  if (!r.isInitialized())
  {
    Serial.println("Pip3D init failed");
    for (;;)
      delay(1000);
  }
  Camera &cam = r.getCamera();
  cam.position = Vector3(-28.0f, 12.0f, -36.0f);
  cam.target = Vector3(0.0f, 6.0f, 8.0f);
  cam.up = Vector3(0.0f, 1.0f, 0.0f);
  cam.setPerspective(60.0f, 0.1f, CAMERA_FAR);
  cam.markDirty();
  initScene(r);
#if defined(PIP3D_PC)
  syncFreeCameraFromView(cam);
  pipcore::desktop::Runtime::instance().setMouseCapture(true);
#endif
  g_lastMs = millis();
  g_demoTime = 0.0f;
}

void loop()
{
  uint32_t now = millis();
  uint32_t dtMs = now - g_lastMs;
  g_lastMs = now;
  float dt = dtMs * 0.001f;
  if (dt > 0.05f)
    dt = 0.05f;
  Renderer &r = renderer();
  if (!r.isInitialized())
  {
    delay(100);
    return;
  }
  updateScene(r, dt);
  for (int band = 0; band < SCREEN_BAND_COUNT; ++band)
  {
    r.beginFrameBand(band);
    r.drawSkyboxBackground();
    if (g_sunState.visibility > 0.02f)
      r.drawSunSprite(g_sunState.worldPos, g_sunState.color, g_sunState.glow, g_sunState.size);
    renderWorld(r);
    if (band == 0)
      drawHud(r);
    r.endFrameBand(band);
  }
}
