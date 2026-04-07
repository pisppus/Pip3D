#pragma once

#include "Core/Core.h"
#include "Rendering/Display/ZBuffer.h"
#include "Shading.h"
#include <algorithm>

namespace pip3D
{

    class Rasterizer
    {
    public:
        static void accumulateShadowTriangle(float x0, float y0, float z0,
                                             float x1, float y1, float z1,
                                             float x2, float y2, float z2,
                                             int16_t *shadowDepthBuffer,
                                             const DisplayConfig &config)
        {
            const int16_t width = config.width;
            const int16_t height = config.height;
            const int16_t clearDepth = ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT>::clearDepthValue();

            if (!shadowDepthBuffer)
                return;

            if (y0 > y1)
            {
                std::swap(x0, x1);
                std::swap(y0, y1);
                std::swap(z0, z1);
            }
            if (y1 > y2)
            {
                std::swap(x1, x2);
                std::swap(y1, y2);
                std::swap(z1, z2);
            }
            if (y0 > y1)
            {
                std::swap(x0, x1);
                std::swap(y0, y1);
                std::swap(z0, z1);
            }

            if (y0 == y2)
                return;

            float dy1 = y1 - y0;
            float dy2 = y2 - y0;
            if (fabsf(dy1) < 1e-6f && fabsf(dy2) < 1e-6f)
                return;
            const float depthScale = 32767.0f;

            auto rasterHalf = [&](float xa0, float ya0, float za0,
                                  float xa1, float ya1, float za1,
                                  float xb0, float yb0, float zb0,
                                  float xb1, float yb1, float zb1,
                                  int startY, int endYExclusive)
            {
                float dya = ya1 - ya0;
                float dyb = yb1 - yb0;
                if (fabsf(dya) < 1e-6f || fabsf(dyb) < 1e-6f)
                    return;

                float invDya = 1.0f / dya;
                float invDyb = 1.0f / dyb;

                for (int y = startY; y < endYExclusive; ++y)
                {
                    if (y < 0 || y >= height)
                        continue;

                    float sampleY = static_cast<float>(y) + 0.5f;
                    float tA = (sampleY - ya0) * invDya;
                    float tB = (sampleY - yb0) * invDyb;

                    float leftX = xa0 + (xa1 - xa0) * tA;
                    float rightX = xb0 + (xb1 - xb0) * tB;
                    float leftZ = za0 + (za1 - za0) * tA;
                    float rightZ = zb0 + (zb1 - zb0) * tB;

                    if (leftX > rightX)
                    {
                        std::swap(leftX, rightX);
                        std::swap(leftZ, rightZ);
                    }

                    int16_t xStart = static_cast<int16_t>(ceilf(leftX));
                    int16_t xEnd = static_cast<int16_t>(ceilf(rightX)) - 1;
                    if (xStart < 0)
                        xStart = 0;
                    if (xEnd >= width)
                        xEnd = width - 1;
                    if (xStart > xEnd)
                        continue;

                    float dx = rightX - leftX;
                    float zStep = fabsf(dx) > 1e-6f ? (rightZ - leftZ) / dx : 0.0f;
                    float z = leftZ + zStep * ((static_cast<float>(xStart) + 0.5f) - leftX);
                    int32_t depth = static_cast<int32_t>(z * depthScale);
                    int32_t depthStep = static_cast<int32_t>(zStep * depthScale);

                    int16_t *row = shadowDepthBuffer + static_cast<size_t>(y) * width;
                    for (int16_t x = xStart; x <= xEnd; ++x)
                    {
                        int16_t shadowDepth = static_cast<int16_t>(depth);
                        if (row[x] == clearDepth || shadowDepth < row[x])
                            row[x] = shadowDepth;
                        depth += depthStep;
                    }
                }
            };

            int startTop = static_cast<int>(ceilf(y0 - 0.5f));
            int endTopExclusive = static_cast<int>(ceilf(y1 - 0.5f));
            int startBottom = static_cast<int>(ceilf(y1 - 0.5f));
            int endBottomExclusive = static_cast<int>(ceilf(y2 - 0.5f));

            if (fabsf(y1 - y0) > 1e-6f)
            {
                rasterHalf(x0, y0, z0,
                           x1, y1, z1,
                           x0, y0, z0,
                           x2, y2, z2,
                           startTop, endTopExclusive);
            }

            if (fabsf(y2 - y1) > 1e-6f)
            {
                rasterHalf(x1, y1, z1,
                           x2, y2, z2,
                           x0, y0, z0,
                           x2, y2, z2,
                           startBottom, endBottomExclusive);
            }
        }

