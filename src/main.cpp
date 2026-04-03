#include <Arduino.h>
#include <math.h>
#include <stdio.h>

#define TFT_MOSI 11
#define TFT_MISO -1
#define TFT_SCLK 12

#include "Pip3D.h"

using namespace pip3D;

static const int8_t TFT_CS_PIN = 10;
static const int8_t TFT_DC_PIN = 9;
static const int8_t TFT_RST_PIN = -1;
static const int8_t TFT_BL_PIN = -1;

static constexpr float DEMO_DURATION = 32.0f;
static constexpr float SECTION_REVEAL_END = 8.0f;
static constexpr float SECTION_GLIDE_END = 16.0f;
static constexpr float SECTION_DETAIL_END = 24.0f;
static constexpr float ROAD_LOOP = 56.0f;

static constexpr int GROUND_SEGMENT_COUNT = 5;
static constexpr int LANE_MARKER_COUNT = 8;
static constexpr int SIDE_POST_COUNT = 10;

enum DemoSection
{
  SECTION_REVEAL = 0,
  SECTION_GLIDE = 1,
  SECTION_DETAIL = 2,
  SECTION_FINAL = 3
};

enum CarPartId
{
  PART_BODY = 0,
  PART_SILL_L,
  PART_SILL_R,
  PART_HOOD,
  PART_TAIL,
  PART_CABIN,
  PART_ROOF,
  PART_WINDSHIELD,
  PART_REAR_GLASS,
  PART_BUMPER_FRONT,
  PART_BUMPER_REAR,
  PART_SPOILER_L,
  PART_SPOILER_R,
  PART_SPOILER_WING,
  PART_LIGHT_L,
  PART_LIGHT_R,
  PART_TAILLIGHT_L,
  PART_TAILLIGHT_R,
  PART_MIRROR_L,
  PART_MIRROR_R,
  PART_WHEEL_FL,
  PART_WHEEL_FR,
  PART_WHEEL_RL,
  PART_WHEEL_RR,
  PART_HUB_FL,
  PART_HUB_FR,
  PART_HUB_RL,
  PART_HUB_RR,
  PART_COUNT
};

struct RigPart
{
  Node *node;
  MeshInstance *instance;
};

static InstanceManager g_instances;

static Plane *g_groundPanelMesh = nullptr;
static Cube *g_cubeMesh = nullptr;
static Sphere *g_sphereMesh = nullptr;
static Cylinder *g_cylinderMesh = nullptr;
static Cone *g_coneMesh = nullptr;

static MeshInstance *g_groundPanels[GROUND_SEGMENT_COUNT];
static MeshInstance *g_laneMarkers[LANE_MARKER_COUNT];
static MeshInstance *g_sidePosts[SIDE_POST_COUNT * 2];
static MeshInstance *g_sunMarker = nullptr;

static Node *g_carRoot = nullptr;
static Node *g_bodyNode = nullptr;
static Node *g_steerFL = nullptr;
static Node *g_steerFR = nullptr;
static Node *g_wheelFL = nullptr;
static Node *g_wheelFR = nullptr;
static Node *g_wheelRL = nullptr;
static Node *g_wheelRR = nullptr;

static RigPart g_carParts[PART_COUNT];

static uint32_t g_lastMs = 0;
static float g_demoTime = 0.0f;

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

static float sectionT(float t, float a, float b)
{
  if (b <= a)
    return 0.0f;
  return saturate((t - a) / (b - a));
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
  if (t < SECTION_REVEAL_END)
    return "REVEAL ARC";
  if (t < SECTION_GLIDE_END)
    return "SIDE GLIDE";
  if (t < SECTION_DETAIL_END)
    return "DETAIL RUN";
  return "SUNSET HERO";
}

static DemoSection getSection(float t)
{
  if (t < SECTION_REVEAL_END)
    return SECTION_REVEAL;
  if (t < SECTION_GLIDE_END)
    return SECTION_GLIDE;
  if (t < SECTION_DETAIL_END)
    return SECTION_DETAIL;
  return SECTION_FINAL;
}

