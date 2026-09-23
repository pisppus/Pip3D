#include <new>
#include <vector>

#include "Core/Platform.hpp"
#include "Math/Algebra.hpp"
#include "Camera/Camera.hpp"
#include "Rendering/Buffers/FrameBuffer.hpp"
#include "Rendering/Buffers/ZBuffer.hpp"
#include "Rendering/Lighting/Fog.hpp"
#include "Rendering/Lighting/Lighting.hpp"
#include "Rendering/Pipeline/Culling.hpp"
#include "Rendering/Pipeline/DrawCache.hpp"
#include "Rendering/Pipeline/MeshDraw.hpp"
#include "Rendering/Pipeline/Rasterizer/Smooth.hpp"
#include "Rendering/Pipeline/Rasterizer/Solid.hpp"
#include "Rendering/Pipeline/Rasterizer/Textured.hpp"
#include "Rendering/Pipeline/Rasterizer/Lightmap.hpp"
#include "Rendering/Pipeline/Shading.hpp"
#include "Rendering/Lighting/Baked.hpp"
#include "Rendering/Resources/Texture.hpp"
#include "Rendering/Resources/Textures/Missing.hpp"
#include "Rendering/Renderer.hpp"

namespace pip3D
{
    namespace
    {

        struct ProbeLightSample
        {
            float sunVis;
            float skyAO;
            float staticR, staticG, staticB;
        };

        PIP3D_HOT inline void sampleProbe(const MeshInstance *inst, const Vector3 &worldPos,
                                          ProbeLightSample &out) noexcept
        {
            const auto *grid = Rasterizer::g_bakedState.probes;
            if (grid && grid->valid() && inst && !inst->getIgnoreBakedProbes())
            {
                grid->sampleWithTint(worldPos, out.sunVis, out.skyAO,
                                     out.staticR, out.staticG, out.staticB);
            }
            else
            {
                out.sunVis = 1.0f;
                out.skyAO = 1.0f;
                out.staticR = out.staticG = out.staticB = 0.0f;
            }
        }

        PIP3D_HOT inline bool usesProbes(const MeshInstance *inst) noexcept
        {
            return inst && !inst->hasLightmap() && !inst->getIgnoreBakedProbes() &&
                   Rasterizer::g_bakedState.probes && Rasterizer::g_bakedState.probes->valid() &&
                   Rasterizer::g_bakedState.mode != static_cast<uint8_t>(BakedLightMode::OFF);
        }

        PIP3D_FORCE_INLINE void probeShadeFace(float &outR, float &outG, float &outB,
                                               float faceR, float faceG, float faceB,
                                               const ProbeLightSample &s) noexcept
        {
            const float m = ProbeConsts::skyFaceMod(s.skyAO);
            outR = clamp(faceR * m + s.staticR * faceR, 0.0f, 1.0f);
            outG = clamp(faceG * m + s.staticG * faceG, 0.0f, 1.0f);
            outB = clamp(faceB * m + s.staticB * faceB, 0.0f, 1.0f);
        }

        PIP3D_FORCE_INLINE void probeScaleLights(Light *dst, const Light *src, int count,
                                                 const ProbeLightSample &s) noexcept
        {
            const float k = ProbeConsts::sunMod(s.sunVis);
            for (int i = 0; i < count; ++i)
            {
                dst[i] = src[i];
                if (dst[i].type == LIGHT_DIRECTIONAL)
                {
                    dst[i].intensity *= k;
                    dst[i].cachedR *= k;
                    dst[i].cachedG *= k;
                    dst[i].cachedB *= k;
                }
            }
        }

        PIP3D_FORCE_INLINE void probeShadeVertex(
            const Vector3 &vertexPos, const Vector3 &normal, const Vector3 &camPos,
            const Light *lights, int lightCount,
            float faceR, float faceG, float faceB,
            const ProbeLightSample &s,
            float &lr, float &lg, float &lb) noexcept
        {
            const float m = ProbeConsts::skyFaceMod(s.skyAO);
            Shading::calculateVertexLightingGouraud(vertexPos, normal, camPos,
                                                    lights, lightCount,
                                                    faceR * m, faceG * m, faceB * m,
                                                    lr, lg, lb,
                                                    ProbeConsts::sunMod(s.sunVis));
            lr = clamp(lr + s.staticR * faceR, 0.0f, 1.0f);
            lg = clamp(lg + s.staticG * faceG, 0.0f, 1.0f);
            lb = clamp(lb + s.staticB * faceB, 0.0f, 1.0f);
        }
    }

    PIP3D_HOT IRAM_ATTR static int collectActiveLightsForBounds(
        const Vector3 &center, float radius,
        const Light *allLights, int allLightCount,
        Light *outLights, int maxLights)
    {
        int count = 0;

        for (int i = 0; i < allLightCount && count < maxLights; ++i)
        {
            if (allLights[i].type == LIGHT_DIRECTIONAL)
                outLights[count++] = allLights[i];
        }

        if (count >= maxLights)
            return count;

        struct LightScore
        {
            int index;
            float score;
        };
        LightScore scores[16];
        int scoreCount = 0;

        for (int i = 0; i < allLightCount && scoreCount < 16; ++i)
        {
            const Light &l = allLights[i];
            if (l.type != LIGHT_POINT)
                continue;

            const float dx = l.position.x - center.x;
            const float dy = l.position.y - center.y;
            const float dz = l.position.z - center.z;
            const float distSq = dx * dx + dy * dy + dz * dz;

            const float maxDist = l.range + radius;
            if (l.range > 0.0f && distSq > maxDist * maxDist)
                continue;

            float atten = 1.0f;
            if (l.range > 0.0f)
                atten = FastMath::fastReciprocal(1.0f + distSq * l.invRangeSq);

            scores[scoreCount++] = {i, l.intensity * atten};
        }

        const int remaining = maxLights - count;
        if (scoreCount > remaining)
        {
            for (int i = 1; i < scoreCount; ++i)
            {
                const LightScore temp = scores[i];
                int j = i - 1;
                while (j >= 0 && scores[j].score < temp.score)
                {
                    scores[j + 1] = scores[j];
                    --j;
                }
                scores[j + 1] = temp;
            }
        }

        for (int i = 0; i < scoreCount && count < maxLights; ++i)
            outLights[count++] = allLights[scores[i].index];

        return count;
    }

    void Renderer::draw(MeshInstance *instance)
    {
        if (unlikely(!instance || !instance->isVisible()))
            return;

        Mesh *mesh = instance->getMesh();
        if (!mesh)
            return;

        if (shadowsEnabled)
        {
            if (!instance->hasActiveLightmap(bakedLightMode_))
            {
                if (instance->getBlobShadow())
                    blobShadowQueue_.push_back(instance);
                else if (mesh->getCastShadows())
                    shadowQueue_.push_back(instance);
            }
        }

        if (instance->isEmissive())
            emissiveQueue_.push_back(instance);

        opaqueQueue_.push_back(instance);
    }

    void Renderer::flushQueue()
    {
        const size_t n = opaqueQueue_.size();

        if (opaqueSortEnabled_ && n > 1)
        {
            const Camera &cam = cameras[activeCameraIndex];
            const Vector3 camPos = cam.position;
            const Vector3 camFwd = cam.forward();

            float *PIP3D_RESTRICT eyeZ = static_cast<float *>(alloca(n * sizeof(float)));
            for (size_t i = 0; i < n; ++i)
                eyeZ[i] = Culling::computeEyeZ(opaqueQueue_[i]->center(), camPos, camFwd);

            for (size_t i = 1; i < n; ++i)
            {
                const float keyZ = eyeZ[i];
                MeshInstance *keyInst = opaqueQueue_[i];
                size_t j = i;
                while (j > 0 && eyeZ[j - 1] > keyZ)
                {
                    eyeZ[j] = eyeZ[j - 1];
                    opaqueQueue_[j] = opaqueQueue_[j - 1];
                    --j;
                }
                eyeZ[j] = keyZ;
                opaqueQueue_[j] = keyInst;
            }
        }

        for (size_t i = 0; i < opaqueQueue_.size(); ++i)
            drawMeshInstanceInternal(opaqueQueue_[i], false);

        for (size_t i = 0; i < shadowQueue_.size(); ++i)
            drawMeshInstanceShadow(shadowQueue_[i]);

        const float blobOpacity = shadowSettings.shadowOpacity;
        for (size_t i = 0; i < blobShadowQueue_.size(); ++i)
        {
            MeshInstance *inst = blobShadowQueue_[i];
            drawBlobShadow(inst->pos(), inst->radius(), blobOpacity);
        }
    }