        static void fillShadowTriangle(float x0, float y0, float z0,
                                       float x1, float y1, float z1,
                                       float x2, float y2, float z2,
                                       uint16_t shadowColor,
                                       uint8_t alpha,
                                       uint16_t *frameBuffer,
                                       ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer,
                                       const DisplayConfig &config,
                                       bool softEdges = true,
                                       int16_t offsetY = 0,
                                       int16_t bandHeight = -1)
        {
            const int16_t width = config.width;
            const int16_t height = config.height;

            if (!frameBuffer || !zBuffer)
                return;

            if (y0 > y1)
            {
                std::swap(x0, x1);
                std::swap(y0, y1);
                std::swap(z0, z1);
            }
            if (y1 > y2)
            {
                std::swap(x1, x2);
                std::swap(y1, y2);
                std::swap(z1, z2);
            }
            if (y0 > y1)
            {
                std::swap(x0, x1);
                std::swap(y0, y1);
                std::swap(z0, z1);
            }

            if (y0 == y2)
                return;
            if (fabsf(x0 - x1) < 1e-6f && fabsf(x1 - x2) < 1e-6f)
                return;

            const uint16_t sr = (shadowColor >> 11) & 0x1F;
            const uint16_t sg = (shadowColor >> 5) & 0x3F;
            const uint16_t sb = shadowColor & 0x1F;

            const float depthScale = 32767.0f;
            const int16_t clearDepth = ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT>::clearDepthValue();
            const int16_t shadowMask = ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT>::shadowFlagMask();
            const int16_t invShadowMask = static_cast<int16_t>(~shadowMask);
            const int16_t *__restrict__ zbBaseConst = zBuffer->getBufferPtr();
            int16_t *__restrict__ zbBase = const_cast<int16_t *>(zbBaseConst);
            auto rasterHalf = [&](float xa0, float ya0, float za0,
                                  float xa1, float ya1, float za1,
                                  float xb0, float yb0, float zb0,
                                  float xb1, float yb1, float zb1,
                                  int startY, int endYExclusive)
            {
                float dya = ya1 - ya0;
                float dyb = yb1 - yb0;
                if (fabsf(dya) < 1e-6f || fabsf(dyb) < 1e-6f)
                    return;

                float invDya = 1.0f / dya;
                float invDyb = 1.0f / dyb;

                for (int y = startY; y < endYExclusive; ++y)
                {
                    if (y < 0 || y >= height)
                        continue;

                    float sampleY = static_cast<float>(y) + 0.5f;
                    float tA = (sampleY - ya0) * invDya;
                    float tB = (sampleY - yb0) * invDyb;

                    float xaf = xa0 + (xa1 - xa0) * tA;
                    float xbf = xb0 + (xb1 - xb0) * tB;
                    float za = za0 + (za1 - za0) * tA;
                    float zb = zb0 + (zb1 - zb0) * tB;

                    if (xaf > xbf)
                    {
                        std::swap(xaf, xbf);
                        std::swap(za, zb);
                    }

                    int16_t xStart = static_cast<int16_t>(ceilf(xaf));
                    int16_t xEnd = static_cast<int16_t>(ceilf(xbf)) - 1;
                    if (xStart < 0)
                        xStart = 0;
                    if (xEnd >= width)
                        xEnd = width - 1;
                    if (xStart > xEnd)
                        continue;

                    uint8_t edgeAlpha = alpha;
                    if (softEdges)
                    {
                        float edgeDist = 1.0f;
                        if (fabsf(sampleY - y0) < 0.51f || fabsf(sampleY - y2) < 0.51f)
                            edgeDist = 0.5f;
                        edgeAlpha = static_cast<uint8_t>(alpha * edgeDist);
                    }

                    const uint16_t invEdgeAlpha = 255 - edgeAlpha;
                    float dx = xbf - xaf;
                    float zStep = fabsf(dx) > 1e-6f ? (zb - za) / dx : 0.0f;
                    float z = za + zStep * ((static_cast<float>(xStart) + 0.5f) - xaf);

                    int32_t depth = static_cast<int32_t>(z * depthScale);
                    int32_t depthStep = static_cast<int32_t>(zStep * depthScale);

                    int16_t yLocal = static_cast<int16_t>(y - offsetY);
                    size_t index = static_cast<size_t>(yLocal) * width + xStart;
                    int16_t *__restrict__ zbRow = zbBase + static_cast<size_t>(y) * width;

                    for (int16_t x = xStart; x <= xEnd; ++x, ++index)
                    {
                        const int16_t stored = zbRow[x];
                        const int16_t depthNoShadow = static_cast<int16_t>(stored & invShadowMask);
                        if (depthNoShadow == clearDepth || (stored & shadowMask) != 0)
                        {
                            depth += depthStep;
                            continue;
                        }

                        const int16_t shadowDepth = static_cast<int16_t>(depth);
                        const int16_t frontTolerance = 40 + (depthNoShadow >> 10);
                        const int16_t backTolerance = 10 + (depthNoShadow >> 11);
                        const int16_t depthDelta = static_cast<int16_t>(depthNoShadow - shadowDepth);
                        if (depthDelta < -backTolerance || depthDelta > frontTolerance)
                        {
                            depth += depthStep;
                            continue;
                        }

                        uint16_t bgColor = frameBuffer[index];
                        uint16_t br = (bgColor >> 11) & 0x1F;
                        uint16_t bg = (bgColor >> 5) & 0x3F;
                        uint16_t bb = bgColor & 0x1F;

                        uint16_t r = (br * invEdgeAlpha + sr * edgeAlpha) >> 8;
                        uint16_t g = (bg * invEdgeAlpha + sg * edgeAlpha) >> 8;
                        uint16_t b = (bb * invEdgeAlpha + sb * edgeAlpha) >> 8;

                        frameBuffer[index] = static_cast<uint16_t>((r << 11) | (g << 5) | b);
                        zbRow[x] = static_cast<int16_t>(stored | shadowMask);

                        depth += depthStep;
                    }
                }
            };

            int startTop = static_cast<int>(ceilf(y0 - 0.5f));
            int endTopExclusive = static_cast<int>(ceilf(y1 - 0.5f));
            int startBottom = static_cast<int>(ceilf(y1 - 0.5f));
            int endBottomExclusive = static_cast<int>(ceilf(y2 - 0.5f));

            if (fabsf(y1 - y0) > 1e-6f)
            {
                rasterHalf(x0, y0, z0,
                           x1, y1, z1,
                           x0, y0, z0,
                           x2, y2, z2,
                           startTop, endTopExclusive);
            }

            if (fabsf(y2 - y1) > 1e-6f)
            {
                rasterHalf(x1, y1, z1,
                           x2, y2, z2,
                           x0, y0, z0,
                           x2, y2, z2,
                           startBottom, endBottomExclusive);
            }
        }

