## Камеры PIP3D

Этот раздел описывает, как использовать камеры из `lib/Pip3D/Core/Camera.h`
в прошивках и играх на ESP32.

Файл даёт вам:

- **`Camera`** – базовая камера (позиция, таргет, матрицы).
- **`FreeCam`** – свободная камера для отладочных/игровых режимов полёта.
- **`OrbitCam`** – камера, вращающаяся вокруг точки интереса.
- **`CameraBuilder`** – удобный билдер для создания настроенной камеры.

Использовать всё это лучше через `#include "Pip3D.h"` и пространство имён
`pip3D`.

---

### Быстрый пример: простая игровая камера

```cpp
using namespace pip3D;

Camera cam(
    Vector3(0, 2, -5),  // позиция камеры
    Vector3(0, 1,  0),  // точка, куда смотрим
    Vector3(0, 1,  0)   // вектор "вверх"
);

// Настраиваем перспективную проекцию
cam.setPerspective(60.0f, 0.1f, 100.0f);
```

Разбор `Vector3(0, 2, -5)`:

- **`0`** – координата **X**, влево/вправо.
- **`2`** – координата **Y**, вверх/вниз (камера поднята на 2 единицы).
- **`-5`** – координата **Z**, вперёд/назад (камера стоит в 5 единицах перед
  центром сцены, если считать вперёд по +Z).

Типичный игровой цикл:

```cpp
float dt = getDeltaTimeSeconds();

// Обновляем анимации камеры (если запущены)
cam.updateAnim(dt);

// Получаем матрицу вида-проекции для рендера
float aspect = float(viewport.width) / viewport.height;
const Matrix4x4& vp = cam.getViewProjectionMatrix(aspect);
```

---

## Базовые типы

### ProjectionType

```cpp
enum ProjectionType
{
    PERSPECTIVE,
    ORTHOGRAPHIC,
    FISHEYE
};
```

- **`PERSPECTIVE`** – обычная перспективная 3D‑камера (то, что нужно
  в большинстве игр).
- **`ORTHOGRAPHIC`** – ортографическая проекция (вид сбоку/сверху, UI, карты).
- **`FISHEYE`** – перспективная камера с лёгким «fisheye» эффектом.

Обычно вы не меняете `projectionType` напрямую, а вызываете методы:

- `setPerspective(...)`
- `setOrtho(...)`
- `setFisheye(...)`

### CameraConfig

```cpp
struct CameraConfig
{
    float aspectEps;  // чувствительность к смене aspect
};
```

- **`aspectEps`** – насколько сильно должен измениться `aspect`, чтобы
  пересчитать матрицу проекции.
  - По умолчанию `1e-6f` – максимально точное поведение.
  - Можно увеличить (например `1e-3f`), если `aspect` скачет от
    бендинга/переворотов и вам не нужна супер‑точность.

Пример изменения конфигурации:

```cpp
Camera cam;
cam.config.aspectEps = 1e-4f;  // реже пересчитывать проекцию
```

---

## Класс Camera

Базовый класс камеры. Хранит позицию, точку, куда смотрит, проекцию и кэш
матриц.

### Поля, которые удобно править руками

```cpp
Camera cam;

cam.position = Vector3(0, 2, -5);  // где стоит камера
cam.target   = Vector3(0, 1,  0);  // куда смотрим
cam.up       = Vector3(0, 1,  0);  // "верх" камеры
```

После прямого изменения этих полей **желательно** вызвать `cam.markDirty()`,
чтобы кэш матриц пересчитался при следующем запросе:

```cpp
cam.position = Vector3(1, 3, -4);
cam.target   = Vector3(0, 1,  0);
cam.markDirty();
```

Другие полезные публичные поля:

- **`float fov`** – текущий FOV в градусах (при перспективе).
- **`float nearPlane, farPlane`** – границы видимого диапазона.
- **`float orthoWidth, orthoHeight`** – размер ортографического окна.
- **`float fisheyeStrength`** – сила fisheye‑эффекта (0..1).
- **`ProjectionType projectionType`** – текущий тип проекции
  (обычно управляется через `setPerspective`/`setOrtho`/`setFisheye`).

### Конструктор

