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

#include "Pip3D.hpp"

using namespace pip3D;

static const int8_t TFT_CS_PIN = 7;
static const int8_t TFT_DC_PIN = 8;
static const int8_t TFT_RST_PIN = -1;
static const int8_t TFT_BL_PIN = -1;

static constexpr float TRACK_LENGTH = 320.0f;
static constexpr float CAMERA_SPEED = 6.0f;
static constexpr float CAMERA_FAR = 220.0f;

enum ObstacleType
{
    OBS_GLASS_WALL = 0,
    OBS_SPINNING_CRYSTAL,
    OBS_SWINGING_PENDULUM,
    OBS_TARGET
};

struct Obstacle
{
    MeshInstance *instance = nullptr;
    Vector3 initialPos;
    Vector3 currentPos;
    ObstacleType type;
    float radius = 1.0f;
    float rotationSpeed = 0.0f;
    float timeOffset = 0.0f;
    Color color;
};

static InstanceManager g_instances;
static TimeOfDayController g_timeOfDay;

static Cube *g_cubeMesh = nullptr;
static Sphere *g_sphereMesh = nullptr;
static Cylinder *g_cylinderMesh = nullptr;
static Cone *g_coneMesh = nullptr;
static Pyramid *g_pyramidMesh = nullptr;
static TrefoilKnot *g_knotMesh = nullptr;
static Plane *g_planeMesh = nullptr; // <--- Добавили указатель на плоскость

static std::vector<MeshInstance *> g_trackParts;
static std::vector<MeshInstance *> g_arches;
static std::vector<Obstacle> g_obstacles;

static float g_demoTime = 0.0f;
static uint32_t g_lastMs = 0;

static float hash01(int seed)
{
    float x = sinf(seed * 12.9898f + 78.233f) * 43758.5453f;
    return x - floorf(x);
}