static void getSkyColorsForType(SkyboxType type, Color &top, Color &horizon, Color &ground)
{
  Sky sky(type);
  top = sky.top;
  horizon = sky.horizon;
  ground = sky.ground;
}

static Quaternion quaternionFromBasis(const Matrix4x4 &m)
{
  const float trace = m.m[0] + m.m[5] + m.m[10];

  if (trace > 0.0f)
  {
    float s = sqrtf(trace + 1.0f) * 2.0f;
    Quaternion q((m.m[9] - m.m[6]) / s,
                 (m.m[2] - m.m[8]) / s,
                 (m.m[4] - m.m[1]) / s,
                 0.25f * s);
    q.normalize();
    return q;
  }

  if (m.m[0] > m.m[5] && m.m[0] > m.m[10])
  {
    float s = sqrtf(1.0f + m.m[0] - m.m[5] - m.m[10]) * 2.0f;
    Quaternion q(0.25f * s,
                 (m.m[1] + m.m[4]) / s,
                 (m.m[2] + m.m[8]) / s,
                 (m.m[9] - m.m[6]) / s);
    q.normalize();
    return q;
  }

  if (m.m[5] > m.m[10])
  {
    float s = sqrtf(1.0f + m.m[5] - m.m[0] - m.m[10]) * 2.0f;
    Quaternion q((m.m[1] + m.m[4]) / s,
                 0.25f * s,
                 (m.m[6] + m.m[9]) / s,
                 (m.m[2] - m.m[8]) / s);
    q.normalize();
    return q;
  }

  float s = sqrtf(1.0f + m.m[10] - m.m[0] - m.m[5]) * 2.0f;
  Quaternion q((m.m[2] + m.m[8]) / s,
               (m.m[6] + m.m[9]) / s,
               0.25f * s,
               (m.m[4] - m.m[1]) / s);
  q.normalize();
  return q;
}

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

static void setupPart(CarPartId id,
                      Node *parent,
                      Mesh *mesh,
                      const Color &color,
                      const Vector3 &position,
                      const Vector3 &scale,
                      const Vector3 &rotation = Vector3(0.0f, 0.0f, 0.0f))
{
  Node *node = new Node();
  parent->addChild(node);
  node->setPosition(position);
  node->setRotation(rotation);
  node->setScale(scale);

  MeshInstance *instance = g_instances.create(mesh);
  instance->setColor(color);

  g_carParts[id].node = node;
  g_carParts[id].instance = instance;
}