```cpp
Camera(const Vector3& pos = Vector3(0, 0, -5),
       const Vector3& tgt = Vector3(0, 0,  0),
       const Vector3& up  = Vector3(0, 1,  0));
``;

- **`pos`** – позиция камеры.
- **`tgt`** – точка, в которую камера смотрит.
- **`up`** – вектор «вверх» камеры (будет нормализован).

Пример:

```cpp
Camera cam(
    Vector3(0, 2, -5),  // камера чуть выше и перед сценой
    Vector3(0, 1,  0),  // смотрим в центр объекта на высоте 1
    Vector3(0, 1,  0)   // классический "мировой вверх" по Y
);
```

### Настройка проекции

```cpp
void setPerspective(float fovDegrees = 60,
                    float near      = 0.1f,
                    float far       = 100.0f);

void setOrtho(float width  = 10.0f,
              float height = 10.0f,
              float near   = 0.1f,
              float far    = 100.0f);

void setFisheye(float fovDegrees = 120.0f,
                float strength   = 1.0f,
                float near       = 0.1f,
                float far        = 100.0f);
```

- **`setPerspective`** – стандартная перспективная камера.
  - `fovDegrees` – угол обзора по вертикали.
  - `near` / `far` – ближняя и дальняя плоскости (клэмпятся к разумным
    значениям, чтобы избежать артефактов).

- **`setOrtho`** – ортографическая камера.
  - `width` / `height` – базовый размер окна; фактический объём
    подстраивается под `aspect`.
  - Удобно для UI, карт, изометрии.

- **`setFisheye`** – перспектива с fisheye‑эффектом.
  - `strength` в диапазоне `[0, 1]`, где `1` – максимальное искажение.

Пример переключения режимов:

```cpp
cam.setPerspective(75.0f, 0.1f, 200.0f);   // обычная 3D‑камера
cam.setOrtho(20.0f, 20.0f, 0.1f, 50.0f);  // вид сверху без перспективы
```

### Движение камеры

Базовый метод:

```cpp
void move(float forwardAmount,
          float rightAmount,
          float upAmount);
```

- **`forwardAmount`** – сдвиг вдоль направления `forward()`.
- **`rightAmount`** – сдвиг вдоль `right()`.
- **`upAmount`** – сдвиг вдоль `upVec()`.

Камера и её `target` сдвигаются вместе – камера «едет» без
изменения направления.

Упрощённые методы:

- `moveForward(float distance);`
- `moveBackward(float distance);`
- `moveRight(float distance);`
- `moveLeft(float distance);`
- `moveUp(float distance);`
- `moveDown(float distance);`

Пример WASD‑управления:

```cpp
float spd = 5.0f * dt;
cam.move(
    (w ? spd : (s ? -spd : 0)),   // вперёд/назад
    (d ? spd : (a ? -spd : 0)),   // вправо/влево
    0.0f                          // вверх/вниз отдельно
);
```

### Вращение камеры

```cpp
void rotate(float yaw, float pitch, bool degrees = true);
void rotateDeg(float yawDegrees, float pitchDegrees);
void rotateRad(float yawRad, float pitchRad);
```

- **`yaw`** – поворот вокруг вертикальной оси (влево/вправо).
- **`pitch`** – наклон вверх/вниз.
- Вращение изменяет только `target`, `position` остаётся на месте.

Пример мышиного управления:

```cpp
cam.rotate(mouseDeltaX * sens, mouseDeltaY * sens, true);
```

### Быстрые операции взгляда и орбиты

```cpp
void lookAt(const Vector3& newTarget);
void lookAt(const Vector3& newTarget, const Vector3& newUp);

void orbit(const Vector3& center,
           float radius,
           float azimuth,
           float elevation,
           bool degrees = true);
```

- `lookAt(target)` – мгновенно повернуть камеру на новую цель.
- `lookAt(target, up)` – то же, плюс сменить вектор «вверх».
- `orbit(center, radius, azimuth, elevation)` – поставить камеру на сферу
  вокруг `center` и смотреть в неё.

Пример орбиты вокруг объекта:

```cpp
Vector3 center = Vector3(0, 1, 0);
cam.orbit(center, 5.0f, azimuthDeg, elevationDeg, true);
```

### Получение направлений и матриц

```cpp
const Vector3& forward() const;   // направление взгляда
const Vector3& right() const;     // вправо от камеры
const Vector3& upVec() const;     // вектор "вверх" камеры

