#include "Renderer.hpp"
#include "Rendering/Pipeline/MeshDraw.hpp"
#include "Rendering/Pipeline/Shading.hpp"
#include "Geometry/Billboard.hpp"
#include "Rendering/Display/Textures/Sun.hpp"
#include "Math/Algebra.hpp"
#include "Debug/Logging.hpp"

namespace pip3D
{
    void Renderer::drawWaterMesh(Mesh *mesh, float time)
    {
        if (!mesh)
            return;

        MeshRenderer::drawWaterMesh(mesh,
                                    cameras[activeCameraIndex],
                                    viewport,
                                    frustum,
                                    viewProjMatrix,
                                    framebuffer,
                                    zBuffer,
                                    time,
                                    reflectBuffer,
                                    REFLECT_WIDTH,
                                    REFLECT_HEIGHT);
    }

    void Renderer::drawWater(float yLevel, float size, Color color, float alpha, float time)
    {
        uint16_t *fb = framebuffer.getBuffer();
        if (!fb || !zBuffer)
            return;

        if (alpha <= 0.0f)
            return;
        if (alpha > 1.0f)
            alpha = 1.0f;

        const uint8_t alphaByte = static_cast<uint8_t>(alpha * COLOR_BYTE_MAX_F);
        const DisplayConfig &cfg = framebuffer.getConfig();

        const Vector3 center(0.0f, yLevel, 0.0f);
        const float radius = size * 0.75f;
        if (!frustum.sphere(center, radius))
            return;

        constexpr int GRID = 32;
        const float half = size * 0.5f;
        constexpr float invGRID = 1.0f / static_cast<float>(GRID);
        const float step = size * invGRID;

        const float freq = 0.6f;
        const float amp = size * 0.02f;

        float sinTerms[GRID + 1];
        float cosTerms[GRID + 1];
        float val = -half;
        for (int i = 0; i <= GRID; ++i)
        {
            float s, c;
            FastMath::fastSinCos(val * freq + time, s, c);
            sinTerms[i] = s * amp;
            cosTerms[i] = c * amp;
            val += step;
        }

        const float yBase = yLevel;
        for (int iz = 0; iz < GRID; ++iz)
        {
            const float z0 = -half + step * static_cast<float>(iz);
            const float z1 = z0 + step;
            const float cosZ0 = cosTerms[iz];
            const float cosZ1 = cosTerms[iz + 1];

            for (int ix = 0; ix < GRID; ++ix)
            {
                const float x0 = -half + step * static_cast<float>(ix);
                const float x1 = x0 + step;
                const float sinX0 = sinTerms[ix];
                const float sinX1 = sinTerms[ix + 1];

                const Vector3 v00(x0, yBase + sinX0 + cosZ0, z0);
                const Vector3 v10(x1, yBase + sinX1 + cosZ0, z0);
                const Vector3 v01(x0, yBase + sinX0 + cosZ1, z1);
                const Vector3 v11(x1, yBase + sinX1 + cosZ1, z1);

                drawWaterTriangleInternal(v00, v10, v11, color, alphaByte, cfg, fb);
                drawWaterTriangleInternal(v00, v11, v01, color, alphaByte, cfg, fb);
            }
        }
    }

    void Renderer::drawSky()
    {
        if (!sunEnabled || !sunVisible)
            return;

        const Camera &cam = cameras[activeCameraIndex];
        const float skyDist = cam.farPlane * 0.85f;
        const Vector3 sunPos = cam.position + sunWorldDir * skyDist;

        const Vector3 toSun = sunPos - cam.position;
        if (toSun.dot(cam.forward()) <= 0.0f)
            return;

        drawSunSprite(sunPos, sunColor, sunIntensity, 1.0f);
    }

    void Renderer::drawSunSprite(const Vector3 &worldPos, const Color &color, float glow, float sizeScale)
    {
        const int16_t minDim = viewport.width < viewport.height ? viewport.width : viewport.height;
        if (minDim <= 0)
            return;

        if (sizeScale < 0.2f)
            sizeScale = 0.2f;
        if (sizeScale > 3.0f)
            sizeScale = 3.0f;

        float extra = glow;
        if (extra < 0.0f)
            extra = 0.0f;
        if (extra > 1.0f)
            extra = 1.0f;

        const float diameter = 2.0f * minDim * 0.065f * sizeScale * (0.85f + extra * 0.35f);
        if (diameter < 2.0f)
            return;

        const uint8_t intensityByte = static_cast<uint8_t>(extra * COLOR_BYTE_MAX_F);
        if (intensityByte == 0)
            return;

        Billboard bb;
        bb.position = worldPos;
        bb.width = diameter;
        bb.height = diameter;
        bb.texture = &g_sunTexture;
        bb.tint = color;
        bb.chromaKey = 0x0000;
        bb.orientation = BB_SCREEN_ALIGNED;
        bb.blend = BB_BLEND_ADDITIVE;
        bb.alpha = intensityByte;
        bb.screenSpaceSize = true;
        bb.visible = true;
        bb.lit = false;
        drawBillboard(bb);
    }