static void initCarRig()
{
  if (g_carRoot)
    return;

  g_carRoot = new Node("CarRoot");
  g_bodyNode = new Node("BodyRoot");
  g_carRoot->addChild(g_bodyNode);

  setupPart(PART_BODY, g_bodyNode, g_cubeMesh, Color::fromRGB888(228, 62, 52), Vector3(0.0f, 0.72f, 0.10f), Vector3(3.25f, 0.42f, 8.35f));
  setupPart(PART_SILL_L, g_bodyNode, g_cubeMesh, Color::fromRGB888(150, 24, 24), Vector3(-1.64f, 0.48f, 0.10f), Vector3(0.20f, 0.22f, 6.00f));
  setupPart(PART_SILL_R, g_bodyNode, g_cubeMesh, Color::fromRGB888(150, 24, 24), Vector3(1.64f, 0.48f, 0.10f), Vector3(0.20f, 0.22f, 6.00f));
  setupPart(PART_HOOD, g_bodyNode, g_cubeMesh, Color::fromRGB888(248, 86, 66), Vector3(0.0f, 0.98f, 2.62f), Vector3(2.42f, 0.12f, 2.75f), Vector3(15.0f, 0.0f, 0.0f));
  setupPart(PART_TAIL, g_bodyNode, g_cubeMesh, Color::fromRGB888(182, 34, 32), Vector3(0.0f, 0.96f, -3.22f), Vector3(2.24f, 0.14f, 1.82f), Vector3(-10.0f, 0.0f, 0.0f));
  setupPart(PART_CABIN, g_bodyNode, g_cubeMesh, Color::fromRGB888(240, 76, 68), Vector3(0.0f, 1.30f, -0.24f), Vector3(2.10f, 0.54f, 2.92f));
  setupPart(PART_ROOF, g_bodyNode, g_cubeMesh, Color::fromRGB888(228, 62, 52), Vector3(0.0f, 1.76f, -0.54f), Vector3(1.54f, 0.11f, 1.42f));
  setupPart(PART_WINDSHIELD, g_bodyNode, g_cubeMesh, Color::fromRGB888(150, 210, 255), Vector3(0.0f, 1.36f, 1.06f), Vector3(1.84f, 0.06f, 1.28f), Vector3(64.0f, 0.0f, 0.0f));
  setupPart(PART_REAR_GLASS, g_bodyNode, g_cubeMesh, Color::fromRGB888(122, 172, 230), Vector3(0.0f, 1.38f, -1.84f), Vector3(1.64f, 0.06f, 1.00f), Vector3(-50.0f, 0.0f, 0.0f));
  setupPart(PART_BUMPER_FRONT, g_bodyNode, g_cubeMesh, Color::fromRGB888(32, 34, 44), Vector3(0.0f, 0.48f, 4.28f), Vector3(2.56f, 0.16f, 0.24f));
  setupPart(PART_BUMPER_REAR, g_bodyNode, g_cubeMesh, Color::fromRGB888(32, 34, 44), Vector3(0.0f, 0.52f, -4.24f), Vector3(2.36f, 0.15f, 0.24f));
  setupPart(PART_SPOILER_L, g_bodyNode, g_cubeMesh, Color::fromRGB888(26, 28, 38), Vector3(-0.96f, 1.62f, -4.02f), Vector3(0.10f, 0.28f, 0.10f));
  setupPart(PART_SPOILER_R, g_bodyNode, g_cubeMesh, Color::fromRGB888(26, 28, 38), Vector3(0.96f, 1.62f, -4.02f), Vector3(0.10f, 0.28f, 0.10f));
  setupPart(PART_SPOILER_WING, g_bodyNode, g_cubeMesh, Color::fromRGB888(38, 42, 54), Vector3(0.0f, 1.82f, -4.00f), Vector3(2.06f, 0.08f, 0.42f));
  setupPart(PART_LIGHT_L, g_bodyNode, g_sphereMesh, Color::fromRGB888(255, 248, 205), Vector3(-0.90f, 0.98f, 4.26f), Vector3(0.18f, 0.18f, 0.18f));
  setupPart(PART_LIGHT_R, g_bodyNode, g_sphereMesh, Color::fromRGB888(255, 248, 205), Vector3(0.90f, 0.98f, 4.26f), Vector3(0.18f, 0.18f, 0.18f));
  setupPart(PART_TAILLIGHT_L, g_bodyNode, g_cubeMesh, Color::fromRGB888(255, 70, 70), Vector3(-0.90f, 0.90f, -4.34f), Vector3(0.38f, 0.10f, 0.06f));
  setupPart(PART_TAILLIGHT_R, g_bodyNode, g_cubeMesh, Color::fromRGB888(255, 70, 70), Vector3(0.90f, 0.90f, -4.34f), Vector3(0.38f, 0.10f, 0.06f));
  setupPart(PART_MIRROR_L, g_bodyNode, g_coneMesh, Color::fromRGB888(38, 42, 54), Vector3(-1.34f, 1.26f, 0.60f), Vector3(0.15f, 0.24f, 0.15f), Vector3(0.0f, 0.0f, 90.0f));
  setupPart(PART_MIRROR_R, g_bodyNode, g_coneMesh, Color::fromRGB888(38, 42, 54), Vector3(1.34f, 1.26f, 0.60f), Vector3(0.15f, 0.24f, 0.15f), Vector3(0.0f, 0.0f, -90.0f));

  g_steerFL = new Node("SteerFL");
  g_steerFR = new Node("SteerFR");
  g_wheelFL = new Node("WheelFL");
  g_wheelFR = new Node("WheelFR");
  g_wheelRL = new Node("WheelRL");
  g_wheelRR = new Node("WheelRR");

  g_bodyNode->addChild(g_steerFL);
  g_bodyNode->addChild(g_steerFR);
  g_bodyNode->addChild(g_wheelRL);
  g_bodyNode->addChild(g_wheelRR);

  g_steerFL->setPosition(-1.92f, 0.44f, 2.54f);
  g_steerFR->setPosition(1.92f, 0.44f, 2.54f);
  g_wheelRL->setPosition(-1.92f, 0.44f, -2.66f);
  g_wheelRR->setPosition(1.92f, 0.44f, -2.66f);

  g_steerFL->addChild(g_wheelFL);
  g_steerFR->addChild(g_wheelFR);

  setupPart(PART_WHEEL_FL, g_wheelFL, g_cylinderMesh, Color::fromRGB888(24, 24, 28), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.98f, 0.38f, 0.98f), Vector3(0.0f, 0.0f, 90.0f));
  setupPart(PART_WHEEL_FR, g_wheelFR, g_cylinderMesh, Color::fromRGB888(24, 24, 28), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.98f, 0.38f, 0.98f), Vector3(0.0f, 0.0f, 90.0f));
  setupPart(PART_WHEEL_RL, g_wheelRL, g_cylinderMesh, Color::fromRGB888(24, 24, 28), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.98f, 0.38f, 0.98f), Vector3(0.0f, 0.0f, 90.0f));
  setupPart(PART_WHEEL_RR, g_wheelRR, g_cylinderMesh, Color::fromRGB888(24, 24, 28), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.98f, 0.38f, 0.98f), Vector3(0.0f, 0.0f, 90.0f));

  setupPart(PART_HUB_FL, g_wheelFL, g_sphereMesh, Color::fromRGB888(205, 210, 220), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.30f, 0.30f, 0.30f));
  setupPart(PART_HUB_FR, g_wheelFR, g_sphereMesh, Color::fromRGB888(205, 210, 220), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.30f, 0.30f, 0.30f));
  setupPart(PART_HUB_RL, g_wheelRL, g_sphereMesh, Color::fromRGB888(205, 210, 220), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.30f, 0.30f, 0.30f));
  setupPart(PART_HUB_RR, g_wheelRR, g_sphereMesh, Color::fromRGB888(205, 210, 220), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.30f, 0.30f, 0.30f));
}

