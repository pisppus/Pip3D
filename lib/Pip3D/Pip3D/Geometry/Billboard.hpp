#pragma once

#include "Math/Algebra.hpp"
#include "Core/Color.hpp"
#include "Rendering/Display/Texture.hpp"
#include "Debug/Logging.hpp"
#include <vector>
#include <algorithm>
#include <cmath>

namespace pip3D
{
    class Renderer;

    enum BillboardOrientation : uint8_t
    {
        BB_SCREEN_ALIGNED = 0,
        BB_AXIAL_Y = 1,
        BB_FIXED_YAW = 2
    };

    enum BillboardBlend : uint8_t
    {
        BB_BLEND_OPAQUE = 0,
        BB_BLEND_CUTOUT = 1,
        BB_BLEND_ALPHA = 2,
        BB_BLEND_ADDITIVE = 3
    };

    struct Billboard
    {
        Vector3 position;
        float width;
        float height;
        const Texture *texture;
        Color tint;
        uint16_t chromaKey;
        uint8_t alpha;
        float yawDeg;
        BillboardOrientation orientation;
        BillboardBlend blend;
        bool screenSpaceSize;
        bool visible;
        bool lit;

        Billboard()
            : position(0, 0, 0), width(1.0f), height(1.0f), texture(nullptr),
              tint(Color::WHITE), chromaKey(0xF81F), alpha(255), yawDeg(0.0f),
              orientation(BB_SCREEN_ALIGNED), blend(BB_BLEND_CUTOUT),
              screenSpaceSize(false), visible(true), lit(false) {}

        Billboard *at(float x, float y, float z)
        {
            position = Vector3(x, y, z);
            return this;
        }
        Billboard *at(const Vector3 &pos)
        {
            position = pos;
            return this;
        }
        Billboard *size(float w, float h)
        {
            width = w;
            height = h;
            return this;
        }
        Billboard *size(float s)
        {
            width = s;
            height = s;
            return this;
        }
        Billboard *tex(const Texture *t)
        {
            texture = t;
            return this;
        }
        Billboard *tintCol(const Color &c)
        {
            tint = c;
            return this;
        }
        Billboard *face(BillboardOrientation o)
        {
            orientation = o;
            return this;
        }
        Billboard *blendMode(BillboardBlend b)
        {
            blend = b;
            return this;
        }
        Billboard *chroma(uint16_t key)
        {
            chromaKey = key;
            return this;
        }
        Billboard *transparency(uint8_t a)
        {
            alpha = a;
            return this;
        }
        Billboard *yaw(float deg)
        {
            yawDeg = deg;
            orientation = BB_FIXED_YAW;
            return this;
        }
        Billboard *screenSize(bool s)
        {
            screenSpaceSize = s;
            return this;
        }
        Billboard *litFlag(bool l)
        {
            lit = l;
            return this;
        }
        Billboard *show()
        {
            visible = true;
            return this;
        }
        Billboard *hide()
        {
            visible = false;
            return this;
        }

        Billboard *asCutout()
        {
            blend = BB_BLEND_CUTOUT;
            return this;
        }
        Billboard *asAlpha()
        {
            blend = BB_BLEND_ALPHA;
            return this;
        }
        Billboard *asOpaque()
        {
            blend = BB_BLEND_OPAQUE;
            return this;
        }
        Billboard *asAdditive()
        {
            blend = BB_BLEND_ADDITIVE;
            return this;
        }
        Billboard *asScreenAligned()
        {
            orientation = BB_SCREEN_ALIGNED;
            return this;
        }
        Billboard *asAxialY()
        {
            orientation = BB_AXIAL_Y;
            return this;
        }
    };

    class BillboardManager
    {
    private:
        std::vector<Billboard *> billboards;
        std::vector<Billboard *> pool;

        struct SortedBillboard
        {
            float distanceSq;
            Billboard *billboard;
            bool operator<(const SortedBillboard &other) const
            {
                return distanceSq > other.distanceSq;
            }
        };