        __attribute__((always_inline, hot)) static inline void IRAM_ATTR fillTriangleSmooth(int16_t x0, int16_t y0, float z0,
                                                                                            int16_t x1, int16_t y1, float z1,
                                                                                            int16_t x2, int16_t y2, float z2,
                                                                                            float r0, float g0, float b0,
                                                                                            float r1, float g1, float b1,
                                                                                            float r2, float g2, float b2,
                                                                                            uint16_t *frameBuffer,
                                                                                            ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer,
                                                                                            const DisplayConfig &config)
        {
            const int16_t width = config.width;
            const int16_t height = config.height;

            const float depthScale = 32767.0f;
            int16_t *const zBufferData = const_cast<int16_t *>(zBuffer ? zBuffer->getBufferPtr() : nullptr);
            const int16_t shadowMask = ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT>::shadowFlagMask();
            const int16_t invShadowMask = static_cast<int16_t>(~shadowMask);

            if (!frameBuffer || !zBuffer || !zBufferData)
                return;

            if (y0 > y1)
            {
                std::swap(x0, x1);
                std::swap(y0, y1);
                std::swap(z0, z1);
                std::swap(r0, r1);
                std::swap(g0, g1);
                std::swap(b0, b1);
            }
            if (y1 > y2)
            {
                std::swap(x1, x2);
                std::swap(y1, y2);
                std::swap(z1, z2);
                std::swap(r1, r2);
                std::swap(g1, g2);
                std::swap(b1, b2);
            }
            if (y0 > y1)
            {
                std::swap(x0, x1);
                std::swap(y0, y1);
                std::swap(z0, z1);
                std::swap(r0, r1);
                std::swap(g0, g1);
                std::swap(b0, b1);
            }

            if (y0 == y2)
                return;
            if (x0 == x1 && x1 == x2)
                return;

            int16_t dy1 = y1 - y0;
            int16_t dy2 = y2 - y0;
            if (dy1 == 0 && dy2 == 0)
                return;

            float dax_step = 0, dbx_step = 0;
            float dz1_step = 0, dz2_step = 0;
            float dr1_step = 0, dg1_step = 0, db1_step = 0;
            float dr2_step = 0, dg2_step = 0, db2_step = 0;

            if (dy1)
            {
                float invDy1 = 1.0f / dy1;
                dax_step = (float)(x1 - x0) * invDy1;
                dz1_step = (z1 - z0) * invDy1;
                dr1_step = (r1 - r0) * invDy1;
                dg1_step = (g1 - g0) * invDy1;
                db1_step = (b1 - b0) * invDy1;
            }
            if (dy2)
            {
                float invDy2 = 1.0f / dy2;
                dbx_step = (float)(x2 - x0) * invDy2;
                dz2_step = (z2 - z0) * invDy2;
                dr2_step = (r2 - r0) * invDy2;
                dg2_step = (g2 - g0) * invDy2;
                db2_step = (b2 - b0) * invDy2;
            }

            float ax = x0, bx = x0;
            float az = z0, bz = z0;
            float ar = r0, ag = g0, ab = b0;
            float br = r0, bg = g0, bb = b0;

            if (dy1)
            {
                for (int16_t y = y0; y <= y1; y++)
                {
                    if (y < 0 || y >= height)
                    {
                        ax += dax_step;
                        az += dz1_step;
                        ar += dr1_step;
                        ag += dg1_step;
                        ab += db1_step;
                        bx += dbx_step;
                        bz += dz2_step;
                        br += dr2_step;
                        bg += dg2_step;
                        bb += db2_step;
                        continue;
                    }

                    float xaf = ax;
                    float xbf = bx;
                    float za = az, zb = bz;
                    float ra = ar, ga = ag, ba = ab;
                    float rb = br, gb = bg, bb2 = bb;

                    if (xaf > xbf)
                    {
                        std::swap(xaf, xbf);
                        std::swap(za, zb);
                        std::swap(ra, rb);
                        std::swap(ga, gb);
                        std::swap(ba, bb2);
                    }

                    int16_t x_start = static_cast<int16_t>(ceilf(xaf));
                    int16_t x_end = static_cast<int16_t>(ceilf(xbf)) - 1;
                    if (x_start < 0)
                        x_start = 0;
                    if (x_end >= width)
                        x_end = width - 1;

                    if (x_start <= x_end && x_start < width && x_end >= 0)
                    {
                        float dx = xbf - xaf;
                        float invDx = dx != 0.0f ? 1.0f / dx : 0.0f;
                        float z_step = (zb - za) * invDx;
                        float r_step = (rb - ra) * invDx;
                        float g_step = (gb - ga) * invDx;
                        float b_step = (bb2 - ba) * invDx;

                        float offset = (float)x_start - xaf;
                        float z = za + z_step * offset;
                        float r = ra + r_step * offset;
                        float g = ga + g_step * offset;
                        float b = ba + b_step * offset;

                        int32_t depthStep = static_cast<int32_t>(z_step * depthScale);
                        int32_t depth = static_cast<int32_t>(z * depthScale);

                        size_t index = (size_t)y * width + x_start;
                        int16_t *zbRow = zBufferData + static_cast<size_t>(y) * width;
                        uint16_t span = (uint16_t)(x_end - x_start + 1);
                        int16_t x = x_start;

                        while (span >= 4)
                        {
                            int16_t stored = zbRow[x];
                            int16_t depthNoShadow = static_cast<int16_t>(stored & invShadowMask);
                            if (static_cast<int16_t>(depth) < depthNoShadow)
                            {
                                zbRow[x] = static_cast<int16_t>((stored & shadowMask) | static_cast<int16_t>(depth));
                                frameBuffer[index] = Shading::applyDithering(r, g, b, x, y);
                            }
                            depth += depthStep;
                            r += r_step;
                            g += g_step;
                            b += b_step;
                            index++;
                            x++;

                            stored = zbRow[x];
                            depthNoShadow = static_cast<int16_t>(stored & invShadowMask);
                            if (static_cast<int16_t>(depth) < depthNoShadow)
                            {
                                zbRow[x] = static_cast<int16_t>((stored & shadowMask) | static_cast<int16_t>(depth));
                                frameBuffer[index] = Shading::applyDithering(r, g, b, x, y);
                            }
                            depth += depthStep;
                            r += r_step;
                            g += g_step;
                            b += b_step;
                            index++;
                            x++;

                            stored = zbRow[x];
                            depthNoShadow = static_cast<int16_t>(stored & invShadowMask);
                            if (static_cast<int16_t>(depth) < depthNoShadow)
                            {
                                zbRow[x] = static_cast<int16_t>((stored & shadowMask) | static_cast<int16_t>(depth));
                                frameBuffer[index] = Shading::applyDithering(r, g, b, x, y);
                            }
                            depth += depthStep;
                            r += r_step;
                            g += g_step;
                            b += b_step;
                            index++;
                            x++;

                            stored = zbRow[x];
                            depthNoShadow = static_cast<int16_t>(stored & invShadowMask);
                            if (static_cast<int16_t>(depth) < depthNoShadow)
                            {
                                zbRow[x] = static_cast<int16_t>((stored & shadowMask) | static_cast<int16_t>(depth));
                                frameBuffer[index] = Shading::applyDithering(r, g, b, x, y);
                            }
                            depth += depthStep;
                            r += r_step;
                            g += g_step;
                            b += b_step;
                            index++;
                            x++;

                            span -= 4;
                        }

                        while (span > 0)
                        {
                            const int16_t stored = zbRow[x];
                            const int16_t depthNoShadow = static_cast<int16_t>(stored & invShadowMask);
                            if (static_cast<int16_t>(depth) < depthNoShadow)
                            {
                                zbRow[x] = static_cast<int16_t>((stored & shadowMask) | static_cast<int16_t>(depth));
                                frameBuffer[index] = Shading::applyDithering(r, g, b, x, y);
                            }
                            depth += depthStep;
                            r += r_step;
                            g += g_step;
                            b += b_step;
                            index++;
                            x++;
                            span--;
                        }
                    }

                    ax += dax_step;
                    az += dz1_step;
                    ar += dr1_step;
                    ag += dg1_step;
                    ab += db1_step;
                    bx += dbx_step;
                    bz += dz2_step;
                    br += dr2_step;
                    bg += dg2_step;
                    bb += db2_step;
                }
            }

            dy1 = y2 - y1;
            if (dy1)
            {
                float invDy1 = 1.0f / dy1;
                dax_step = (float)(x2 - x1) * invDy1;
                dz1_step = (z2 - z1) * invDy1;
                dr1_step = (r2 - r1) * invDy1;
                dg1_step = (g2 - g1) * invDy1;
                db1_step = (b2 - b1) * invDy1;

                ax = x1;
                az = z1;
                ar = r1;
                ag = g1;
                ab = b1;
                bx = x0 + dbx_step * (float)(y1 - y0);
                bz = z0 + dz2_step * (float)(y1 - y0);
                br = r0 + dr2_step * (float)(y1 - y0);
                bg = g0 + dg2_step * (float)(y1 - y0);
                bb = b0 + db2_step * (float)(y1 - y0);

                for (int16_t y = y1 + 1; y <= y2; y++)
                {
                    if (y < 0 || y >= height)
                    {
                        ax += dax_step;
                        az += dz1_step;
                        ar += dr1_step;
                        ag += dg1_step;
                        ab += db1_step;
                        bx += dbx_step;
                        bz += dz2_step;
                        br += dr2_step;
                        bg += dg2_step;
                        bb += db2_step;
                        continue;
                    }

                    float xaf = ax;
                    float xbf = bx;
                    float za = az, zb = bz;
                    float ra = ar, ga = ag, ba = ab;
                    float rb = br, gb = bg, bb2 = bb;

                    if (xaf > xbf)
                    {
                        std::swap(xaf, xbf);
                        std::swap(za, zb);
                        std::swap(ra, rb);
                        std::swap(ga, gb);
                        std::swap(ba, bb2);
                    }

                    int16_t x_start = static_cast<int16_t>(ceilf(xaf));
                    int16_t x_end = static_cast<int16_t>(ceilf(xbf)) - 1;
                    if (x_start < 0)
                        x_start = 0;
                    if (x_end >= width)
                        x_end = width - 1;

                    if (x_start <= x_end && x_start < width && x_end >= 0)
                    {
                        float dx = xbf - xaf;
                        float invDx = dx != 0.0f ? 1.0f / dx : 0.0f;
                        float z_step = (zb - za) * invDx;
                        float r_step = (rb - ra) * invDx;
                        float g_step = (gb - ga) * invDx;
                        float b_step = (bb2 - ba) * invDx;

                        float offset = (float)x_start - xaf;
                        float z = za + z_step * offset;
                        float r = ra + r_step * offset;
                        float g = ga + g_step * offset;
                        float b = ba + b_step * offset;

                        int32_t depthStep = static_cast<int32_t>(z_step * depthScale);
                        int32_t depth = static_cast<int32_t>(z * depthScale);

                        size_t index = (size_t)y * width + x_start;
                        int16_t *zbRow = zBufferData + static_cast<size_t>(y) * width;
                        uint16_t span = (uint16_t)(x_end - x_start + 1);
                        int16_t x = x_start;

                        while (span >= 4)
                        {
                            int16_t stored = zbRow[x];
                            int16_t depthNoShadow = static_cast<int16_t>(stored & invShadowMask);
                            if (static_cast<int16_t>(depth) < depthNoShadow)
                            {
                                zbRow[x] = static_cast<int16_t>((stored & shadowMask) | static_cast<int16_t>(depth));
                                frameBuffer[index] = Shading::applyDithering(r, g, b, x, y);
                            }
                            depth += depthStep;
                            r += r_step;
                            g += g_step;
                            b += b_step;
                            index++;
                            x++;

                            stored = zbRow[x];
                            depthNoShadow = static_cast<int16_t>(stored & invShadowMask);
                            if (static_cast<int16_t>(depth) < depthNoShadow)
                            {
                                zbRow[x] = static_cast<int16_t>((stored & shadowMask) | static_cast<int16_t>(depth));
                                frameBuffer[index] = Shading::applyDithering(r, g, b, x, y);
                            }
                            depth += depthStep;
                            r += r_step;
                            g += g_step;
                            b += b_step;
                            index++;
                            x++;

                            stored = zbRow[x];
                            depthNoShadow = static_cast<int16_t>(stored & invShadowMask);
                            if (static_cast<int16_t>(depth) < depthNoShadow)
                            {
                                zbRow[x] = static_cast<int16_t>((stored & shadowMask) | static_cast<int16_t>(depth));
                                frameBuffer[index] = Shading::applyDithering(r, g, b, x, y);
                            }
                            depth += depthStep;
                            r += r_step;
                            g += g_step;
                            b += b_step;
                            index++;
                            x++;

                            stored = zbRow[x];
                            depthNoShadow = static_cast<int16_t>(stored & invShadowMask);
                            if (static_cast<int16_t>(depth) < depthNoShadow)
                            {
                                zbRow[x] = static_cast<int16_t>((stored & shadowMask) | static_cast<int16_t>(depth));
                                frameBuffer[index] = Shading::applyDithering(r, g, b, x, y);
                            }
                            depth += depthStep;
                            r += r_step;
                            g += g_step;
                            b += b_step;
                            index++;
                            x++;

                            span -= 4;
                        }

                        while (span > 0)
                        {
                            const int16_t stored = zbRow[x];
                            const int16_t depthNoShadow = static_cast<int16_t>(stored & invShadowMask);
                            if (static_cast<int16_t>(depth) < depthNoShadow)
                            {
                                zbRow[x] = static_cast<int16_t>((stored & shadowMask) | static_cast<int16_t>(depth));
                                frameBuffer[index] = Shading::applyDithering(r, g, b, x, y);
                            }
                            depth += depthStep;
                            r += r_step;
                            g += g_step;
                            b += b_step;
                            index++;
                            x++;
                            span--;
                        }
                    }

                    ax += dax_step;
                    az += dz1_step;
                    ar += dr1_step;
                    ag += dg1_step;
                    ab += db1_step;
                    bx += dbx_step;
                    bz += dz2_step;
                    br += dr2_step;
                    bg += dg2_step;
                    bb += db2_step;
                }
            }
        }