    MeshInstance *Renderer::acquirePlacementInstance()
    {
        if (!placementInstance_)
            placementInstance_ = new MeshInstance();
        return placementInstance_;
    }

    void Renderer::drawPlacements(const PlacementSet &set)
    {
        if (!set.placements || set.placementCount == 0 || !set.props || set.propCount == 0)
            return;

        MeshInstance *inst = acquirePlacementInstance();

        const float scale = set.unitScale;
        const float yawToRad = kTwoPi * (1.0f / 256.0f);

        for (uint32_t i = 0; i < set.placementCount; ++i)
        {
            const Placement &pl = set.placements[i];
            if (!(pl.flags & kPlacementVisible) || pl.propIndex >= set.propCount)
                continue;

            Mesh *mesh = set.props[pl.propIndex];
            if (!mesh)
                continue;

            inst->setMesh(mesh);
            inst->setPosition(Vector3(static_cast<float>(pl.x) * scale,
                                      static_cast<float>(pl.y) * scale,
                                      static_cast<float>(pl.z) * scale));
            inst->setRotation(Quaternion::fromAxisAngle(Vector3(0.0f, 1.0f, 0.0f),
                                                        static_cast<float>(pl.yaw) * yawToRad));
            const float propScale = (set.scaleProps && mesh->getQScale() > 0.0f)
                                        ? scale / mesh->getQScale()
                                        : 1.0f;
            inst->setScale(Vector3(propScale, propScale, propScale));
            inst->setColor(pl.color);

            drawMeshInstanceInternal(inst, true);
        }
    }

    IRAM_ATTR void Renderer::drawMeshInstanceInternal(MeshInstance *instance, bool performFrustumCull)
    {
        if (!instance || !instance->isVisible())
            return;

        Mesh *mesh = instance->getMesh();
        if (!mesh)
            return;

        const Vector3 center = instance->center();
        const float radius = instance->radius();

        if (performFrustumCull)
        {
            if (!frustum.testSphere(center, radius))
            {
                statsInstancesFrustumCulled++;
                return;
            }
        }

        const Camera &cam = cameras[activeCameraIndex];
        const Vector3 &camFwd = cam.forward();

        const float zEye = Culling::computeEyeZ(center, cam.position, camFwd);

        float radiusPixels = 0.0f;
        if (zEye > cam.nearPlane)
        {
            const float projScale = Culling::ensureProjScale(cam, viewport);
            radiusPixels = Culling::computeScreenRadius(radius, zEye, projScale);

            if (radiusPixels < 1.0f)
            {
                statsInstancesTotal++;
                return;
            }
        }

        drawMeshInstanceBanded(instance, zEye, radiusPixels);
    }

    PIP3D_HOT IRAM_ATTR static bool buildChunkBandCache(
        Renderer &r,
        MeshInstance *instance,
        ChunkBandCache &cache,
        uint32_t frameStamp)
    {
        Mesh *mesh = instance->getMesh();
        const uint32_t chunkCount = mesh->numChunks();
        const MeshChunk *PIP3D_RESTRICT chunks = mesh->chunkData();
        const float qs = mesh->getQScale();

        const Camera &cam = r.getCamera();
        const Vector3 camPos = cam.position;
        const Vector3 camFwd = cam.forward();
        const float nearPlane = cam.nearPlane;
        const float projScale = Culling::ensureProjScale(cam, r.getViewport());
        const Frustum &frustum = r.getFrustum();
        const Matrix4x4 &viewProjMatrix = r.getViewProjMatrix();
        const Matrix4x4 &worldTransform = instance->transform();

        const float viewportHalfWidth = static_cast<float>(r.getViewport().width) * 0.5f;
        const float viewportHalfHeight = static_cast<float>(r.getViewport().height) * 0.5f;
        const float viewportWidthF = static_cast<float>(r.getViewport().width);

        const float r00 = fabsf(worldTransform.m[0]), r01 = fabsf(worldTransform.m[4]), r02 = fabsf(worldTransform.m[8]);
        const float r10 = fabsf(worldTransform.m[1]), r11 = fabsf(worldTransform.m[5]), r12 = fabsf(worldTransform.m[9]);
        const float r20 = fabsf(worldTransform.m[2]), r21 = fabsf(worldTransform.m[6]), r22 = fabsf(worldTransform.m[10]);

        const int32_t bandCountInt = static_cast<int32_t>(SCREEN_BAND_COUNT);
        const int32_t bandHeightInt = static_cast<int32_t>(SCREEN_BAND_HEIGHT);

        cache.reset(frameStamp);

        if (chunkCount == 0)
            return true;

        if (!cache.ensure(static_cast<uint16_t>(chunkCount)))
            return false;

        for (uint32_t i = 0; i < chunkCount; ++i)
        {
            const MeshChunk &chunk = chunks[i];

            const Vector3 localMin(static_cast<float>(chunk.minX) * qs,
                                   static_cast<float>(chunk.minY) * qs,
                                   static_cast<float>(chunk.minZ) * qs);
            const Vector3 localMax(static_cast<float>(chunk.maxX) * qs,
                                   static_cast<float>(chunk.maxY) * qs,
                                   static_cast<float>(chunk.maxZ) * qs);
            const Vector3 localCenter = (localMin + localMax) * 0.5f;
            const Vector3 localHalf = (localMax - localMin) * 0.5f;

            const Vector3 worldCenter = worldTransform.transformNoDiv(localCenter);
            const float rX = r00 * localHalf.x + r01 * localHalf.y + r02 * localHalf.z;
            const float rY = r10 * localHalf.x + r11 * localHalf.y + r12 * localHalf.z;
            const float rZ = r20 * localHalf.x + r21 * localHalf.y + r22 * localHalf.z;

            const Vector3 worldMin = worldCenter - Vector3(rX, rY, rZ);
            const Vector3 worldMax = worldCenter + Vector3(rX, rY, rZ);

            if (!frustum.testAABB(worldMin, worldMax))
                continue;

            const float radiusSq = rX * rX + rY * rY + rZ * rZ;
            const float chunkRadius = (radiusSq > 0.0f)
                                          ? radiusSq * FastMath::fastInvSqrt(radiusSq)
                                          : 0.0f;

            if (chunk.coneSin > 0)
            {
                const float toCamX = camPos.x - worldCenter.x;
                const float toCamY = camPos.y - worldCenter.y;
                const float toCamZ = camPos.z - worldCenter.z;
                const float distSq = toCamX * toCamX + toCamY * toCamY + toCamZ * toCamZ;
                if (distSq > 4.0f * radiusSq)
                {
                    const Vector3 axisL = PackedNormal(chunk.coneNormal).get();
                    float axW = worldTransform.m[0] * axisL.x + worldTransform.m[4] * axisL.y + worldTransform.m[8] * axisL.z;
                    float ayW = worldTransform.m[1] * axisL.x + worldTransform.m[5] * axisL.y + worldTransform.m[9] * axisL.z;
                    float azW = worldTransform.m[2] * axisL.x + worldTransform.m[6] * axisL.y + worldTransform.m[10] * axisL.z;
                    const float axSq = axW * axW + ayW * ayW + azW * azW;
                    if (axSq > 1e-16f)
                    {
                        const float invDist = FastMath::fastInvSqrt(distSq);
                        const float invAx = FastMath::fastInvSqrt(axSq);
                        const float dot = (axW * invAx) * (toCamX * invDist) +
                                          (ayW * invAx) * (toCamY * invDist) +
                                          (azW * invAx) * (toCamZ * invDist);

                        if (dot <= -(static_cast<float>(chunk.coneSin) * (1.0f / 255.0f) +
                                     chunkRadius * invDist))
                            continue;
                    }
                }
            }

            const float chunkEyeZ = (worldCenter.x - camPos.x) * camFwd.x +
                                    (worldCenter.y - camPos.y) * camFwd.y +
                                    (worldCenter.z - camPos.z) * camFwd.z;

            uint8_t minBand = 0;
            uint8_t maxBand = static_cast<uint8_t>(SCREEN_BAND_COUNT - 1);

            if (chunkEyeZ > nearPlane)
            {
                const float chunkEyeZNear = (chunkEyeZ - chunkRadius) > nearPlane
                                                ? (chunkEyeZ - chunkRadius)
                                                : nearPlane;
                const float chunkRadiusPx = chunkRadius * projScale * FastMath::fastReciprocal(chunkEyeZNear);
                const Vector3 scrCenter = CameraController::project(worldCenter, viewProjMatrix,
                                                                    viewportHalfWidth, viewportHalfHeight, 0, 0);

                const float chunkMinX = scrCenter.x - chunkRadiusPx;
                const float chunkMaxX = scrCenter.x + chunkRadiusPx;
                if (chunkMaxX < 0.0f || chunkMinX >= viewportWidthF)
                    continue;

                const float chunkMinY = scrCenter.y - chunkRadiusPx;
                const float chunkMaxY = scrCenter.y + chunkRadiusPx;

                const int32_t minBand32 = static_cast<int32_t>(chunkMinY) / bandHeightInt;
                const int32_t maxBand32 = static_cast<int32_t>(chunkMaxY) / bandHeightInt;

                if (maxBand32 < 0 || minBand32 >= bandCountInt)
                    continue;

                minBand = static_cast<uint8_t>(clamp(minBand32, 0, bandCountInt - 1));
                maxBand = static_cast<uint8_t>(clamp(maxBand32, 0, bandCountInt - 1));
            }

            cache.records[cache.visibleCount++] = {
                static_cast<uint16_t>(i),
                minBand,
                maxBand};
        }

        return true;
    }

