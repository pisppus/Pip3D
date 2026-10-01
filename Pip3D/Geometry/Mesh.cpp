#include <cmath>
#include <cstring>

#include "Debug/Logging.hpp"
#include "Geometry/Mesh.hpp"

namespace pip3D
{
    PIP3D_COLD void Mesh::calculateBoundingSphere() const
    {
        if (unlikely(!chunks_ || chunkCount_ == 0 || vertexCount_ == 0))
        {
            boundsCenter_ = Vector3(0.0f, 0.0f, 0.0f);
            boundsRadius_ = 0.0f;
            boundsHalfExtents_ = Vector3(0.0f, 0.0f, 0.0f);
            flags_ |= kFlagBoundsValid;
            return;
        }

        float minX = 1e30f, maxX = -1e30f;
        float minY = 1e30f, maxY = -1e30f;
        float minZ = 1e30f, maxZ = -1e30f;
        forEachPosition([&](const Vector3 &p)
                        {
            if (p.x < minX) minX = p.x;
            if (p.x > maxX) maxX = p.x;
            if (p.y < minY) minY = p.y;
            if (p.y > maxY) maxY = p.y;
            if (p.z < minZ) minZ = p.z;
            if (p.z > maxZ) maxZ = p.z; });

        const Vector3 c((minX + maxX) * 0.5f, (minY + maxY) * 0.5f, (minZ + maxZ) * 0.5f);
        float maxDistSq = 0.0f;
        forEachPosition([&](const Vector3 &p)
                        {
            const float dx = p.x - c.x;
            const float dy = p.y - c.y;
            const float dz = p.z - c.z;
            const float dSq = dx * dx + dy * dy + dz * dz;
            if (dSq > maxDistSq)
                maxDistSq = dSq; });

        boundsCenter_ = c;
        boundsRadius_ = sqrtf(maxDistSq);
        boundsHalfExtents_ = Vector3((maxX - minX) * 0.5f, (maxY - minY) * 0.5f, (maxZ - minZ) * 0.5f);
        flags_ |= kFlagBoundsValid;
    }

    PIP3D_COLD void Mesh::recomputeHalfExtents() const
    {
        if (unlikely(!chunks_ || chunkCount_ == 0 || vertexCount_ == 0))
            return;
        float minX = 1e30f, maxX = -1e30f;
        float minY = 1e30f, maxY = -1e30f;
        float minZ = 1e30f, maxZ = -1e30f;
        forEachPosition([&](const Vector3 &p)
                        {
            if (p.x < minX) minX = p.x;
            if (p.x > maxX) maxX = p.x;
            if (p.y < minY) minY = p.y;
            if (p.y > maxY) maxY = p.y;
            if (p.z < minZ) minZ = p.z;
            if (p.z > maxZ) maxZ = p.z; });
        boundsHalfExtents_ = Vector3((maxX - minX) * 0.5f, (maxY - minY) * 0.5f, (maxZ - minZ) * 0.5f);
    }

    PIP3D_COLD void Mesh::buildFromInterleaved(const Vertex *verts, uint32_t vertCount,
                                               const Face16 *faces, uint32_t faceCount,
                                               bool staticStorage, bool hasUV, bool hasNormals,
                                               uint32_t maxTrisPerChunk)
    {
        if (maxTrisPerChunk == 0 || maxTrisPerChunk > 85)
            maxTrisPerChunk = 85;
        if (unlikely(faceCount == 0 || vertCount == 0))
            return;

        const bool withUV = hasUV;
        const bool withNormals = hasNormals;
        const uint32_t attrRecSize = (withUV ? 4u : 0u) + (withNormals ? 2u : 0u);
        const uint32_t faceStride = (withUV || withNormals) ? 6u : 3u;

        if (withUV)
        {
            float minU = 1e30f, maxU = -1e30f, minV = 1e30f, maxV = -1e30f;
            for (uint32_t fi = 0; fi < faceCount; ++fi)
            {
                const uint16_t idx[3] = {faces[fi].v0, faces[fi].v1, faces[fi].v2};
                for (int k = 0; k < 3; ++k)
                {
                    const Vertex &v = verts[idx[k]];
                    if (v.tu < minU)
                        minU = v.tu;
                    if (v.tu > maxU)
                        maxU = v.tu;
                    if (v.tv < minV)
                        minV = v.tv;
                    if (v.tv > maxV)
                        maxV = v.tv;
                }
            }
            const float spanU = (maxU - minU) > 1e-9f ? (maxU - minU) : 1.0f;
            const float spanV = (maxV - minV) > 1e-9f ? (maxV - minV) : 1.0f;
            finalizeUVRange(minU, spanU, minV, spanV);
        }

        struct ChunkScratch
        {
            uint16_t posTable[256];
            uint16_t attrTable[256];
            int16_t pos[256 * 3];
            uint16_t attr[256 * 3];
            uint16_t faceP[85 * 3];
            uint16_t faceA[85 * 3];
            uint16_t posCount;
            uint16_t attrCount;
            uint16_t faceCount;
        };
        auto *sc = static_cast<ChunkScratch *>(MemUtils::allocData(sizeof(ChunkScratch), 4));
        if (unlikely(!sc))
        {
            LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES, "Mesh build: scratch alloc failed (%u tris)", (unsigned)faceCount);
            return;
        }

