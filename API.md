# Pip3D API

Этот файл описывает актуальный публичный API `Pip3D`, который пользователь может использовать в своих проектах через `#include "Pip3D.h"`.

Здесь intentionally не описаны внутренности движка, которые не нужны внешнему коду:

- dirty regions
- band-local внутреннее состояние
- rasterizer internals
- internal helpers рендера
- debug/internal infrastructure, которая не нужна для обычного использования

---

# 1. Требования к компиляции

`Pip3D` использует C++17.

Для PlatformIO:

```ini
build_unflags =
    -std=gnu++11
build_flags =
    -std=gnu++17
```

---

# 2. Build-time конфигурация

Основные compile-time флаги для `Pip3D` и нижнего слоя `PipCore`:

- `PIP3D_SCREEN_WIDTH`
  - логическая ширина экрана
- `PIP3D_SCREEN_HEIGHT`
  - логическая высота экрана
- `PIP3D_SCREEN_BAND_COUNT`
  - количество горизонтальных band'ов для banded rendering
- `PIPCORE_PLATFORM`
  - платформа `PipCore`
  - пример: `ESP32`
- `PIPCORE_DISPLAY`
  - драйвер дисплея
  - пример: `ILI9488`, `ST7789`
- `PIPCORE_ENABLE_PREFS`
  - `0` или `1`
- `PIPCORE_ENABLE_WIFI`
  - `0` или `1`
- `PIPCORE_ENABLE_OTA`
  - `0` или `1`
- `PIPCORE_OTA_PROJECT_URL`
  - URL для OTA backend, если OTA реально используется

Пример из проекта через `platformio.ini`:

```ini
build_flags =
    -std=gnu++17
    -Ilib/Pip3D
    -Ilib/Pip3D/Pip3D
    -Iinclude
    -DPIP3D_SCREEN_WIDTH=480
    -DPIP3D_SCREEN_HEIGHT=320
    -DPIP3D_SCREEN_BAND_COUNT=2
    -DPIPCORE_PLATFORM=ESP32
    -DPIPCORE_DISPLAY=ILI9488
```

То же самое можно задавать через `include/config.hpp`, если тебе так удобнее держать project-level конфиг.

Что важно:

- `PIP3D_SCREEN_WIDTH` и `PIP3D_SCREEN_HEIGHT` это не косметика, а базовые compile-time константы движка
- runtime `begin3D(width, height, ...)` не должен противоречить этим макросам
- если экран у проекта `480x320`, то и compile-time флаги должны быть `480x320`
- `PIP3D_SCREEN_BAND_COUNT=2` или больше уменьшает память под framebuffer/z-buffer, но усложняет pipeline
- для обычных проектов разумный старт это `2`
- optional-модули `PipCore` по умолчанию выключены, и внешний код не должен использовать Wi-Fi / OTA API без явного включения флагов

---

# 3. Подключение и точка входа

Основное подключение:

```cpp
#include "Pip3D.h"
using namespace pip3D;
```

Главные публичные entry points:

- `const char* getVersion()`
- `Color RGB888(uint8_t r, uint8_t g, uint8_t b)`
- `Renderer& renderer()`
- `Renderer& begin3D(uint16_t width, uint16_t height, int8_t cs, int8_t dc, int8_t rst, int8_t bl = -1, uint32_t spi_freq = 80000000)`
- `Renderer& begin3D(int8_t cs, int8_t dc, int8_t rst, int8_t bl = -1, uint32_t spi_freq = 80000000)`

Пример:

```cpp
Renderer& r = begin3D(480, 320, 10, 9, 8, -1, 80000000);
```

Что делает `begin3D(...)`:

- инициализирует singleton-рендерер
- настраивает display/framebuffer/z-buffer
- включает `SKYBOX_DAY`
- включает тени
- ставит shadow plane на `Y = 0`

Что важно:

- overload `begin3D(cs, dc, rst, ...)` использует дефолт `320x240`
- если твой проект не `320x240`, используй overload с явными `width` и `height`
- `renderer()` возвращает глобальный singleton `Renderer`

---

# 4. Базовые типы