const Matrix4x4& getViewMatrix() const;
const Matrix4x4& getProjectionMatrix(float aspect) const;
const Matrix4x4& getViewProjectionMatrix(float aspect) const;
```

- Векторы `forward/right/upVec` удобно использовать для движения и стрельбы.
- Матрицы `view`, `proj`, `viewProj` нужны рендереру.

Пример использования с собственным рендерером:

```cpp
float aspect = float(viewport.width) / viewport.height;
const Matrix4x4& viewProj = cam.getViewProjectionMatrix(aspect);
```

### Анимация камеры

```cpp
void animateTo(const Vector3& newPos,
               const Vector3& newTgt,
               float duration = 1.0f,
               CameraAnimation::Type type = CameraAnimation::SMOOTH);

void animatePos(const Vector3& newPos, float duration = 1.0f);
void animateTarget(const Vector3& newTgt, float duration = 1.0f);
void animateFOV(float newFov, float duration = 1.0f);

void updateAnim(float deltaTime);
void stopAnim();
bool isAnimating() const;
```

- **`animateTo`** – плавный перелёт камеры из текущего положения в `newPos` и
  `newTgt`.
- **`animatePos`** – анимировать только позицию, сохраняя направление взгляда.
- **`animateTarget`** – анимировать только цель.
- **`animateFOV`** – плавный зум FOV.
- **`updateAnim`** – вызывать **каждый кадр**, чтобы анимация двигалась.
- **`stopAnim`** / **`isAnimating`** – управление и проверка статуса.

Пример плавного перелёта камеры к новой точке:

```cpp
cam.animateTo(
    Vector3(0, 3, -8),  // новая позиция
    Vector3(0, 1,  0),  // новый таргет
    1.5f,               // 1.5 секунды
    CameraAnimation::SMOOTH
);

// В game loop:
cam.updateAnim(dt);
```

---

## FreeCam – свободная камера

```cpp
class FreeCam : public Camera
{
public:
    float rotSpeed = 90.0f;  // скорость вращения (град/сек)
    float moveSpeed = 5.0f;  // скорость движения (ед/сек)

    FreeCam(const Vector3& pos = Vector3(0, 0, -5));

    void handleJoystick(float joyX, float joyY, float deltaTime);
    void handleButtons(bool fwd, bool back, bool left, bool right,
                       bool up, bool down, float deltaTime);
    void handleDPad(int8_t dirX, int8_t dirY, float deltaTime);
    void handleRotateButtons(bool rotLeft, bool rotRight,
                             bool rotUp, bool rotDown,
                             float deltaTime);
};
```

**Назначение:**

- Быстрая free‑fly камера под геймпад/кнопки.
- Удобна для отладки сцены, полёта по уровню, режима «noclip».

Пример:

```cpp
FreeCam cam(Vector3(0, 2, -5));
cam.setPerspective(60.0f, 0.1f, 100.0f);

// В главном цикле
cam.handleJoystick(joyX, joyY, dt);
cam.handleButtons(btnFwd, btnBack, btnLeft, btnRight, btnUp, btnDown, dt);
cam.updateAnim(dt);  // если нужны анимации
```

Параметры обработчиков:

- **`joyX`, `joyY`** – аналоговый стик в диапазоне примерно `[-1, 1]`.
- **`fwd/back/left/right/up/down`** – булевы кнопки движения.
- **`dirX/dirY`** – дискретный D‑Pad, обычно `-1, 0, 1`.

Все методы используют внутренние `move/rotate` базовой `Camera`.

---

## OrbitCam – орбитальная камера

```cpp
class OrbitCam : public Camera
{
public:
    Vector3 center = Vector3(0, 0, 0);
    float   radius  = 10.0f;
    float   azimuth = 0.0f;
    float   elevation = 0.0f;
    float   zoomSpd = 1.0f;
    float   rotSpd  = 90.0f;

    OrbitCam(const Vector3& c = Vector3(0, 0, 0), float r = 10.0f);