static void initEnvironment()
{
  if (!g_groundPanelMesh)
    g_groundPanelMesh = new Plane(28.0f, 16.0f, 1, Color::fromRGB888(194, 194, 188));

  for (int i = 0; i < GROUND_SEGMENT_COUNT; ++i)
  {
    g_groundPanels[i] = g_instances.create(g_groundPanelMesh);
    g_groundPanels[i]->setColor((i & 1) == 0 ? Color::fromRGB888(194, 194, 188) : Color::fromRGB888(186, 186, 182));
    g_groundPanels[i]->setScale(1.0f, 1.0f, 1.0f);
  }

  for (int i = 0; i < LANE_MARKER_COUNT; ++i)
  {
    g_laneMarkers[i] = g_instances.create(g_cubeMesh);
    g_laneMarkers[i]->setColor(Color::fromRGB888(246, 244, 226));
    g_laneMarkers[i]->setScale(0.22f, 0.03f, 2.2f);
  }

  for (int i = 0; i < SIDE_POST_COUNT * 2; ++i)
  {
    g_sidePosts[i] = g_instances.create(g_cubeMesh);
    g_sidePosts[i]->setColor((i & 1) == 0 ? Color::fromRGB888(255, 110, 88) : Color::fromRGB888(120, 215, 255));
    g_sidePosts[i]->setScale(0.20f, 1.10f, 0.20f);
  }

  g_sunMarker = g_instances.create(g_sphereMesh);
  g_sunMarker->setScale(0.72f);
  g_sunMarker->setColor(Color::fromRGB888(255, 225, 160));
}

