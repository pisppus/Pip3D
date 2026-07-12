#include "Debug/Gizmos.hpp"
#include "Math/Algebra.hpp"
#include "Math/Collision.hpp"
#include "Rendering/Renderer.hpp"

#if PIP3D_ENABLE_GIZMOS

namespace pip3D
{
    namespace Debug
    {

        static DRAM_ATTR DebugLine g_lines[Gizmos::MAX_DEBUG_LINES];
        static DRAM_ATTR uint16_t g_lineCount = 0;
        static DRAM_ATTR uint16_t g_categoriesMask = DEBUG_CATEGORY_ALL;

        static PIP3D_FORCE_INLINE bool isCategoryEnabled(uint16_t categories)
        {
            return (g_categoriesMask & categories) != 0u;
        }

        static PIP3D_FORCE_INLINE void pushLine(const Vector3 &a, const Vector3 &b,
                                                uint16_t color, uint16_t categories,
                                                uint16_t lifetimeFrames)
        {
            if (g_lineCount < Gizmos::MAX_DEBUG_LINES)
            {
                DebugLine *ln = &g_lines[g_lineCount++];
                ln->a = a;
                ln->b = b;
                ln->color = color;
                ln->categories = categories;
                ln->framesLeft = lifetimeFrames;
            }
        }

        void Gizmos::setCategories(uint16_t mask) { g_categoriesMask = mask; }
        void Gizmos::enableCategories(uint16_t mask) { g_categoriesMask |= mask; }
        void Gizmos::disableCategories(uint16_t mask) { g_categoriesMask &= static_cast<uint16_t>(~mask); }
        uint16_t Gizmos::getCategories() { return g_categoriesMask; }

        void Gizmos::beginFrame()
        {
            if (g_lineCount == 0)
                return;

            uint16_t i = 0;
            while (i < g_lineCount)
            {
                DebugLine &ln = g_lines[i];
                if (ln.framesLeft > 0)
                {
                    if (--ln.framesLeft == 0)
                    {
                        ln = g_lines[--g_lineCount];
                        continue;
                    }
                }
                ++i;
            }
        }

        bool Gizmos::hasPrimitives() { return g_lineCount > 0; }

        void Gizmos::addLine(const Vector3 &a, const Vector3 &b,
                             uint16_t color, uint16_t categories,
                             uint16_t lifetimeFrames)
        {
            if (!isCategoryEnabled(categories))
                return;
            pushLine(a, b, color, categories, lifetimeFrames);
        }

        void Gizmos::addRay(const Vector3 &origin, const Vector3 &dir, float length,
                            uint16_t color, uint16_t categories,
                            uint16_t lifetimeFrames)
        {
            if (!isCategoryEnabled(categories) || length <= 0.0f)
                return;
            const float lenSq = dir.lengthSquared();
            if (lenSq < 1e-8f)
                return;
            const float invLen = FastMath::fastInvSqrt(lenSq) * length;
            pushLine(origin, origin + dir * invLen, color, categories, lifetimeFrames);
        }

        void Gizmos::addArrow(const Vector3 &origin, const Vector3 &dir,
                              float length, float headSize,
                              uint16_t color, uint16_t categories,
                              uint16_t lifetimeFrames)
        {
            if (!isCategoryEnabled(categories) || length <= 0.0f)
                return;

            const float lenSq = dir.lengthSquared();
            if (lenSq < 1e-8f)
                return;
            const float invLen = FastMath::fastInvSqrt(lenSq);
            const Vector3 unitD = dir * invLen;
            const Vector3 tip = origin + unitD * length;

            pushLine(origin, tip, color, categories, lifetimeFrames);

            const float h = (headSize > 0.0f) ? headSize : (length * 0.15f);
            const Vector3 ref = (fabsf(unitD.x) < 0.9f) ? Vector3(1.0f, 0.0f, 0.0f)
                                                        : Vector3(0.0f, 1.0f, 0.0f);
            Vector3 u = unitD.cross(ref);
            u = u * FastMath::fastInvSqrt(u.lengthSquared());
            const Vector3 v = unitD.cross(u);
            const Vector3 uH = u * h;
            const Vector3 vH = v * h;
            const Vector3 back = unitD * (-h);

            pushLine(tip, tip + back + uH, color, categories, lifetimeFrames);
            pushLine(tip, tip + back - uH, color, categories, lifetimeFrames);
            pushLine(tip, tip + back + vH, color, categories, lifetimeFrames);
        }