    namespace
    {

        constexpr uint16_t kProbeCacheMaxVerts = 512;
        struct ProbeCache
        {

            uint8_t *PIP3D_RESTRICT data = nullptr;
            const float *tintR = nullptr;
            const float *tintG = nullptr;
            const float *tintB = nullptr;

            PIP3D_FORCE_INLINE void sample(uint32_t i, ProbeLightSample &out) const noexcept
            {
                const uint8_t *p = data + i * 4;
                out.sunVis = static_cast<float>(p[0]) * (1.0f / 255.0f);
                out.skyAO = static_cast<float>(p[1]) * (1.0f / 255.0f);
                const float l = static_cast<float>(p[2]) * (1.0f / 255.0f);
                const uint8_t ti = p[3] & 3;
                out.staticR = tintR[ti] * l;
                out.staticG = tintG[ti] * l;
                out.staticB = tintB[ti] * l;
            }
        };

        struct FaceDrawCtx
        {
            MeshInstance *instance;
            Mesh *mesh;
            const Matrix4x4 &worldTransform;
            Vector3 *worldVerts;
            Vector3 *screenVerts;
            Vector3 *worldNormals;
            const Face *fbase;
            const uint8_t *attrBase;
            uint32_t attrRecSize;
            const MeshChunk *chunk;
            const uint8_t *posBase;
            uint32_t posRecSize;
            ChunkPosDecode posDecode;
            uint32_t winPosBase;
            uint32_t winNormBase;
            uint32_t subMeshCount;
            bool hasSubMeshes;
            uint32_t faceStride;
            float nearZWThreshold;

            ShadingMode effectiveMode;
            bool effectiveTextured;
            const Texture *meshTexture;
            const LMPaletteAtlas *lmAtlas;
            bool lmActive;
            bool useProbes;
            bool useUniformColor;
            uint16_t uniformColor;
            float instR, instG, instB;
            const Light *localLights;
            int localLightCount;

            const Matrix4x4 &viewProjMatrix;
            const Viewport &viewport;
            const DisplayConfig &fbConfig;
            FrameBuffer &framebuffer;
            uint16_t *frameBuffer;
            ZBuffer *zBuffer;
            Vector3 camPos;
            Vector3 camFwd;
            float nearClip;
            bool doBackfaceCull;
            int16_t bandTop, bandBottom;
            float bandTopF;
            float viewportWidth, viewportHalfWidth, viewportHalfHeight;

            uint32_t &currentSubMesh;
            uint32_t &statsTotal;
            uint32_t &statsCulled;
        };

