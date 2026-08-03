#pragma once

#include "Core/Platform.hpp"
#include "Math/Algebra.hpp"
#include "Core/Color.hpp"
#include "Rendering/Display/Texture.hpp"
#include "Camera/Camera.hpp"
#include "Camera/Frustum.hpp"
#include "Camera/Controllers.hpp"
#include "Rendering/Display/ZBuffer.hpp"
#include "Rendering/Display/FrameBuffer.hpp"
#include "Rendering/Pipeline/Rasterizer.hpp"
#include "Rendering/Pipeline/Shading.hpp"
#include "Rendering/Pipeline/Rasterizer/Billboard.hpp"

#if PIP3D_DEBUG_BILLBOARD
#include "Debug/Logging.hpp"
#endif

#include <vector>
#include <algorithm>
#include <cmath>

namespace pip3D
{
    enum BillboardOrientation : uint8_t
    {
        BB_SCREEN_ALIGNED = 0,
        BB_AXIAL_Y = 1,
        BB_FIXED_YAW = 2
    };

    struct alignas(16) BillboardQuad
    {
        Vector3 screen[4];
        float camW[4];
        float litR, litG, litB;
        const Texture *texture;
        uint16_t chromaKey;
        uint8_t alpha;
        uint8_t blend;
        float distSq;
        bool lit;
    };
    static_assert(sizeof(BillboardQuad) % 4 == 0, "BillboardQuad must be 4-byte aligned");

    struct BillboardFrameContext
    {
        Vector3 camPos;
        Vector3 camFwd;
        Vector3 camRight;
        Vector3 camUp;
        Vector3 camUpProjected;
        Vector3 camRightXZnormalized;
        float projScale;
        float halfViewportHeight;
        float halfViewportWidth;
        float invHalfVPHeight;
        int16_t viewportX;
        int16_t viewportY;
        int16_t viewportWidth;
        int16_t viewportHeight;
        const Matrix4x4 *viewProj;
        const Frustum *frustum;
    };

    PIP3D_FORCE_INLINE BillboardFrameContext
    makeBillboardFrameContext(const Camera &cam, const Viewport &vp,
                              const Matrix4x4 &viewProj, const Frustum &frustum)
    {
        BillboardFrameContext ctx;
        ctx.camPos = cam.position;
        ctx.camFwd = cam.forward();
        ctx.camRight = cam.right();
        ctx.camUp = cam.upVec();

        {
            const float d = ctx.camUp.dot(ctx.camFwd);
            Vector3 proj = ctx.camUp - ctx.camFwd * d;
            const float lenSq = proj.lengthSquared();
            const float invLen = (lenSq > 1e-8f) ? FastMath::fastInvSqrt(lenSq) : 0.0f;
            ctx.camUpProjected = proj * invLen;
        }

        const float fovRad = cam.fov * kDegToRad;
        ctx.projScale = FastMath::fastReciprocal(tanf(fovRad * 0.5f));
        ctx.halfViewportHeight = static_cast<float>(vp.height) * 0.5f;
        ctx.halfViewportWidth = static_cast<float>(vp.width) * 0.5f;
        ctx.invHalfVPHeight = FastMath::fastReciprocal(ctx.halfViewportHeight);
        ctx.viewportX = vp.x;
        ctx.viewportY = vp.y;
        ctx.viewportWidth = vp.width;
        ctx.viewportHeight = vp.height;
        ctx.viewProj = &viewProj;
        ctx.frustum = &frustum;
        {
            Vector3 r(ctx.camRight.x, 0.0f, ctx.camRight.z);
            float lenSq = r.lengthSquared();
            if (unlikely(lenSq < 1e-6f))
            {
                r = Vector3(ctx.camFwd.x, 0.0f, ctx.camFwd.z);
                lenSq = r.lengthSquared();
            }
            const float invLen = (lenSq > 1e-8f) ? FastMath::fastInvSqrt(lenSq) : 0.0f;
            ctx.camRightXZnormalized = r * invLen;
        }

        return ctx;
    }