        void Gizmos::addAABB(const AABB &box, uint16_t color,
                             uint16_t categories, uint16_t lifetimeFrames)
        {
            if (!isCategoryEnabled(categories))
                return;
            const float mnX = box.min.x, mnY = box.min.y, mnZ = box.min.z;
            const float mxX = box.max.x, mxY = box.max.y, mxZ = box.max.z;
            const Vector3 c000(mnX, mnY, mnZ);
            const Vector3 c001(mnX, mnY, mxZ);
            const Vector3 c010(mnX, mxY, mnZ);
            const Vector3 c011(mnX, mxY, mxZ);
            const Vector3 c100(mxX, mnY, mnZ);
            const Vector3 c101(mxX, mnY, mxZ);
            const Vector3 c110(mxX, mxY, mnZ);
            const Vector3 c111(mxX, mxY, mxZ);

            pushLine(c000, c100, color, categories, lifetimeFrames);
            pushLine(c100, c110, color, categories, lifetimeFrames);
            pushLine(c110, c010, color, categories, lifetimeFrames);
            pushLine(c010, c000, color, categories, lifetimeFrames);
            pushLine(c001, c101, color, categories, lifetimeFrames);
            pushLine(c101, c111, color, categories, lifetimeFrames);
            pushLine(c111, c011, color, categories, lifetimeFrames);
            pushLine(c011, c001, color, categories, lifetimeFrames);
            pushLine(c000, c001, color, categories, lifetimeFrames);
            pushLine(c100, c101, color, categories, lifetimeFrames);
            pushLine(c110, c111, color, categories, lifetimeFrames);
            pushLine(c010, c011, color, categories, lifetimeFrames);
        }

        void Gizmos::addOBB(const Vector3 &center, const Vector3 &half,
                            const Quaternion &orientation,
                            uint16_t color, uint16_t categories,
                            uint16_t lifetimeFrames)
        {
            if (!isCategoryEnabled(categories))
                return;

            const float qx = orientation.x + orientation.x;
            const float qy = orientation.y + orientation.y;
            const float qz = orientation.z + orientation.z;
            const float qxx = orientation.x * qx;
            const float qyy = orientation.y * qy;
            const float qzz = orientation.z * qz;
            const float qxy = orientation.x * qy;
            const float qxz = orientation.x * qz;
            const float qyz = orientation.y * qz;
            const float qwx = orientation.w * qx;
            const float qwy = orientation.w * qy;
            const float qwz = orientation.w * qz;
            const float r0 = 1.0f - qyy - qzz;
            const float r1 = qxy + qwz;
            const float r2 = qxz - qwy;
            const float r3 = qxy - qwz;
            const float r4 = 1.0f - qxx - qzz;
            const float r5 = qyz + qwx;
            const float r6 = qxz + qwy;
            const float r7 = qyz - qwx;
            const float r8 = 1.0f - qxx - qyy;

            const float hx = half.x, hy = half.y, hz = half.z;
            const float s[8][3] = {
                {-hx, -hy, -hz},
                {hx, -hy, -hz},
                {hx, hy, -hz},
                {-hx, hy, -hz},
                {-hx, -hy, hz},
                {hx, -hy, hz},
                {hx, hy, hz},
                {-hx, hy, hz},
            };
            Vector3 corners[8];
            for (int i = 0; i < 8; ++i)
            {
                corners[i] = center + Vector3(
                                          r0 * s[i][0] + r1 * s[i][1] + r2 * s[i][2],
                                          r3 * s[i][0] + r4 * s[i][1] + r5 * s[i][2],
                                          r6 * s[i][0] + r7 * s[i][1] + r8 * s[i][2]);
            }

            static constexpr int edges[12][2] = {
                {0, 1},
                {1, 2},
                {2, 3},
                {3, 0},
                {4, 5},
                {5, 6},
                {6, 7},
                {7, 4},
                {0, 4},
                {1, 5},
                {2, 6},
                {3, 7},
            };
            for (int e = 0; e < 12; ++e)
                pushLine(corners[edges[e][0]], corners[edges[e][1]],
                         color, categories, lifetimeFrames);
        }

