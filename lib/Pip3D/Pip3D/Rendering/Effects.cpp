#include "Renderer.hpp"
#include "Rendering/Pipeline/MeshDraw.hpp"
#include "Math/Algebra.hpp"

namespace pip3D
{
    void Renderer::drawWaterMesh(Mesh *mesh, float time)
    {
        if (!mesh)
            return;

        updateReflectionBufferOnDemand();

        MeshRenderer::drawWaterMesh(mesh,
                                    cameras[activeCameraIndex],
                                    viewport,
                                    frustum,
                                    viewProjMatrix,
                                    framebuffer,
                                    zBuffer,
                                    time,
                                    reflectionBuffer,
                                    reflectionWidth,
                                    reflectionHeight);
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

        Camera &cam = cameras[activeCameraIndex];

        addDirtyRect(nullptr, 0, 0, viewport.width, viewport.height);

        const Vector3 center(0.0f, yLevel, 0.0f);
        const float radius = size * 0.75f;
        if (!frustum.sphere(center, radius))
        {
            return;
        }

        const int GRID = 32;
        const float half = size * 0.5f;
        const float step = size / static_cast<float>(GRID);

        const float freq = 0.6f;
        const float amp = size * 0.02f;

        float sinTerms[GRID + 1];
        float cosTerms[GRID + 1];
        for (int i = 0; i <= GRID; ++i)
        {
            float val = -half + step * static_cast<float>(i);
            sinTerms[i] = FastMath::fastSin(val * freq + time) * amp;
            cosTerms[i] = FastMath::fastCos(val * freq + time) * amp;
        }

        for (int iz = 0; iz < GRID; ++iz)
        {
            float z0 = -half + step * static_cast<float>(iz);
            float z1 = z0 + step;

            for (int ix = 0; ix < GRID; ++ix)
            {
                float x0 = -half + step * static_cast<float>(ix);
                float x1 = x0 + step;

                Vector3 v00(x0, yLevel + sinTerms[ix] + cosTerms[iz], z0);
                Vector3 v10(x1, yLevel + sinTerms[ix + 1] + cosTerms[iz], z0);
                Vector3 v01(x0, yLevel + sinTerms[ix] + cosTerms[iz + 1], z1);
                Vector3 v11(x1, yLevel + sinTerms[ix + 1] + cosTerms[iz + 1], z1);

                drawWaterTriangleInternal(v00, v10, v11, color, alphaByte, cam, cfg, fb);
                drawWaterTriangleInternal(v00, v11, v01, color, alphaByte, cam, cfg, fb);
            }
        }
    }

    void Renderer::drawSunSprite(const Vector3 &worldPos, const Color &color, float glow, float sizeScale)
    {
        Vector3 p = project(worldPos);
        if (cameras[activeCameraIndex].projectionType == PERSPECTIVE && p.z <= 0.0f)
        {
            return;
        }
        drawSunDiscAtScreen((int16_t)p.x, (int16_t)p.y, color, glow, sizeScale);
    }

    void Renderer::drawSunSpriteDirectional(const Vector3 &sunDir, const Color &color, float glow, float sizeScale)
    {
        const Camera &cam = cameras[activeCameraIndex];
        Vector3 dir = sunDir;
        const float lenSq = dir.lengthSquared();
        if (lenSq <= 1e-6f)
        {
            return;
        }
        dir *= (1.0f / sqrtf(lenSq));

        const float forwardDot = dir.dot(cam.forward());
        if (forwardDot <= 0.001f)
        {
            return;
        }

        const float aspect = viewport.height > 0 ? (float)viewport.width / (float)viewport.height : 1.0f;
        const float tanHalfFov = tanf(cam.fov * kDegToRad * 0.5f);
        if (tanHalfFov <= 1e-6f)
        {
            return;
        }

        const float rightDot = dir.dot(cam.right());
        const float upDot = dir.dot(cam.upVec());
        const float ndcX = rightDot / (forwardDot * tanHalfFov * aspect);
        const float ndcY = upDot / (forwardDot * tanHalfFov);
        if (ndcX < -1.2f || ndcX > 1.2f || ndcY < -1.2f || ndcY > 1.2f)
        {
            return;
        }

        const int16_t screenX = static_cast<int16_t>((ndcX + 1.0f) * 0.5f * viewport.width);
        const int16_t screenY = static_cast<int16_t>((1.0f - ndcY) * 0.5f * viewport.height);
        drawSunDiscAtScreen(screenX, screenY, color, glow, sizeScale);
    }