        PIP3D_HOT PIP3D_FORCE_INLINE static void drawMeshFace(FaceDrawCtx &ctx,
                                                              const ProbeCache *__restrict__ probes,
                                                              const uint32_t faceIdx) noexcept
        {
            const uint8_t *PIP3D_RESTRICT frec =
                reinterpret_cast<const uint8_t *>(ctx.fbase) +
                static_cast<size_t>(faceIdx) * ctx.faceStride;
            const uint32_t pi0 = frec[0], pi1 = frec[1], pi2 = frec[2];

            const Vector3 &camPos = ctx.camPos;
            const Vector3 &camFwd = ctx.camFwd;
            Vector3 v0, v1, v2;
            if (likely(ctx.worldVerts))
            {
                v0 = ctx.worldVerts[ctx.winPosBase + pi0];
                v1 = ctx.worldVerts[ctx.winPosBase + pi1];
                v2 = ctx.worldVerts[ctx.winPosBase + pi2];
            }
            else
            {
                const MeshChunk &chunk = *ctx.chunk;
                v0 = ctx.worldTransform.transformNoDiv(Mesh::chunkPosition(chunk, ctx.posDecode, ctx.posBase + pi0 * ctx.posRecSize));
                v1 = ctx.worldTransform.transformNoDiv(Mesh::chunkPosition(chunk, ctx.posDecode, ctx.posBase + pi1 * ctx.posRecSize));
                v2 = ctx.worldTransform.transformNoDiv(Mesh::chunkPosition(chunk, ctx.posDecode, ctx.posBase + pi2 * ctx.posRecSize));
            }

            if (ctx.doBackfaceCull)
            {
                const float e1x = v1.x - v0.x, e1y = v1.y - v0.y, e1z = v1.z - v0.z;
                const float e2x = v2.x - v0.x, e2y = v2.y - v0.y, e2z = v2.z - v0.z;
                const float nx = e1y * e2z - e1z * e2y;
                const float ny = e1z * e2x - e1x * e2z;
                const float nz = e1x * e2y - e1y * e2x;
                const float vx = v0.x - camPos.x;
                const float vy = v0.y - camPos.y;
                const float vz = v0.z - camPos.z;
                if (nx * vx + ny * vy + nz * vz >= 0.0f)
                {
                    ++ctx.statsCulled;
                    return;
                }
            }

            Vector3 p0, p1, p2;
            if (likely(ctx.screenVerts))
            {
                p0 = ctx.screenVerts[ctx.winPosBase + pi0];
                p1 = ctx.screenVerts[ctx.winPosBase + pi1];
                p2 = ctx.screenVerts[ctx.winPosBase + pi2];
            }
            else
            {
                p0 = CameraController::project(v0, ctx.viewProjMatrix, ctx.viewportHalfWidth, ctx.viewportHalfHeight, 0, 0);
                p1 = CameraController::project(v1, ctx.viewProjMatrix, ctx.viewportHalfWidth, ctx.viewportHalfHeight, 0, 0);
                p2 = CameraController::project(v2, ctx.viewProjMatrix, ctx.viewportHalfWidth, ctx.viewportHalfHeight, 0, 0);
            }

            const float nearZT = ctx.nearZWThreshold;
            const bool behind0 = (p0.z <= 0.0f) | (p0.z > nearZT);
            const bool behind1 = (p1.z <= 0.0f) | (p1.z > nearZT);
            const bool behind2 = (p2.z <= 0.0f) | (p2.z > nearZT);
            if (unlikely(behind0 & behind1 & behind2))
            {
                ++ctx.statsCulled;
                return;
            }
            const bool partiallyClipped = behind0 | behind1 | behind2;

            if (!partiallyClipped)
            {
                const float minY = (p0.y < p1.y) ? ((p0.y < p2.y) ? p0.y : p2.y) : ((p1.y < p2.y) ? p1.y : p2.y);
                const float maxY = (p0.y > p1.y) ? ((p0.y > p2.y) ? p0.y : p2.y) : ((p1.y > p2.y) ? p1.y : p2.y);
                if (maxY < ctx.bandTop || minY >= ctx.bandBottom)
                    return;
                const float minX = (p0.x < p1.x) ? ((p0.x < p2.x) ? p0.x : p2.x) : ((p1.x < p2.x) ? p1.x : p2.x);
                const float maxX = (p0.x > p1.x) ? ((p0.x > p2.x) ? p0.x : p2.x) : ((p1.x > p2.x) ? p1.x : p2.x);
                if (maxX < 0.0f || minX >= ctx.viewportWidth)
                    return;
            }

            ++ctx.statsTotal;

            float d0 = 0.0f, d1 = 0.0f, d2 = 0.0f;
            if (partiallyClipped || ctx.effectiveTextured || ctx.lmActive ||
                ctx.effectiveMode == SHADING_PHONG)
            {
                d0 = (v0.x - camPos.x) * camFwd.x + (v0.y - camPos.y) * camFwd.y + (v0.z - camPos.z) * camFwd.z;
                d1 = (v1.x - camPos.x) * camFwd.x + (v1.y - camPos.y) * camFwd.y + (v1.z - camPos.z) * camFwd.z;
                d2 = (v2.x - camPos.x) * camFwd.x + (v2.y - camPos.y) * camFwd.y + (v2.z - camPos.z) * camFwd.z;
            }

            float faceR = ctx.instR, faceG = ctx.instG, faceB = ctx.instB;
            if (ctx.hasSubMeshes)
            {
                while (ctx.currentSubMesh < ctx.subMeshCount &&
                       faceIdx >= ctx.mesh->subMeshFaceEnd(ctx.currentSubMesh))
                    ++ctx.currentSubMesh;
                if (ctx.currentSubMesh < ctx.subMeshCount)
                {
                    float sr, sg, sb;
                    ctx.mesh->subMeshColor(ctx.currentSubMesh).toFloat(sr, sg, sb);
                    faceR = ctx.instR * sr;
                    faceG = ctx.instG * sg;
                    faceB = ctx.instB * sb;
                }
            }

            if (ctx.effectiveTextured)
            {
                float tu0, tv0, tu1, tv1, tu2, tv2;
                if (ctx.mesh->hasUV())
                {
                    ctx.mesh->attrUV(ctx.attrBase + frec[3] * ctx.attrRecSize, tu0, tv0);
                    ctx.mesh->attrUV(ctx.attrBase + frec[4] * ctx.attrRecSize, tu1, tv1);
                    ctx.mesh->attrUV(ctx.attrBase + frec[5] * ctx.attrRecSize, tu2, tv2);
                }
                else
                {
                    tu0 = tv0 = tu1 = tv1 = tu2 = tv2 = 0.0f;
                }

                if (ctx.lmActive)
                {
                    float mu0, mv0, mu1, mv1, mu2, mv2;
                    ctx.instance->lightmapCornerUV(faceIdx, 0, mu0, mv0);
                    ctx.instance->lightmapCornerUV(faceIdx, 1, mu1, mv1);
                    ctx.instance->lightmapCornerUV(faceIdx, 2, mu2, mv2);
                    bool drew = false;
                    if (!partiallyClipped)
                    {
                        drew = Rasterizer::fillTriangleTexturedLM(
                            p0.x, p0.y - ctx.bandTopF, p0.z,
                            p1.x, p1.y - ctx.bandTopF, p1.z,
                            p2.x, p2.y - ctx.bandTopF, p2.z,
                            tu0, tv0,
                            tu1, tv1,
                            tu2, tv2,
                            mu0, mv0, mu1, mv1, mu2, mv2,
                            d0, d1, d2,
                            *ctx.meshTexture, *ctx.lmAtlas,
                            ctx.frameBuffer, ctx.zBuffer, ctx.fbConfig);
                    }
                    else
                    {
                        const MeshRenderer::ClipVertLM cv[3] = {
                            {v0, tu0, tv0, d0, 0, 0, 0, mu0, mv0},
                            {v1, tu1, tv1, d1, 0, 0, 0, mu1, mv1},
                            {v2, tu2, tv2, d2, 0, 0, 0, mu2, mv2}};
                        drew = MeshRenderer::clipAndDrawNear(cv, ctx.nearClip, ctx.viewport, ctx.viewProjMatrix, ctx.framebuffer,
                                                             [&](const Vector3 *proj, const MeshRenderer::ClipVertLM *cvp, int a, int b, int c) -> bool
                                                             {
                                                                 return Rasterizer::fillTriangleTexturedLM(
                                                                     proj[a].x, proj[a].y - ctx.bandTopF, proj[a].z,
                                                                     proj[b].x, proj[b].y - ctx.bandTopF, proj[b].z,
                                                                     proj[c].x, proj[c].y - ctx.bandTopF, proj[c].z,
                                                                     cvp[a].u, cvp[a].v,
                                                                     cvp[b].u, cvp[b].v,
                                                                     cvp[c].u, cvp[c].v,
                                                                     cvp[a].mu, cvp[a].mv,
                                                                     cvp[b].mu, cvp[b].mv,
                                                                     cvp[c].mu, cvp[c].mv,
                                                                     cvp[a].d, cvp[b].d, cvp[c].d,
                                                                     *ctx.meshTexture, *ctx.lmAtlas,
                                                                     ctx.frameBuffer, ctx.zBuffer, ctx.fbConfig);
                                                             });
                    }
                    if (unlikely(!drew))
                        ++ctx.statsCulled;
                    return;
                }

                float lr0 = 0.0f, lg0 = 0.0f, lb0 = 0.0f;
                float lr1 = 0.0f, lg1 = 0.0f, lb1 = 0.0f;
                float lr2 = 0.0f, lg2 = 0.0f, lb2 = 0.0f;

                if (ctx.effectiveMode == SHADING_GOURAUD)
                {
                    Vector3 n0, n1, n2;
                    if (likely(ctx.worldNormals))
                    {
                        n0 = ctx.worldNormals[ctx.winNormBase + frec[3]];
                        n1 = ctx.worldNormals[ctx.winNormBase + frec[4]];
                        n2 = ctx.worldNormals[ctx.winNormBase + frec[5]];
                    }
                    else if (ctx.mesh->hasNormals())
                    {
                        n0 = ctx.mesh->attrNormal(ctx.attrBase + frec[3] * ctx.attrRecSize);
                        n1 = ctx.mesh->attrNormal(ctx.attrBase + frec[4] * ctx.attrRecSize);
                        n2 = ctx.mesh->attrNormal(ctx.attrBase + frec[5] * ctx.attrRecSize);
                    }
                    else
                    {
                        Vector3 fn = (v1 - v0).cross(v2 - v0);
                        const float len = sqrtf(fn.x * fn.x + fn.y * fn.y + fn.z * fn.z);
                        if (len > 1e-8f)
                            fn = fn * (1.0f / len);
                        n0 = n1 = n2 = fn;
                    }

                    if (ctx.useProbes)
                    {
                        ProbeLightSample p0, p1, p2;
                        if (probes)
                        {
                            probes->sample(ctx.winPosBase + pi0, p0);
                            probes->sample(ctx.winPosBase + pi1, p1);
                            probes->sample(ctx.winPosBase + pi2, p2);
                        }
                        else
                        {
                            sampleProbe(ctx.instance, v0, p0);
                            sampleProbe(ctx.instance, v1, p1);
                            sampleProbe(ctx.instance, v2, p2);
                        }
                        probeShadeVertex(v0, n0, camPos, ctx.localLights, ctx.localLightCount,
                                         faceR, faceG, faceB, p0, lr0, lg0, lb0);
                        probeShadeVertex(v1, n1, camPos, ctx.localLights, ctx.localLightCount,
                                         faceR, faceG, faceB, p1, lr1, lg1, lb1);
                        probeShadeVertex(v2, n2, camPos, ctx.localLights, ctx.localLightCount,
                                         faceR, faceG, faceB, p2, lr2, lg2, lb2);
                    }
                    else
                    {
                        Shading::calculateVertexLightingGouraud(v0, n0, camPos,
                                                                ctx.localLights, ctx.localLightCount, faceR, faceG, faceB, lr0, lg0, lb0);
                        Shading::calculateVertexLightingGouraud(v1, n1, camPos,
                                                                ctx.localLights, ctx.localLightCount, faceR, faceG, faceB, lr1, lg1, lb1);
                        Shading::calculateVertexLightingGouraud(v2, n2, camPos,
                                                                ctx.localLights, ctx.localLightCount, faceR, faceG, faceB, lr2, lg2, lb2);
                    }
                }
                else
                {

                    float prFaceR = faceR, prFaceG = faceG, prFaceB = faceB;
                    const Light *prLights = ctx.localLights;
                    Light prMod[4];
                    if (ctx.useProbes)
                    {
                        Vector3 cen = (v0 + v1 + v2) * (1.0f / 3.0f);
                        ProbeLightSample ps;
                        sampleProbe(ctx.instance, cen, ps);
                        probeScaleLights(prMod, ctx.localLights, ctx.localLightCount, ps);
                        prLights = prMod;
                        probeShadeFace(prFaceR, prFaceG, prFaceB, faceR, faceG, faceB, ps);
                    }
                    Shading::calculateFaceLighting(
                        v0, v1, v2, camPos,
                        prLights, ctx.localLightCount,
                        prFaceR, prFaceG, prFaceB,
                        lr0, lg0, lb0);
                    lr1 = lr0;
                    lg1 = lg0;
                    lb1 = lb0;
                    lr2 = lr0;
                    lg2 = lg0;
                    lb2 = lb0;
                }

                bool drew = false;
                if (!partiallyClipped)
                {
                    drew = Rasterizer::fillTriangleTextured(
                        p0.x, p0.y - ctx.bandTopF, p0.z,
                        p1.x, p1.y - ctx.bandTopF, p1.z,
                        p2.x, p2.y - ctx.bandTopF, p2.z,
                        tu0, tv0,
                        tu1, tv1,
                        tu2, tv2,
                        d0, d1, d2,
                        lr0, lg0, lb0,
                        lr1, lg1, lb1,
                        lr2, lg2, lb2,
                        *ctx.meshTexture,
                        ctx.frameBuffer,
                        ctx.zBuffer,
                        ctx.fbConfig);
                }
                else
                {
                    const MeshRenderer::ClipVertLM cv[3] = {
                        {v0, tu0, tv0, d0, lr0, lg0, lb0},
                        {v1, tu1, tv1, d1, lr1, lg1, lb1},
                        {v2, tu2, tv2, d2, lr2, lg2, lb2}};
                    drew = MeshRenderer::clipAndDrawNear(cv, ctx.nearClip, ctx.viewport, ctx.viewProjMatrix, ctx.framebuffer,
                                                         [&](const Vector3 *proj, const MeshRenderer::ClipVertLM *cvp, int a, int b, int c) -> bool
                                                         {
                                                             return Rasterizer::fillTriangleTextured(
                                                                 proj[a].x, proj[a].y - ctx.bandTopF, proj[a].z,
                                                                 proj[b].x, proj[b].y - ctx.bandTopF, proj[b].z,
                                                                 proj[c].x, proj[c].y - ctx.bandTopF, proj[c].z,
                                                                 cvp[a].u, cvp[a].v,
                                                                 cvp[b].u, cvp[b].v,
                                                                 cvp[c].u, cvp[c].v,
                                                                 cvp[a].d, cvp[b].d, cvp[c].d,
                                                                 cvp[a].lr, cvp[a].lg, cvp[a].lb,
                                                                 cvp[b].lr, cvp[b].lg, cvp[b].lb,
                                                                 cvp[c].lr, cvp[c].lg, cvp[c].lb,
                                                                 *ctx.meshTexture, ctx.frameBuffer, ctx.zBuffer, ctx.fbConfig);
                                                         });
                }
                if (unlikely(!drew))
                    ++ctx.statsCulled;
                return;
            }

            if (ctx.lmActive)
            {
                float mu0, mv0, mu1, mv1, mu2, mv2;
                ctx.instance->lightmapCornerUV(faceIdx, 0, mu0, mv0);
                ctx.instance->lightmapCornerUV(faceIdx, 1, mu1, mv1);
                ctx.instance->lightmapCornerUV(faceIdx, 2, mu2, mv2);
                const uint16_t solid565 = Color::fromFloat(faceR, faceG, faceB).rgb565;
                bool drew = false;
                if (!partiallyClipped)
                {
                    drew = Rasterizer::fillTriangleSolidLM(
                        p0.x, p0.y - ctx.bandTopF, p0.z,
                        p1.x, p1.y - ctx.bandTopF, p1.z,
                        p2.x, p2.y - ctx.bandTopF, p2.z,
                        mu0, mv0, mu1, mv1, mu2, mv2,
                        d0, d1, d2,
                        solid565, *ctx.lmAtlas,
                        ctx.frameBuffer, ctx.zBuffer, ctx.fbConfig);
                }
                else
                {
                    const MeshRenderer::ClipVertLM cv[3] = {
                        {v0, 0, 0, d0, 0, 0, 0, mu0, mv0},
                        {v1, 0, 0, d1, 0, 0, 0, mu1, mv1},
                        {v2, 0, 0, d2, 0, 0, 0, mu2, mv2}};
                    drew = MeshRenderer::clipAndDrawNear(cv, ctx.nearClip, ctx.viewport, ctx.viewProjMatrix, ctx.framebuffer,
                                                         [&](const Vector3 *proj, const MeshRenderer::ClipVertLM *cvp, int a, int b, int c) -> bool
                                                         {
                                                             return Rasterizer::fillTriangleSolidLM(
                                                                 proj[a].x, proj[a].y - ctx.bandTopF, proj[a].z,
                                                                 proj[b].x, proj[b].y - ctx.bandTopF, proj[b].z,
                                                                 proj[c].x, proj[c].y - ctx.bandTopF, proj[c].z,
                                                                 cvp[a].mu, cvp[a].mv,
                                                                 cvp[b].mu, cvp[b].mv,
                                                                 cvp[c].mu, cvp[c].mv,
                                                                 cvp[a].d, cvp[b].d, cvp[c].d,
                                                                 solid565, *ctx.lmAtlas, ctx.frameBuffer, ctx.zBuffer, ctx.fbConfig);
                                                         });
                }
                if (unlikely(!drew))
                    ++ctx.statsCulled;
                return;
            }

            switch (ctx.effectiveMode)
            {
            case SHADING_FLAT:
            {
                float prFaceR = faceR, prFaceG = faceG, prFaceB = faceB;
                const Light *prLights = ctx.localLights;
                int prCount = ctx.localLightCount;
                Light prMod[4];
                if (ctx.useProbes)
                {
                    Vector3 cen = (v0 + v1 + v2) * (1.0f / 3.0f);
                    ProbeLightSample ps;
                    sampleProbe(ctx.instance, cen, ps);
                    probeScaleLights(prMod, ctx.localLights, ctx.localLightCount, ps);
                    prLights = prMod;
                    probeShadeFace(prFaceR, prFaceG, prFaceB, faceR, faceG, faceB, ps);
                }
                if (unlikely(!MeshRenderer::drawTriangle3D_Preprojected(
                        v0, v1, v2, p0, p1, p2,
                        d0, d1, d2,
                        partiallyClipped, ctx.nearClip, camPos,
                        Color::fromFloat(prFaceR, prFaceG, prFaceB).rgb565,
                        ctx.viewProjMatrix, ctx.viewport,
                        ctx.viewportHalfWidth, ctx.viewportHalfHeight, ctx.viewportWidth,
                        ctx.bandTop, ctx.bandBottom, ctx.bandTopF,
                        ctx.framebuffer, ctx.zBuffer,
                        prLights, prCount,
                        ctx.useUniformColor, ctx.uniformColor)))
                    ++ctx.statsCulled;
                break;
            }

            case SHADING_GOURAUD:
            {
                Vector3 n0, n1, n2;
                if (likely(ctx.worldNormals))
                {
                    n0 = ctx.worldNormals[ctx.winNormBase + frec[3]];
                    n1 = ctx.worldNormals[ctx.winNormBase + frec[4]];
                    n2 = ctx.worldNormals[ctx.winNormBase + frec[5]];
                }
                else if (ctx.mesh->hasNormals())
                {
                    n0 = ctx.mesh->attrNormal(ctx.attrBase + frec[3] * ctx.attrRecSize);
                    n1 = ctx.mesh->attrNormal(ctx.attrBase + frec[4] * ctx.attrRecSize);
                    n2 = ctx.mesh->attrNormal(ctx.attrBase + frec[5] * ctx.attrRecSize);
                }
                else
                {
                    Vector3 fn = (v1 - v0).cross(v2 - v0);
                    const float len = sqrtf(fn.x * fn.x + fn.y * fn.y + fn.z * fn.z);
                    if (len > 1e-8f)
                        fn = fn * (1.0f / len);
                    n0 = n1 = n2 = fn;
                }

                float lr0 = 0.0f, lg0 = 0.0f, lb0 = 0.0f;
                float lr1 = 0.0f, lg1 = 0.0f, lb1 = 0.0f;
                float lr2 = 0.0f, lg2 = 0.0f, lb2 = 0.0f;
                if (ctx.useProbes)
                {
                    ProbeLightSample p0, p1, p2;
                    if (probes)
                    {
                        probes->sample(ctx.winPosBase + pi0, p0);
                        probes->sample(ctx.winPosBase + pi1, p1);
                        probes->sample(ctx.winPosBase + pi2, p2);
                    }
                    else
                    {
                        sampleProbe(ctx.instance, v0, p0);
                        sampleProbe(ctx.instance, v1, p1);
                        sampleProbe(ctx.instance, v2, p2);
                    }
                    probeShadeVertex(v0, n0, camPos, ctx.localLights, ctx.localLightCount,
                                     faceR, faceG, faceB, p0, lr0, lg0, lb0);
                    probeShadeVertex(v1, n1, camPos, ctx.localLights, ctx.localLightCount,
                                     faceR, faceG, faceB, p1, lr1, lg1, lb1);
                    probeShadeVertex(v2, n2, camPos, ctx.localLights, ctx.localLightCount,
                                     faceR, faceG, faceB, p2, lr2, lg2, lb2);
                }
                else
                {
                    Shading::calculateVertexLightingGouraud(v0, n0, camPos,
                                                            ctx.localLights, ctx.localLightCount, faceR, faceG, faceB, lr0, lg0, lb0);
                    Shading::calculateVertexLightingGouraud(v1, n1, camPos,
                                                            ctx.localLights, ctx.localLightCount, faceR, faceG, faceB, lr1, lg1, lb1);
                    Shading::calculateVertexLightingGouraud(v2, n2, camPos,
                                                            ctx.localLights, ctx.localLightCount, faceR, faceG, faceB, lr2, lg2, lb2);
                }

                bool drew = false;
                if (likely(!partiallyClipped))
                {
                    drew = MeshRenderer::drawTriangle3D_Smooth_Preprojected(
                        p0, p1, p2,
                        lr0, lg0, lb0, lr1, lg1, lb1, lr2, lg2, lb2,
                        ctx.viewportWidth, ctx.bandTop, ctx.bandBottom, ctx.bandTopF,
                        ctx.framebuffer, ctx.zBuffer);
                }
                else
                {
                    const MeshRenderer::ClipVertSmooth cv[3] = {
                        {v0, d0, lr0, lg0, lb0},
                        {v1, d1, lr1, lg1, lb1},
                        {v2, d2, lr2, lg2, lb2}};
                    drew = MeshRenderer::clipAndDrawNearSmooth(
                        cv, ctx.nearClip, ctx.viewport, ctx.viewProjMatrix,
                        ctx.framebuffer, ctx.zBuffer);
                }
                if (unlikely(!drew))
                    ++ctx.statsCulled;
                break;
            }

            case SHADING_PHONG:
            {
                Vector3 n0, n1, n2;
                if (likely(ctx.worldNormals))
                {
                    n0 = ctx.worldNormals[ctx.winNormBase + frec[3]];
                    n1 = ctx.worldNormals[ctx.winNormBase + frec[4]];
                    n2 = ctx.worldNormals[ctx.winNormBase + frec[5]];
                }
                else if (ctx.mesh->hasNormals())
                {
                    n0 = ctx.mesh->attrNormal(ctx.attrBase + frec[3] * ctx.attrRecSize);
                    n1 = ctx.mesh->attrNormal(ctx.attrBase + frec[4] * ctx.attrRecSize);
                    n2 = ctx.mesh->attrNormal(ctx.attrBase + frec[5] * ctx.attrRecSize);
                }
                else
                {
                    Vector3 fn = (v1 - v0).cross(v2 - v0);
                    const float len = sqrtf(fn.x * fn.x + fn.y * fn.y + fn.z * fn.z);
                    if (len > 1e-8f)
                        fn = fn * (1.0f / len);
                    n0 = n1 = n2 = fn;
                }
                float prFaceR = faceR, prFaceG = faceG, prFaceB = faceB;
                const Light *prLights = ctx.localLights;
                int prCount = ctx.localLightCount;
                Light prMod[4];
                if (ctx.useProbes)
                {
                    Vector3 cen = (v0 + v1 + v2) * (1.0f / 3.0f);
                    ProbeLightSample ps;
                    sampleProbe(ctx.instance, cen, ps);
                    probeScaleLights(prMod, ctx.localLights, ctx.localLightCount, ps);
                    prLights = prMod;
                    probeShadeFace(prFaceR, prFaceG, prFaceB, faceR, faceG, faceB, ps);
                }

                bool drew = false;
                if (likely(!partiallyClipped))
                {
                    drew = MeshRenderer::drawTriangle3D_Phong_Preprojected(
                        v0, v1, v2, p0, p1, p2,
                        n0, n1, n2, d0, d1, d2,
                        prFaceR, prFaceG, prFaceB, camPos,
                        ctx.viewportWidth, ctx.bandTop, ctx.bandBottom, ctx.bandTopF,
                        ctx.framebuffer, ctx.zBuffer,
                        prLights, prCount);
                }
                else
                {
                    const MeshRenderer::ClipVertPhong cv[3] = {
                        {v0, n0, d0},
                        {v1, n1, d1},
                        {v2, n2, d2}};
                    drew = MeshRenderer::clipAndDrawNearPhong(
                        cv, ctx.nearClip, prFaceR, prFaceG, prFaceB, camPos,
                        ctx.viewport, ctx.viewProjMatrix,
                        ctx.framebuffer, ctx.zBuffer,
                        prLights, prCount);
                }
                if (unlikely(!drew))
                    ++ctx.statsCulled;
                break;
            }
            }
        }
    }