        static bool buildQuad(const Billboard &bb,
                              const Vector3 &camPos,
                              const Vector3 &camRight,
                              const Vector3 &camUp,
                              const Vector3 &camForward,
                              float projScale,
                              float halfViewportHeight,
                              bool perspective,
                              Vector3 outWorld[4],
                              float &outCenterDist)
        {
            const Vector3 toObj = bb.position - camPos;
            const float distSq = toObj.lengthSquared();
            if (unlikely(distSq < 1e-6f))
                return false;
            const float dist = sqrtf(distSq);
            outCenterDist = dist;

            const float zView = toObj.dot(camForward);
            if (perspective && zView <= 0.01f)
                return false;

            float halfW, halfH;
            if (bb.screenSpaceSize)
            {
                if (perspective && zView > 0.01f)
                {
                    const float invZ = FastMath::fastReciprocal(zView);
                    halfW = (bb.width * 0.5f) * zView * FastMath::fastReciprocal(projScale * halfViewportHeight);
                    halfH = (bb.height * 0.5f) * zView * FastMath::fastReciprocal(projScale * halfViewportHeight);
                }
                else
                {
                    halfW = bb.width * 0.5f;
                    halfH = bb.height * 0.5f;
                }
            }
            else
            {
                halfW = bb.width * 0.5f;
                halfH = bb.height * 0.5f;
            }

            Vector3 right, up;
            switch (bb.orientation)
            {
            case BB_SCREEN_ALIGNED:
                right = camRight;
                up = camUp - camForward * camUp.dot(camForward);
                up.normalize();
                break;
            case BB_AXIAL_Y:
            {
                right = Vector3(camRight.x, 0.0f, camRight.z);
                if (unlikely(right.lengthSquared() < 1e-6f))
                {
                    right = Vector3(camForward.x, 0.0f, camForward.z);
                }
                right.normalize();
                up = Vector3(0.0f, 1.0f, 0.0f);
                break;
            }
            case BB_FIXED_YAW:
            {
                const float yr = bb.yawDeg * kDegToRad;
                float s, c;
                FastMath::fastSinCos(yr, s, c);
                right = Vector3(c, 0.0f, -s);
                up = Vector3(0.0f, 1.0f, 0.0f);
                break;
            }
            default:
                right = camRight;
                up = camUp;
                break;
            }

            const Vector3 c = bb.position;
            outWorld[0] = c - right * halfW - up * halfH;
            outWorld[1] = c + right * halfW - up * halfH;
            outWorld[2] = c + right * halfW + up * halfH;
            outWorld[3] = c - right * halfW + up * halfH;
            return true;
        }

    public:
        BillboardManager() = default;
        ~BillboardManager() { destroyAll(); }

        BillboardManager(const BillboardManager &) = delete;
        BillboardManager &operator=(const BillboardManager &) = delete;

        void destroyAll()
        {
            for (auto *b : billboards)
                delete b;
            for (auto *b : pool)
                delete b;
            billboards.clear();
            pool.clear();
            billboards.shrink_to_fit();
            pool.shrink_to_fit();
        }

        Billboard *create(const Texture *texture = nullptr, const Vector3 &pos = Vector3())
        {
            Billboard *b;
            if (!pool.empty())
            {
                b = pool.back();
                pool.pop_back();
                *b = Billboard();
            }
            else
            {
                b = new Billboard();
            }
            b->texture = texture;
            b->position = pos;
            billboards.push_back(b);
            return b;
        }

        void remove(Billboard *b)
        {
            if (unlikely(!b))
            {
                LOGW(::pip3D::Debug::LOG_MODULE_SCENE,
                     "BillboardManager::remove called with null billboard");
                return;
            }

            size_t found = billboards.size();
            for (size_t i = 0; i < billboards.size(); ++i)
            {
                if (billboards[i] == b)
                {
                    found = i;
                    break;
                }
            }
            if (found == billboards.size())
            {
                LOGW(::pip3D::Debug::LOG_MODULE_SCENE,
                     "BillboardManager::remove: billboard not found (count=%u)",
                     static_cast<unsigned>(billboards.size()));
                return;
            }

            billboards[found] = billboards.back();
            billboards.pop_back();
            pool.push_back(b);
        }

        void clear()
        {
            for (auto *b : billboards)
                pool.push_back(b);
            billboards.clear();
        }

        void hideAll()
        {
            for (auto *b : billboards)
                b->visible = false;
        }
        void showAll()
        {
            for (auto *b : billboards)
                b->visible = true;
        }

        size_t count() const { return billboards.size(); }
        const std::vector<Billboard *> &all() const { return billboards; }

        void render(Renderer &renderer);
    };
}
