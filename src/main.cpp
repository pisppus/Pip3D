#include <Arduino.h>
#include <algorithm>
#include <array>
#include <vector>
#include <math.h>
#include <stdio.h>
#if defined(PIP3D_PC)
#include <PipCore/Platforms/Desktop/Runtime.hpp>
#endif

#define TFT_MOSI 6
#define TFT_MISO -1
#define TFT_SCLK 5

#include "Pip3D.h"

using namespace pip3D;

static const int8_t TFT_CS_PIN = 7;
static const int8_t TFT_DC_PIN = 8;
static const int8_t TFT_RST_PIN = -1;
static const int8_t TFT_BL_PIN = -1;

static constexpr float TRACK_LENGTH = 320.0f;
static constexpr float CAMERA_SPEED = 6.0f;
static constexpr float CAMERA_FAR = 220.0f;

// Типы геометрических объектов
enum ObstacleType {
    OBS_GLASS_WALL = 0,
    OBS_SPINNING_CRYSTAL,
    OBS_SWINGING_PENDULUM,
    OBS_TARGET
};

struct Obstacle {
    MeshInstance* instance = nullptr;
    MeshInstance* subInstance = nullptr; // для составных частей маятника
    Vector3 initialPos;
    Vector3 currentPos;
    ObstacleType type;
    float radius = 1.0f;
    float rotationSpeed = 0.0f;
    float swingSpeed = 0.0f;
    float swingRange = 0.0f;
    float timeOffset = 0.0f;
    Color color;
};

static InstanceManager g_instances;
static TimeOfDayController g_timeOfDay;

static Cube* g_cubeMesh = nullptr;
static Sphere* g_sphereMesh = nullptr;
static Cylinder* g_cylinderMesh = nullptr;
static Cone* g_coneMesh = nullptr;
static Pyramid* g_pyramidMesh = nullptr;
static TrefoilKnot* g_knotMesh = nullptr;

static std::vector<MeshInstance*> g_corridorParts;
static std::vector<Obstacle> g_obstacles;

static float g_demoTime = 0.0f;
static uint32_t g_lastMs = 0;

static float hash01(int seed) {
    float x = sinf(seed * 12.9898f + 78.233f) * 43758.5453f;
    return x - floorf(x);
}

static float saturate(float v) {
    return pip3D::clamp(v, 0.0f, 1.0f);
}