        void Gizmos::addSphere(const Vector3 &center, float radius, uint16_t color,
                               uint16_t categories, uint16_t lifetimeFrames,
                               int segments)
        {
            if (!isCategoryEnabled(categories) || radius <= 0.0f)
                return;
            if (segments < 4)
                segments = 4;
            if (segments > 64)
                segments = 64;

            const float step = kTwoPi / static_cast<float>(segments);
            Vector3 prev1(center.x + radius, center.y, center.z);
            Vector3 prev2(center.x + radius, center.y, center.z);
            Vector3 prev3(center.x, center.y + radius, center.z);

            for (int i = 1; i <= segments; ++i)
            {
                float sa, ca;
                FastMath::fastSinCos(step * i, sa, ca);
                const float rca = radius * ca;
                const float rsa = radius * sa;

                const Vector3 p1(center.x + rca, center.y, center.z + rsa);
                const Vector3 p2(center.x + rca, center.y + rsa, center.z);
                const Vector3 p3(center.x, center.y + rca, center.z + rsa);

                pushLine(prev1, p1, color, categories, lifetimeFrames);
                pushLine(prev2, p2, color, categories, lifetimeFrames);
                pushLine(prev3, p3, color, categories, lifetimeFrames);
                prev1 = p1;
                prev2 = p2;
                prev3 = p3;
            }
        }

        void Gizmos::addCircle(const Vector3 &center, const Vector3 &normal,
                               float radius, uint16_t color,
                               uint16_t categories, uint16_t lifetimeFrames,
                               int segments)
        {
            if (!isCategoryEnabled(categories) || radius <= 0.0f)
                return;
            if (segments < 4)
                segments = 4;
            if (segments > 64)
                segments = 64;

            const float nLenSq = normal.lengthSquared();
            if (nLenSq < 1e-8f)
                return;
            const Vector3 n = normal * FastMath::fastInvSqrt(nLenSq);

            const Vector3 ref = (fabsf(n.x) < 0.9f) ? Vector3(1.0f, 0.0f, 0.0f)
                                                    : Vector3(0.0f, 1.0f, 0.0f);
            Vector3 u = n.cross(ref);
            u = u * FastMath::fastInvSqrt(u.lengthSquared());
            const Vector3 v = n.cross(u);

            const float step = kTwoPi / static_cast<float>(segments);
            Vector3 prev = center + u * radius;
            for (int i = 1; i <= segments; ++i)
            {
                float sa, ca;
                FastMath::fastSinCos(step * i, sa, ca);
                const Vector3 p = center + u * (radius * ca) + v * (radius * sa);
                pushLine(prev, p, color, categories, lifetimeFrames);
                prev = p;
            }
        }

        void Gizmos::addCapsule(const Vector3 &p0, const Vector3 &p1, float radius,
                                uint16_t color, uint16_t categories,
                                uint16_t lifetimeFrames)
        {
            if (!isCategoryEnabled(categories) || radius <= 0.0f)
                return;

            const Vector3 axis = p1 - p0;
            const float axisLenSq = axis.lengthSquared();
            if (axisLenSq < 1e-8f)
            {
                addSphere(p0, radius, color, categories, lifetimeFrames);
                return;
            }
            const Vector3 n = axis * FastMath::fastInvSqrt(axisLenSq);

            const Vector3 ref = (fabsf(n.x) < 0.9f) ? Vector3(1.0f, 0.0f, 0.0f)
                                                    : Vector3(0.0f, 1.0f, 0.0f);
            Vector3 u = n.cross(ref);
            u = u * FastMath::fastInvSqrt(u.lengthSquared());
            const Vector3 v = n.cross(u);
            const Vector3 ru = u * radius;
            const Vector3 rv = v * radius;

            pushLine(p0 + ru, p1 + ru, color, categories, lifetimeFrames);
            pushLine(p0 - ru, p1 - ru, color, categories, lifetimeFrames);
            pushLine(p0 + rv, p1 + rv, color, categories, lifetimeFrames);
            pushLine(p0 - rv, p1 - rv, color, categories, lifetimeFrames);

            addCircle(p0, n, radius, color, categories, lifetimeFrames, 8);
            addCircle(p1, n, radius, color, categories, lifetimeFrames, 8);
        }