static void initMeshes()
{
  if (!g_cubeMesh)
    g_cubeMesh = new Cube(1.0f, Color::WHITE);
  if (!g_sphereMesh)
    g_sphereMesh = new Sphere(0.7f, 16, 12, Color::WHITE);
  if (!g_cylinderMesh)
    g_cylinderMesh = new Cylinder(0.55f, 1.8f, 14, Color::WHITE);
  if (!g_coneMesh)
    g_coneMesh = new Cone(0.7f, 1.4f, 14, Color::WHITE);
}

static void initCamera(Renderer &r)
{
  Camera &cam = r.getCamera();
  cam.position = Vector3(-7.5f, 2.8f, 24.0f);
  cam.target = Vector3(0.0f, 1.3f, 20.0f);
  cam.up = Vector3(0.0f, 1.0f, 0.0f);
  cam.setPerspective(60.0f, 0.1f, 180.0f);
  cam.markDirty();
}

static void initScene(Renderer &r)
{
  initMeshes();
  initEnvironment();
  initCarRig();

  r.setSkyboxWithLighting(SKYBOX_DAY);
  r.setShadowsEnabled(true);
  r.setShadowPlaneY(0.0f);
  r.setBackfaceCullingEnabled(true);
  r.setOcclusionCullingEnabled(false);
}

static void updateCarRig(float t)
{
  const float revealBlend = ease(sectionT(t, 0.0f, SECTION_REVEAL_END));
  const float finalBlend = ease(sectionT(t, SECTION_DETAIL_END, DEMO_DURATION));

  const float drivePhase = t * 4.7f;
  const float bodyBob = sinf(drivePhase) * 0.025f;
  const float bodyPitch = sinf(t * 1.25f) * 1.8f - finalBlend * 1.2f;
  const float bodyRoll = sinf(t * 1.55f + 0.6f) * 1.6f + revealBlend * 0.4f;

  const float yawSweep =
      (getSection(t) == SECTION_REVEAL) ? (-10.0f + 18.0f * revealBlend) :
      (getSection(t) == SECTION_GLIDE) ? 6.0f * sinf((t - SECTION_REVEAL_END) * 0.55f) :
      (getSection(t) == SECTION_DETAIL) ? (-7.0f + 14.0f * ease(sectionT(t, SECTION_GLIDE_END, SECTION_DETAIL_END))) :
      (5.0f * sinf((t - SECTION_DETAIL_END) * 0.5f));

  const float carZ = 18.0f + sinf(t * 0.35f) * 0.6f + finalBlend * 1.2f;
  g_carRoot->setPosition(0.0f, 0.10f + bodyBob, carZ);
  g_carRoot->setRotation(0.0f, yawSweep, 0.0f);

  g_bodyNode->setPosition(0.0f, 0.0f, 0.0f);
  g_bodyNode->setRotation(bodyPitch, 0.0f, bodyRoll);

  float steer =
      (getSection(t) == SECTION_REVEAL) ? 10.0f * sinf(t * 0.7f) :
      (getSection(t) == SECTION_GLIDE) ? 14.0f * sinf((t - SECTION_REVEAL_END) * 0.95f) :
      (getSection(t) == SECTION_DETAIL) ? 5.0f * sinf((t - SECTION_GLIDE_END) * 1.8f) :
      -8.0f + 10.0f * sinf((t - SECTION_DETAIL_END) * 0.7f);

  g_steerFL->setRotation(0.0f, steer, 0.0f);
  g_steerFR->setRotation(0.0f, steer, 0.0f);

  const float wheelSpin = fmodf(t * 540.0f, 360.0f);
  g_wheelFL->setRotation(wheelSpin, 0.0f, 90.0f);
  g_wheelFR->setRotation(wheelSpin, 0.0f, 90.0f);
  g_wheelRL->setRotation(wheelSpin, 0.0f, 90.0f);
  g_wheelRR->setRotation(wheelSpin, 0.0f, 90.0f);

  const float headlightGlow = 0.25f + finalBlend * 0.75f;
  const Color headOff = Color::fromRGB888(255, 238, 190);
  const Color headOn = Color::fromRGB888(255, 255, 240);
  g_carParts[PART_LIGHT_L].instance->setColor(mixColor565(headOff, headOn, headlightGlow));
  g_carParts[PART_LIGHT_R].instance->setColor(mixColor565(headOff, headOn, headlightGlow));

  const float taillightGlow = 0.35f + finalBlend * 0.65f;
  const Color tailOff = Color::fromRGB888(190, 40, 40);
  const Color tailOn = Color::fromRGB888(255, 70, 70);
  g_carParts[PART_TAILLIGHT_L].instance->setColor(mixColor565(tailOff, tailOn, taillightGlow));
  g_carParts[PART_TAILLIGHT_R].instance->setColor(mixColor565(tailOff, tailOn, taillightGlow));

  for (int i = 0; i < PART_COUNT; ++i)
  {
    syncNodeToInstance(g_carParts[i].node, g_carParts[i].instance);
  }
}