        uint32_t chunkCount = 0;
        uint32_t streamBytes = 0;
        uint32_t totalPos = 0;
        uint32_t totalAttr = 0;

        MeshChunk *outChunks = nullptr;
        uint8_t *outFaces = nullptr;
        uint8_t *outStream = nullptr;
        uint32_t chunkDataOffset = 0;
        uint32_t chunkFaceOffset = 0;

        const auto flushChunk = [&]()
        {
            if (sc->faceCount == 0)
                return;

            int32_t minX = 32767, minY = 32767, minZ = 32767;
            int32_t maxX = -32768, maxY = -32768, maxZ = -32768;
            for (uint32_t i = 0; i < sc->posCount; ++i)
            {
                const int16_t *p = sc->pos + static_cast<size_t>(i) * 3;
                if (p[0] < minX)
                    minX = p[0];
                if (p[0] > maxX)
                    maxX = p[0];
                if (p[1] < minY)
                    minY = p[1];
                if (p[1] > maxY)
                    maxY = p[1];
                if (p[2] < minZ)
                    minZ = p[2];
                if (p[2] > maxZ)
                    maxZ = p[2];
            }

            const bool delta8 = (maxX - minX) <= 255 && (maxY - minY) <= 255 && (maxZ - minZ) <= 255;
            const uint32_t posRec = delta8 ? 3u : 6u;
            const uint32_t posBytes = static_cast<uint32_t>(sc->posCount) * posRec;
            const uint32_t chunkStream = posBytes + (posBytes & 1u) +
                                         static_cast<uint32_t>(sc->attrCount) * attrRecSize;

            if (outChunks)
            {
                MeshChunk &ch = outChunks[chunkCount];
                ch.minX = static_cast<int16_t>(minX);
                ch.minY = static_cast<int16_t>(minY);
                ch.minZ = static_cast<int16_t>(minZ);
                ch.maxX = static_cast<int16_t>(maxX);
                ch.maxY = static_cast<int16_t>(maxY);
                ch.maxZ = static_cast<int16_t>(maxZ);
                ch.dataOffset = chunkDataOffset;
                ch.faceOffset = chunkFaceOffset;
                ch.posCount = static_cast<uint8_t>(sc->posCount);
                ch.attrCount = static_cast<uint8_t>(sc->attrCount);
                ch.faceCount = static_cast<uint8_t>(sc->faceCount);
                ch.flags = delta8 ? kChunkPosDelta8 : 0;
                ch.coneNormal = 0;
                ch.coneSin = 0;
                ch.posShift = 0;

                uint8_t *out = outStream + chunkDataOffset;
                for (uint32_t i = 0; i < sc->posCount; ++i)
                {
                    const int16_t *p = sc->pos + static_cast<size_t>(i) * 3;
                    if (delta8)
                    {
                        out[0] = static_cast<uint8_t>(p[0] - minX);
                        out[1] = static_cast<uint8_t>(p[1] - minY);
                        out[2] = static_cast<uint8_t>(p[2] - minZ);
                        out += 3;
                    }
                    else
                    {
                        const uint16_t rx = static_cast<uint16_t>(p[0] - minX);
                        const uint16_t ry = static_cast<uint16_t>(p[1] - minY);
                        const uint16_t rz = static_cast<uint16_t>(p[2] - minZ);
                        memcpy(out, &rx, 2);
                        memcpy(out + 2, &ry, 2);
                        memcpy(out + 4, &rz, 2);
                        out += 6;
                    }
                }
                if (posBytes & 1u)
                    *out++ = 0;

                for (uint32_t i = 0; i < sc->attrCount; ++i)
                {
                    const uint16_t *a = sc->attr + static_cast<size_t>(i) * 3;
                    if (withUV)
                    {
                        memcpy(out, a, 2);
                        memcpy(out + 2, a + 1, 2);
                        out += 4;
                    }
                    if (withNormals)
                    {
                        memcpy(out, a + 2, 2);
                        out += 2;
                    }
                }
            }

            chunkDataOffset += chunkStream + ((4 - (chunkStream & 3)) & 3);
            chunkFaceOffset += sc->faceCount;
            totalPos += sc->posCount;
            totalAttr += sc->attrCount;
            streamBytes += chunkStream + ((4 - (chunkStream & 3)) & 3);
            ++chunkCount;
            sc->posCount = 0;
            sc->attrCount = 0;
            sc->faceCount = 0;
            memset(sc->posTable, 0, sizeof(sc->posTable));
            memset(sc->attrTable, 0, sizeof(sc->attrTable));
        };