    void setCenter(const Vector3& c);
    void zoom(float delta);
    void handleJoystick(float joyX, float joyY, float deltaTime);
    void handleButtons(bool zoomIn, bool zoomOut, float deltaTime);
};
```

**Назначение:**

- Камера редактора/просмотра модели: вращается вокруг `center`, всегда
  смотрит в центр.

Пример:

```cpp
OrbitCam cam(Vector3(0, 1, 0), 5.0f);  // орбита вокруг объекта на высоте 1
cam.setPerspective(60.0f, 0.1f, 100.0f);

// В цикле
cam.handleJoystick(joyX, joyY, dt);     // вращение
cam.handleButtons(zoomIn, zoomOut, dt); // зум
```

Пояснения:

- **`center`** – точка, вокруг которой вращаемся.
- **`radius`** – расстояние от камеры до `center`.
- **`azimuth`** – угол вокруг вертикальной оси (лево/право).
- **`elevation`** – угол наклона (вверх/вниз).
- **`zoom(delta)`** – изменить `radius` (клэмпится ≥ 0.1, чтобы камера не
  схлопнулась в центр).

---

## CameraBuilder – удобное создание камеры

```cpp
class CameraBuilder
{
public:
    CameraBuilder& at(const Vector3& pos);
    CameraBuilder& lookAt(const Vector3& tgt);
    CameraBuilder& withUp(const Vector3& up);

    CameraBuilder& persp(float fov = 60.0f,
                         float near = 0.1f,
                         float far  = 100.0f);

    CameraBuilder& ortho(float w = 10.0f,
                         float h = 10.0f,
                         float near = 0.1f,
                         float far  = 100.0f);

    CameraBuilder& fisheye(float fov = 120.0f,
                           float strength = 1.0f,
                           float near = 0.1f,
                           float far  = 100.0f);

    CameraBuilder& withConfig(const CameraConfig& cfg);

    Camera build();
};
```

**Назначение:**

- Делает создание камеры читаемым и цепочным.
- Подходит для инициализации в `setup()` или конфиг‑файлах.

Пример:

```cpp
Camera cam = CameraBuilder()
    .at(Vector3(0, 2, -5))
    .lookAt(Vector3(0, 1, 0))
    .withUp(Vector3(0, 1, 0))
    .persp(60.0f, 0.1f, 100.0f)
    .build();
```

Разбор по шагам:

- **`at(Vector3(0, 2, -5))`** – задаём позицию камеры.
- **`lookAt(Vector3(0, 1, 0))`** – смотрим на объект в центре сцены.
- **`withUp(Vector3(0, 1, 0))`** – фиксируем, что «верх» по оси Y.
- **`persp(60, 0.1, 100)`** – настраиваем перспективную проекцию.
- **`build()`** – получаем готовую `Camera` с помеченными грязными матрицами,
  которые пересчитаются при первом использовании.

---

Этого набора API достаточно, чтобы:

- создать одну или несколько камер под разные режимы;
- управлять ими из игрового цикла (движение, вращение, орбита);
- плавно анимировать перелёты и зум;
- получать готовые матрицы `view`, `proj`, `viewProj` для своего рендера
  или использовать камеры через высокоуровневый `Renderer`.

В большинстве случаев вам достаточно `Camera` или `FreeCam`/`OrbitCam` и
нескольких методов: `setPerspective`, `move*`, `rotate*`, `lookAt`,
`getViewProjectionMatrix`, `animateTo` + `updateAnim`.



cl /EHsc /W4 /std:c++17 /DPIP3D_PC pc_viewer.cpp ^
  lib\Pip3D\Math\Math.cpp ^
  lib\Pip3D\Core\Core.cpp ^
  lib\Pip3D\Core\Debug\Logging.cpp ^
  lib\Pip3D\Core\Jobs.cpp ^
  lib\Pip3D\Graphics\Font.cpp ^
  lib\Pip3D\Rendering\Rasterizer\Shading.cpp ^
  lib\Pip3D\Rendering\Display\Drivers\PcDisplayDriver.cpp ^
  user32.lib gdi32.lib ^
  /Fe:pc_viewer.exe