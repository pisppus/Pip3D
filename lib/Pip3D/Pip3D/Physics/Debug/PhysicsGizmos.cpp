

#include "../World.hpp"

#include "Rendering/Renderer.hpp"
#include "Debug/Gizmos.hpp"
#include "Core/Color.hpp"

namespace pip3D
{

    void PhysicsWorld::Gizmos(Renderer &renderer)
    {
        (void)renderer;

        const size_t bodyCount = bodies.size();
        for (size_t i = 0; i < bodyCount; ++i)
        {
            RigidBody *b = bodies[i];
            if (!b)
                continue;

            uint16_t shapeColor = Color::GREEN;
            if (b->isStatic)
                shapeColor = Color::RED;
            else if (b->isKinematic)
                shapeColor = Color::YELLOW;
            else if (b->isSleeping)
                shapeColor = Color::GRAY;

            if (b->isStatic)
            {
                const uint16_t aabbColor = Color::rgb(80, 40, 40);
                DBG_AABB(b->bounds, aabbColor, ::pip3D::Debug::DEBUG_CATEGORY_PHYSICS);
            }

            if (b->shape == BODY_SHAPE_SPHERE)
            {
                DBG_SPHERE(b->position, b->radius, shapeColor,
                           ::pip3D::Debug::DEBUG_CATEGORY_PHYSICS);
            }
            else if (b->shape == BODY_SHAPE_CAPSULE)
            {
                Vector3 axisY = b->orientation.rotate(
                    Vector3(0.0f, b->capsuleHalfHeight, 0.0f));
                Vector3 p0 = b->position - axisY;
                Vector3 p1 = b->position + axisY;
                DBG_CAPSULE(p0, p1, b->radius, shapeColor,
                            ::pip3D::Debug::DEBUG_CATEGORY_PHYSICS);
            }
            else if (b->shape == BODY_SHAPE_CONVEX)
            {

                Vector3 wv[RigidBody::kMaxConvexVerts];
                const int vc = b->convexCount;
                for (int vi = 0; vi < vc; ++vi)
                    wv[vi] = b->position + b->orientation.rotate(b->convexVerts[vi]);

                static const Vector3 kDirs[14] = {
                    Vector3(1, 0, 0), Vector3(-1, 0, 0),
                    Vector3(0, 1, 0), Vector3(0, -1, 0),
                    Vector3(0, 0, 1), Vector3(0, 0, -1),
                    Vector3(0.57735f, 0.57735f, 0.57735f),
                    Vector3(-0.57735f, 0.57735f, 0.57735f),
                    Vector3(0.57735f, -0.57735f, 0.57735f),
                    Vector3(-0.57735f, -0.57735f, 0.57735f),
                    Vector3(0.57735f, 0.57735f, -0.57735f),
                    Vector3(-0.57735f, 0.57735f, -0.57735f),
                    Vector3(0.57735f, -0.57735f, -0.57735f),
                    Vector3(-0.57735f, -0.57735f, -0.57735f)};

                for (int d = 0; d < 14; ++d)
                {
                    float bestDot0 = -FLT_MAX;
                    float bestDot1 = -FLT_MAX;
                    int bestIdx0 = 0;
                    int bestIdx1 = 0;
                    for (int vi = 0; vi < vc; ++vi)
                    {
                        const float dot = wv[vi].dot(kDirs[d]);
                        if (dot > bestDot0)
                        {
                            bestDot1 = bestDot0;
                            bestIdx1 = bestIdx0;
                            bestDot0 = dot;
                            bestIdx0 = vi;
                        }
                        else if (dot > bestDot1)
                        {
                            bestDot1 = dot;
                            bestIdx1 = vi;
                        }
                    }
                    if (bestIdx0 != bestIdx1)
                    {
                        DBG_LINE(wv[bestIdx0], wv[bestIdx1], shapeColor,
                                 ::pip3D::Debug::DEBUG_CATEGORY_PHYSICS);
                    }
                }
            }
            else
            {
                DBG_OBB(b->position, b->size * 0.5f, b->orientation, shapeColor,
                        ::pip3D::Debug::DEBUG_CATEGORY_PHYSICS);
            }
        }

        const size_t constraintCount = contactConstraints.size();
        int contactMarkersDrawn = 0;
        const int kMaxContactMarkers = 32;
        for (size_t i = 0; i < constraintCount && contactMarkersDrawn < kMaxContactMarkers; ++i)
        {
            ContactManifold &info = contactConstraints[i];
            if (!info.hasCollision || info.contactCount <= 0)
                continue;

            Vector3 n = info.normal;
            float nLenSq = n.lengthSquared();
            if (nLenSq > 1e-8f)
                n *= FastMath::fastReciprocal(sqrtf(nLenSq));

            for (int j = 0; j < info.contactCount && contactMarkersDrawn < kMaxContactMarkers; ++j)
            {
                Contact &c = info.contacts[j];
                DBG_SPHERE(c.pos, 0.04f, Color::RED,
                           ::pip3D::Debug::DEBUG_CATEGORY_PHYSICS);
                DBG_ARROW(c.pos, n, 0.12f, 0.025f, Color::YELLOW,
                          ::pip3D::Debug::DEBUG_CATEGORY_PHYSICS);
                ++contactMarkersDrawn;
            }
        }
    }

}
