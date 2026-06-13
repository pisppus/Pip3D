// Standalone PC editor/viewer for the PIP3D engine.
// Builds a Win32 window, hooks a PC blit callback,
// and renders a simple scene with a rotating cube and a sphere.
//      lib\Pip3D\Rendering\Rasterizer\Shading.cpp \
//      tools\PcViewer\PcDisplayBlit.cpp \
//      user32.lib gdi32.lib
//
// Or with MinGW (from repo root):
//   g++ -std=c++17 pc_viewer.cpp -lgdi32 -o pc_viewer.exe
//
// Make sure you run the compiler from the project root so that
// the relative includes to lib/Pip3D/... work.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>

// Windows.h определяет макросы near/far, которые конфликтуют с
// одноимёнными параметрами в Camera::setPerspective/setOrtho/setFisheye.
// Для этого инструмента достаточно просто их убрать.
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif

#include "Core/Core.h"
#include "Math/Algebra.h"
#include "Rendering/Renderer.h"
#include "Geometry/Geometry/Primitives.h"
#include "PcDisplayBlit.h"

using namespace pip3D;

static const int VIEW_WIDTH = SCREEN_WIDTH;
static const int VIEW_HEIGHT = SCREEN_HEIGHT;
static const int UI_HEIGHT = 56;
static const int INSPECTOR_WIDTH = 220;

// UI color scheme (dark theme)
static const COLORREF UI_COLOR_BG = RGB(18, 18, 24);
static const COLORREF UI_COLOR_PANEL = RGB(28, 28, 36);
static const COLORREF UI_COLOR_BUTTON = RGB(44, 44, 56);
static const COLORREF UI_COLOR_BUTTON_SELECTED = RGB(60, 60, 82);
static const COLORREF UI_COLOR_BUTTON_PRESSED = RGB(80, 80, 104);
static const COLORREF UI_COLOR_BUTTON_BORDER = RGB(70, 70, 90);
static const COLORREF UI_COLOR_TEXT = RGB(230, 230, 230);
static const COLORREF UI_COLOR_TEXT_MUTED = RGB(180, 180, 190);
static const COLORREF UI_COLOR_ACCENT = RGB(255, 120, 40);

static HFONT g_uiFont = nullptr;
static HBRUSH g_brushBg = nullptr;
static HBRUSH g_brushPanel = nullptr;
static HBRUSH g_brushInspector = nullptr;

static HWND g_hwnd = nullptr;
static HWND g_hostParent = nullptr;
static BITMAPINFO g_bmi = {};
static uint32_t *g_framebuffer = nullptr;
static bool g_hostedMode = false;
static bool g_drawHud = true;
static std::string g_sceneFilePath;
static std::filesystem::file_time_type g_sceneFileStamp{};
static uint32_t g_lastScenePollMs = 0;

static HWND g_btnMode = nullptr;
static HWND g_btnResetCam = nullptr;
static HWND g_btnSelCube = nullptr;
static HWND g_btnSelSphere = nullptr;

static HWND g_lblTransform = nullptr;
static HWND g_editPosX = nullptr;
static HWND g_editPosY = nullptr;
static HWND g_editPosZ = nullptr;
static HWND g_editRotX = nullptr;
static HWND g_editRotY = nullptr;
static HWND g_editRotZ = nullptr;
static HWND g_lblRendering = nullptr;
static HWND g_chkVisible = nullptr;
static HWND g_chkCastShadows = nullptr;

enum
{
    ID_BTN_MODE = 1001,
    ID_BTN_RESET_CAM = 1002,
    ID_BTN_SEL_CUBE = 1003,
    ID_BTN_SEL_SPHERE = 1004,
    ID_EDIT_POS_X = 1101,
    ID_EDIT_POS_Y,
    ID_EDIT_POS_Z,
    ID_EDIT_ROT_X,
    ID_EDIT_ROT_Y,
    ID_EDIT_ROT_Z,
    ID_CHK_VISIBLE,
    ID_CHK_CAST_SHADOWS
};

static Renderer *g_renderer = nullptr;
static Plane *g_ground = nullptr;
static Cube *g_cube = nullptr;
static Sphere *g_sphere = nullptr;
static std::vector<Mesh *> g_sceneMeshes;

static Cube *g_axisX = nullptr;
static Cube *g_axisY = nullptr;
static Cube *g_axisZ = nullptr;

static Color g_cubeBaseColor;
static Color g_sphereBaseColor;

static uint32_t g_lastMs = 0;
static float g_time = 0.0f;

static bool g_rmbDown = false;
static POINT g_lastMousePos = {};
static float g_pendingYaw = 0.0f;
static float g_pendingPitch = 0.0f;

enum EditorMode
{
    MODE_EDIT = 0,
    MODE_PLAY = 1
};

static EditorMode g_editorMode = MODE_EDIT;
static int g_selectedObject = 1;
static bool g_tabPrevDown = false;
static bool g_key1PrevDown = false;
static bool g_key2PrevDown = false;

static Vector3 g_cubePos;
static Vector3 g_spherePos;

static bool g_axisDragActive = false;
static int g_axisActiveId = 0;
static Vector3 g_axisOrigin;
static Vector3 g_axisDir;
static Vector3 g_axisStartObjPos;
static float g_axisStartParam = 0.0f;

struct RuntimeCameraState
{
    bool enabled = false;
    Vector3 position;
    Vector3 rotation;
    float fov = 60.0f;
};

struct RuntimeLightState
{
    bool enabled = false;
    Vector3 rotation;
    float intensity = 1.2f;
    Color color = Color::WHITE;
};

static RuntimeCameraState g_runtimeCamera;
static RuntimeLightState g_runtimeLight;

static void initCamera(Renderer &r);

static int clientViewportWidth()
{
    if (!g_hwnd)
        return VIEW_WIDTH;

    RECT rc{};
    GetClientRect(g_hwnd, &rc);
    int width = rc.right - rc.left;
    if (!g_hostedMode)
        width = VIEW_WIDTH;
    return width > 0 ? width : VIEW_WIDTH;
}

static std::vector<std::string> splitLine(const std::string &line, char delim)
{
    std::vector<std::string> parts;
    std::stringstream ss(line);
    std::string part;
    while (std::getline(ss, part, delim))
    {
        parts.push_back(part);
    }
    return parts;
}

static float parseFloat(const std::string &text, float fallback = 0.0f)
{
    char *endPtr = nullptr;
    const float value = std::strtof(text.c_str(), &endPtr);
    return endPtr == text.c_str() ? fallback : value;
}

static bool parseBoolToken(const std::string &text)
{
    return text == "1" || text == "true" || text == "True" || text == "TRUE";
}

static Color parseHexColor(const std::string &hex, const Color &fallback = Color::WHITE)
{
    if (hex.size() != 7 || hex[0] != '#')
        return fallback;

    auto parseByte = [&](int offset) -> uint8_t
    {
        const std::string chunk = hex.substr(offset, 2);
        return static_cast<uint8_t>(std::strtoul(chunk.c_str(), nullptr, 16));
    };

    return Color::fromRGB888(parseByte(1), parseByte(3), parseByte(5));
}

static Vector3 rotationToForward(const Vector3 &rotationDegrees)
{
    const float pitch = rotationDegrees.x * kDegToRad;
    const float yaw = rotationDegrees.y * kDegToRad;
    const float cp = cosf(pitch);
    const float sp = sinf(pitch);
    const float cy = cosf(yaw);
    const float sy = sinf(yaw);
    Vector3 dir(sy * cp, -sp, cy * cp);
    dir.normalize();
    return dir;
}