## 4.1. `Vector3`

Главный векторный тип движка:

```cpp
Vector3 p(0.0f, 2.0f, -5.0f);
```

Поддерживает:

- `+`, `-`, `*`
- `+=`, `-=`, `*=`
- `normalize()`
- `length()`
- `lengthSquared()`
- `dot(...)`
- `cross(...)`

## 4.2. `Color`

Главный цветовой тип движка, хранится в `RGB565`.

Основные способы создания:

```cpp
Color a = Color::rgb(255, 180, 120);
Color b = Color::fromRGB888(255, 180, 120);
Color c = RGB888(255, 180, 120);
Color d = Color::hsv(0.08f, 0.7f, 1.0f);
Color e = Color::fromTemperature(5500.0f);
```

Полезные методы:

- `blend(...)`
- `darken(...)`
- `lighten(...)`
- `brightness()`

## 4.3. `DisplayConfig`

Публичный alias для display config:

```cpp
DisplayConfig cfg(480, 320, 10, 9, 8);
cfg.bl = -1;
cfg.spi_freq = 80000000;
```

Обычно внешний код не создает `DisplayConfig` вручную, а использует `begin3D(...)`.

## 4.4. `Skybox` и `SkyboxType`

Публичные skybox preset'ы:

- `SKYBOX_DAY`
- `SKYBOX_SUNSET`
- `SKYBOX_NIGHT`
- `SKYBOX_DAWN`
- `SKYBOX_OVERCAST`
- `SKYBOX_CUSTOM`

Для custom skybox:

```cpp
r.getSkybox().setCustom(
    Color::fromRGB888(20, 60, 120),
    Color::fromRGB888(180, 210, 255),
    Color::fromRGB888(70, 80, 95)
);
```

---

# 5. `Renderer`

`Renderer` это главный публичный API движка.

## 5.1. Инициализация

Обычно так:

```cpp
Renderer& r = begin3D(480, 320, 10, 9, 8);
```

Либо вручную:

```cpp
Renderer r;
DisplayConfig cfg(480, 320, 10, 9, 8);
r.init(cfg);
```

## 5.2. Кадр

Базовый frame lifecycle:

```cpp
r.beginFrame();

// draw calls

r.endFrame();
```

Для большинства проектов этого достаточно.

Advanced API:

- `beginFrameBand(int bandIndex)`
- `endFrameBand(int bandIndex)`

Эти методы нужны только если ты сам осознанно управляешь banded rendering. Для обычного пользовательского кода лучше их не трогать.

## 5.3. Камера

Основные методы:

- `Camera& getCamera()`
- `Camera& getCamera(int index)`
- `int createCamera()`
- `void setActiveCamera(int index)`
- `int getActiveCameraIndex()`
- `int getCameraCount()`

## 5.4. Рисование геометрии

Основные методы:

- `drawMesh(Mesh* mesh)`
- `drawMesh(Mesh* mesh, ShadingMode mode)`
- `drawMeshInstance(MeshInstance* instance)`
- `drawMeshInstanceStatic(MeshInstance* instance)`
- `drawInstances(InstanceManager& manager)`
- `drawTriangle3D(...)`
- `project(const Vector3& world)`

Пример:

```cpp
Cube cube(1.0f, Color::fromRGB888(220, 220, 220));
cube.setPosition(0.0f, 0.5f, 4.0f);

r.beginFrame();
r.drawMesh(&cube);
r.endFrame();
```

## 5.5. Свет

Основные методы:

- `setLight(int index, const Light& light)`
- `int addLight(const Light& light)`
- `removeLight(int index)`
- `Light* getLight(int index)`
- `clearLights()`
- `getLightCount()`
- `setMainDirectionalLight(...)`
- `setMainPointLight(...)`
- `setLightColor(...)`
- `setLightPosition(...)`
- `setLightDirection(...)`
- `setLightTemperature(...)`
- `setLightType(...)`
- `getLightColor()`

Пример directional light:

```cpp
r.setMainDirectionalLight(
    Vector3(-0.4f, -1.0f, -0.3f),
    Color::fromTemperature(5200.0f),
    1.0f
);
```