        void Gizmos::addAxes(const Vector3 &origin, float size,
                             uint16_t categories, uint16_t lifetimeFrames)
        {
            if (!isCategoryEnabled(categories) || size <= 0.0f)
                return;
            pushLine(origin, origin + Vector3(size, 0.0f, 0.0f), 0xF800u, categories, lifetimeFrames);
            pushLine(origin, origin + Vector3(0.0f, size, 0.0f), 0x07E0u, categories, lifetimeFrames);
            pushLine(origin, origin + Vector3(0.0f, 0.0f, size), 0x001Fu, categories, lifetimeFrames);
        }

        void Gizmos::addTriangle(const Vector3 &a, const Vector3 &b, const Vector3 &c,
                                 uint16_t color, uint16_t categories,
                                 uint16_t lifetimeFrames)
        {
            if (!isCategoryEnabled(categories))
                return;
            pushLine(a, b, color, categories, lifetimeFrames);
            pushLine(b, c, color, categories, lifetimeFrames);
            pushLine(c, a, color, categories, lifetimeFrames);
        }

        void Gizmos::addFrustum(const Vector3 corners[8], uint16_t color,
                                uint16_t categories, uint16_t lifetimeFrames)
        {
            if (!isCategoryEnabled(categories))
                return;
            pushLine(corners[0], corners[1], color, categories, lifetimeFrames);
            pushLine(corners[1], corners[2], color, categories, lifetimeFrames);
            pushLine(corners[2], corners[3], color, categories, lifetimeFrames);
            pushLine(corners[3], corners[0], color, categories, lifetimeFrames);
            pushLine(corners[4], corners[5], color, categories, lifetimeFrames);
            pushLine(corners[5], corners[6], color, categories, lifetimeFrames);
            pushLine(corners[6], corners[7], color, categories, lifetimeFrames);
            pushLine(corners[7], corners[4], color, categories, lifetimeFrames);
            pushLine(corners[0], corners[4], color, categories, lifetimeFrames);
            pushLine(corners[1], corners[5], color, categories, lifetimeFrames);
            pushLine(corners[2], corners[6], color, categories, lifetimeFrames);
            pushLine(corners[3], corners[7], color, categories, lifetimeFrames);
        }

        void Gizmos::addGrid(float originY, float sizeX, float sizeZ,
                             int divisionsX, int divisionsZ,
                             uint16_t color, uint16_t categories,
                             uint16_t lifetimeFrames)
        {
            if (!isCategoryEnabled(categories))
                return;
            if (divisionsX < 1)
                divisionsX = 1;
            if (divisionsZ < 1)
                divisionsZ = 1;

            const int remaining = Gizmos::MAX_DEBUG_LINES - static_cast<int>(g_lineCount);
            if (remaining < 4)
                return;

            const int need = (divisionsX + 1) + (divisionsZ + 1);
            if (need > remaining)
            {
                const float ratio = static_cast<float>(remaining) / static_cast<float>(need);
                divisionsX = static_cast<int>(divisionsX * ratio);
                divisionsZ = static_cast<int>(divisionsZ * ratio);
                if (divisionsX < 1)
                    divisionsX = 1;
                if (divisionsZ < 1)
                    divisionsZ = 1;
            }

            const float stepX = sizeX / static_cast<float>(divisionsX);
            const float stepZ = sizeZ / static_cast<float>(divisionsZ);
            const float x0 = -sizeX * 0.5f;
            const float z0 = -sizeZ * 0.5f;

            for (int i = 0; i <= divisionsZ; ++i)
            {
                const float z = z0 + stepZ * static_cast<float>(i);
                pushLine(Vector3(x0, originY, z),
                         Vector3(x0 + sizeX, originY, z),
                         color, categories, lifetimeFrames);
            }
            for (int i = 0; i <= divisionsX; ++i)
            {
                const float x = x0 + stepX * static_cast<float>(i);
                pushLine(Vector3(x, originY, z0),
                         Vector3(x, originY, z0 + sizeZ),
                         color, categories, lifetimeFrames);
            }
        }