    IRAM_ATTR void Renderer::drawMeshInstanceBanded(MeshInstance *instance,
                                                    float zEye, float radiusPixels)
    {
        if (!instance || !instance->isVisible())
            return;

        Mesh *mesh = instance->getMesh();
        if (!mesh)
            return;

        const Camera &cam = cameras[activeCameraIndex];
        if (zEye > cam.nearPlane && radiusPixels < 1.0f)
            return;

        const ShadingMode effectiveMode = instance->getEffectiveShadingMode(shadingMode);

        const Vector3 center = instance->center();
        const float radius = instance->radius();
        const DisplayConfig &framebufferConfig = framebuffer.getConfig();

        const uint16_t instColor565 = instance->color().rgb565;
        float instR, instG, instB;
        MeshRenderer::decodeColorToFloat(instColor565, instR, instG, instB);

        const uint32_t subMeshCount = mesh->numSubMeshes();
        const bool hasSubMeshes = mesh->hasSubMeshes();

        const bool isEmissiveInst = instance->isEmissive();

        const bool lmActiveEarly = instance->hasActiveLightmap(bakedLightMode_);

        bool useUniformColor = !lmActiveEarly && (mesh->getSingleColorLighting() || isEmissiveInst) && effectiveMode == SHADING_FLAT;
        uint16_t uniformColor = 0;

        Light localLights[4];
        const int localLightCount = lmActiveEarly
                                        ? 0
                                        : collectActiveLightsForBounds(
                                              center, radius, lights.data(), activeLightCount, localLights, 4);

        const Matrix4x4 &worldTransform = instance->transform();

        if (useUniformColor)
        {
            if (isEmissiveInst)
            {
                uniformColor = instance->color().rgb565;
            }
            else
            {
                NormalMatrix nmUniform(worldTransform);
                Vector3 localNormal(0.0f, 1.0f, 0.0f);
                if (mesh->hasNormals() && mesh->numAttrs() > 0)
                    localNormal = mesh->attrNormal(mesh->chunkAttrs(mesh->getChunk(0)));
                const Vector3 worldNormal = nmUniform.transform(localNormal);

                float litR, litG, litB;
                Shading::calculateLambert(worldNormal, localLights, localLightCount,
                                          instR, instG, instB,
                                          litR, litG, litB);

                const float vx = cam.position.x - center.x;
                const float vy = cam.position.y - center.y;
                const float vz = cam.position.z - center.z;
                const float viewDistSq = vx * vx + vy * vy + vz * vz;
                const float invLen = (viewDistSq > 1e-8f) ? FastMath::fastInvSqrt(viewDistSq) : 0.0f;
                const float dist = viewDistSq * invLen;
                Shading::applyFog(dist, litR, litG, litB, litR, litG, litB);

                uniformColor = Color::fromFloat(litR, litG, litB).rgb565;
            }
        }

        const bool needsWorldNormals = (effectiveMode != SHADING_FLAT);

        alignas(alignof(NormalMatrix)) unsigned char nmStorage[sizeof(NormalMatrix)];
        NormalMatrix *const nmWorld =
            needsWorldNormals ? ::new (static_cast<void *>(nmStorage)) NormalMatrix(worldTransform) : nullptr;

        DrawCache *const cache = &instance->drawCache();

        Vector3 *PIP3D_RESTRICT worldVerts = nullptr;
        Vector3 *PIP3D_RESTRICT worldNormals = nullptr;
        Vector3 *PIP3D_RESTRICT screenVerts = nullptr;

        const uint32_t frameStamp = g_frameStamp;
        const uint32_t instanceVersion = instance->version();

        const int16_t bandTop = g_bandOffsetY;
        const int16_t bandBottom = static_cast<int16_t>(bandTop + g_bandHeight);
        const float bandTopF = static_cast<float>(bandTop);
        const float viewportWidth = static_cast<float>(viewport.width);
        const float viewportHalfWidth = viewportWidth * 0.5f;
        const float viewportHalfHeight = static_cast<float>(viewport.height) * 0.5f;
        const Vector3 camPos = cam.position;
        const Vector3 camFwd = cam.forward();
        constexpr float kNearClipEps = 1e-4f;
        const float nearClip = cam.nearPlane + kNearClipEps;

        const float nearZWThreshold = g_wBufferScale * FastMath::fastReciprocal(nearClip);
        const bool isTextured = mesh->isTextured();
        const bool doBackfaceCull = backfaceCullingEnabled;

        const Texture *meshTexture = isTextured ? mesh->getTexture() : nullptr;
        if (!meshTexture && mesh->wantsTexture())
        {
            meshTexture = &g_missingTexture;
        }
        const bool effectiveTextured = (meshTexture != nullptr);

        const uint32_t chunkCount = mesh->numChunks();
        const MeshChunk *PIP3D_RESTRICT chunks = mesh->chunkData();
        const bool hasChunks = (chunkCount > 0 && chunks != nullptr);

        const bool lmActive = lmActiveEarly;
        const bool useProbes = usesProbes(instance);

        uint32_t currentSubMesh = 0;

        FaceDrawCtx ctx{
            instance, mesh, worldTransform, nullptr, nullptr, nullptr,
            nullptr, nullptr, 0u,
            nullptr, nullptr, 0u, ChunkPosDecode{},
            0u, 0u,
            subMeshCount, hasSubMeshes,
            mesh->faceStride(), nearZWThreshold,
            effectiveMode, effectiveTextured, meshTexture,
            lmActive ? instance->lightmapAtlas() : nullptr,
            lmActive, useProbes, useUniformColor, uniformColor,
            instR, instG, instB, localLights, localLightCount,
            viewProjMatrix, viewport, framebufferConfig,
            framebuffer, framebuffer.getBuffer(), &zBuffer,
            camPos, camFwd, nearClip, doBackfaceCull,
            bandTop, bandBottom, bandTopF,
            viewportWidth, viewportHalfWidth, viewportHalfHeight,
            currentSubMesh, statsTrianglesTotal, statsTrianglesBackfaceCulled};

        const uint32_t maxChunkPos = mesh->maxChunkPosCount();
        const uint32_t maxChunkAttr = mesh->maxChunkAttrCount();
        const uint32_t totalPos = mesh->numVertices();
        const uint32_t totalAttr = mesh->numAttrs();
        const bool mergedWindow = (totalPos > 0 && totalPos <= 256 && totalAttr <= 256);
        const uint32_t windowPos = mergedWindow ? totalPos : maxChunkPos;
        const uint32_t windowAttr = mergedWindow ? totalAttr : maxChunkAttr;
        if (likely(cache->ensureCapacity(static_cast<uint16_t>(windowPos),
                                         needsWorldNormals ? static_cast<uint16_t>(windowAttr) : 0u,
                                         needsWorldNormals)))
        {
            worldVerts = cache->worldVerts();
            worldNormals = cache->worldNormals();
            screenVerts = cache->screenVerts();
        }

        const int32_t bandIndex32 = static_cast<int32_t>(g_bandOffsetY) / static_cast<int32_t>(SCREEN_BAND_HEIGHT);
        const uint8_t bandIndex = static_cast<uint8_t>(
            (bandIndex32 < 0) ? 0
                              : ((bandIndex32 >= static_cast<int32_t>(SCREEN_BAND_COUNT))
                                     ? static_cast<int32_t>(SCREEN_BAND_COUNT - 1)
                                     : bandIndex32));

        ChunkBandCache &chunkCache = instance->chunkBandCache();
        if (chunkCache.frameStamp != frameStamp || chunkCache.instanceVersion != instanceVersion)
        {
            if (buildChunkBandCache(*this, instance, chunkCache, frameStamp))
                chunkCache.instanceVersion = instanceVersion;
            else
            {

                chunkCache.visibleCount = 0;
                chunkCache.instanceVersion = instanceVersion;
            }
        }

        bool useProbeCache = useProbes && (effectiveMode == SHADING_GOURAUD) && worldVerts && windowPos > 0 && windowPos <= kProbeCacheMaxVerts;
        ProbeCache probeCache;
        if (useProbeCache)
        {
            if (uint8_t *PIP3D_RESTRICT pd = cache->ensureProbePlanes(static_cast<uint16_t>(windowPos)))
            {
                probeCache.data = pd;
                if (const BakedProbeGrid *grid = Rasterizer::g_bakedState.probes)
                {
                    probeCache.tintR = grid->tintR;
                    probeCache.tintG = grid->tintG;
                    probeCache.tintB = grid->tintB;
                }
            }
            else
                useProbeCache = false;
        }

        if (useProbeCache && cache->probeStateVersion() != Rasterizer::g_bakedStateVersion)
        {
            cache->commitProbeStateVersion(Rasterizer::g_bakedStateVersion);
            cache->invalidateWorldVerts();
        }

        const float qScale = mesh->getQScale();

        const BakedProbeGrid *probeGrid = Rasterizer::g_bakedState.probes;
        const bool probeGridOk = probeGrid && probeGrid->valid() && !instance->getIgnoreBakedProbes();
        const auto sampleProbes = [&](uint8_t *PIP3D_RESTRICT pd, uint32_t base, uint32_t count)
        {
            for (uint32_t vi = 0; vi < count; ++vi)
            {
                float ps, pa, luma;
                uint8_t ti = 0;
                if (probeGridOk)
                    probeGrid->sample(worldVerts[base + vi], ps, pa, luma, ti);
                else
                {
                    ps = 1.0f;
                    pa = 1.0f;
                    luma = 0.0f;
                }
                const uint32_t o = (base + vi) * 4;
                pd[o + 0] = static_cast<uint8_t>(ps * 255.0f + 0.5f);
                pd[o + 1] = static_cast<uint8_t>(pa * 255.0f + 0.5f);
                pd[o + 2] = static_cast<uint8_t>(luma * 255.0f + 0.5f);
                pd[o + 3] = ti;
            }
        };

        const auto indexPosBase = [&](uint32_t ci) -> uint32_t
        {
            uint32_t s = 0;
            for (uint32_t j = 0; j < ci; ++j)
                s += chunks[j].posCount;
            return s;
        };
        const auto indexNormBase = [&](uint32_t ci) -> uint32_t
        {
            uint32_t s = 0;
            for (uint32_t j = 0; j < ci; ++j)
                s += chunks[j].attrCount;
            return s;
        };

        for (uint16_t k = 0; k < chunkCache.visibleCount && hasChunks; ++k)
        {
            {
                const ChunkBandRecord rec = chunkCache.records[k];
                if (bandIndex < rec.minBand || bandIndex > rec.maxBand)
                    continue;

                const MeshChunk &chunk = chunks[rec.chunkIdx];
                const uint16_t chunkPosCount = chunk.posCount;
                const uint32_t chunkFOffset = chunk.faceOffset;
                const uint16_t chunkFCount = chunk.faceCount;

                const ChunkPosDecode posDecode = Mesh::chunkPosDecode(chunk, qScale);
                const uint8_t *PIP3D_RESTRICT posBase = mesh->chunkPositions(chunk);
                const uint32_t posRecSize = Mesh::posRecordSize(chunk);
                const uint8_t *PIP3D_RESTRICT attrBase = mesh->chunkAttrs(chunk);
                const uint32_t attrRecSize = mesh->attrRecordSize();

                if (likely(worldVerts && screenVerts))
                {
                    if (mergedWindow)
                    {
                        const DrawCache::ProjState st = cache->beginProjection(frameStamp, instanceVersion);
                        if (st == DrawCache::ProjState::NeedsTransformAndProject)
                        {

                            uint32_t fillPos = 0;
                            uint32_t fillNorm = 0;
                            for (uint32_t ci = 0; ci < chunkCount; ++ci)
                            {
                                const MeshChunk &ck = chunks[ci];
                                const ChunkPosDecode dec = Mesh::chunkPosDecode(ck, qScale);
                                const uint8_t *recs = mesh->chunkPositions(ck);
                                const uint32_t recSize = Mesh::posRecordSize(ck);
                                for (uint16_t i = 0; i < ck.posCount; ++i)
                                {
                                    const Vector3 world = worldTransform.transformNoDiv(
                                        Mesh::chunkPosition(ck, dec, recs + static_cast<uint32_t>(i) * recSize));
                                    worldVerts[fillPos + i] = world;
                                    screenVerts[fillPos + i] = CameraController::project(world, viewProjMatrix,
                                                                                         viewportHalfWidth, viewportHalfHeight, 0, 0);
                                }
                                if (needsWorldNormals && worldNormals)
                                {
                                    const uint8_t *arecs = mesh->chunkAttrs(ck);
                                    const uint32_t arecSize = mesh->attrRecordSize();
                                    for (uint16_t a = 0; a < ck.attrCount; ++a)
                                        worldNormals[fillNorm + a] = nmWorld->transform(
                                            mesh->attrNormal(arecs + static_cast<uint32_t>(a) * arecSize));
                                }
                                if (useProbeCache)
                                    sampleProbes(probeCache.data, fillPos, ck.posCount);
                                fillPos += ck.posCount;
                                fillNorm += ck.attrCount;
                            }
                            cache->commitProjection(frameStamp, instanceVersion);
                        }
                        else if (st == DrawCache::ProjState::NeedsReproject)
                        {

                            for (uint16_t kk = 0; kk < chunkCache.visibleCount; ++kk)
                            {
                                const uint32_t visChunkIdx = chunkCache.records[kk].chunkIdx;
                                const MeshChunk &ck = chunks[visChunkIdx];
                                const uint32_t base = indexPosBase(visChunkIdx);
                                for (uint16_t i = 0; i < ck.posCount; ++i)
                                {
                                    screenVerts[base + i] = CameraController::project(worldVerts[base + i], viewProjMatrix,
                                                                                      viewportHalfWidth, viewportHalfHeight, 0, 0);
                                }
                            }
                            cache->commitProjection(frameStamp, instanceVersion);
                        }
                    }
                    else if (chunkCache.currentChunkIdx != rec.chunkIdx)
                    {

                        for (uint16_t i = 0; i < chunkPosCount; ++i)
                        {
                            const Vector3 local = Mesh::chunkPosition(chunk, posDecode,
                                                                      posBase + static_cast<uint32_t>(i) * posRecSize);
                            const Vector3 world = worldTransform.transformNoDiv(local);
                            worldVerts[i] = world;
                            screenVerts[i] = CameraController::project(world, viewProjMatrix,
                                                                       viewportHalfWidth, viewportHalfHeight, 0, 0);
                        }
                        if (needsWorldNormals && worldNormals)
                        {
                            for (uint16_t a = 0; a < chunk.attrCount; ++a)
                                worldNormals[a] = nmWorld->transform(
                                    mesh->attrNormal(attrBase + static_cast<uint32_t>(a) * attrRecSize));
                        }
                        chunkCache.currentChunkIdx = rec.chunkIdx;
                        if (useProbeCache)
                            sampleProbes(probeCache.data, 0, chunkPosCount);
                    }
                }

                ctx.worldVerts = worldVerts;
                ctx.screenVerts = screenVerts;
                ctx.worldNormals = worldNormals;
                ctx.fbase = mesh->faceData();
                ctx.attrBase = attrBase;
                ctx.attrRecSize = attrRecSize;
                ctx.chunk = &chunk;
                ctx.posBase = posBase;
                ctx.posRecSize = posRecSize;
                ctx.posDecode = posDecode;
                ctx.faceStride = mesh->faceStride();
                ctx.winPosBase = mergedWindow ? indexPosBase(rec.chunkIdx) : 0u;
                ctx.winNormBase = mergedWindow ? indexNormBase(rec.chunkIdx) : 0u;
                currentSubMesh = 0;

                const ProbeCache *probes = (useProbeCache && worldVerts) ? &probeCache : nullptr;

                for (uint16_t fi = 0; fi < chunkFCount; ++fi)
                    drawMeshFace(ctx, probes, chunkFOffset + fi);
            }
        }
    }

    void Renderer::drawTriangle3D(const Vector3 &v0, const Vector3 &v1, const Vector3 &v2, uint16_t color)
    {
        MeshRenderer::drawTriangle3D(v0, v1, v2, color, cameras[activeCameraIndex], viewport,
                                     viewProjMatrix, framebuffer, &zBuffer, lights.data(), activeLightCount,
                                     false, 0);
    }
}