        const auto posHash = [](const Vertex &v) -> uint8_t
        {
            uint32_t h = static_cast<uint32_t>(static_cast<uint16_t>(v.px)) * 0x9E3779B1u;
            h ^= static_cast<uint32_t>(static_cast<uint16_t>(v.py)) * 0x85EBCA77u;
            h ^= static_cast<uint32_t>(static_cast<uint16_t>(v.pz)) * 0xC2B2AE3Du;
            h ^= h >> 16;
            h *= 0x7FEB352Du;
            h ^= h >> 15;
            return static_cast<uint8_t>(h);
        };
        const auto attrHash = [](uint16_t qu, uint16_t qv, uint16_t oct) -> uint8_t
        {
            uint32_t h = static_cast<uint32_t>(qu) * 0x9E3779B1u;
            h ^= static_cast<uint32_t>(qv) * 0x85EBCA77u;
            h ^= static_cast<uint32_t>(oct) * 0xC2B2AE3Du;
            h ^= h >> 16;
            h *= 0x7FEB352Du;
            h ^= h >> 15;
            return static_cast<uint8_t>(h);
        };

        const auto findPos = [&](const Vertex &v) -> int
        {
            uint8_t slot = posHash(v);
            while (sc->posTable[slot] != 0)
            {
                const uint16_t s = static_cast<uint16_t>(sc->posTable[slot] - 1);
                const int16_t *p = sc->pos + static_cast<size_t>(s) * 3;
                if (p[0] == v.px && p[1] == v.py && p[2] == v.pz)
                    return s;
                slot = static_cast<uint8_t>(slot + 1);
            }
            return -1;
        };
        const auto insertPos = [&](const Vertex &v) -> uint16_t
        {
            uint8_t slot = posHash(v);
            while (sc->posTable[slot] != 0)
            {
                const uint16_t s = static_cast<uint16_t>(sc->posTable[slot] - 1);
                const int16_t *p = sc->pos + static_cast<size_t>(s) * 3;
                if (p[0] == v.px && p[1] == v.py && p[2] == v.pz)
                    return s;
                slot = static_cast<uint8_t>(slot + 1);
            }
            int16_t *dst = sc->pos + static_cast<size_t>(sc->posCount) * 3;
            dst[0] = v.px;
            dst[1] = v.py;
            dst[2] = v.pz;
            sc->posTable[slot] = static_cast<uint16_t>(sc->posCount + 1);
            return sc->posCount++;
        };
        const auto findAttr = [&](uint16_t qu, uint16_t qv, uint16_t oct) -> int
        {
            uint8_t slot = attrHash(qu, qv, oct);
            while (sc->attrTable[slot] != 0)
            {
                const uint16_t s = static_cast<uint16_t>(sc->attrTable[slot] - 1);
                const uint16_t *a = sc->attr + static_cast<size_t>(s) * 3;
                if (a[0] == qu && a[1] == qv && a[2] == oct)
                    return s;
                slot = static_cast<uint8_t>(slot + 1);
            }
            return -1;
        };
        const auto insertAttr = [&](uint16_t qu, uint16_t qv, uint16_t oct) -> uint16_t
        {
            uint8_t slot = attrHash(qu, qv, oct);
            while (sc->attrTable[slot] != 0)
            {
                const uint16_t s = static_cast<uint16_t>(sc->attrTable[slot] - 1);
                const uint16_t *a = sc->attr + static_cast<size_t>(s) * 3;
                if (a[0] == qu && a[1] == qv && a[2] == oct)
                    return s;
                slot = static_cast<uint8_t>(slot + 1);
            }
            uint16_t *dst = sc->attr + static_cast<size_t>(sc->attrCount) * 3;
            dst[0] = qu;
            dst[1] = qv;
            dst[2] = oct;
            sc->attrTable[slot] = static_cast<uint16_t>(sc->attrCount + 1);
            return sc->attrCount++;
        };