    void Renderer::drawSunDiscAtScreen(int16_t cx, int16_t cyFull, const Color &color, float glow, float sizeScale)
    {
        auto cfg = framebuffer.getConfig();
        uint16_t *fb = framebuffer.getBuffer();
        if (!fb)
            return;

        int16_t minDim = viewport.width < viewport.height ? viewport.width : viewport.height;
        if (minDim <= 0)
            return;

        if (sizeScale < 0.2f)
            sizeScale = 0.2f;
        if (sizeScale > 3.0f)
            sizeScale = 3.0f;

        float baseRadius = minDim * 0.018f * sizeScale;
        if (baseRadius < 1.0f)
            return;

        float extra = glow;
        if (extra < 0.0f)
            extra = 0.0f;
        if (extra > 1.0f)
            extra = 1.0f;

        int16_t radius = (int16_t)(baseRadius * (0.85f + extra * 0.35f));
        if (radius <= 0)
            return;

        const int16_t bandTop = currentBandOffsetY();
        const int16_t bandBottom = static_cast<int16_t>(bandTop + cfg.height);
        if (cyFull + radius < bandTop || cyFull - radius >= bandBottom)
            return;

        int16_t cy = static_cast<int16_t>(cyFull - bandTop);
        int r2 = radius * radius;

        for (int dy = -radius; dy <= radius; ++dy)
        {
            int yy = cy + dy;
            if (yy < 0 || yy >= cfg.height)
                continue;

            for (int dx = -radius; dx <= radius; ++dx)
            {
                int xx = cx + dx;
                if (xx < 0 || xx >= cfg.width)
                    continue;

                int d2 = dx * dx + dy * dy;
                if (d2 <= r2)
                {
                    fb[yy * cfg.width + xx] = color.rgb565;
                }
            }
        }
    }

    void Renderer::drawWaterTriangleInternal(const Vector3 &v0,
                                             const Vector3 &v1,
                                             const Vector3 &v2,
                                             const Color &waterColor,
                                             uint8_t alphaByte,
                                             const Camera &cam,
                                             const DisplayConfig &cfg,
                                             uint16_t *frameBufferPtr)
    {
        Vector3 p0 = CameraController::project(v0, viewProjMatrix, viewport);
        Vector3 p1 = CameraController::project(v1, viewProjMatrix, viewport);
        Vector3 p2 = CameraController::project(v2, viewProjMatrix, viewport);

        float x0 = p0.x, y0 = p0.y, z0 = p0.z;
        float x1 = p1.x, y1 = p1.y, z1 = p1.z;
        float x2 = p2.x, y2 = p2.y, z2 = p2.z;

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

        int16_t bandTop = currentBandOffsetY();
        int16_t bandH = currentBandHeight();
        int16_t bandBottom = static_cast<int16_t>(bandTop + bandH);

        if (maxY < bandTop || minY >= bandBottom)
            return;

        if (minY < bandTop)
            minY = bandTop;
        if (maxY >= bandBottom)
            maxY = static_cast<int16_t>(bandBottom - 1);

        float denom = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2);
        if (fabsf(denom) < 1e-6f)
            return;
        float invDenom = 1.0f / denom;

        for (int16_t y = minY; y <= maxY; ++y)
        {
            float py = static_cast<float>(y) + 0.5f;
            int16_t yLocal = static_cast<int16_t>(y - bandTop);

            for (int16_t x = minX; x <= maxX; ++x)
            {
                float px = static_cast<float>(x) + 0.5f;

                float w0 = ((y1 - y2) * (px - x2) + (x2 - x1) * (py - y2)) * invDenom;
                float w1 = ((y2 - y0) * (px - x2) + (x0 - x2) * (py - y2)) * invDenom;
                float w2 = 1.0f - w0 - w1;

                if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f)
                    continue;

                uint16_t &dst = frameBufferPtr[yLocal * cfg.width + x];
                Color bg(dst);
                dst = bg.blend(waterColor, alphaByte).rgb565;
            }
        }
    }
}