static void clearRuntimeScene()
{
    for (Mesh *mesh : g_sceneMeshes)
    {
        delete mesh;
    }
    g_sceneMeshes.clear();
}

static Mesh *createRuntimeMesh(const std::string &assetKey, const Color &color)
{
    if (_stricmp(assetKey.c_str(), "Cube") == 0)
        return new Cube(1.0f, color);
    if (_stricmp(assetKey.c_str(), "Sphere") == 0)
        return new Sphere(1.0f, color);
    if (_stricmp(assetKey.c_str(), "Cylinder") == 0)
        return new Cylinder(0.5f, 1.5f, 16, color);
    if (_stricmp(assetKey.c_str(), "Plane") == 0)
        return new Plane(4.0f, 4.0f, 1, color);
    return nullptr;
}

static void applyRuntimeCamera(Renderer &r)
{
    if (!g_runtimeCamera.enabled)
    {
        initCamera(r);
        return;
    }

    Camera &cam = r.getCamera();
    cam.position = g_runtimeCamera.position;
    cam.target = g_runtimeCamera.position + rotationToForward(g_runtimeCamera.rotation) * 8.0f;
    cam.up = Vector3(0.0f, 1.0f, 0.0f);
    cam.setPerspective(g_runtimeCamera.fov, 0.1f, 80.0f);
    cam.markDirty();
}

static void applyRuntimeLight(Renderer &r)
{
    if (!g_runtimeLight.enabled)
    {
        Vector3 lightDir(-0.6f, -1.0f, -0.4f);
        lightDir.normalize();
        r.setMainDirectionalLight(lightDir, Color::WHITE, 1.2f);
        return;
    }

    Vector3 lightDir = rotationToForward(g_runtimeLight.rotation);
    lightDir.normalize();
    r.setMainDirectionalLight(lightDir, g_runtimeLight.color, g_runtimeLight.intensity);
}

static bool loadRuntimeScene(Renderer &r)
{
    if (g_sceneFilePath.empty() || !std::filesystem::exists(g_sceneFilePath))
        return false;

    std::ifstream input(g_sceneFilePath);
    if (!input)
        return false;

    clearRuntimeScene();
    g_runtimeCamera = RuntimeCameraState{};
    g_runtimeLight = RuntimeLightState{};

    std::string line;
    while (std::getline(input, line))
    {
        if (line.empty())
            continue;

        const auto parts = splitLine(line, '|');
        if (parts.empty())
            continue;

        if (parts[0] == "CAMERA" && parts.size() >= 8)
        {
            g_runtimeCamera.enabled = true;
            g_runtimeCamera.position = Vector3(parseFloat(parts[1]), parseFloat(parts[2]), parseFloat(parts[3]));
            g_runtimeCamera.rotation = Vector3(parseFloat(parts[4]), parseFloat(parts[5]), parseFloat(parts[6]));
            g_runtimeCamera.fov = parseFloat(parts[7], 60.0f);
            continue;
        }

        if (parts[0] == "LIGHT" && parts.size() >= 9)
        {
            g_runtimeLight.enabled = true;
            g_runtimeLight.rotation = Vector3(parseFloat(parts[4]), parseFloat(parts[5]), parseFloat(parts[6]));
            g_runtimeLight.intensity = parseFloat(parts[7], 1.2f);
            g_runtimeLight.color = parseHexColor(parts[8], Color::WHITE);
            continue;
        }

        if (parts[0] == "NODE" && parts.size() >= 15)
        {
            Mesh *mesh = createRuntimeMesh(parts[1], parseHexColor(parts[3], Color::WHITE));
            if (!mesh)
                continue;

            mesh->setPosition(parseFloat(parts[6]), parseFloat(parts[7]), parseFloat(parts[8]));
            mesh->setRotation(parseFloat(parts[9]), parseFloat(parts[10]), parseFloat(parts[11]));
            mesh->setScale(parseFloat(parts[12], 1.0f), parseFloat(parts[13], 1.0f), parseFloat(parts[14], 1.0f));
            mesh->setCastShadows(parseBoolToken(parts[5]));
            if (parseBoolToken(parts[4]))
                mesh->show();
            else
                mesh->hide();

            g_sceneMeshes.push_back(mesh);
        }
    }

    g_sceneFileStamp = std::filesystem::last_write_time(g_sceneFilePath);
    r.setSkyboxWithLighting(SKYBOX_DAY);
    r.setShadowsEnabled(true);
    r.setShadowPlaneY(0.0f);
    r.setBackfaceCullingEnabled(false);
    applyRuntimeCamera(r);
    applyRuntimeLight(r);
    return true;
}

static void maybeReloadRuntimeScene(Renderer &r)
{
    if (g_sceneFilePath.empty())
        return;

    const uint32_t now = millis();
    if (now - g_lastScenePollMs < 250u)
        return;
    g_lastScenePollMs = now;

    std::error_code ec;
    const auto currentStamp = std::filesystem::last_write_time(g_sceneFilePath, ec);
    if (ec)
        return;

    if (currentStamp != g_sceneFileStamp)
    {
        loadRuntimeScene(r);
    }
}

static int clientViewportHeight()
{
    if (!g_hwnd)
        return VIEW_HEIGHT;

    RECT rc{};
    GetClientRect(g_hwnd, &rc);
    int height = rc.bottom - rc.top;
    if (!g_hostedMode)
        height -= UI_HEIGHT;
    return height > 0 ? height : VIEW_HEIGHT;
}

static void mapClientPointToView(int clientX, int clientY, int &viewX, int &viewY)
{
    int viewportX = clientX;
    int viewportY = clientY;
    if (!g_hostedMode)
        viewportY -= UI_HEIGHT;

    int width = clientViewportWidth();
    int height = clientViewportHeight();
    viewX = static_cast<int>((static_cast<float>(viewportX) / static_cast<float>(width)) * static_cast<float>(VIEW_WIDTH));
    viewY = static_cast<int>((static_cast<float>(viewportY) / static_cast<float>(height)) * static_cast<float>(VIEW_HEIGHT));
}

static bool isAnyInspectorEditFocused()
{
    HWND focus = GetFocus();
    return focus == g_editPosX ||
           focus == g_editPosY ||
           focus == g_editPosZ ||
           focus == g_editRotX ||
           focus == g_editRotY ||
           focus == g_editRotZ;
}

static bool isInputActive()
{
    if (!g_hwnd)
        return false;

    HWND fg = GetForegroundWindow();
    if (fg != g_hwnd)
        return false;

    if (isAnyInspectorEditFocused())
        return false;

    return true;
}

static void updateModeButtonText()
{
    if (!g_btnMode)
        return;
    const char *text = (g_editorMode == MODE_EDIT) ? "Mode: EDIT" : "Mode: PLAY";
    SetWindowTextA(g_btnMode, text);
}

static void setEditorMode(EditorMode newMode)
{
    if (g_editorMode == newMode)
        return;

    g_editorMode = newMode;

    if (g_editorMode == MODE_PLAY)
    {
        g_time = 0.0f;
    }

    updateModeButtonText();
}

static void updateSelectionButtons()
{
    if (!g_btnSelCube || !g_btnSelSphere)
        return;

    if (g_selectedObject == 1)
    {
        SetWindowTextA(g_btnSelCube, "Cube [*]");
        SetWindowTextA(g_btnSelSphere, "Sphere");
    }
    else if (g_selectedObject == 2)
    {
        SetWindowTextA(g_btnSelCube, "Cube");
        SetWindowTextA(g_btnSelSphere, "Sphere [*]");
    }
    else
    {
        SetWindowTextA(g_btnSelCube, "Cube");
        SetWindowTextA(g_btnSelSphere, "Sphere");
    }
}