## 5.6. Тени

Публичные методы:

- `setShadowsEnabled(bool enabled)`
- `getShadowsEnabled()`
- `setShadowOpacity(float opacity)`
- `setShadowColor(const Color& color)`
- `setShadowPlane(const Vector3& normal, float distance)`
- `setShadowPlaneY(float y)`
- `ShadowSettings& getShadowSettings()`
- `drawMeshShadow(Mesh* mesh)`
- `drawMeshInstanceShadow(MeshInstance* instance)`

Типичный вариант:

```cpp
r.setShadowsEnabled(true);
r.setShadowPlaneY(0.0f);
r.setShadowOpacity(0.6f);
```

## 5.7. Skybox и фон

Публичные методы:

- `setSkyboxEnabled(bool enabled)`
- `isSkyboxEnabled()`
- `setSkyboxType(SkyboxType type)`
- `setSkyboxWithLighting(SkyboxType type)`
- `Skybox& getSkybox()`
- `setClearColor(Color color)`
- `drawSkyboxBackground()`

Что делает `setSkyboxWithLighting(...)`:

- меняет preset skybox
- подбирает соответствующую цветовую температуру света

## 5.8. HUD и overlay

Публичные методы:

- `drawText(...)`
- `drawTextAdaptive(...)`
- `getTextWidth(...)`
- `getAdaptiveTextColor(...)`

Пример:

```cpp
r.drawText(8, 8, "Pip3D demo", Color::WHITE);
r.drawTextAdaptive(8, 20, "FPS auto-color");
```

## 5.9. Диагностика

Полезные runtime-метрики:

- `getFPS()`
- `getAverageFPS()`
- `getFrameTime()`
- `getStatsTrianglesTotal()`
- `getStatsTrianglesBackfaceCulled()`
- `getStatsInstancesTotal()`
- `getStatsInstancesFrustumCulled()`
- `getStatsInstancesOcclusionCulled()`

## 5.10. Дополнительные флаги рендера

- `setBackfaceCullingEnabled(bool enabled)`
- `getBackfaceCullingEnabled()`
- `setOcclusionCullingEnabled(bool enabled)`
- `getOcclusionCullingEnabled()`
- `setDebugShowDirtyRegions(bool enabled)`
- `getDebugShowDirtyRegions()`
- `setShadingMode(ShadingMode mode)`
- `getShadingMode()`

Сейчас публично доступен один shading mode:

- `Renderer::SHADING_FLAT`

## 5.11. Визуальные helper'ы

Публичные helper-методы:

- `drawSunSprite(...)`
- `drawSunSpriteDirectional(...)`
- `drawWater(...)`

Это user-facing API, но это именно convenience effects, а не базовая часть пайплайна.

---

# 6. Камеры

## 6.1. `Camera`

Базовый тип камеры:

```cpp
Camera cam(
    Vector3(0, 2, -6),
    Vector3(0, 1, 0),
    Vector3(0, 1, 0)
);
```

Публичные поля:

- `position`
- `target`
- `up`
- `projectionType`
- `fov`
- `nearPlane`
- `farPlane`
- `orthoWidth`
- `orthoHeight`
- `fisheyeStrength`
- `config`
- `anim`

Поддерживаемые projection types:

- `PERSPECTIVE`
- `ORTHOGRAPHIC`
- `FISHEYE`

Основные методы:

- `setPerspective(...)`
- `setOrtho(...)`
- `setFisheye(...)`
- `forward()`
- `right()`
- `upVec()`
- `move(...)`
- `moveForward(...)`
- `moveBackward(...)`
- `moveRight(...)`
- `moveLeft(...)`
- `moveUp(...)`
- `moveDown(...)`
- `rotate(...)`
- `rotateDeg(...)`
- `rotateRad(...)`
- `lookAt(...)`
- `orbit(...)`
- `markDirty()`
- `getViewMatrix()`
- `getProjectionMatrix(float aspect)`
- `getViewProjectionMatrix(float aspect)`

Пример:

```cpp
Camera& cam = r.getCamera();
cam.position = Vector3(0.0f, 2.0f, -6.0f);
cam.lookAt(Vector3(0.0f, 1.0f, 4.0f));
cam.setPerspective(60.0f, 0.1f, 100.0f);
```

## 6.2. Анимация камеры

Встроенные методы:

- `animateTo(...)`
- `animatePos(...)`
- `animateTarget(...)`
- `animateFOV(...)`
- `updateAnim(float deltaTime)`
- `stopAnim()`
- `isAnimating()`

Типы интерполяции:

- `CameraAnimation::LINEAR`
- `CameraAnimation::SMOOTH`
- `CameraAnimation::EASE`

## 6.3. `FreeCam`

Готовая свободная камера для прототипов и debug/free-fly режимов.

Публичные поля:

- `rotSpeed`
- `moveSpeed`

Основные методы:

- `handleJoystick(...)`
- `handleButtons(...)`
- `handleDPad(...)`
- `handleRotateButtons(...)`

## 6.4. `OrbitCam`

Камера, вращающаяся вокруг центра.

Публичные поля:

- `center`
- `radius`
- `azimuth`
- `elevation`
- `zoomSpd`
- `rotSpd`

Основные методы:

- `setCenter(...)`
- `zoom(...)`
- `handleJoystick(...)`
- `handleButtons(...)`

## 6.5. `CameraBuilder`

Fluent builder для камеры:

```cpp
Camera cam = CameraBuilder()
    .at(0.0f, 2.0f, -6.0f)
    .lookAt(0.0f, 1.0f, 0.0f)
    .persp(60.0f, 0.1f, 100.0f)
    .build();
```

Методы:

- `at(...)`
- `lookAt(...)`
- `withUp(...)`
- `persp(...)`
- `ortho(...)`
- `fisheye(...)`
- `withConfig(...)`
- `build()`

## 6.6. `CameraTimeline`

Публичный timeline для катсцен и scripted camera motion.

Основные типы и методы:

- `CameraKeyframe`
- `setTrack(...)`
- `start(Camera& cam)`
- `update(Camera& cam, float dt)`
- `isPlaying()`

---

# 7. Меши и примитивы

## 7.1. `Mesh`

`Mesh` это базовый публичный геометрический класс.

Два базовых сценария:

- создать procedural mesh вручную
- использовать готовые primitive-классы

Конструкторы:

- `Mesh(uint16_t maxVerts, uint16_t maxFaces, const Color& color = Color::WHITE)`
- `Mesh(const Vertex* externalVertices, uint16_t vertCount, const Face* externalFaces, uint16_t faceCount, const Color& color = Color::WHITE, bool staticStorage = true)`

Основные методы для procedural mesh:

- `addVertex(...)`
- `addFace(...)`
- `clear()`
- `finalizeNormals()`
- `calculateBoundingSphere()`

Основные transform-методы:

- `setPosition(...)`
- `setRotation(...)`
- `setScale(...)`
- `translate(...)`
- `rotate(...)`

Основные query/helper методы:

- `numVertices()`
- `numFaces()`
- `vert(...)`
- `face(...)`
- `center()`
- `radius()`
- `normal(...)`
- `getTransform()`

Видимость и тени:

- `show()`
- `hide()`
- `isVisible()`
- `color(...)`
- `color()`
- `setCastShadows(bool enabled)`
- `getCastShadows()`

Минимальный пример procedural mesh:

```cpp
Mesh tri(3, 1, Color::fromRGB888(255, 180, 120));
tri.addVertex(Vector3(-1, 0, 0));
tri.addVertex(Vector3(1, 0, 0));
tri.addVertex(Vector3(0, 1, 0));
tri.addFace(0, 1, 2);
tri.finalizeNormals();
tri.calculateBoundingSphere();
```

## 7.2. Готовые примитивы

Публичные primitive mesh-классы:

- `Cube`
- `Pyramid`
- `Sphere`
- `Plane`
- `Cylinder`
- `Cone`
- `Capsule`
- `Teapot`
- `TrefoilKnot`

Примеры:

```cpp
Cube cube(1.0f, Color::fromRGB888(220, 220, 220));
Sphere sun(0.5f, 16, 12, Color::fromRGB888(255, 220, 160));
Plane ground(20.0f, 20.0f, 1, Color::fromRGB888(90, 90, 90));
```

---

# 8. Инстансы

## 8.1. `MeshInstance`

`MeshInstance` это способ много раз использовать один и тот же `Mesh` с разными transform/color.

Основные методы:

- `setMesh(...)`
- `getMesh()`
- `setPosition(...)`
- `setRotation(const Quaternion&)`
- `setEuler(...)`
- `setScale(...)`
- `rotate(...)`
- `setColor(...)`
- `color()`
- `show()`
- `hide()`
- `isVisible()`
- `center()`
- `radius()`
- `transform()`

Fluent helper-методы:

- `at(...)`
- `euler(...)`
- `size(...)`
- `color(...)`

Пример:

```cpp
Cube buildingMesh(1.0f, Color::fromRGB888(120, 130, 145));
MeshInstance instance(&buildingMesh);
instance.at(0.0f, 2.0f, 8.0f)->size(2.0f, 4.0f, 2.0f);
```

## 8.2. `InstanceManager`

Менеджер для большого числа инстансов.

Основные методы:

- `create(Mesh* mesh)`
- `batch(Mesh* mesh, size_t count)`
- `remove(MeshInstance* inst)`
- `clear()`
- `destroyAll()`
- `count()`
- `all()`
- `cull(...)`
- `sort(...)`

Рисование:

```cpp
r.drawInstances(manager);
```

---

# 9. Свет

## 9.1. `LightType`

Поддерживаемые типы:

- `LIGHT_DIRECTIONAL`
- `LIGHT_POINT`

## 9.2. `Light`

Публичные поля:

- `type`
- `intensity`
- `direction`
- `position`
- `color`
- `range`

Публичные методы:

- `setRange(float r)`
- `getCachedRGB(...)`

Пример point light:

```cpp
Light lamp;
lamp.type = LIGHT_POINT;
lamp.position = Vector3(0.0f, 3.0f, 5.0f);
lamp.color = Color::fromRGB888(255, 210, 170);
lamp.intensity = 1.2f;
lamp.setRange(8.0f);

r.addLight(lamp);
```

---

# 10. Scene Graph

## 10.1. `Node`

Базовый scene node.

Основные методы:

- `addChild(...)`
- `removeChild(...)`
- `getChild(...)`
- `findChild(...)`
- `getChildCount()`
- `getParent()`
- `setPosition(...)`
- `setRotation(...)`
- `setScale(...)`
- `translate(...)`
- `rotate(...)`
- `getPosition()`
- `getRotation()`
- `getScale()`
- `getWorldPosition()`
- `getWorldTransform()`
- `getLocalTransform()`
- `setVisible(...)`
- `isVisible()`
- `setEnabled(...)`
- `isEnabled()`
- `setName(...)`
- `getName()`
- `update(float deltaTime)`
- `render(Renderer* renderer)`

## 10.2. `MeshNode`

Node для `Mesh`.

Основные методы:

- `setMesh(...)`
- `getMesh()`
- `setCastShadows(bool cast)`
- `getCastShadows()`

## 10.3. `CameraNode`

Node для камеры.

Основные методы:

- `setFOV(...)`
- `getFOV()`
- `setNearPlane(...)`
- `getNearPlane()`
- `setFarPlane(...)`
- `getFarPlane()`
- `setProjectionType(...)`
- `getProjectionType()`
- `applyToCamera(Camera& camera)`

## 10.4. `LightNode`

Node для света.

Основные методы:

- `setLightType(...)`
- `getLightType()`
- `setColor(...)`
- `getColor()`
- `setIntensity(...)`
- `getIntensity()`
- `setDirection(...)`
- `getDirection()`
- `setRange(...)`
- `getRange()`
- `applyToLight(Light& light)`

## 10.5. `SceneGraph`

Основной container для scene graph.

Основные методы:

- `getRoot()`
- `createNode<T>(...)`
- `createMeshNode(...)`
- `createCameraNode(...)`
- `createLightNode(...)`
- `setActiveCamera(...)`
- `getActiveCamera()`
- `update(float deltaTime)`
- `render()`
- `findNode(...)`

Пример:

```cpp
SceneGraph scene(&r);

CameraNode* camNode = scene.createCameraNode("MainCamera");
camNode->setPosition(0.0f, 2.0f, -6.0f);
scene.setActiveCamera(camNode);

Cube* cube = new Cube(1.0f, Color::WHITE);
MeshNode* cubeNode = scene.createMeshNode(cube, "Cube");
cubeNode->setPosition(0.0f, 0.5f, 5.0f);
```

## 10.6. `SceneBuilder`

Convenience builder:

- `withCamera(...)`
- `withSun(...)`
- `withPointLight(...)`
- `addMesh(...)`
- `build()`

---

# 11. Character Controller

## 11.1. `CharacterInput`

Поля:

- `moveX`
- `moveY`
- `jump`
- `sprint`

## 11.2. `CharacterController`

Готовый публичный controller для простого персонажа.

Основные методы:

- `setVisual(MeshInstance* mesh)`
- `setVisualRig(...)`
- `initDefaultVisual()`
- `setPosition(...)`
- `getPosition()`
- `getYaw()`
- `setYaw(...)`
- `update(float deltaTime, const CharacterInput& input, const Camera& camera)`
- `applyToCamera(Camera& cam, const Vector3& offset) const`
- `applyFirstPerson(Camera& cam) const`
- `render(Renderer* renderer)`

Что важно:

- это не физический character controller AAA-уровня
- по умолчанию он дает базовое движение, гравитацию, jump и простой визуальный риг
- для прототипов, демо и простых игр этого достаточно

---

# 12. Время суток

## 12.1. `TimeOfDayConfig`

Поля:

- `dayLengthSeconds`
- `startHour`
- `baseIntensity`
- `nightIntensity`
- `autoAdvance`

## 12.2. `TimeOfDayController`

Контроллер неба и основного directional light по циклу дня.

Основные методы:

- `init(Renderer* r, const TimeOfDayConfig& cfg)`
- `setRenderer(Renderer* r)`
- `setDayLengthSeconds(float seconds)`
- `setAutoAdvance(bool enabled)`
- `setBaseIntensity(float intensity)`
- `setNightIntensity(float intensity)`
- `setTime(float hours, float minutes = 0.0f)`
- `getTimeHours() const`
- `getTime01() const`
- `update(float deltaSeconds)`

Пример:

```cpp
TimeOfDayController tod;

TimeOfDayConfig cfg;
cfg.dayLengthSeconds = 180.0f;
cfg.startHour = 8.0f;

tod.init(&r, cfg);
```

В цикле:

```cpp
float dt = getDeltaTime();
tod.update(dt);
```

---

# 13. Эффекты частиц

## 13.1. `ParticleEmitterConfig`

Поля:

- `maxParticles`
- `emitRate`
- `minLifetime`
- `maxLifetime`
- `initialSpeed`
- `spread`
- `acceleration`
- `startColor`
- `endColor`
- `startSize`
- `endSize`
- `looping`
- `additive`

## 13.2. `ParticleEmitter`

Основные методы:

- `setPosition(...)`
- `getPosition()`
- `setVelocityOffset(...)`
- `setEnabled(...)`
- `isEnabled()`
- `triggerBurst(int count)`
- `update(float dt)`
- `render(Renderer& renderer) const`

## 13.3. `FXSystem`

Менеджер для emitters и готовых preset-эффектов.

Основные методы:

- `createEmitter(...)`
- `destroyEmitter(...)`
- `clear()`
- `update(float dt)`
- `render(Renderer& renderer) const`
- `createFire(...)`
- `createSmoke(...)`
- `createExplosion(...)`
- `createSparks(...)`

Пример:

```cpp
FXSystem fx;
ParticleEmitter* smoke = fx.createSmoke(Vector3(0.0f, 0.5f, 6.0f));
```

В цикле:

```cpp
fx.update(dt);
fx.render(r);
```

---

# 14. Input API