static float saturate(float v)
{
    return pip3D::clamp(v, 0.0f, 1.0f);
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

static void initMeshes()
{
    if (!g_cubeMesh)
        g_cubeMesh = new Cube(1.0f, Color::WHITE);
    if (!g_sphereMesh)
        g_sphereMesh = new Sphere(0.5f, 12, 10, Color::WHITE);
    if (!g_cylinderMesh)
        g_cylinderMesh = new Cylinder(0.5f, 1.0f, 12, Color::WHITE);
    if (!g_coneMesh)
        g_coneMesh = new Cone(0.5f, 1.0f, 12, Color::WHITE);
    if (!g_pyramidMesh)
        g_pyramidMesh = new Pyramid(1.0f, Color::WHITE);
    if (!g_knotMesh)
        g_knotMesh = new TrefoilKnot(0.5f, 32, 8, Color::WHITE);
    if (!g_planeMesh)
        g_planeMesh = new Plane(1.0f, 1.0f, 1, Color::WHITE); // <--- Инициализация плоскости
}

static void buildTrackAndArches()
{
    g_trackParts.clear();
    g_arches.clear();

    const int numSections = static_cast<int>(TRACK_LENGTH / 15.0f);

    const Color floorColor = Color::fromRGB888(45, 52, 64);
    const Color archColor = Color::fromRGB888(120, 130, 145);

    for (int i = 0; i < numSections; ++i)
    {
        float z = i * 15.0f;

        // Центральный продолговатый пол (теперь используем g_planeMesh)
        MeshInstance *floor = g_instances.create(g_planeMesh);
        floor->setPosition(0.0f, 0.0f, z + 7.5f); // Y = 0.0f (плоскость не имеет толщины)
        floor->setScale(10.0f, 1.0f, 15.1f);      // X - ширина, Z - длина
        floor->setColor(floorColor);
        g_trackParts.push_back(floor);

        // Расставляем П-образные арки через каждые 30 метров
        if (i % 2 == 0 && z < TRACK_LENGTH - 10.0f)
        {
            // Левая колонна рамы
            MeshInstance *leftPillar = g_instances.create(g_cubeMesh);
            leftPillar->setPosition(-4.8f, 2.5f, z);
            leftPillar->setScale(0.4f, 5.0f, 0.4f);
            leftPillar->setColor(archColor);
            g_arches.push_back(leftPillar);

            // Правая колонна рамы
            MeshInstance *rightPillar = g_instances.create(g_cubeMesh);
            rightPillar->setPosition(4.8f, 2.5f, z);
            rightPillar->setScale(0.4f, 5.0f, 0.4f);
            rightPillar->setColor(archColor);
            g_arches.push_back(rightPillar);

            // Верхняя перекладина буквы П
            MeshInstance *topBar = g_instances.create(g_cubeMesh);
            topBar->setPosition(0.0f, 5.0f, z);
            topBar->setScale(10.0f, 0.4f, 0.4f);
            topBar->setColor(archColor);
            g_arches.push_back(topBar);
        }
    }
}

static void buildObstacles()
{
    g_obstacles.clear();

    for (float z = 20.0f; z < TRACK_LENGTH - 20.0f; z += 15.0f)
    {
        Obstacle obs;
        obs.timeOffset = z * 0.17f;

        // Распределяем объекты по бокам (чередуя левую и правую обочины)
        // Чтобы камера (идущая в пределах X = [-1.2, 1.2]) никогда не врезалась в них
        bool isLeft = (static_cast<int>(z / 15.0f) % 2 == 0);
        obs.initialPos = Vector3(isLeft ? -3.6f : 3.6f, 1.5f, z);
        obs.currentPos = obs.initialPos;

        int pattern = static_cast<int>(z / 15.0f) % 4;

        if (pattern == 0)
        {
            // Кристалл на обочине
            obs.type = OBS_SPINNING_CRYSTAL;
            obs.radius = 1.0f;
            obs.rotationSpeed = 100.0f + hash01(static_cast<int>(z)) * 80.0f;
            obs.color = Color::fromRGB888(255, 92, 160);

            obs.instance = g_instances.create(g_pyramidMesh);
            obs.instance->setPosition(obs.currentPos);
            obs.instance->setScale(1.5f, 1.5f, 1.5f);
            obs.instance->setColor(obs.color);
        }
        else if (pattern == 1)
        {
            // Вращающийся трефовый узел
            obs.type = OBS_TARGET;
            obs.radius = 1.0f;
            obs.rotationSpeed = 80.0f;
            obs.color = Color::fromRGB888(255, 135, 30);

            obs.instance = g_instances.create(g_knotMesh);
            obs.instance->setPosition(obs.currentPos);
            obs.instance->setScale(1.2f, 1.2f, 1.2f);
            obs.instance->setColor(obs.color);
        }
        else if (pattern == 2)
        {
            // Быстро крутящийся конус
            obs.type = OBS_SPINNING_CRYSTAL;
            obs.radius = 1.0f;
            obs.rotationSpeed = 140.0f;
            obs.color = Color::fromRGB888(50, 200, 120);

            obs.instance = g_instances.create(g_coneMesh);
            obs.instance->setPosition(obs.currentPos);
            obs.instance->setScale(1.2f, 1.6f, 1.2f);
            obs.instance->setColor(obs.color);
        }
        else
        {
            // Парящая сфера, плавно покачивающаяся вверх-вниз
            obs.type = OBS_GLASS_WALL;
            obs.radius = 1.0f;
            obs.color = Color::fromRGB888(100, 190, 255);

            obs.instance = g_instances.create(g_sphereMesh);
            obs.instance->setPosition(obs.currentPos);
            obs.instance->setScale(1.4f, 1.4f, 1.4f);
            obs.instance->setColor(obs.color);
        }

        g_obstacles.push_back(obs);
    }
}

static void resetAllObstacles()
{
    for (auto &obs : g_obstacles)
    {
        if (obs.instance)
            obs.instance->show();
    }
}

static void updateScene(Renderer &r, float dt)
{
    g_demoTime += dt;

    // Плавное обновление положения солнца/времени суток
    g_timeOfDay.update(dt);

    float cameraZ = g_demoTime * CAMERA_SPEED;
    if (cameraZ >= TRACK_LENGTH)
    {
        g_demoTime = 0.0f;
        cameraZ = 0.0f;
        resetAllObstacles();
    }

    // Траектория движения камеры: легкое смещение влево/вправо (X в диапазоне [-1.2, 1.2])
    const Vector3 camPos = Vector3(sinf(g_demoTime * 0.4f) * 1.2f, 2.0f + cosf(g_demoTime * 0.8f) * 0.2f, cameraZ);
    const Vector3 targetLook = camPos + Vector3(0.0f, -0.1f, 15.0f);

    // Обновление анимаций объектов на обочине
    for (auto &obs : g_obstacles)
    {
        if (obs.type == OBS_SPINNING_CRYSTAL)
        {
            float angle = g_demoTime * obs.rotationSpeed;
            obs.instance->setEuler(angle, angle * 0.5f, 0.0f);
        }
        else if (obs.type == OBS_TARGET)
        {
            float angle = g_demoTime * obs.rotationSpeed;
            obs.instance->setEuler(angle, angle * 0.7f, angle * 0.3f);
        }
        else if (obs.type == OBS_GLASS_WALL)
        {
            // Плавное парение вверх/вниз
            float hover = sinf((g_demoTime + obs.timeOffset) * 2.0f) * 0.4f;
            Vector3 p = obs.initialPos;
            p.y += hover;
            obs.instance->setPosition(p);
            obs.instance->setEuler(0.0f, g_demoTime * 45.0f, 0.0f);
        }
    }

    Camera &cam = r.getCamera();
    cam.position = camPos;
    cam.target = targetLook;
    cam.up = Vector3(0.0f, 1.0f, 0.0f);
    cam.setPerspective(68.0f, 0.1f, CAMERA_FAR);
    cam.markDirty();
}

static void renderWorld(Renderer &r)
{
    // 1. Отрисовка продолговатого дорожного полотна
    for (MeshInstance *inst : g_trackParts)
    {
        if (inst->isVisible())
        {
            r.drawMeshInstanceStatic(inst);
            r.drawMeshInstanceShadow(inst);
        }
    }

    // 2. Отрисовка П-образных арок и их теней
    for (MeshInstance *inst : g_arches)
    {
        if (inst->isVisible())
        {
            r.drawMeshInstanceStatic(inst);
            r.drawMeshInstanceShadow(inst);
        }
    }

    // 3. Отрисовка боковых вращающихся примитивов и их теней
    for (const auto &obs : g_obstacles)
    {
        if (obs.instance && obs.instance->isVisible())
        {
            r.drawMeshInstance(obs.instance);
            r.drawMeshInstanceShadow(obs.instance);
        }
    }
}

static void drawHud(Renderer &r)
{
    const float intro = 1.0f - smoother(sectionT(g_demoTime, 2.0f, 7.0f));
    if (intro > 0.05f)
    {
        r.drawText(12, 14, "GEOMETRIC ARCHWAY", mixColor565(Color::fromRGB888(70, 80, 105), Color::fromRGB888(245, 235, 220), intro));
        r.drawText(12, 24, "3d shadow projection test", mixColor565(Color::fromRGB888(40, 55, 90), Color::fromRGB888(165, 220, 255), intro));
    }

    // Мониторинг производительности
    char buf[64];

    // 1. Текущий и средний FPS + время кадра в миллисекундах
    snprintf(buf, sizeof(buf), "FPS: %.1f (%.1f ms)", r.getAverageFPS(), r.getFrameTime() / 1000.0f);
    r.drawText(200, 12, buf, Color::GREEN);

    // 2. Отрисованные треугольники (всего выслано минус отсеченные Backface Culling)
    uint32_t activeTris = r.getStatsTrianglesTotal() - r.getStatsTrianglesBackfaceCulled();
    snprintf(buf, sizeof(buf), "Tris: %lu (BFC: %lu)", (unsigned long)activeTris, (unsigned long)r.getStatsTrianglesBackfaceCulled());
    r.drawText(12, 216, buf, Color::YELLOW);

    // 3. Статистика инстансов (всего на сцене против отсеченных Frustum Culling)
    snprintf(buf, sizeof(buf), "Inst: %lu (Cull: %lu)", (unsigned long)r.getStatsInstancesTotal(), (unsigned long)r.getStatsInstancesFrustumCulled());
    r.drawText(12, 226, buf, Color::CYAN);
}

void setup()
{
    Serial.begin(115200);
    Renderer &r = begin3D(SCREEN_WIDTH, SCREEN_HEIGHT, TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN, TFT_BL_PIN, 80000000);
    if (!r.isInitialized())
    {
        Serial.println("Pip3D init failed");
        for (;;)
            delay(1000);
    }
    Camera &cam = r.getCamera();
    cam.position = Vector3(0.0f, 2.0f, -10.0f);
    cam.target = Vector3(0.0f, 1.9f, 10.0f);
    cam.up = Vector3(0.0f, 1.0f, 0.0f);
    cam.setPerspective(68.0f, 0.1f, CAMERA_FAR);
    cam.markDirty();

    initMeshes();
    buildTrackAndArches();
    buildObstacles();

    // Запуск цикла времени суток (динамическое перемещение солнца и смена оттенков неба)
    TimeOfDayConfig todCfg;
    todCfg.dayLengthSeconds = 40.0f; // День/ночь сменяются каждые 40 секунд
    todCfg.startHour = 6.0f;         // Старт на рассвете
    todCfg.baseIntensity = 1.2f;
    todCfg.nightIntensity = 0.25f;
    todCfg.autoAdvance = true;
    g_timeOfDay.init(&r, todCfg);

    r.setBackfaceCullingEnabled(true);

    // Настройки теней
    r.setShadowsEnabled(true);
    r.setShadowPlaneY(0.0f); // Тень ложится ровно на плоскость пола
    r.setShadowOpacity(0.35f);
    r.setShadowColor(Color::fromRGB888(20, 24, 32));

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
        renderWorld(r);

        drawHud(r);

        r.endFrameBand(band);
    }
}