static void ensureUiResources()
{
    if (!g_uiFont)
    {
        g_uiFont = CreateFontA(
            -13,
            0,
            0,
            0,
            FW_SEMIBOLD,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            "Segoe UI");
    }
    if (!g_brushBg)
        g_brushBg = CreateSolidBrush(UI_COLOR_BG);
    if (!g_brushPanel)
        g_brushPanel = CreateSolidBrush(UI_COLOR_PANEL);
    if (!g_brushInspector)
        g_brushInspector = CreateSolidBrush(UI_COLOR_BG);
}

static void updateSelectionHighlight()
{
    if (g_cube)
        g_cube->color(g_cubeBaseColor);
    if (g_sphere)
        g_sphere->color(g_sphereBaseColor);

    if (g_selectedObject == 1 && g_cube)
    {
        g_cube->color(Color::fromRGB888(255, 240, 120));
    }
    else if (g_selectedObject == 2 && g_sphere)
    {
        g_sphere->color(Color::fromRGB888(200, 225, 255));
    }
}

static Mesh *getSelectedMesh()
{
    if (g_selectedObject == 1)
        return g_cube;
    if (g_selectedObject == 2)
        return g_sphere;
    return nullptr;
}

static bool computeAxisClosestParam(const Vector3 &rayOrigin,
                                    const Vector3 &rayDir,
                                    const Vector3 &axisOrigin,
                                    const Vector3 &axisDir,
                                    float &outT)
{
    Vector3 w0 = rayOrigin - axisOrigin;
    float a = rayDir.dot(rayDir);
    float b = rayDir.dot(axisDir);
    float c = axisDir.dot(axisDir);
    float d = rayDir.dot(w0);
    float e = axisDir.dot(w0);

    float D = a * c - b * b;
    float sc;

    if (fabsf(D) < 1e-5f || c < 1e-5f)
    {
        sc = 0.0f;
        outT = -e / (c > 1e-5f ? c : 1.0f);
        return true;
    }

    sc = (b * e - c * d) / D;
    outT = (a * e - b * d) / D;
    return true;
}

static bool pickAxisGizmo(const Vector3 &rayOrigin,
                          const Vector3 &rayDir,
                          const Vector3 &axisOrigin,
                          const Vector3 &axisDir,
                          float pickRadius,
                          float &outT)
{
    float t = 0.0f;
    if (!computeAxisClosestParam(rayOrigin, rayDir, axisOrigin, axisDir, t))
        return false;

    // Recompute closest points to measure distance
    Vector3 w0 = rayOrigin - axisOrigin;
    float a = rayDir.dot(rayDir);
    float b = rayDir.dot(axisDir);
    float c = axisDir.dot(axisDir);
    float d = rayDir.dot(w0);
    float e = axisDir.dot(w0);
    float D = a * c - b * b;
    float sc;

    if (fabsf(D) < 1e-5f || c < 1e-5f)
    {
        sc = 0.0f;
    }
    else
    {
        sc = (b * e - c * d) / D;
    }

    Vector3 pRay = rayOrigin + rayDir * sc;
    Vector3 pAxis = axisOrigin + axisDir * t;
    Vector3 diff = pRay - pAxis;
    float distSq = diff.lengthSquared();
    if (distSq > pickRadius * pickRadius)
        return false;

    outT = t;
    return true;
}

static void setEditFloat(HWND edit, float value)
{
    if (!edit)
        return;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.3f", value);
    SetWindowTextA(edit, buf);
}

static bool tryParseEditFloat(HWND edit, float &outValue)
{
    if (!edit)
        return false;

    char buf[64];
    int len = GetWindowTextA(edit, buf, sizeof(buf));
    if (len <= 0)
        return false;

    char *endPtr = nullptr;
    float v = std::strtof(buf, &endPtr);
    if (endPtr == buf)
        return false;

    outValue = v;
    return true;
}

static void updateInspectorFromSelection()
{
    Mesh *mesh = getSelectedMesh();

    if (!mesh)
    {
        if (g_editPosX)
            SetWindowTextA(g_editPosX, "");
        if (g_editPosY)
            SetWindowTextA(g_editPosY, "");
        if (g_editPosZ)
            SetWindowTextA(g_editPosZ, "");
        if (g_editRotX)
            SetWindowTextA(g_editRotX, "");
        if (g_editRotY)
            SetWindowTextA(g_editRotY, "");
        if (g_editRotZ)
            SetWindowTextA(g_editRotZ, "");

        if (g_chkVisible)
            SendMessageA(g_chkVisible, BM_SETCHECK, BST_UNCHECKED, 0);
        if (g_chkCastShadows)
            SendMessageA(g_chkCastShadows, BM_SETCHECK, BST_UNCHECKED, 0);

        return;
    }

    Vector3 p = mesh->pos();
    Vector3 r = mesh->rot();

    setEditFloat(g_editPosX, p.x);
    setEditFloat(g_editPosY, p.y);
    setEditFloat(g_editPosZ, p.z);

    setEditFloat(g_editRotX, r.x);
    setEditFloat(g_editRotY, r.y);
    setEditFloat(g_editRotZ, r.z);

    if (g_chkVisible)
    {
        SendMessageA(g_chkVisible,
                     BM_SETCHECK,
                     mesh->isVisible() ? BST_CHECKED : BST_UNCHECKED,
                     0);
    }

    if (g_chkCastShadows)
    {
        SendMessageA(g_chkCastShadows,
                     BM_SETCHECK,
                     mesh->getCastShadows() ? BST_CHECKED : BST_UNCHECKED,
                     0);
    }
}

static void applyInspectorTransform()
{
    Mesh *mesh = getSelectedMesh();
    if (!mesh)
        return;

    Vector3 p = mesh->pos();
    Vector3 r = mesh->rot();

    float v;
    if (tryParseEditFloat(g_editPosX, v))
        p.x = v;
    if (tryParseEditFloat(g_editPosY, v))
        p.y = v;
    if (tryParseEditFloat(g_editPosZ, v))
        p.z = v;

    if (tryParseEditFloat(g_editRotX, v))
        r.x = v;
    if (tryParseEditFloat(g_editRotY, v))
        r.y = v;
    if (tryParseEditFloat(g_editRotZ, v))
        r.z = v;

    mesh->setPosition(p.x, p.y, p.z);
    mesh->setRotation(r.x, r.y, r.z);

    if (mesh == g_cube)
    {
        g_cubePos = p;
    }
    else if (mesh == g_sphere)
    {
        g_spherePos = p;
    }

    updateInspectorFromSelection();
}