static void updateEnvironment(float t)
{
  const Vector3 carPos = g_carRoot->getWorldPosition();
  const float roadDrift = fmodf(t * 11.0f, ROAD_LOOP);
  const float groundLoop = 80.0f;

  for (int i = 0; i < GROUND_SEGMENT_COUNT; ++i)
  {
    float z = carPos.z - 16.0f + i * 16.0f + fmodf(t * 6.0f, groundLoop);
    while (z > carPos.z + 40.0f)
      z -= groundLoop;
    g_groundPanels[i]->setPosition(0.0f, 0.0f, z);
  }

  for (int i = 0; i < LANE_MARKER_COUNT; ++i)
  {
    float z = carPos.z - 18.0f + i * 7.0f + roadDrift;
    while (z > carPos.z + 34.0f)
      z -= ROAD_LOOP;
    g_laneMarkers[i]->setPosition(0.0f, 0.01f, z);
  }

  for (int side = 0; side < 2; ++side)
  {
    const float x = (side == 0) ? -6.5f : 6.5f;
    for (int i = 0; i < SIDE_POST_COUNT; ++i)
    {
      int idx = side * SIDE_POST_COUNT + i;
      float z = carPos.z - 16.0f + i * 6.0f + roadDrift * 0.85f;
      while (z > carPos.z + 32.0f)
        z -= ROAD_LOOP;
      g_sidePosts[idx]->setPosition(x, 0.56f, z);
      g_sidePosts[idx]->setScale(0.18f, 1.12f + 0.10f * sinf(t * 0.8f + i + side), 0.18f);
    }
  }
}

static void updateLightingAndSky(Renderer &r, float t)
{
  const float keyTimes[] = {8.0f, 16.0f, 24.0f};
  const SkyboxType keySkies[] = {SKYBOX_DAY, SKYBOX_SUNSET, SKYBOX_DAWN, SKYBOX_NIGHT};

  int idx = 0;
  while (idx < 3 && t >= keyTimes[idx])
    ++idx;

  Color topA, horizonA, groundA;
  Color topB, horizonB, groundB;
  getSkyColorsForType(keySkies[idx], topA, horizonA, groundA);
  topB = topA;
  horizonB = horizonA;
  groundB = groundA;

  if (idx < 3)
  {
    float blend = smoother(sectionT(t, keyTimes[idx] - 2.0f, keyTimes[idx] + 1.2f));
    getSkyColorsForType(keySkies[idx + 1], topB, horizonB, groundB);
    r.getSkybox().setCustom(mixColor565(topA, topB, blend),
                            mixColor565(horizonA, horizonB, blend),
                            mixColor565(groundA, groundB, blend));
  }
  else
  {
    r.getSkybox().setCustom(topA, horizonA, groundA);
  }

  const float sunPath = sectionT(t, 0.0f, DEMO_DURATION);
  Vector3 lightDir(-0.72f + 0.42f * sunPath,
                   -1.05f + 0.55f * cosf(sunPath * PI),
                   -0.18f - 0.58f * sunPath);
  lightDir.normalize();

  const Color noon = Color::fromRGB888(255, 244, 224);
  const Color sunset = Color::fromRGB888(255, 172, 114);
  const Color dusk = Color::fromRGB888(162, 190, 255);

  const float sunsetMix = smoother(sectionT(t, 8.0f, 24.0f));
  const float duskMix = smoother(sectionT(t, 22.0f, DEMO_DURATION));

  Color lightColor = mixColor565(noon, sunset, sunsetMix);
  lightColor = mixColor565(lightColor, dusk, duskMix);

  const float intensity = 1.42f - 0.52f * duskMix;
  r.setMainDirectionalLight(lightDir, lightColor, intensity);

  Vector3 sunPos = r.getCamera().position + Vector3(-lightDir.x * 36.0f,
                                                    11.0f - lightDir.y * 18.0f,
                                                    30.0f - lightDir.z * 14.0f);
  Color sunColor = mixColor565(Color::fromRGB888(255, 235, 180),
                               Color::fromRGB888(255, 148, 108),
                               sunsetMix);
  sunColor = mixColor565(sunColor, Color::fromRGB888(255, 180, 210), duskMix * 0.55f);
  r.drawSunSprite(sunPos, sunColor, 0.95f - duskMix * 0.25f);

  if (g_sunMarker)
  {
    g_sunMarker->setPosition(sunPos);
    g_sunMarker->setColor(sunColor);
  }
}