    void Renderer::drawWaterTriangleInternal(const Vector3 &v0,
                                             const Vector3 &v1,
                                             const Vector3 &v2,
                                             const Color &waterColor,
                                             uint8_t alphaByte,
                                             const DisplayConfig &cfg,
                                             uint16_t *frameBufferPtr)
    {
        const Vector3 p0 = CameraController::project(v0, viewProjMatrix, viewport);
        const Vector3 p1 = CameraController::project(v1, viewProjMatrix, viewport);
        const Vector3 p2 = CameraController::project(v2, viewProjMatrix, viewport);

        const float x0 = p0.x, y0 = p0.y;
        const float x1 = p1.x, y1 = p1.y;
        const float x2 = p2.x, y2 = p2.y;

        float minXf = fminf(x0, fminf(x1, x2));
        float maxXf = fmaxf(x0, fmaxf(x1, x2));
        float minYf = fminf(y0, fminf(y1, y2));
        float maxYf = fmaxf(y0, fmaxf(y1, y2));

        int16_t minX = static_cast<int16_t>(floorf(minXf));
        int16_t maxX = static_cast<int16_t>(ceilf(maxXf));
        int16_t minY = static_cast<int16_t>(floorf(minYf));
        int16_t maxY = static_cast<int16_t>(ceilf(maxYf));

        if (maxX < 0 || maxY < 0 || minX >= (int16_t)SCREEN_WIDTH || minY >= (int16_t)SCREEN_HEIGHT)
            return;

        if (minX < 0)
            minX = 0;
        if (minY < 0)
            minY = 0;
        if (maxX >= (int16_t)SCREEN_WIDTH)
            maxX = (int16_t)SCREEN_WIDTH - 1;
        if (maxY >= (int16_t)SCREEN_HEIGHT)
            maxY = (int16_t)SCREEN_HEIGHT - 1;

        const int16_t bandTop = currentBandOffsetY();
        const int16_t bandH = currentBandHeight();
        const int16_t bandBottom = static_cast<int16_t>(bandTop + bandH);

        if (maxY < bandTop || minY >= bandBottom)
            return;

        if (minY < bandTop)
            minY = bandTop;
        if (maxY >= bandBottom)
            maxY = static_cast<int16_t>(bandBottom - 1);

        const float denom = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2);
        if (fabsf(denom) < 1e-6f)
            return;
        const float invDenom = 1.0f / denom;

        if (alphaByte == 0)
            return;

        const bool opaque = (alphaByte == 255);
        const uint16_t solidColor = opaque ? waterColor.rgb565 : 0;
        const Color waterColorLocal = waterColor;

        const int32_t stride = cfg.width;
        const int16_t yLocalBase = static_cast<int16_t>(minY - bandTop);
        uint16_t *row = frameBufferPtr + static_cast<int32_t>(yLocalBase) * stride + minX;

        const float px0 = static_cast<float>(minX) + 0.5f;
        const float py0 = static_cast<float>(minY) + 0.5f;

        const float dw0dx = (y1 - y2) * invDenom;
        const float dw0dy = (x2 - x1) * invDenom;
        const float dw1dx = (y2 - y0) * invDenom;
        const float dw1dy = (x0 - x2) * invDenom;

        const float w0row0 = (dw0dx * (px0 - x2) + dw0dy * (py0 - y2));
        const float w1row0 = (dw1dx * (px0 - x2) + dw1dy * (py0 - y2));