static float smoother(float t) {
    t = saturate(t);
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

static float sectionT(float t, float a, float b) {
    if (b <= a)
        return 0.0f;
    return saturate((t - a) / (b - a));
}

static Color mixColor565(const Color &a, const Color &b, float t) {
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

static void initMeshes() {
    if (!g_cubeMesh) g_cubeMesh = new Cube(1.0f, Color::WHITE);
    if (!g_sphereMesh) g_sphereMesh = new Sphere(0.5f, 12, 10, Color::WHITE);
    if (!g_cylinderMesh) g_cylinderMesh = new Cylinder(0.5f, 1.0f, 12, Color::WHITE);
    if (!g_coneMesh) g_coneMesh = new Cone(0.5f, 1.0f, 12, Color::WHITE);
    if (!g_pyramidMesh) g_pyramidMesh = new Pyramid(1.0f, Color::WHITE);
    if (!g_knotMesh) g_knotMesh = new TrefoilKnot(0.5f, 32, 8, Color::WHITE);
}

static void buildCorridor() {
    g_corridorParts.clear();
    
    const int numSections = static_cast<int>(TRACK_LENGTH / 15.0f);
    
    const Color floorColor = Color::fromRGB888(45, 52, 64);
    const Color wallColor = Color::fromRGB888(26, 28, 34);
    const Color pillarColor = Color::fromRGB888(80, 90, 105);
    const Color archColor = Color::fromRGB888(115, 125, 140);

    for (int i = 0; i < numSections; ++i) {
        float z = i * 15.0f;

        // Пол
        MeshInstance* floor = g_instances.create(g_cubeMesh);
        floor->setPosition(0.0f, -0.1f, z + 7.5f);
        floor->setScale(10.0f, 0.2f, 15.1f);
        floor->setColor(floorColor);
        g_corridorParts.push_back(floor);

        // Потолок
        MeshInstance* ceiling = g_instances.create(g_cubeMesh);
        ceiling->setPosition(0.0f, 5.1f, z + 7.5f);
        ceiling->setScale(10.0f, 0.2f, 15.1f);
        ceiling->setColor(floorColor);
        g_corridorParts.push_back(ceiling);

        // Левая стена
        MeshInstance* leftWall = g_instances.create(g_cubeMesh);
        leftWall->setPosition(-5.1f, 2.5f, z + 7.5f);
        leftWall->setScale(0.2f, 5.0f, 15.1f);
        leftWall->setColor(wallColor);
        g_corridorParts.push_back(leftWall);

        // Правая стена
        MeshInstance* rightWall = g_instances.create(g_cubeMesh);
        rightWall->setPosition(5.1f, 2.5f, z + 7.5f);
        rightWall->setScale(0.2f, 5.0f, 15.1f);
        rightWall->setColor(wallColor);
        g_corridorParts.push_back(rightWall);

        // Колонны (пилоны)
        MeshInstance* leftPillar = g_instances.create(g_cylinderMesh);
        leftPillar->setPosition(-4.8f, 2.5f, z);
        leftPillar->setScale(0.35f, 5.0f, 0.35f);
        leftPillar->setColor(pillarColor);
        g_corridorParts.push_back(leftPillar);

        MeshInstance* rightPillar = g_instances.create(g_cylinderMesh);
        rightPillar->setPosition(4.8f, 2.5f, z);
        rightPillar->setScale(0.35f, 5.0f, 0.35f);
        rightPillar->setColor(pillarColor);
        g_corridorParts.push_back(rightPillar);

        // Арки сверху
        MeshInstance* arch = g_instances.create(g_cylinderMesh);
        arch->setPosition(0.0f, 4.9f, z);
        arch->setScale(9.6f, 0.25f, 0.25f);
        arch->setEuler(0.0f, 0.0f, 90.0f);
        arch->setColor(archColor);
        g_corridorParts.push_back(arch);
    }
}

static void buildObstacles() {
    g_obstacles.clear();
    
    for (float z = 25.0f; z < TRACK_LENGTH - 20.0f; z += 20.0f) {
        Obstacle obs;
        obs.initialPos = Vector3(0.0f, 0.0f, z);
        obs.currentPos = obs.initialPos;
        obs.timeOffset = z * 0.17f;

        int pattern = static_cast<int>(z / 20.0f) % 4;
        
        if (pattern == 0) {
            // Стеклянная стена
            obs.type = OBS_GLASS_WALL;
            obs.initialPos.y = 2.0f;
            obs.currentPos = obs.initialPos;
            obs.radius = 1.6f;
            obs.color = Color::fromRGB888(100, 190, 255);
            
            obs.instance = g_instances.create(g_cubeMesh);
            obs.instance->setPosition(obs.currentPos);
            obs.instance->setScale(3.2f, 3.8f, 0.15f);
            obs.instance->setColor(obs.color);
        }
        else if (pattern == 1) {
            // Вращающийся кристалл
            obs.type = OBS_SPINNING_CRYSTAL;
            obs.initialPos.y = 2.2f;
            obs.currentPos = obs.initialPos;
            obs.radius = 1.3f;
            obs.rotationSpeed = 120.0f + hash01(static_cast<int>(z)) * 100.0f;
            obs.color = Color::fromRGB888(255, 92, 160);
            
            obs.instance = g_instances.create(g_pyramidMesh);
            obs.instance->setPosition(obs.currentPos);
            obs.instance->setScale(1.8f, 1.8f, 1.8f);
            obs.instance->setColor(obs.color);
        }
        else if (pattern == 2) {
            // Маятник
            obs.type = OBS_SWINGING_PENDULUM;
            obs.initialPos.y = 4.8f;
            obs.currentPos = obs.initialPos;
            obs.radius = 1.2f;
            obs.swingSpeed = 2.2f;
            obs.swingRange = 3.2f;
            obs.color = Color::fromRGB888(90, 105, 140);
            
            // Стержень маятника
            obs.instance = g_instances.create(g_cylinderMesh);
            obs.instance->setPosition(obs.currentPos);
            obs.instance->setScale(0.2f, 3.5f, 0.2f);
            obs.instance->setColor(obs.color);

            // Шар на конце маятника
            obs.subInstance = g_instances.create(g_sphereMesh);
            obs.subInstance->setPosition(obs.currentPos + Vector3(0.0f, -1.8f, 0.0f));
            obs.subInstance->setScale(0.85f, 0.85f, 0.85f);
            obs.subInstance->setColor(Color::fromRGB888(255, 195, 75));
        }
        else {
            // Мишень
            obs.type = OBS_TARGET;
            obs.initialPos.y = 2.5f + sinf(z) * 0.8f;
            obs.initialPos.x = (hash01(static_cast<int>(z)) > 0.5f ? 1.0f : -1.0f) * 2.2f;
            obs.currentPos = obs.initialPos;
            obs.radius = 0.9f;
            obs.rotationSpeed = 85.0f;
            obs.color = Color::fromRGB888(255, 135, 30);
            
            obs.instance = g_instances.create(g_knotMesh);
            obs.instance->setPosition(obs.currentPos);
            obs.instance->setScale(1.3f, 1.3f, 1.3f);
            obs.instance->setColor(obs.color);
        }

        g_obstacles.push_back(obs);
    }
}

static void resetAllObstacles() {
    for (auto& obs : g_obstacles) {
        if (obs.instance) obs.instance->show();
        if (obs.subInstance) obs.subInstance->show();
    }
}

static void updateScene(Renderer& r, float dt) {
    g_demoTime += dt;
    
    // Плавная авто-смена дня и ночи
    g_timeOfDay.update(dt);

    float cameraZ = g_demoTime * CAMERA_SPEED;
    if (cameraZ >= TRACK_LENGTH) {
        g_demoTime = 0.0f;
        cameraZ = 0.0f;
        resetAllObstacles();
    }

    // Траектория движения камеры вперед
    const Vector3 camPos = Vector3(sinf(g_demoTime * 0.45f) * 1.6f, 2.2f + cosf(g_demoTime * 0.75f) * 0.25f, cameraZ);
    const Vector3 targetLook = camPos + Vector3(0.0f, -0.1f, 15.0f);

    // Анимации всех элементов
    for (auto& obs : g_obstacles) {
        if (obs.type == OBS_GLASS_WALL) {
            // Стеклянные перегородки плавно покачиваются
            float angle = sinf((g_demoTime + obs.timeOffset) * 0.8f) * 15.0f;
            obs.instance->setEuler(0.0f, angle, 0.0f);
        }
        else if (obs.type == OBS_SPINNING_CRYSTAL) {
            float angle = g_demoTime * obs.rotationSpeed;
            obs.instance->setEuler(angle, angle * 0.5f, 0.0f);
        }
        else if (obs.type == OBS_SWINGING_PENDULUM) {
            float angle = sinf((g_demoTime + obs.timeOffset) * obs.swingSpeed);
            float xOffset = angle * obs.swingRange;
            
            // Стержень маятника
            obs.currentPos = obs.initialPos + Vector3(xOffset * 0.5f, -1.75f, 0.0f);
            obs.instance->setPosition(obs.currentPos);
            obs.instance->setEuler(0.0f, 0.0f, angle * 35.0f);

            // Тяжелый шар маятника
            Vector3 bobOffset = Vector3(sinf(angle * 35.0f * DEG2RAD) * -1.8f, cosf(angle * 35.0f * DEG2RAD) * -1.8f, 0.0f);
            obs.subInstance->setPosition(obs.currentPos + bobOffset);
        }
        else if (obs.type == OBS_TARGET) {
            float angle = g_demoTime * obs.rotationSpeed;
            obs.instance->setEuler(angle, angle * 0.7f, angle * 0.3f);
        }
    }

    Camera& cam = r.getCamera();
    cam.position = camPos;
    cam.target = targetLook;
    cam.up = Vector3(0.0f, 1.0f, 0.0f);
    cam.setPerspective(68.0f, 0.1f, CAMERA_FAR);
    cam.markDirty();
}

static void renderWorld(Renderer& r) {
    // 1. Отрисовка структуры коридора + проецирование теней на пол
    for (MeshInstance* inst : g_corridorParts) {
        if (inst->isVisible()) {
            r.drawMeshInstanceStatic(inst);
            r.drawMeshInstanceShadow(inst);
        }
    }

    // 2. Отрисовка вращающихся препятствий + проецирование их теней
    for (const auto& obs : g_obstacles) {
        if (obs.instance && obs.instance->isVisible()) {
            r.drawMeshInstance(obs.instance);
            r.drawMeshInstanceShadow(obs.instance);
        }
        if (obs.subInstance && obs.subInstance->isVisible()) {
            r.drawMeshInstance(obs.subInstance);
            r.drawMeshInstanceShadow(obs.subInstance);
        }
    }
}

static void drawHud(Renderer& r) {
    const float intro = 1.0f - smoother(sectionT(g_demoTime, 2.0f, 7.0f));
    if (intro > 0.05f) {
        r.drawText(12, 14, "SKYLINE PASS", mixColor565(Color::fromRGB888(70, 80, 105), Color::fromRGB888(245, 235, 220), intro));
        r.drawText(12, 24, "city flight cinematic", mixColor565(Color::fromRGB888(40, 55, 90), Color::fromRGB888(165, 220, 255), intro));
    }
}

void setup() {
    Serial.begin(115200);
    Renderer &r = begin3D(SCREEN_WIDTH, SCREEN_HEIGHT, TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN, TFT_BL_PIN, 60000000);
    if (!r.isInitialized()) {
        Serial.println("Pip3D init failed");
        for (;;)
            delay(1000);
    }
    Camera &cam = r.getCamera();
    cam.position = Vector3(0.0f, 2.2f, -10.0f);
    cam.target = Vector3(0.0f, 2.1f, 10.0f);
    cam.up = Vector3(0.0f, 1.0f, 0.0f);
    cam.setPerspective(68.0f, 0.1f, CAMERA_FAR);
    cam.markDirty();
    
    initMeshes();
    buildCorridor();
    buildObstacles();

    // Запуск цикла времени суток
    TimeOfDayConfig todCfg;
    todCfg.dayLengthSeconds = 40.0f; // День/ночь за 40 секунд
    todCfg.startHour = 6.0f;        // Рассвет
    todCfg.baseIntensity = 1.2f;
    todCfg.nightIntensity = 0.25f;
    todCfg.autoAdvance = true;
    g_timeOfDay.init(&r, todCfg);

    r.setBackfaceCullingEnabled(true);
    
    // ВКЛЮЧАЕМ тени и настраиваем плоскость проекции теней (на уровне пола)
    r.setShadowsEnabled(true); 
    r.setShadowPlaneY(0.0f);
    r.setShadowOpacity(0.35f);
    r.setShadowColor(Color::fromRGB888(20, 24, 32));
    
    g_lastMs = millis();
    g_demoTime = 0.0f;
}

void loop() {
    uint32_t now = millis();
    uint32_t dtMs = now - g_lastMs;
    g_lastMs = now;
    float dt = dtMs * 0.001f;
    if (dt > 0.05f)
        dt = 0.05f;
        
    Renderer &r = renderer();
    if (!r.isInitialized()) {
        delay(100);
        return;
    }
    
    updateScene(r, dt);
    
    for (int band = 0; band < SCREEN_BAND_COUNT; ++band) {
        r.beginFrameBand(band);
        r.drawSkyboxBackground();
        renderWorld(r);
        if (band == 0) {
            drawHud(r);
        }
        r.endFrameBand(band);
    }
}