static void updateCamera(Renderer &r, float t)
{
  Camera &cam = r.getCamera();
  const Vector3 carPos = g_carRoot->getWorldPosition();
  const Vector3 target = carPos + Vector3(0.0f, 1.30f, 0.0f);

  if (t < SECTION_REVEAL_END)
  {
    float u = ease(sectionT(t, 0.0f, SECTION_REVEAL_END));
    float angle = -2.30f + 2.05f * u;
    float radius = 8.8f - 1.6f * u;
    cam.setPerspective(58.0f - 4.0f * u, 0.1f, 180.0f);
    cam.position = carPos + Vector3(sinf(angle) * radius, 2.70f + 0.25f * sinf(t * 1.2f), cosf(angle) * radius);
    cam.target = target + Vector3(0.0f, 0.22f, 0.35f);
  }
  else if (t < SECTION_GLIDE_END)
  {
    float u = ease(sectionT(t, SECTION_REVEAL_END, SECTION_GLIDE_END));
    cam.setPerspective(53.0f, 0.1f, 180.0f);
    cam.position = carPos + Vector3(-6.4f + 6.7f * u, 1.42f + 0.10f * sinf(t * 1.4f), 3.0f - 1.6f * u);
    cam.target = target + Vector3(0.62f, 0.02f, 1.34f - 0.50f * u);
  }
  else if (t < SECTION_DETAIL_END)
  {
    float u = ease(sectionT(t, SECTION_GLIDE_END, SECTION_DETAIL_END));
    float angle = -0.18f + 1.15f * u;
    float radius = 4.2f + 0.8f * sinf(u * PI);
    cam.setPerspective(48.0f - 2.0f * sinf(u * PI), 0.1f, 180.0f);
    cam.position = carPos + Vector3(sinf(angle) * radius, 1.86f + 0.18f * sinf(t * 1.9f), cosf(angle) * radius - 0.6f);
    cam.target = target + Vector3(0.0f, 0.10f, -0.35f);
  }
  else
  {
    float u = ease(sectionT(t, SECTION_DETAIL_END, DEMO_DURATION));
    float angle = 0.55f + 1.55f * u;
    float radius = 8.8f + 1.4f * u;
    cam.setPerspective(60.0f - 6.0f * u, 0.1f, 180.0f);
    cam.position = carPos + Vector3(sinf(angle) * radius, 2.75f - 0.35f * u, cosf(angle) * radius);
    cam.target = target + Vector3(0.0f, -0.10f, -0.20f);
  }

  cam.up = Vector3(0.0f, 1.0f, 0.0f);
  cam.markDirty();
}