        for (int16_t y = minY; y <= maxY; ++y)
        {
            float w0 = w0row0 + dw0dy * static_cast<float>(y - minY);
            float w1 = w1row0 + dw1dy * static_cast<float>(y - minY);

            if (opaque)
            {
                for (int16_t x = minX; x <= maxX; ++x)
                {
                    const float w2 = 1.0f - w0 - w1;
                    if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f)
                        row[x - minX] = solidColor;
                    w0 += dw0dx;
                    w1 += dw1dx;
                }
            }
            else
            {
                for (int16_t x = minX; x <= maxX; ++x)
                {
                    const float w2 = 1.0f - w0 - w1;
                    if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f)
                    {
                        uint16_t &dst = row[x - minX];
                        dst = Color(dst).blend(waterColorLocal, alphaByte).rgb565;
                    }
                    w0 += dw0dx;
                    w1 += dw1dx;
                }
            }

            row += stride;
        }
    }

    static void buildBillboardQuad(const Billboard &bb,
                                   const Vector3 &camPos,
                                   const Vector3 &camRight,
                                   const Vector3 &camUp,
                                   const Vector3 &camForward,
                                   float projScale,
                                   float halfViewportHeight,
                                   bool perspective,
                                   Vector3 outWorld[4],
                                   Vector3 &outNormal)
    {
        float halfW, halfH;
        if (bb.screenSpaceSize)
        {
            const float zView = (bb.position - camPos).dot(camForward);
            if (perspective && zView > 0.01f)
            {
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
            if (right.lengthSquared() < 1e-6f)
                right = Vector3(camForward.x, 0.0f, camForward.z);
            right.normalize();
            up = Vector3(0.0f, 1.0f, 0.0f);
            break;
        }
        case BB_FIXED_YAW:
        default:
        {
            const float yr = bb.yawDeg * kDegToRad;
            float s, c;
            FastMath::fastSinCos(yr, s, c);
            right = Vector3(c, 0.0f, -s);
            up = Vector3(0.0f, 1.0f, 0.0f);
            break;
        }
        }

        const Vector3 &c = bb.position;
        outWorld[0] = c - right * halfW - up * halfH;
        outWorld[1] = c + right * halfW - up * halfH;
        outWorld[2] = c + right * halfW + up * halfH;
        outWorld[3] = c - right * halfW + up * halfH;

        outNormal = right.cross(up);
        outNormal.normalize();
    }

    void Renderer::drawBillboard(const Billboard &bb)
    {
        if (!bb.visible || !bb.texture)
        {
#if defined(PIP3D_DEBUG_BILLBOARD)
            LOGW(::pip3D::Debug::LOG_MODULE_RENDER,
                 "[BB-draw] early-out: visible=%d tex=%p",
                 (int)bb.visible, static_cast<const void *>(bb.texture));
#endif
            return;
        }

        const Camera &cam = cameras[activeCameraIndex];
        const Vector3 camPos = cam.position;
        const Vector3 camFwd = cam.forward();
        const Vector3 camRight = cam.right();
        const Vector3 camUp = cam.upVec();
        const bool perspective = (cam.projectionType == PERSPECTIVE);

        const float fovRad = cam.fov * kDegToRad;
        const float projScale = perspective ? FastMath::fastReciprocal(tanf(fovRad * 0.5f)) : 1.0f;
        const float halfViewportHeight = viewport.height * 0.5f;

        if (perspective)
        {
            const float zView = (bb.position - camPos).dot(camFwd);
            if (zView <= cam.nearPlane || zView >= cam.farPlane)
            {
#if defined(PIP3D_DEBUG_BILLBOARD)
                LOGW(::pip3D::Debug::LOG_MODULE_RENDER,
                     "[BB-draw] culled zView=%.3f near=%.3f far=%.3f",
                     zView, cam.nearPlane, cam.farPlane);
#endif
                return;
            }
        }

        Vector3 worldQuad[4];
        Vector3 quadNormal;
        buildBillboardQuad(bb, camPos, camRight, camUp, camFwd,
                           projScale, halfViewportHeight, perspective, worldQuad, quadNormal);

        const float halfWidthF = viewport.width * 0.5f;
        const float halfHeightF = viewport.height * 0.5f;

        Vector3 p[4];
        float camW[4];
        float minSx = 1e30f, maxSx = -1e30f, minSy = 1e30f, maxSy = -1e30f;
        bool anyValid = false;
        for (int i = 0; i < 4; ++i)
        {
            p[i] = CameraController::project(worldQuad[i], viewProjMatrix,
                                             halfWidthF, halfHeightF,
                                             viewport.x, viewport.y);
            camW[i] = (worldQuad[i] - camPos).dot(camFwd);
            if (camW[i] < 0.01f)
                camW[i] = 0.01f;

            if (!isfinite(p[i].x) || !isfinite(p[i].y) || !isfinite(p[i].z))
            {
#if defined(PIP3D_DEBUG_BILLBOARD)
                LOGW(::pip3D::Debug::LOG_MODULE_RENDER,
                     "[BB-draw] NaN in project v%d: (%.1f,%.1f,%.3f)",
                     i, p[i].x, p[i].y, p[i].z);
#endif
                return;
            }
            if (p[i].z <= 0.0f || p[i].z >= 1.0f)
                continue;
            anyValid = true;
            if (p[i].x < minSx)
                minSx = p[i].x;
            if (p[i].x > maxSx)
                maxSx = p[i].x;
            if (p[i].y < minSy)
                minSy = p[i].y;
            if (p[i].y > maxSy)
                maxSy = p[i].y;
        }
        if (!anyValid)
        {
#if defined(PIP3D_DEBUG_BILLBOARD)
            LOGW(::pip3D::Debug::LOG_MODULE_RENDER, "[BB-draw] no valid projected verts");
#endif
            return;
        }

        const int16_t bandTop = currentBandOffsetY();
        const int16_t bandBottom = static_cast<int16_t>(bandTop + currentBandHeight());
        if (maxSy < bandTop || minSy >= bandBottom)
            return;
        if (maxSx < 0.0f || minSx >= viewport.width)
            return;

        float litR = 1.0f, litG = 1.0f, litB = 1.0f;
        if (bb.lit)
        {
            const float baseR = static_cast<float>((bb.tint.rgb565 >> 11) & 0x1F) * (1.0f / 31.0f);
            const float baseG = static_cast<float>((bb.tint.rgb565 >> 5) & 0x3F) * (1.0f / 63.0f);
            const float baseB = static_cast<float>(bb.tint.rgb565 & 0x1F) * (1.0f / 31.0f);

            Vector3 viewDir = camPos - bb.position;
            viewDir.normalize();

            Vector3 faceNormal = quadNormal;
            if (faceNormal.dot(viewDir) < 0.0f)
                faceNormal = -faceNormal;

            Shading::calculateLighting(bb.position, faceNormal, viewDir,
                                       lights.data(), activeLightCount,
                                       baseR, baseG, baseB,
                                       litR, litG, litB, true);
        }
        else
        {
            litR = static_cast<float>((bb.tint.rgb565 >> 11) & 0x1F) * (1.0f / 31.0f);
            litG = static_cast<float>((bb.tint.rgb565 >> 5) & 0x3F) * (1.0f / 63.0f);
            litB = static_cast<float>(bb.tint.rgb565 & 0x1F) * (1.0f / 31.0f);
        }

#if defined(PIP3D_DEBUG_BILLBOARD)
        const int16_t dbgBand = bandTop;
        LOGI(::pip3D::Debug::LOG_MODULE_RENDER,
             "[BB-draw band=%d] AABB x=[%.0f..%.0f] y=[%.0f..%.0f] band=[%d..%d] "
             "lit=%d tint=(%.2f,%.2f,%.2f) mode=%d chroma=0x%04X w=[%.2f,%.2f,%.2f,%.2f]",
             (int)dbgBand, minSx, maxSx, minSy, maxSy, (int)bandTop, (int)bandBottom,
             (int)bb.lit, litR, litG, litB, (int)bb.blend, bb.chromaKey,
             camW[0], camW[1], camW[2], camW[3]);
#endif

        const DisplayConfig &cfg = framebuffer.getConfig();
        const BillboardBlendMode mode = static_cast<BillboardBlendMode>(bb.blend);

        auto drawTri = [&](int a, int b, int c,
                           float ua, float va, float ub, float vb, float uc, float vc,
                           bool writeZ)
        {
            Rasterizer::fillTriangleBillboard(
                p[a].x, p[a].y - (float)bandTop, p[a].z,
                p[b].x, p[b].y - (float)bandTop, p[b].z,
                p[c].x, p[c].y - (float)bandTop, p[c].z,
                ua, va, ub, vb, uc, vc,
                camW[a], camW[b], camW[c],
                litR, litG, litB, litR, litG, litB, litR, litG, litB,
                *bb.texture,
                mode,
                bb.chromaKey,
                bb.alpha,
                writeZ,
                framebuffer.getBuffer(),
                zBuffer,
                cfg);
        };

        const bool firstWritesZ = (mode == BB_ALPHA);
        drawTri(0, 1, 2, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, firstWritesZ);
        drawTri(0, 2, 3, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, false);
    }
}