static void createEditorUI(HWND hwnd)
{
    ensureUiResources();

    const int margin = 8;
    const int btnH = 24;
    int x = margin;
    int y = (UI_HEIGHT - btnH) / 2;

    g_btnMode = CreateWindowExA(0,
                                "BUTTON",
                                "Mode: EDIT",
                                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                x,
                                y,
                                110,
                                btnH,
                                hwnd,
                                (HMENU)(INT_PTR)ID_BTN_MODE,
                                (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE),
                                nullptr);
    SendMessageA(g_btnMode, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
    x += 120;

    g_btnResetCam = CreateWindowExA(0,
                                    "BUTTON",
                                    "Reset Camera",
                                    WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                    x,
                                    y,
                                    110,
                                    btnH,
                                    hwnd,
                                    (HMENU)(INT_PTR)ID_BTN_RESET_CAM,
                                    (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE),
                                    nullptr);
    SendMessageA(g_btnResetCam, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
    x += 130;

    g_btnSelCube = CreateWindowExA(0,
                                   "BUTTON",
                                   "Cube [*]",
                                   WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                   x,
                                   y,
                                   80,
                                   btnH,
                                   hwnd,
                                   (HMENU)(INT_PTR)ID_BTN_SEL_CUBE,
                                   (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE),
                                   nullptr);
    SendMessageA(g_btnSelCube, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
    x += 90;

    g_btnSelSphere = CreateWindowExA(0,
                                     "BUTTON",
                                     "Sphere",
                                     WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                     x,
                                     y,
                                     80,
                                     btnH,
                                     hwnd,
                                     (HMENU)(INT_PTR)ID_BTN_SEL_SPHERE,
                                     (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE),
                                     nullptr);

    SendMessageA(g_btnSelSphere, WM_SETFONT, (WPARAM)g_uiFont, TRUE);

    int panelX = VIEW_WIDTH + 16;
    int panelY = 16;
    int labelH = 18;
    int editH = 20;
    int rowGap = 4;
    int colLabelW = 40;
    int colEditW = 55;

    g_lblTransform = CreateWindowExA(0,
                                     "STATIC",
                                     "Transform",
                                     WS_CHILD | WS_VISIBLE,
                                     panelX,
                                     panelY,
                                     150,
                                     labelH,
                                     hwnd,
                                     nullptr,
                                     (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE),
                                     nullptr);
    SendMessageA(g_lblTransform, WM_SETFONT, (WPARAM)g_uiFont, TRUE);

    panelY += labelH + rowGap;

    HWND lblPos = CreateWindowExA(0,
                                  "STATIC",
                                  "Position",
                                  WS_CHILD | WS_VISIBLE,
                                  panelX,
                                  panelY,
                                  150,
                                  labelH,
                                  hwnd,
                                  nullptr,
                                  (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE),
                                  nullptr);
    SendMessageA(lblPos, WM_SETFONT, (WPARAM)g_uiFont, TRUE);

    panelY += labelH + rowGap;

    int xAxisLabelX = panelX;
    int xAxisEditX = panelX + colLabelW;

    HWND lblX = CreateWindowExA(0,
                                "STATIC",
                                "X",
                                WS_CHILD | WS_VISIBLE,
                                xAxisLabelX,
                                panelY,
                                colLabelW,
                                labelH,
                                hwnd,
                                nullptr,
                                (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE),
                                nullptr);
    SendMessageA(lblX, WM_SETFONT, (WPARAM)g_uiFont, TRUE);

    g_editPosX = CreateWindowExA(WS_EX_CLIENTEDGE,
                                 "EDIT",
                                 "",
                                 WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                 xAxisEditX,
                                 panelY,
                                 colEditW,
                                 editH,
                                 hwnd,
                                 (HMENU)(INT_PTR)ID_EDIT_POS_X,
                                 (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE),
                                 nullptr);
    SendMessageA(g_editPosX, WM_SETFONT, (WPARAM)g_uiFont, TRUE);

    int yAxisEditX = xAxisEditX + colEditW + 8;
    int zAxisEditX = yAxisEditX + colEditW + 8;

    HWND lblY = CreateWindowExA(0,
                                "STATIC",
                                "Y",
                                WS_CHILD | WS_VISIBLE,
                                yAxisEditX - colLabelW,
                                panelY,
                                colLabelW,
                                labelH,
                                hwnd,
                                nullptr,
                                (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE),
                                nullptr);
    SendMessageA(lblY, WM_SETFONT, (WPARAM)g_uiFont, TRUE);

    g_editPosY = CreateWindowExA(WS_EX_CLIENTEDGE,
                                 "EDIT",
                                 "",
                                 WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                 yAxisEditX,
                                 panelY,
                                 colEditW,
                                 editH,
                                 hwnd,
                                 (HMENU)(INT_PTR)ID_EDIT_POS_Y,
                                 (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE),
                                 nullptr);
    SendMessageA(g_editPosY, WM_SETFONT, (WPARAM)g_uiFont, TRUE);

    HWND lblZ = CreateWindowExA(0,
                                "STATIC",
                                "Z",
                                WS_CHILD | WS_VISIBLE,
                                zAxisEditX - colLabelW,
                                panelY,
                                colLabelW,
                                labelH,
                                hwnd,
                                nullptr,
                                (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE),
                                nullptr);
    SendMessageA(lblZ, WM_SETFONT, (WPARAM)g_uiFont, TRUE);

    g_editPosZ = CreateWindowExA(WS_EX_CLIENTEDGE,
                                 "EDIT",
                                 "",
                                 WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                 zAxisEditX,
                                 panelY,
                                 colEditW,
                                 editH,
                                 hwnd,
                                 (HMENU)(INT_PTR)ID_EDIT_POS_Z,
                                 (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE),
                                 nullptr);
    SendMessageA(g_editPosZ, WM_SETFONT, (WPARAM)g_uiFont, TRUE);

    panelY += editH + rowGap * 2;

    HWND lblRot = CreateWindowExA(0,
                                  "STATIC",
                                  "Rotation",
                                  WS_CHILD | WS_VISIBLE,
                                  panelX,
                                  panelY,
                                  150,
                                  labelH,
                                  hwnd,
                                  nullptr,
                                  (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE),
                                  nullptr);
    SendMessageA(lblRot, WM_SETFONT, (WPARAM)g_uiFont, TRUE);

    panelY += labelH + rowGap;

    HWND lblRotX = CreateWindowExA(0,
                                   "STATIC",
                                   "X",
                                   WS_CHILD | WS_VISIBLE,
                                   xAxisLabelX,
                                   panelY,
                                   colLabelW,
                                   labelH,
                                   hwnd,
                                   nullptr,
                                   (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE),
                                   nullptr);
    SendMessageA(lblRotX, WM_SETFONT, (WPARAM)g_uiFont, TRUE);

    g_editRotX = CreateWindowExA(WS_EX_CLIENTEDGE,
                                 "EDIT",
                                 "",
                                 WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                 xAxisEditX,
                                 panelY,
                                 colEditW,
                                 editH,
                                 hwnd,
                                 (HMENU)(INT_PTR)ID_EDIT_ROT_X,
                                 (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE),
                                 nullptr);
    SendMessageA(g_editRotX, WM_SETFONT, (WPARAM)g_uiFont, TRUE);

    HWND lblRotY = CreateWindowExA(0,
                                   "STATIC",
                                   "Y",
                                   WS_CHILD | WS_VISIBLE,
                                   yAxisEditX - colLabelW,
                                   panelY,
                                   colLabelW,
                                   labelH,
                                   hwnd,
                                   nullptr,
                                   (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE),
                                   nullptr);
    SendMessageA(lblRotY, WM_SETFONT, (WPARAM)g_uiFont, TRUE);

    g_editRotY = CreateWindowExA(WS_EX_CLIENTEDGE,
                                 "EDIT",
                                 "",
                                 WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                 yAxisEditX,
                                 panelY,
                                 colEditW,
                                 editH,
                                 hwnd,
                                 (HMENU)(INT_PTR)ID_EDIT_ROT_Y,
                                 (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE),
                                 nullptr);
    SendMessageA(g_editRotY, WM_SETFONT, (WPARAM)g_uiFont, TRUE);

    HWND lblRotZ = CreateWindowExA(0,
                                   "STATIC",
                                   "Z",
                                   WS_CHILD | WS_VISIBLE,
                                   zAxisEditX - colLabelW,
                                   panelY,
                                   colLabelW,
                                   labelH,
                                   hwnd,
                                   nullptr,
                                   (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE),
                                   nullptr);
    SendMessageA(lblRotZ, WM_SETFONT, (WPARAM)g_uiFont, TRUE);

    g_editRotZ = CreateWindowExA(WS_EX_CLIENTEDGE,
                                 "EDIT",
                                 "",
                                 WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                 zAxisEditX,
                                 panelY,
                                 colEditW,
                                 editH,
                                 hwnd,
                                 (HMENU)(INT_PTR)ID_EDIT_ROT_Z,
                                 (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE),
                                 nullptr);
    SendMessageA(g_editRotZ, WM_SETFONT, (WPARAM)g_uiFont, TRUE);

    panelY += editH + rowGap * 2;

    g_lblRendering = CreateWindowExA(0,
                                     "STATIC",
                                     "Rendering",
                                     WS_CHILD | WS_VISIBLE,
                                     panelX,
                                     panelY,
                                     150,
                                     labelH,
                                     hwnd,
                                     nullptr,
                                     (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE),
                                     nullptr);
    SendMessageA(g_lblRendering, WM_SETFONT, (WPARAM)g_uiFont, TRUE);

    y += labelH + rowGap;

    g_chkVisible = CreateWindowExA(0,
                                   "BUTTON",
                                   "Visible",
                                   WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                   panelX,
                                   panelY,
                                   120,
                                   labelH,
                                   hwnd,
                                   (HMENU)(INT_PTR)ID_CHK_VISIBLE,
                                   (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE),
                                   nullptr);
    SendMessageA(g_chkVisible, WM_SETFONT, (WPARAM)g_uiFont, TRUE);

    y += labelH + rowGap;

    g_chkCastShadows = CreateWindowExA(0,
                                       "BUTTON",
                                       "Cast shadows",
                                       WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                       panelX,
                                       panelY,
                                       140,
                                       labelH,
                                       hwnd,
                                       (HMENU)(INT_PTR)ID_CHK_CAST_SHADOWS,
                                       (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE),
                                       nullptr);
    SendMessageA(g_chkCastShadows, WM_SETFONT, (WPARAM)g_uiFont, TRUE);

    updateModeButtonText();
    updateSelectionButtons();
    updateSelectionHighlight();
    updateInspectorFromSelection();
}

static void initBitmapInfo()
{
    ZeroMemory(&g_bmi, sizeof(g_bmi));
    g_bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    g_bmi.bmiHeader.biWidth = VIEW_WIDTH;
    // Negative height makes the bitmap top-down
    g_bmi.bmiHeader.biHeight = -VIEW_HEIGHT;
    g_bmi.bmiHeader.biPlanes = 1;
    g_bmi.bmiHeader.biBitCount = 32;
    g_bmi.bmiHeader.biCompression = BI_RGB;
}

// Callback used by the PC blit helper to copy RGB565 framebuffer
// from the engine into our 32-bit window buffer.
static void pcDisplayBlit(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *src, int32_t stridePixels)
{
    if (!g_framebuffer || !src || w <= 0 || h <= 0 || stridePixels < w)
        return;

    for (int row = 0; row < h; ++row)
    {
        int dstY = y + row;
        if (dstY < 0 || dstY >= VIEW_HEIGHT)
            continue;

        const uint16_t *srcRow = src + static_cast<size_t>(row) * static_cast<size_t>(stridePixels);
        uint32_t *dstBase = g_framebuffer + static_cast<size_t>(dstY) * static_cast<size_t>(VIEW_WIDTH);

        for (int col = 0; col < w; ++col)
        {
            int dstX = x + col;
            if (dstX < 0 || dstX >= VIEW_WIDTH)
                continue;

            uint16_t c = srcRow[col];
            uint8_t r5 = static_cast<uint8_t>((c >> 11) & 0x1F);
            uint8_t g6 = static_cast<uint8_t>((c >> 5) & 0x3F);
            uint8_t b5 = static_cast<uint8_t>(c & 0x1F);

            uint8_t r8 = static_cast<uint8_t>(r5 << 3);
            uint8_t g8 = static_cast<uint8_t>(g6 << 2);
            uint8_t b8 = static_cast<uint8_t>(b5 << 3);

            uint32_t out = static_cast<uint32_t>(b8) |
                           (static_cast<uint32_t>(g8) << 8) |
                           (static_cast<uint32_t>(r8) << 16);

            dstBase[dstX] = out;
        }
    }
}

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
    if (!g_sceneFilePath.empty() && loadRuntimeScene(r))
    {
        return;
    }

    if (!g_ground)
        g_ground = new Plane(20.0f, 20.0f, 1, Color::fromRGB888(100, 100, 100));
    g_ground->setPosition(0.0f, 0.0f, 0.0f);

    const float cubeSize = 1.5f;
    if (!g_cube)
        g_cube = new Cube(cubeSize, Color::fromRGB888(220, 180, 80));
    g_cubePos = Vector3(-1.5f, cubeSize * 1.2f, 0.0f);
    g_cube->setPosition(g_cubePos.x, g_cubePos.y, g_cubePos.z);
    g_cube->setRotation(0.0f, 0.0f, 0.0f);

    const float sphereRadius = 1.0f;
    if (!g_sphere)
        g_sphere = new Sphere(sphereRadius, Color::fromRGB888(150, 200, 255));
    g_spherePos = Vector3(2.0f, sphereRadius * 1.5f, 0.0f);
    g_sphere->setPosition(g_spherePos.x, g_spherePos.y, g_spherePos.z);
    g_sphere->setRotation(0.0f, 0.0f, 0.0f);

    if (g_cube)
        g_cubeBaseColor = g_cube->color();
    if (g_sphere)
        g_sphereBaseColor = g_sphere->color();
    updateSelectionHighlight();

    if (!g_axisX)
        g_axisX = new Cube(1.0f, Color::fromRGB888(255, 80, 80));
    if (!g_axisY)
        g_axisY = new Cube(1.0f, Color::fromRGB888(80, 230, 120));
    if (!g_axisZ)
        g_axisZ = new Cube(1.0f, Color::fromRGB888(80, 140, 255));

    const float gizmoLen = 2.0f;
    const float gizmoThick = 0.05f;

    if (g_axisX)
    {
        g_axisX->setScale(gizmoLen, gizmoThick, gizmoThick);
        g_axisX->setCastShadows(false);
        g_axisX->hide();
    }
    if (g_axisY)
    {
        g_axisY->setScale(gizmoThick, gizmoLen, gizmoThick);
        g_axisY->setCastShadows(false);
        g_axisY->hide();
    }
    if (g_axisZ)
    {
        g_axisZ->setScale(gizmoThick, gizmoThick, gizmoLen);
        g_axisZ->setCastShadows(false);
        g_axisZ->hide();
    }

    r.setSkyboxWithLighting(SKYBOX_DAY);
    r.setShadowsEnabled(true);
    r.setShadowPlaneY(0.0f);
    r.setBackfaceCullingEnabled(false);
}

static void updateScene(Renderer &r, float dt)
{
    maybeReloadRuntimeScene(r);
    g_time += dt;
    Camera &cam = r.getCamera();

    bool inputActive = isInputActive();

    if (inputActive)
    {
        float moveSpeed = 5.0f * dt;
        if (GetAsyncKeyState('W') & 0x8000)
            cam.move(moveSpeed, 0.0f, 0.0f);
        if (GetAsyncKeyState('S') & 0x8000)
            cam.move(-moveSpeed, 0.0f, 0.0f);
        if (GetAsyncKeyState('A') & 0x8000)
            cam.move(0.0f, -moveSpeed, 0.0f);
        if (GetAsyncKeyState('D') & 0x8000)
            cam.move(0.0f, moveSpeed, 0.0f);
        if (GetAsyncKeyState('Q') & 0x8000)
            cam.move(0.0f, 0.0f, -moveSpeed);
        if (GetAsyncKeyState('E') & 0x8000)
            cam.move(0.0f, 0.0f, moveSpeed);

        if (g_pendingYaw != 0.0f || g_pendingPitch != 0.0f)
        {
            float rotSpeed = 0.2f;
            float yaw = g_pendingYaw * rotSpeed;
            float pitch = g_pendingPitch * rotSpeed;
            cam.rotate(yaw, pitch, true);
            g_pendingYaw = 0.0f;
            g_pendingPitch = 0.0f;
        }

        bool tabDown = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;
        if (tabDown && !g_tabPrevDown)
        {
            setEditorMode((g_editorMode == MODE_EDIT) ? MODE_PLAY : MODE_EDIT);
        }
        g_tabPrevDown = tabDown;

        bool key1Down = (GetAsyncKeyState('1') & 0x8000) != 0;
        if (key1Down && !g_key1PrevDown)
        {
            g_selectedObject = 1;
            updateSelectionButtons();
            updateSelectionHighlight();
            updateInspectorFromSelection();
        }
        g_key1PrevDown = key1Down;

        bool key2Down = (GetAsyncKeyState('2') & 0x8000) != 0;
        if (key2Down && !g_key2PrevDown)
        {
            g_selectedObject = 2;
            updateSelectionButtons();
            updateSelectionHighlight();
            updateInspectorFromSelection();
        }
        g_key2PrevDown = key2Down;
    }

    if (g_editorMode == MODE_EDIT)
    {
        Vector3 *selPos = nullptr;
        if (g_selectedObject == 1 && g_cube)
        {
            selPos = &g_cubePos;
        }
        else if (g_selectedObject == 2 && g_sphere)
        {
            selPos = &g_spherePos;
        }

        if (selPos)
        {
            float step = 3.0f * dt;
            if (GetAsyncKeyState(VK_LEFT) & 0x8000)
                selPos->x -= step;
            if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
                selPos->x += step;
            if (GetAsyncKeyState(VK_UP) & 0x8000)
                selPos->z -= step;
            if (GetAsyncKeyState(VK_DOWN) & 0x8000)
                selPos->z += step;
            if (GetAsyncKeyState(VK_PRIOR) & 0x8000)
                selPos->y += step;
            if (GetAsyncKeyState(VK_NEXT) & 0x8000)
                selPos->y -= step;
        }

        if (g_cube)
        {
            g_cube->setPosition(g_cubePos.x, g_cubePos.y, g_cubePos.z);
        }
        if (g_sphere)
        {
            g_sphere->setPosition(g_spherePos.x, g_spherePos.y, g_spherePos.z);
        }
    }
    else
    {
        if (g_cube)
        {
            float rotY = fmodf(g_time * 45.0f, 360.0f);
            float rotX = fmodf(g_time * 30.0f, 360.0f);
            g_cube->setRotation(rotX, rotY, 0.0f);
            g_cube->setPosition(g_cubePos.x, g_cubePos.y, g_cubePos.z);
        }

        if (g_sphere)
        {
            float orbitRadius = 3.0f;
            float orbitSpeed = 0.7f;
            float angle = g_time * orbitSpeed;
            float x = g_spherePos.x + cosf(angle) * orbitRadius;
            float z = g_spherePos.z + sinf(angle) * orbitRadius;
            float y = g_spherePos.y;
            g_sphere->setPosition(x, y, z);

            float rotY = fmodf(g_time * 60.0f, 360.0f);
            g_sphere->setRotation(0.0f, rotY, 0.0f);
        }
    }

    Mesh *sel = getSelectedMesh();
    if (g_editorMode == MODE_EDIT && sel)
    {
        Vector3 center = sel->pos();
        if (g_axisX)
        {
            g_axisX->setPosition(center.x, center.y, center.z);
            g_axisX->show();
        }
        if (g_axisY)
        {
            g_axisY->setPosition(center.x, center.y, center.z);
            g_axisY->show();
        }
        if (g_axisZ)
        {
            g_axisZ->setPosition(center.x, center.y, center.z);
            g_axisZ->show();
        }
    }
    else
    {
        if (g_axisX)
            g_axisX->hide();
        if (g_axisY)
            g_axisY->hide();
        if (g_axisZ)
            g_axisZ->hide();
    }

    applyRuntimeLight(r);

    cam.markDirty();

    if (!isAnyInspectorEditFocused())
    {
        updateInspectorFromSelection();
    }
}

static void renderWorld(Renderer &r)
{
    if (!g_sceneFilePath.empty())
    {
        for (Mesh *mesh : g_sceneMeshes)
        {
            if (!mesh)
                continue;
            r.drawMesh(mesh);
            if (mesh->getCastShadows())
                r.drawMeshShadow(mesh);
        }
        return;
    }

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

    if (g_editorMode == MODE_EDIT)
    {
        if (g_axisX)
            r.drawMesh(g_axisX);
        if (g_axisY)
            r.drawMesh(g_axisY);
        if (g_axisZ)
            r.drawMesh(g_axisZ);
    }
}

static void drawHud(Renderer &r)
{
    char buf[64];
    uint16_t y = 4;
    const uint16_t line = 9;

    float fps = r.getFPS();
    float avgFps = r.getAverageFPS();
    std::snprintf(buf, sizeof(buf), "FPS: %.1f AVG: %.1f", fps, avgFps);
    r.drawText(4, y, buf, Color::fromRGB888(255, 255, 0));
    y += line;

    uint32_t frameTimeUs = r.getFrameTime();
    uint32_t frameTimeMs = frameTimeUs / 1000u;
    std::snprintf(buf, sizeof(buf), "FT: %lums", static_cast<unsigned long>(frameTimeMs));
    r.drawText(4, y, buf, Color::fromRGB888(255, 255, 0));
    y += line;

    uint32_t triTotal = r.getStatsTrianglesTotal();
    uint32_t triBack = r.getStatsTrianglesBackfaceCulled();
    uint32_t instTotal = r.getStatsInstancesTotal();
    uint32_t instFrustum = r.getStatsInstancesFrustumCulled();
    uint32_t instOcc = r.getStatsInstancesOcclusionCulled();

    uint32_t triVisible = triTotal - triBack;
    uint32_t instVisible = instTotal - instFrustum - instOcc;

    std::snprintf(buf, sizeof(buf), "TRI: %lu V:%lu C:%lu",
                  static_cast<unsigned long>(triTotal),
                  static_cast<unsigned long>(triVisible),
                  static_cast<unsigned long>(triBack));
    r.drawText(4, y, buf, Color::fromRGB888(255, 255, 0));
    y += line;

    std::snprintf(buf, sizeof(buf), "INST: %lu V:%lu F:%lu O:%lu",
                  static_cast<unsigned long>(instTotal),
                  static_cast<unsigned long>(instVisible),
                  static_cast<unsigned long>(instFrustum),
                  static_cast<unsigned long>(instOcc));
    r.drawText(4, y, buf, Color::fromRGB888(255, 255, 0));
    y += line;

    const char *modeStr = (g_editorMode == MODE_EDIT) ? "EDIT" : "PLAY";
    const char *objStr = (g_selectedObject == 1) ? "CUBE" : ((g_selectedObject == 2) ? "SPHERE" : "NONE");
    std::snprintf(buf, sizeof(buf), "MODE: %s OBJ: %s", modeStr, objStr);
    r.drawText(4, y, buf, Color::fromRGB888(0, 255, 0));
    y += line;

    std::snprintf(buf, sizeof(buf), "Tab mode 1/2 obj Arrows/PgUpDn move");
    r.drawText(4, y, buf, Color::fromRGB888(0, 255, 0));
}

static void blitToWindow()
{
    HDC hdc = GetDC(g_hwnd);
    if (!hdc)
        return;

    RECT rc{};
    GetClientRect(g_hwnd, &rc);
    int dstX = 0;
    int dstY = g_hostedMode ? 0 : UI_HEIGHT;
    int dstW = g_hostedMode ? (rc.right - rc.left) : VIEW_WIDTH;
    int dstH = g_hostedMode ? (rc.bottom - rc.top) : VIEW_HEIGHT;

    StretchDIBits(hdc,
                  dstX,
                  dstY,
                  dstW,
                  dstH,
                  0,
                  0,
                  VIEW_WIDTH,
                  VIEW_HEIGHT,
                  g_framebuffer,
                  &g_bmi,
                  DIB_RGB_COLORS,
                  SRCCOPY);

    ReleaseDC(g_hwnd, hdc);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_ERASEBKGND:
    {
        HDC hdc = (HDC)wParam;
        ensureUiResources();
        RECT rc;
        GetClientRect(hwnd, &rc);

        FillRect(hdc, &rc, g_brushBg);

        if (!g_hostedMode)
        {
            RECT rcPanel = {0, 0, VIEW_WIDTH + INSPECTOR_WIDTH, UI_HEIGHT};
            FillRect(hdc, &rcPanel, g_brushPanel);
        }

        return 1;
    }
    case WM_LBUTTONDOWN:
    {
        SetFocus(hwnd);
        int x = static_cast<short>(LOWORD(lParam));
        int y = static_cast<short>(HIWORD(lParam));
        int viewX = 0;
        int viewY = 0;
        mapClientPointToView(x, y, viewX, viewY);

        bool pointInsideView = g_hostedMode
                                   ? (x >= 0 && y >= 0 && x < clientViewportWidth() && y < clientViewportHeight())
                                   : (x >= 0 && x < VIEW_WIDTH && y >= UI_HEIGHT && y < UI_HEIGHT + VIEW_HEIGHT);

        if (g_renderer && pointInsideView)
        {
            Camera &cam = g_renderer->getCamera();

            float sx = static_cast<float>(viewX) / static_cast<float>(VIEW_WIDTH);
            float sy = static_cast<float>(viewY) / static_cast<float>(VIEW_HEIGHT);
            float nx = sx * 2.0f - 1.0f;
            float ny = 1.0f - sy * 2.0f;

            float aspect = static_cast<float>(VIEW_WIDTH) / static_cast<float>(VIEW_HEIGHT);
            float tanHalfFov = tanf(cam.fov * kDegToRad * 0.5f);

            Vector3 dir = cam.forward();
            dir += cam.right() * (nx * aspect * tanHalfFov);
            dir += cam.upVec() * (ny * tanHalfFov);
            dir.normalize();

            Vector3 origin = cam.position;

            int bestId = 0;
            float bestT = 1e30f;

            auto testPick = [&](Mesh *mesh, int id)
            {
                if (!mesh)
                    return;
                Vector3 c = mesh->center();
                float r = mesh->radius();
                Vector3 oc = origin - c;
                float b = oc.dot(dir);
                float cTerm = oc.dot(oc) - r * r;
                float disc = b * b - cTerm;
                if (disc < 0.0f)
                    return;
                float s = sqrtf(disc);
                float t = -b - s;
                if (t < 0.0f)
                    t = -b + s;
                if (t > 0.0f && t < bestT)
                {
                    bestT = t;
                    bestId = id;
                }
            };

            testPick(g_cube, 1);
            testPick(g_sphere, 2);

            if (bestId == 1 || bestId == 2)
            {
                g_selectedObject = bestId;
                updateSelectionButtons();
                updateSelectionHighlight();
                updateInspectorFromSelection();
            }
        }

        return 0;
    }
    case WM_RBUTTONDOWN:
        g_rmbDown = true;
        g_lastMousePos.x = static_cast<short>(LOWORD(lParam));
        g_lastMousePos.y = static_cast<short>(HIWORD(lParam));
        SetCapture(hwnd);
        return 0;
    case WM_RBUTTONUP:
        g_rmbDown = false;
        ReleaseCapture();
        return 0;
    case WM_CTLCOLORSTATIC:
    {
        ensureUiResources();
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, UI_COLOR_TEXT);
        return (LRESULT)g_brushInspector;
    }
    case WM_CTLCOLOREDIT:
    {
        ensureUiResources();
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, UI_COLOR_BG);
        SetTextColor(hdc, UI_COLOR_TEXT);
        return (LRESULT)g_brushInspector;
    }
    case WM_CTLCOLORBTN:
    {
        ensureUiResources();
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, UI_COLOR_TEXT);
        return (LRESULT)g_brushPanel;
    }
    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
        if (dis->CtlType == ODT_BUTTON)
        {
            HDC hdc = dis->hDC;
            RECT rc = dis->rcItem;

            bool pressed = (dis->itemState & ODS_SELECTED) != 0;
            bool disabled = (dis->itemState & ODS_DISABLED) != 0;

            bool logicalSelected = false;
            if (dis->CtlID == ID_BTN_MODE && g_editorMode == MODE_PLAY)
                logicalSelected = true;
            else if (dis->CtlID == ID_BTN_SEL_CUBE && g_selectedObject == 1)
                logicalSelected = true;
            else if (dis->CtlID == ID_BTN_SEL_SPHERE && g_selectedObject == 2)
                logicalSelected = true;

            COLORREF bg = UI_COLOR_BUTTON;
            if (logicalSelected)
                bg = UI_COLOR_BUTTON_SELECTED;
            if (pressed)
                bg = UI_COLOR_BUTTON_PRESSED;

            HBRUSH br = CreateSolidBrush(bg);
            FillRect(hdc, &rc, br);
            DeleteObject(br);

            // Border
            HPEN pen = CreatePen(PS_SOLID, 1, UI_COLOR_BUTTON_BORDER);
            HGDIOBJ oldPen = SelectObject(hdc, pen);
            HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(pen);

            // Accent line for selected buttons
            if (logicalSelected)
            {
                RECT acc = rc;
                acc.top = acc.bottom - 3;
                HBRUSH brAcc = CreateSolidBrush(UI_COLOR_ACCENT);
                FillRect(hdc, &acc, brAcc);
                DeleteObject(brAcc);
            }

            // Text
            char text[64];
            GetWindowTextA(dis->hwndItem, text, sizeof(text));
            SetBkMode(hdc, TRANSPARENT);
            COLORREF textColor = disabled ? UI_COLOR_TEXT_MUTED : UI_COLOR_TEXT;
            SetTextColor(hdc, textColor);

            HFONT font = (HFONT)SendMessage(dis->hwndItem, WM_GETFONT, 0, 0);
            HFONT oldFont = nullptr;
            if (font)
                oldFont = (HFONT)SelectObject(hdc, font);

            SIZE sz{};
            int len = (int)strlen(text);
            GetTextExtentPoint32A(hdc, text, len, &sz);
            int tx = rc.left + (rc.right - rc.left - sz.cx) / 2;
            int ty = rc.top + (rc.bottom - rc.top - sz.cy) / 2;
            TextOutA(hdc, tx, ty, text, len);

            if (font && oldFont)
                SelectObject(hdc, oldFont);

            return TRUE;
        }
        break;
    }
    case WM_MOUSEMOVE:
        if (g_rmbDown)
        {
            int x = static_cast<short>(LOWORD(lParam));
            int y = static_cast<short>(HIWORD(lParam));
            int dx = x - g_lastMousePos.x;
            int dy = y - g_lastMousePos.y;
            g_lastMousePos.x = x;
            g_lastMousePos.y = y;
            g_pendingYaw += static_cast<float>(dx);
            g_pendingPitch += static_cast<float>(dy);
        }
        break;
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case ID_BTN_MODE:
            setEditorMode((g_editorMode == MODE_EDIT) ? MODE_PLAY : MODE_EDIT);
            break;
        case ID_BTN_RESET_CAM:
            if (g_renderer)
                initCamera(*g_renderer);
            break;
        case ID_BTN_SEL_CUBE:
            g_selectedObject = 1;
            updateSelectionButtons();
            updateSelectionHighlight();
            updateInspectorFromSelection();
            break;
        case ID_BTN_SEL_SPHERE:
            g_selectedObject = 2;
            updateSelectionButtons();
            updateSelectionHighlight();
            updateInspectorFromSelection();
            break;
        case ID_EDIT_POS_X:
        case ID_EDIT_POS_Y:
        case ID_EDIT_POS_Z:
        case ID_EDIT_ROT_X:
        case ID_EDIT_ROT_Y:
        case ID_EDIT_ROT_Z:
            if (HIWORD(wParam) == EN_KILLFOCUS)
                applyInspectorTransform();
            break;
        case ID_CHK_VISIBLE:
        {
            Mesh *mesh = getSelectedMesh();
            if (mesh)
            {
                LRESULT state = SendMessageA(g_chkVisible, BM_GETCHECK, 0, 0);
                if (state == BST_CHECKED)
                    mesh->show();
                else
                    mesh->hide();
            }
            break;
        }
        case ID_CHK_CAST_SHADOWS:
        {
            Mesh *mesh = getSelectedMesh();
            if (mesh)
            {
                LRESULT state = SendMessageA(g_chkCastShadows, BM_GETCHECK, 0, 0);
                mesh->setCastShadows(state == BST_CHECKED);
            }
            break;
        }
        default:
            break;
        }
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    default:
        break;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--hosted") == 0 && i + 1 < argc)
        {
            g_hostedMode = true;
            g_drawHud = false;
            auto hostValue = std::strtoull(argv[i + 1], nullptr, 0);
            g_hostParent = reinterpret_cast<HWND>(static_cast<uintptr_t>(hostValue));
            ++i;
        }
        else if (std::strcmp(argv[i], "--scene") == 0 && i + 1 < argc)
        {
            g_sceneFilePath = argv[i + 1];
            ++i;
        }
    }

    HINSTANCE hInstance = GetModuleHandle(nullptr);
    int nCmdShow = g_hostedMode ? SW_SHOW : SW_SHOWDEFAULT;
    const char CLASS_NAME[] = "PIP3D_PC_VIEWER";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    if (!RegisterClass(&wc))
    {
        return 1;
    }

    DWORD windowStyle = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    DWORD windowExStyle = 0;
    HWND parentHwnd = nullptr;
    int winWidth = VIEW_WIDTH + INSPECTOR_WIDTH;
    int winHeight = VIEW_HEIGHT + UI_HEIGHT;

    if (g_hostedMode)
    {
        windowStyle = WS_CHILD | WS_VISIBLE;
        parentHwnd = g_hostParent;
        RECT hostRect{};
        if (g_hostParent)
            GetClientRect(g_hostParent, &hostRect);
        winWidth = (hostRect.right - hostRect.left) > 0 ? (hostRect.right - hostRect.left) : VIEW_WIDTH;
        winHeight = (hostRect.bottom - hostRect.top) > 0 ? (hostRect.bottom - hostRect.top) : VIEW_HEIGHT;
    }
    else
    {
        RECT rect = {0, 0, VIEW_WIDTH + INSPECTOR_WIDTH, VIEW_HEIGHT + UI_HEIGHT};
        AdjustWindowRect(&rect, windowStyle, FALSE);
        winWidth = rect.right - rect.left;
        winHeight = rect.bottom - rect.top;
    }

    g_hwnd = CreateWindowExA(
        windowExStyle,
        CLASS_NAME,
        g_hostedMode ? "" : "PIP3D Desktop",
        windowStyle,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        winWidth,
        winHeight,
        parentHwnd,
        nullptr,
        hInstance,
        nullptr);

    if (!g_hwnd)
    {
        return 1;
    }

    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);

    if (!g_hostedMode)
        createEditorUI(g_hwnd);

    g_framebuffer = new uint32_t[VIEW_WIDTH * VIEW_HEIGHT];
    if (!g_framebuffer)
    {
        return 1;
    }

    initBitmapInfo();

    // Connect engine framebuffer to our Win32 backbuffer
    setPcBlitCallback(pcDisplayBlit);

    // Renderer имеет приватный деструктор, поэтому создаём его на куче
    // и не уничтожаем явно до завершения процесса.
    Renderer *renderer = new Renderer();
    g_renderer = renderer;

    // Full-screen display configuration for the renderer
    DisplayConfig cfg;
    cfg.width = SCREEN_WIDTH;
    cfg.height = SCREEN_HEIGHT;
    cfg.cs = 0;
    cfg.dc = 0;
    cfg.rst = 0;
    cfg.bl = -1;
    cfg.spi_freq = 60000000;

    if (!renderer->init(cfg))
    {
        return 1;
    }

    initCamera(*renderer);
    initScene(*renderer);

    MSG msg = {};
    bool running = true;

    // Simple fixed-step loop (~60 FPS)
    const DWORD frameMs = 5; // ~16ms

    g_lastMs = GetTickCount();

    while (running)
    {
        if (g_hostedMode && g_hostParent && IsWindow(g_hostParent))
        {
            RECT hostRect{};
            GetClientRect(g_hostParent, &hostRect);
            MoveWindow(g_hwnd, 0, 0, hostRect.right - hostRect.left, hostRect.bottom - hostRect.top, FALSE);
        }

        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (!running)
            break;

        DWORD nowMs = GetTickCount();
        DWORD dtMs = nowMs - g_lastMs;
        g_lastMs = nowMs;

        float dt = static_cast<float>(dtMs) * 0.001f;
        if (dt > 0.1f)
            dt = 0.1f;

        renderer->getCamera().markDirty();
        updateScene(*renderer, dt);

        // Render in bands, same как на ESP
        for (int band = 0; band < SCREEN_BAND_COUNT; ++band)
        {
            renderer->beginFrameBand(band);

            renderWorld(*renderer);
            renderer->drawSkyboxBackground();
            if (band == 0 && g_drawHud)
            {
                drawHud(*renderer);
            }

            renderer->endFrameBand(band);
        }

        blitToWindow();

        Sleep(frameMs);
    }

    delete[] g_framebuffer;
    g_framebuffer = nullptr;

    return 0;
}