    PIP3D_FORCE_INLINE bool
    buildBillboardQuadGeometry(const Vector3 &position,
                               float width, float height,
                               BillboardOrientation orientation,
                               float yawDeg,
                               bool screenSpaceSize,
                               Color tint,
                               const BillboardFrameContext &ctx,
                               BillboardQuad &outQuad)
    {
        if (!outQuad.texture)
            return false;

        const Vector3 toObj = position - ctx.camPos;
        const float distSq = toObj.lengthSquared();
        if (unlikely(distSq < 1e-6f))
            return false;

        const float zView = toObj.dot(ctx.camFwd);
        if (zView <= 0.01f)
            return false;
        outQuad.distSq = zView * zView;

        float halfW = width * 0.5f;
        float halfH = height * 0.5f;
        if (screenSpaceSize)
        {
            const float scale = zView * ctx.invHalfVPHeight *
                                FastMath::fastReciprocal(ctx.projScale);
            halfW *= scale;
            halfH *= scale;
        }

        Vector3 right, up;
        switch (orientation)
        {
        case BB_SCREEN_ALIGNED:
            right = ctx.camRight;
            up = ctx.camUpProjected;
            break;

        case BB_AXIAL_Y:
            right = ctx.camRightXZnormalized;
            up = Vector3(0.0f, 1.0f, 0.0f);
            break;

        case BB_FIXED_YAW:
        default:
        {
            const float yr = yawDeg * kDegToRad;
            float s, c;
            FastMath::fastSinCos(yr, s, c);
            right = Vector3(c, 0.0f, -s);
            up = Vector3(0.0f, 1.0f, 0.0f);
            break;
        }
        }

        const Vector3 c = position;
        const Vector3 extX = right * halfW;
        const Vector3 extY = up * halfH;
        Vector3 worldCorners[4] = {
            c - extX - extY,
            c + extX - extY,
            c + extX + extY,
            c - extX + extY};

        {
            const float maxExt = (halfW > halfH) ? halfW : halfH;
            if (unlikely(!ctx.frustum->testSphere(position, maxExt)))
                return false;
        }

        bool anyValid = false;
        for (int i = 0; i < 4; ++i)
        {
            const Vector3 sp = CameraController::project(
                worldCorners[i], *ctx.viewProj,
                ctx.halfViewportWidth, ctx.halfViewportHeight,
                ctx.viewportX, ctx.viewportY);

            if (!std::isfinite(sp.x) || !std::isfinite(sp.y) || !std::isfinite(sp.z))
                return false;

            outQuad.screen[i] = sp;
            float cw = (worldCorners[i] - ctx.camPos).dot(ctx.camFwd);
            if (cw < 0.01f)
                cw = 0.01f;
            outQuad.camW[i] = cw;

            if (sp.z > 0.0f && sp.z < static_cast<float>(Z_DEPTH_MAX))
                anyValid = true;
        }
        if (!anyValid)
            return false;

        if (outQuad.lit)
        {
            const float baseR = static_cast<float>((tint.rgb565 >> 11) & 0x1F) * (1.0f / 31.0f);
            const float baseG = static_cast<float>((tint.rgb565 >> 5) & 0x3F) * (1.0f / 63.0f);
            const float baseB = static_cast<float>(tint.rgb565 & 0x1F) * (1.0f / 31.0f);

            Vector3 viewDir = ctx.camPos - position;
            viewDir.normalize();

            Vector3 faceNormal = right.cross(up);
            faceNormal.normalize();
            if (faceNormal.dot(viewDir) < 0.0f)
                faceNormal = -faceNormal;

            Shading::calculateLighting(
                position, faceNormal, viewDir,
                nullptr, 0,
                baseR, baseG, baseB,
                outQuad.litR, outQuad.litG, outQuad.litB, true);
        }
        else
        {
            outQuad.litR = static_cast<float>((tint.rgb565 >> 11) & 0x1F) * (1.0f / 31.0f);
            outQuad.litG = static_cast<float>((tint.rgb565 >> 5) & 0x3F) * (1.0f / 63.0f);
            outQuad.litB = static_cast<float>(tint.rgb565 & 0x1F) * (1.0f / 31.0f);
        }
        return true;
    }

    inline void drawBillboardQuadsRaw(
        const BillboardQuad *quads, size_t count,
        uint16_t *frameBuffer,
        ZBuffer *zBuffer,
        const FrameBuffer &framebuffer,
        int16_t bandTop, int16_t bandHeight,
        int16_t viewportWidth, int16_t viewportHeight,
        bool writeZForAlpha)
    {
        if (count == 0 || !frameBuffer || !zBuffer)
            return;

        const int16_t bandBottom = static_cast<int16_t>(bandTop + bandHeight);
        const DisplayConfig &cfg = framebuffer.getConfig();

        for (size_t qi = 0; qi < count; ++qi)
        {
            const BillboardQuad &q = quads[qi];

            float minSx = q.screen[0].x, maxSx = q.screen[0].x;
            float minSy = q.screen[0].y, maxSy = q.screen[0].y;
            for (int i = 1; i < 4; ++i)
            {
                if (q.screen[i].x < minSx)
                    minSx = q.screen[i].x;
                if (q.screen[i].x > maxSx)
                    maxSx = q.screen[i].x;
                if (q.screen[i].y < minSy)
                    minSy = q.screen[i].y;
                if (q.screen[i].y > maxSy)
                    maxSy = q.screen[i].y;
            }
            if (maxSy < bandTop || minSy >= bandBottom)
                continue;
            if (maxSx < 0.0f || minSx >= viewportWidth)
                continue;

            const BillboardBlend mode = static_cast<BillboardBlend>(q.blend);

            auto drawTri = [&](int a, int b, int c,
                               float ua, float va,
                               float ub, float vb,
                               float uc, float vc,
                               bool writeZ)
            {
                Rasterizer::fillTriangleBillboard(
                    q.screen[a].x, q.screen[a].y - (float)bandTop, q.screen[a].z,
                    q.screen[b].x, q.screen[b].y - (float)bandTop, q.screen[b].z,
                    q.screen[c].x, q.screen[c].y - (float)bandTop, q.screen[c].z,
                    ua, va, ub, vb, uc, vc,
                    q.camW[a], q.camW[b], q.camW[c],
                    q.litR, q.litG, q.litB,
                    q.litR, q.litG, q.litB,
                    q.litR, q.litG, q.litB,
                    *q.texture,
                    mode,
                    q.chromaKey,
                    q.alpha,
                    writeZ,
                    frameBuffer,
                    zBuffer,
                    cfg);
            };

            const bool firstWritesZ = (mode == BB_BLEND_ALPHA) && writeZForAlpha;
            drawTri(0, 1, 2, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, firstWritesZ);
            drawTri(0, 2, 3, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, false);
        }
    }
}