        static PIP3D_FORCE_INLINE bool clipLineLocal(int &x0, int &y0,
                                                     int &x1, int &y1,
                                                     int vpW, int vpH)
        {
            const int dx = x1 - x0;
            const int dy = y1 - y0;
            float t0 = 0.0f;
            float t1 = 1.0f;

            const int p[4] = {-dx, dx, -dy, dy};
            const int q[4] = {x0, vpW - 1 - x0, y0, vpH - 1 - y0};

            for (int i = 0; i < 4; ++i)
            {
                const int pi = p[i];
                if (pi == 0)
                {
                    if (q[i] < 0)
                        return false;
                    continue;
                }
                const float r = static_cast<float>(q[i]) / static_cast<float>(pi);
                if (pi < 0)
                {
                    if (r > t1)
                        return false;
                    if (r > t0)
                        t0 = r;
                }
                else
                {
                    if (r < t0)
                        return false;
                    if (r < t1)
                        t1 = r;
                }
            }

            const int newDx = static_cast<int>((t1 - t0) * dx);
            const int newDy = static_cast<int>((t1 - t0) * dy);
            x0 += static_cast<int>(t0 * dx);
            y0 += static_cast<int>(t0 * dy);
            x1 = x0 + newDx;
            y1 = y0 + newDy;
            return true;
        }

        static PIP3D_FORCE_INLINE bool projectToScreen(const Matrix4x4 &vp,
                                                       float halfW, float preHalfW,
                                                       float halfH, float preHalfH,
                                                       const Vector3 &world,
                                                       int &sx, int &sy)
        {
            const float *PIP3D_RESTRICT m = vp.m;
            const float clipW = m[3] * world.x + m[7] * world.y + m[11] * world.z + m[15];
            if (clipW <= 1e-4f)
                return false;

            const float invW = FastMath::fastReciprocal(clipW);
            const float ndcX = (m[0] * world.x + m[4] * world.y + m[8] * world.z + m[12]) * invW;
            const float ndcY = (m[1] * world.x + m[5] * world.y + m[9] * world.z + m[13]) * invW;

            sx = static_cast<int>(ndcX * halfW + preHalfW);
            sy = static_cast<int>(-ndcY * halfH + preHalfH);
            return true;
        }

        static PIP3D_FORCE_INLINE void drawLine2D(uint16_t *PIP3D_RESTRICT fb,
                                                  int vpW,
                                                  int x0, int y0, int x1, int y1,
                                                  uint16_t color)
        {
            int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
            int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
            const int sx = (x0 < x1) ? 1 : -1;
            const int sy = (y0 < y1) ? 1 : -1;
            int err = dx - dy;

            uint16_t *PIP3D_RESTRICT p = fb + static_cast<size_t>(y0) * vpW + x0;
            const int rowStep = sy * vpW;

            while (true)
            {
                *p = color;
                if (x0 == x1 && y0 == y1)
                    break;
                const int e2 = err * 2;
                if (e2 > -dy)
                {
                    err -= dy;
                    x0 += sx;
                    p += sx;
                }
                if (e2 < dx)
                {
                    err += dx;
                    y0 += sy;
                    p += rowStep;
                }
            }
        }

        void Gizmos::render(Renderer &renderer)
        {
            if (g_lineCount == 0)
                return;

            uint16_t *PIP3D_RESTRICT fb = renderer.getFrameBuffer();
            if (!fb)
                return;

            const Viewport &vp = renderer.getViewport();
            if (vp.width == 0 || vp.height == 0)
                return;

            const Matrix4x4 &viewProj = renderer.getViewProjMatrix();
            const float halfW = static_cast<float>(vp.width) * 0.5f;
            const float halfH = static_cast<float>(vp.height) * 0.5f;
            const float preHalfW = halfW + static_cast<float>(vp.x);
            const float preHalfH = halfH + static_cast<float>(vp.y);

            const int bandTop = static_cast<int>(g_bandOffsetY);
            const int bandH = static_cast<int>(g_bandHeight);
            const int vpW = static_cast<int>(vp.width);

            const uint16_t count = g_lineCount;
            for (uint16_t i = 0; i < count; ++i)
            {
                const DebugLine &ln = g_lines[i];

                int x0, y0, x1, y1;
                if (!projectToScreen(viewProj, halfW, preHalfW, halfH, preHalfH, ln.a, x0, y0))
                    continue;
                if (!projectToScreen(viewProj, halfW, preHalfW, halfH, preHalfH, ln.b, x1, y1))
                    continue;

                y0 -= bandTop;
                y1 -= bandTop;

                if (!clipLineLocal(x0, y0, x1, y1, vpW, bandH))
                    continue;

                drawLine2D(fb, vpW, x0, y0, x1, y1, ln.color);
            }
        }
    }
}

#else

namespace pip3D
{
    namespace Debug
    {
    }
}

#endif