        const auto cornerAttr = [&](const Vertex &v, uint16_t &qu, uint16_t &qv, uint16_t &oct)
        {
            if (withUV)
            {
                float q = (v.tu - uvMinU_) / uvKU_ + 0.5f;
                qu = static_cast<uint16_t>(q < 0.0f ? 0.0f : (q > 65535.0f ? 65535.0f : q));
                q = (v.tv - uvMinV_) / uvKV_ + 0.5f;
                qv = static_cast<uint16_t>(q < 0.0f ? 0.0f : (q > 65535.0f ? 65535.0f : q));
            }
            else
            {
                qu = 0;
                qv = 0;
            }
            oct = withNormals ? v.normal.data : uint16_t{0};
        };

        for (uint32_t pass = 0; pass < 2; ++pass)
        {
            if (pass == 1)
            {
                const size_t chunksBytes = static_cast<size_t>(chunkCount) * sizeof(MeshChunk);
                const size_t facesBytes = static_cast<size_t>(faceCount) * faceStride;
                const size_t streamPad = (4 - ((chunksBytes + facesBytes) & 3)) & 3;
                const size_t totalBytes = chunksBytes + facesBytes + streamPad + streamBytes;

                uint8_t *block = static_cast<uint8_t *>(MemUtils::allocData(totalBytes, 16));
                if (unlikely(!block))
                {
                    LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES, "Mesh build: final alloc failed (%u tris, %u bytes)", (unsigned)faceCount, (unsigned)totalBytes);
                    MemUtils::freeData(sc);
                    return;
                }
                outChunks = reinterpret_cast<MeshChunk *>(block);
                outFaces = block + chunksBytes;
                outStream = block + chunksBytes + facesBytes + streamPad;
                heapBlock_ = block;

                                chunkCount = 0;
                streamBytes = 0;
                totalPos = 0;
                totalAttr = 0;
            }

            chunkDataOffset = 0;
            chunkFaceOffset = 0;
            uint32_t faceOut = 0;
            sc->posCount = 0;
            sc->attrCount = 0;
            sc->faceCount = 0;
            memset(sc->posTable, 0, sizeof(sc->posTable));
            memset(sc->attrTable, 0, sizeof(sc->attrTable));

            for (uint32_t fi = 0; fi < faceCount; ++fi)
            {
                const Face16 &face = faces[fi];
                const uint16_t cornerIdx[3] = {face.v0, face.v1, face.v2};

                if (sc->faceCount >= maxTrisPerChunk)
                    flushChunk();

                uint32_t newPos = 0;
                uint32_t newAttr = 0;
                for (int k = 0; k < 3; ++k)
                {
                    const Vertex &v = verts[cornerIdx[k]];
                    if (findPos(v) < 0)
                        ++newPos;

                    uint16_t qu, qv, oct;
                    cornerAttr(v, qu, qv, oct);
                    if (findAttr(qu, qv, oct) < 0)
                        ++newAttr;
                }

                if (sc->posCount + newPos > 255 || sc->attrCount + newAttr > 255)
                    flushChunk();

                uint16_t posSlots[3];
                uint16_t attrSlots[3];
                for (int k = 0; k < 3; ++k)
                {
                    const Vertex &v = verts[cornerIdx[k]];
                    posSlots[k] = insertPos(v);

                    uint16_t qu, qv, oct;
                    cornerAttr(v, qu, qv, oct);
                    attrSlots[k] = insertAttr(qu, qv, oct);
                }

                if (outFaces)
                {
                    uint8_t *f = outFaces + static_cast<size_t>(faceOut) * faceStride;
                    f[0] = static_cast<uint8_t>(posSlots[0]);
                    f[1] = static_cast<uint8_t>(posSlots[1]);
                    f[2] = static_cast<uint8_t>(posSlots[2]);
                    if (faceStride == 6)
                    {
                        f[3] = static_cast<uint8_t>(attrSlots[0]);
                        f[4] = static_cast<uint8_t>(attrSlots[1]);
                        f[5] = static_cast<uint8_t>(attrSlots[2]);
                    }
                }
                ++faceOut;
                ++sc->faceCount;
            }
            flushChunk();
        }

        chunks_ = outChunks;
        faces_ = reinterpret_cast<const Face *>(outFaces);
        stream_ = outStream;
        subMeshes_ = nullptr;
        chunkCount_ = chunkCount;
        subMeshCount_ = 0;
        vertexCount_ = totalPos;
        attrCount_ = totalAttr;
        faceCount_ = faceCount;

        constexpr uint8_t kKeepMask = kFlagCastShadows | kFlagSingleColorLighting | kFlagWantsTexture;
        flags_ = static_cast<uint8_t>((flags_ & kKeepMask) |
                                      (staticStorage ? static_cast<uint8_t>(kFlagStaticStorage) : 0u) |
                                      (withUV ? static_cast<uint8_t>(kFlagHasUV) : 0u) |
                                      (withNormals ? static_cast<uint8_t>(kFlagHasNormals) : 0u));
        MemUtils::freeData(sc);
    }
}