`Pip3D` экспортирует простой Arduino-style input layer в namespace `pip3D::input`.

Что в нем есть:

- `ButtonConfig`
- `Button`
- `AnalogAxisConfig`
- `AnalogAxis`
- `JoystickConfig`
- `Joystick`

## 14.1. `Button`

Основные методы:

- `begin()`
- `update()`
- `isPressed()`
- `wasPressed()`
- `wasReleased()`

## 14.2. `AnalogAxis`

Основные методы:

- `begin()`
- `update(float deltaTime)`
- `value()`

## 14.3. `Joystick`

Основные методы:

- `begin()`
- `update(float deltaTime)`
- `x()`
- `y()`
- `isPressed()`
- `wasPressed()`
- `wasReleased()`

Пример:

```cpp
using namespace pip3D::input;

Joystick stick(
    JoystickConfig(
        AnalogAxisConfig(1, 0, 4095, 0.12f, false),
        AnalogAxisConfig(2, 0, 4095, 0.12f, true),
        ButtonConfig(3, true, 30)
    )
);

stick.begin();
stick.update(dt);
```

Что важно:

- этот input layer рассчитан именно на Arduino/ESP32-style пины
- PC keyboard/mouse runtime в симуляторе это отдельный слой, не часть `Input.h`

---

# 15. Utility API

## 15.1. `getDeltaTime()`

Простой helper:

```cpp
float dt = getDeltaTime();
```

Возвращает `dt` в секундах и clamp'ит слишком большие паузы.

## 15.2. `CameraHelper`

- `quickSetup(Camera& cam, float fov, float nearPlane, float farPlane)`

## 15.3. `MultiCameraHelper`

- `createIsometricCamera(Renderer& renderer, float distance)`

## 15.4. `SceneHelper`

Convenience helper для простой сцены.

Основные методы:

- `addGround(...)`
- `addSun(...)`
- `renderGround()`
- `renderSun(...)`
- `setSunPosition(...)`

Это helper для быстрых прототипов. Для более серьезной сцены обычно лучше работать напрямую через `Renderer`, `Light`, `SceneGraph` и свои меши.

---

# 16. Минимальный рабочий пример

```cpp
#include <Arduino.h>
#include "Pip3D.h"

using namespace pip3D;

Renderer* g_renderer = nullptr;
Cube* g_cube = nullptr;

void setup()
{
    g_renderer = &begin3D(480, 320, 10, 9, 8);

    Camera& cam = g_renderer->getCamera();
    cam.position = Vector3(0.0f, 2.0f, -6.0f);
    cam.lookAt(Vector3(0.0f, 0.5f, 5.0f));
    cam.setPerspective(60.0f, 0.1f, 100.0f);

    g_renderer->setSkyboxWithLighting(SKYBOX_DAY);
    g_renderer->setShadowPlaneY(0.0f);

    g_cube = new Cube(1.0f, Color::fromRGB888(220, 220, 230));
    g_cube->setPosition(0.0f, 0.5f, 5.0f);
}

void loop()
{
    float dt = getDeltaTime();
    g_cube->rotate(0.0f, 40.0f * dt, 0.0f);

    g_renderer->beginFrame();
    g_renderer->drawMesh(g_cube);
    g_renderer->drawText(8, 8, "Pip3D", Color::WHITE);
    g_renderer->endFrame();
}
```

---

# 17. Практические замечания

- Внешнему коду лучше начинать с `begin3D(...)`, `Renderer`, `Camera`, `Mesh`, `Light`.
- Если нужны много однотипных объектов, используй `MeshInstance` и `InstanceManager`, а не отдельные копии `Mesh`.
- Если нужна полноценная иерархия объектов, бери `SceneGraph` и `Node`-производные.
- Если нужна катсцена, используй `CameraTimeline`.
- Если нужен цикл дня, используй `TimeOfDayController`.
- Если нужен простой персонаж для тестов, используй `CharacterController`.
- Если нужен быстрый прототип эффекта, используй `FXSystem`.
- Внутренние хедеры рендера и пайплайна не должны быть основной точкой интеграции пользовательского кода.

