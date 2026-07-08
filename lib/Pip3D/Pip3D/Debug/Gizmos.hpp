#pragma once

#include "Core/Platform.hpp"

#include "Debug/Flags.hpp"
#include "Math/Algebra.hpp"
#include "Math/Collision.hpp"

namespace pip3D
{
    class Renderer;

    namespace Debug
    {
        enum DebugCategory : uint16_t
        {
            DEBUG_CATEGORY_NONE = 0,
            DEBUG_CATEGORY_PHYSICS = 1u << 0,
            DEBUG_CATEGORY_CAMERA = 1u << 1,
            DEBUG_CATEGORY_USER = 1u << 2,
            DEBUG_CATEGORY_ALL = 0xFFFFu
        };

        struct alignas(32) DebugLine
        {
            Vector3 a;
            Vector3 b;
            uint16_t color;
            uint16_t categories;
            uint16_t framesLeft;
            uint16_t _pad;
        };
        static_assert(sizeof(DebugLine) == 32, "DebugLine must be 32 bytes");

        class Gizmos
        {
        public:
            static void setCategories(uint16_t mask);
            static void enableCategories(uint16_t mask);
            static void disableCategories(uint16_t mask);
            static uint16_t getCategories();

            static void setProfileOff() { setCategories(DEBUG_CATEGORY_NONE); }
            static void setProfilePhysics() { setCategories(DEBUG_CATEGORY_PHYSICS); }
            static void setProfileAll() { setCategories(DEBUG_CATEGORY_ALL); }

            static void beginFrame();
            static bool hasPrimitives();

            static void addLine(const Vector3 &a, const Vector3 &b,
                                uint16_t color,
                                uint16_t categories = DEBUG_CATEGORY_USER,
                                uint16_t lifetimeFrames = 1);

            static void addRay(const Vector3 &origin, const Vector3 &dir,
                               float length,
                               uint16_t color,
                               uint16_t categories = DEBUG_CATEGORY_USER,
                               uint16_t lifetimeFrames = 1);

            static void addArrow(const Vector3 &origin, const Vector3 &dir,
                                 float length, float headSize,
                                 uint16_t color,
                                 uint16_t categories = DEBUG_CATEGORY_USER,
                                 uint16_t lifetimeFrames = 1);

            static void addAABB(const AABB &box,
                                uint16_t color,
                                uint16_t categories = DEBUG_CATEGORY_PHYSICS,
                                uint16_t lifetimeFrames = 1);

            static void addOBB(const Vector3 &center, const Vector3 &half,
                               const Quaternion &orientation,
                               uint16_t color,
                               uint16_t categories = DEBUG_CATEGORY_PHYSICS,
                               uint16_t lifetimeFrames = 1);

            static void addSphere(const Vector3 &center, float radius,
                                  uint16_t color,
                                  uint16_t categories = DEBUG_CATEGORY_PHYSICS,
                                  uint16_t lifetimeFrames = 1,
                                  int segments = 12);

            static void addCircle(const Vector3 &center, const Vector3 &normal,
                                  float radius,
                                  uint16_t color,
                                  uint16_t categories = DEBUG_CATEGORY_PHYSICS,
                                  uint16_t lifetimeFrames = 1,
                                  int segments = 12);

            static void addCapsule(const Vector3 &p0, const Vector3 &p1,
                                   float radius,
                                   uint16_t color,
                                   uint16_t categories = DEBUG_CATEGORY_PHYSICS,
                                   uint16_t lifetimeFrames = 1);

            static void addAxes(const Vector3 &origin, float size,
                                uint16_t categories = DEBUG_CATEGORY_CAMERA,
                                uint16_t lifetimeFrames = 1);

            static void addTriangle(const Vector3 &a, const Vector3 &b,
                                    const Vector3 &c,
                                    uint16_t color,
                                    uint16_t categories = DEBUG_CATEGORY_USER,
                                    uint16_t lifetimeFrames = 1);

            static void addFrustum(const Vector3 corners[8],
                                   uint16_t color,
                                   uint16_t categories = DEBUG_CATEGORY_CAMERA,
                                   uint16_t lifetimeFrames = 1);

            static void addGrid(float originY, float sizeX, float sizeZ,
                                int divisionsX, int divisionsZ,
                                uint16_t color,
                                uint16_t categories = DEBUG_CATEGORY_USER,
                                uint16_t lifetimeFrames = 0);

            static void render(Renderer &renderer);

            static constexpr int MAX_DEBUG_LINES = 256;
        };
    }
}

#if PIP3D_ENABLE_GIZMOS

#define DBG_LINE(a, b, color, ...) ::pip3D::Debug::Gizmos::addLine((a), (b), (color), ##__VA_ARGS__)
#define DBG_RAY(origin, dir, len, color, ...) ::pip3D::Debug::Gizmos::addRay((origin), (dir), (len), (color), ##__VA_ARGS__)
#define DBG_ARROW(origin, dir, len, head, color, ...) \
    ::pip3D::Debug::Gizmos::addArrow((origin), (dir), (len), (head), (color), ##__VA_ARGS__)
#define DBG_AABB(box, color, ...) ::pip3D::Debug::Gizmos::addAABB((box), (color), ##__VA_ARGS__)
#define DBG_OBB(center, half, q, color, ...) ::pip3D::Debug::Gizmos::addOBB((center), (half), (q), (color), ##__VA_ARGS__)
#define DBG_SPHERE(c, r, color, ...) ::pip3D::Debug::Gizmos::addSphere((c), (r), (color), ##__VA_ARGS__)
#define DBG_CIRCLE(c, n, r, color, ...) ::pip3D::Debug::Gizmos::addCircle((c), (n), (r), (color), ##__VA_ARGS__)
#define DBG_CAPSULE(p0, p1, r, color, ...) ::pip3D::Debug::Gizmos::addCapsule((p0), (p1), (r), (color), ##__VA_ARGS__)
#define DBG_AXES(origin, size, ...) ::pip3D::Debug::Gizmos::addAxes((origin), (size), ##__VA_ARGS__)
#define DBG_TRIANGLE(a, b, c, color, ...) ::pip3D::Debug::Gizmos::addTriangle((a), (b), (c), (color), ##__VA_ARGS__)
#define DBG_FRUSTUM(corners, color, ...) ::pip3D::Debug::Gizmos::addFrustum((corners), (color), ##__VA_ARGS__)
#define DBG_GRID(originY, sx, sz, dx, dz, color, ...) \
    ::pip3D::Debug::Gizmos::addGrid((originY), (sx), (sz), (dx), (dz), (color), ##__VA_ARGS__)

#else

#define DBG_LINE(a, b, color, ...) ((void)0)
#define DBG_RAY(origin, dir, len, color, ...) ((void)0)
#define DBG_ARROW(origin, dir, len, head, color, ...) ((void)0)
#define DBG_AABB(box, color, ...) ((void)0)
#define DBG_OBB(center, half, q, color, ...) ((void)0)
#define DBG_SPHERE(c, r, color, ...) ((void)0)
#define DBG_CIRCLE(c, n, r, color, ...) ((void)0)
#define DBG_CAPSULE(p0, p1, r, color, ...) ((void)0)
#define DBG_AXES(origin, size, ...) ((void)0)
#define DBG_TRIANGLE(a, b, c, color, ...) ((void)0)
#define DBG_FRUSTUM(corners, color, ...) ((void)0)
#define DBG_GRID(originY, sx, sz, dx, dz, color, ...) ((void)0)

#endif