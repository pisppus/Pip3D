#include "Renderer.hpp"
#include "Rendering/Pipeline/MeshDraw.hpp"
#include "Rendering/Pipeline/Shading.hpp"
#include "Rendering/Pipeline/Billboard.hpp"
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
        if (!frustum.testSphere(center, radius))
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

    void Renderer::drawBillboardQuads(const BillboardQuad *quads, size_t count)
    {
        if (!quads || count == 0)
            return;

        const int16_t bandTop = g_bandOffsetY;
        const int16_t bandHeight = g_bandHeight;

        drawBillboardQuadsRaw(
            quads, count,
            framebuffer.getBuffer(),
            zBuffer,
            framebuffer,
            bandTop, bandHeight,
            static_cast<int16_t>(viewport.width),
            static_cast<int16_t>(viewport.height),
            /*writeZForAlpha=*/true);
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

        const Camera &cam = cameras[activeCameraIndex];
        BillboardFrameContext ctx = makeBillboardFrameContext(
            cam, viewport, viewProjMatrix, frustum);

        BillboardQuad q = {};
        q.texture = &g_sunTexture;
        q.chromaKey = 0x0000;
        q.alpha = intensityByte;
        q.blend = static_cast<uint8_t>(BB_BLEND_ADDITIVE);
        q.lit = false;

        if (!buildBillboardQuadGeometry(
                worldPos, diameter, diameter,
                BB_SCREEN_ALIGNED, /*yawDeg=*/0.0f,
                /*screenSpaceSize=*/true, color,
                ctx, q))
            return;

        drawBillboardQuads(&q, 1);
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

        const int16_t bandTop = g_bandOffsetY;
        const int16_t bandH = g_bandHeight;
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
}