        static void fillTriangle(float x0, float y0, float z0,
                                 float x1, float y1, float z1,
                                 float x2, float y2, float z2,
                                 uint16_t color,
                                 uint16_t *frameBuffer,
                                 ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer,
                                 const DisplayConfig &config)
        {
            const int16_t width = config.width;
            const int16_t height = config.height;

            if (!frameBuffer || !zBuffer)
                return;
            if (y0 > y1)
            {
                std::swap(x0, x1);
                std::swap(y0, y1);
                std::swap(z0, z1);
            }
            if (y1 > y2)
            {
                std::swap(x1, x2);
                std::swap(y1, y2);
                std::swap(z1, z2);
            }
            if (y0 > y1)
            {
                std::swap(x0, x1);
                std::swap(y0, y1);
                std::swap(z0, z1);
            }

            if (y0 == y2)
                return;
            if (x0 == x1 && x1 == x2)
                return;

            float dy1 = y1 - y0;
            float dy2 = y2 - y0;

            if (fabsf(dy1) < 1e-6f && fabsf(dy2) < 1e-6f)
                return;
            const float depthScale = 32767.0f;

            auto rasterHalf = [&](float xa0, float ya0, float za0,
                                  float xa1, float ya1, float za1,
                                  float xb0, float yb0, float zb0,
                                  float xb1, float yb1, float zb1,
                                  int startY, int endYExclusive)
            {
                float dya = ya1 - ya0;
                float dyb = yb1 - yb0;
                if (fabsf(dya) < 1e-6f || fabsf(dyb) < 1e-6f)
                    return;

                float invDya = 1.0f / dya;
                float invDyb = 1.0f / dyb;

                for (int y = startY; y < endYExclusive; ++y)
                {
                    if (y < 0 || y >= height)
                        continue;

                    float sampleY = static_cast<float>(y) + 0.5f;

                    float tA = (sampleY - ya0) * invDya;
                    float tB = (sampleY - yb0) * invDyb;

                    float xaf = xa0 + (xa1 - xa0) * tA;
                    float xbf = xb0 + (xb1 - xb0) * tB;
                    float za = za0 + (za1 - za0) * tA;
                    float zb = zb0 + (zb1 - zb0) * tB;

                    if (xaf > xbf)
                    {
                        std::swap(xaf, xbf);
                        std::swap(za, zb);
                    }

                    int16_t x_start = static_cast<int16_t>(ceilf(xaf));
                    int16_t x_end = static_cast<int16_t>(ceilf(xbf)) - 1;
                    if (x_start < 0)
                        x_start = 0;
                    if (x_start >= width)
                        continue;
                    if (x_end >= width)
                        x_end = width - 1;

                    if (x_start <= x_end)
                    {
                        float dx = xbf - xaf;
                        float z_step = fabsf(dx) > 1e-6f ? (zb - za) / dx : 0.0f;
                        float z = za + z_step * ((static_cast<float>(x_start) + 0.5f) - xaf);

                        int32_t depthStep = static_cast<int32_t>(z_step * depthScale);
                        int32_t depthStart = static_cast<int32_t>(z * depthScale);
                        const int16_t localY = static_cast<int16_t>(y - currentBandOffsetY());
                        if (localY < 0 || localY >= SCREEN_BAND_HEIGHT)
                            continue;

                        zBuffer->testAndSetScanline(static_cast<uint16_t>(localY),
                                                    static_cast<uint16_t>(x_start),
                                                    static_cast<uint16_t>(x_end),
                                                    depthStart,
                                                    depthStep,
                                                    frameBuffer,
                                                    color);
                    }
                }
            };

            int startTop = static_cast<int>(ceilf(y0 - 0.5f));
            int endTopExclusive = static_cast<int>(ceilf(y1 - 0.5f));
            int startBottom = static_cast<int>(ceilf(y1 - 0.5f));
            int endBottomExclusive = static_cast<int>(ceilf(y2 - 0.5f));

            if (fabsf(y1 - y0) > 1e-6f)
            {
                rasterHalf(x0, y0, z0,
                           x1, y1, z1,
                           x0, y0, z0,
                           x2, y2, z2,
                           startTop, endTopExclusive);
            }

            if (fabsf(y2 - y1) > 1e-6f)
            {
                rasterHalf(x1, y1, z1,
                           x2, y2, z2,
                           x0, y0, z0,
                           x2, y2, z2,
                           startBottom, endBottomExclusive);
            }
        }
    };

}