static void updateScene(Renderer &r, float dt)
{
  g_demoTime += dt;
  if (g_demoTime >= DEMO_DURATION)
    g_demoTime = fmodf(g_demoTime, DEMO_DURATION);

  updateCarRig(g_demoTime);
  updateEnvironment(g_demoTime);
  updateCamera(r, g_demoTime);
  updateLightingAndSky(r, g_demoTime);
}

static void renderWorld(Renderer &r)
{
  for (int i = 0; i < GROUND_SEGMENT_COUNT; ++i)
  {
    if (!g_groundPanels[i])
      continue;
    r.drawMeshInstance(g_groundPanels[i]);
    r.drawMeshInstanceShadow(g_groundPanels[i]);
  }

  const std::vector<MeshInstance *> &all = g_instances.all();
  for (size_t i = 0; i < all.size(); ++i)
  {
    MeshInstance *inst = all[i];
    if (!inst || !inst->isVisible())
      continue;

    bool isGroundPanel = false;
    for (int panel = 0; panel < GROUND_SEGMENT_COUNT; ++panel)
    {
      if (inst == g_groundPanels[panel])
      {
        isGroundPanel = true;
        break;
      }
    }
    if (isGroundPanel)
      continue;

    r.drawMeshInstance(inst);
    r.drawMeshInstanceShadow(inst);
  }
}

static void drawHud(Renderer &r)
{
  char buf[64];
  char fpsBuf[16];
  char avgBuf[16];
  char timeBuf[16];
  uint16_t y = 4;
  const uint16_t line = 9;

  auto fixedTenths = [](float value, char *out, size_t outSize) {
    long scaled = static_cast<long>(value * 10.0f + (value >= 0.0f ? 0.5f : -0.5f));
    long whole = scaled / 10;
    long frac = scaled >= 0 ? (scaled % 10) : -(scaled % 10);
    snprintf(out, outSize, "%ld.%ld", whole, frac);
  };

  r.drawText(4, y, "PIP3D CAR DEMO", Color::fromRGB888(255, 238, 180));
  y += line;

  snprintf(buf, sizeof(buf), "%s", getSectionName(g_demoTime));
  r.drawText(4, y, buf, Color::fromRGB888(150, 240, 255));
  y += line;

  fixedTenths(r.getFPS(), fpsBuf, sizeof(fpsBuf));
  fixedTenths(r.getAverageFPS(), avgBuf, sizeof(avgBuf));
  snprintf(buf, sizeof(buf), "FPS %s AVG %s", fpsBuf, avgBuf);
  r.drawText(4, y, buf, Color::fromRGB888(255, 255, 0));
  y += line;

  snprintf(buf, sizeof(buf), "FT %lums", static_cast<unsigned long>(r.getFrameTime() / 1000u));
  r.drawText(4, y, buf, Color::fromRGB888(255, 255, 0));
  y += line;

  snprintf(buf, sizeof(buf), "RAM %lu", static_cast<unsigned long>(ESP.getFreeHeap()));
  r.drawText(4, y, buf, Color::fromRGB888(255, 255, 0));
  y += line;

  fixedTenths(g_demoTime, timeBuf, sizeof(timeBuf));
  snprintf(buf, sizeof(buf), "SHOT T %s", timeBuf);
  r.drawText(4, y, buf, Color::fromRGB888(130, 220, 255));
  y += line;

  snprintf(buf, sizeof(buf), "TRI %lu", static_cast<unsigned long>(r.getStatsTrianglesTotal()));
  r.drawText(4, y, buf, Color::fromRGB888(130, 220, 255));
}

void setup()
{
  Serial.begin(115200);

  Renderer &r = begin3D(320, 240, TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN, TFT_BL_PIN, 80000000);
  initCamera(r);
  initScene(r);

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
  updateScene(r, dt);

  for (int band = 0; band < SCREEN_BAND_COUNT; ++band)
  {
    r.beginFrameBand(band);
    renderWorld(r);
    r.drawSkyboxBackground();
    if (band == 0)
      drawHud(r);
    r.endFrameBand(band);
  }
}
