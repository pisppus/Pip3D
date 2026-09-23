#pragma once

#include <cstdint>
#include <cmath>
#include <cstring>

#include "Math/Algebra.hpp"
#include "Math/Quant.hpp"
#include "Core/Color.hpp"
#include "Core/Memory.hpp"

namespace pip3D
{
    struct Texture;

    struct alignas(16) Vertex
    {
        int16_t px, py, pz;
        PackedNormal normal;
        float tu, tv;

        constexpr Vertex() : px(0), py(0), pz(0), normal(), tu(0.0f), tv(0.0f) {}
        constexpr Vertex(int16_t x, int16_t y, int16_t z, uint16_t norm,
                         float u = 0.0f, float v = 0.0f)
            : px(x), py(y), pz(z), normal(norm), tu(u), tv(v) {}
    };
    static_assert(sizeof(Vertex) == 16);
    static_assert(alignof(Vertex) == 16);

    struct alignas(2) Face16
    {
        uint16_t v0, v1, v2;
        constexpr Face16() : v0(0), v1(0), v2(0) {}
        constexpr Face16(uint16_t a, uint16_t b, uint16_t c) : v0(a), v1(b), v2(c) {}
    };
    static_assert(sizeof(Face16) == 6);

    enum MeshChunkFlags : uint8_t
    {
        kChunkPosDelta8 = 1u << 0,
    };

    struct alignas(4) MeshChunk
    {
        int16_t minX, minY, minZ;
        int16_t maxX, maxY, maxZ;
        uint32_t dataOffset;
        uint32_t faceOffset;
        uint8_t posCount;
        uint8_t attrCount;
        uint8_t faceCount;
        uint8_t flags;
        uint16_t coneNormal;
        uint8_t coneSin;
        uint8_t posShift;
    };
    static_assert(sizeof(MeshChunk) == 28, "MeshChunk layout changed — sync with Convert.py!");

    struct Face
    {
        uint8_t p0, p1, p2;
        uint8_t a0, a1, a2;
    };
    static_assert(sizeof(Face) == 6, "Face layout changed — sync with Convert.py!");

    struct alignas(4) SubMesh
    {
        uint32_t faceOffset;
        uint32_t faceCount;
        Color color;
    };
    static_assert(sizeof(SubMesh) == 12, "SubMesh layout changed — sync with Convert.py!");
    static_assert(alignof(SubMesh) == 4, "SubMesh alignment drift");

    struct ChunkPosDecode
    {
        float baseX, baseY, baseZ;
        float k;
    };

    class Mesh
    {
    private:
        enum FlagBits : uint8_t
        {
            kFlagCastShadows = 1u << 0,
            kFlagSingleColorLighting = 1u << 1,
            kFlagStaticStorage = 1u << 2,
            kFlagBoundsValid = 1u << 3,
            kFlagHasUV = 1u << 4,
            kFlagHasSubMeshes = 1u << 5,
            kFlagWantsTexture = 1u << 6,
            kFlagHasNormals = 1u << 7,
        };

        static constexpr float kQScaleFactor = 0.5f * (1.0f / 32767.0f);

        mutable uint8_t flags_;

        float uvMinU_ = 0.0f, uvKU_ = 1.0f / 65535.0f;
        float uvMinV_ = 0.0f, uvKV_ = 1.0f / 65535.0f;

        mutable uint16_t maxChunkPosCache_ = 0xFFFF;
        mutable uint16_t maxChunkAttrCache_ = 0xFFFF;

        PIP3D_COLD void calculateBoundingSphere() const;
        PIP3D_COLD void recomputeHalfExtents() const;

        PIP3D_FORCE_INLINE void initSubMeshes(const SubMesh *sub, uint32_t count) noexcept
        {
            subMeshes_ = sub;
            if (count > 0)
                flags_ |= kFlagHasSubMeshes;
        }

        PIP3D_COLD void cleanup()
        {
            if (!(flags_ & kFlagStaticStorage) && heapBlock_)
                MemUtils::freeData(heapBlock_);
            heapBlock_ = nullptr;
            chunks_ = nullptr;
            faces_ = nullptr;
            stream_ = nullptr;
            subMeshes_ = nullptr;
        }

        PIP3D_FORCE_INLINE static uint8_t baseFlags(bool staticStorage, bool hasUV,
                                                    bool hasNormals) noexcept
        {
            return static_cast<uint8_t>(kFlagCastShadows |
                                        (staticStorage ? static_cast<uint8_t>(kFlagStaticStorage) : 0u) |
                                        (hasUV ? static_cast<uint8_t>(kFlagHasUV) : 0u) |
                                        (hasNormals ? static_cast<uint8_t>(kFlagHasNormals) : 0u));
        }

        PIP3D_FORCE_INLINE explicit Mesh(uint8_t flags) noexcept
            : flags_(flags),
              chunks_(nullptr), faces_(nullptr), stream_(nullptr),
              subMeshes_(nullptr), heapBlock_(nullptr),
              chunkCount_(0), subMeshCount_(0),
              qScale_(1.0f), vertexCount_(0), attrCount_(0), faceCount_(0),
              boundsCenter_(0.0f, 0.0f, 0.0f), boundsRadius_(0.0f),
              boundsHalfExtents_(0.0f, 0.0f, 0.0f),
              meshTexture_(nullptr), deleter_(&defaultDeleter)
        {
        }

    protected:
        const MeshChunk *chunks_;
        const Face *faces_;
        const uint8_t *stream_;
        const SubMesh *subMeshes_;
        void *heapBlock_;
        uint32_t chunkCount_;
        uint32_t subMeshCount_;
        float qScale_;
        uint32_t vertexCount_;
        uint32_t attrCount_;
        uint32_t faceCount_;
        mutable Vector3 boundsCenter_;
        mutable float boundsRadius_;
        mutable Vector3 boundsHalfExtents_;
        const Texture *meshTexture_;
        void (*deleter_)(Mesh *);

        PIP3D_COLD void buildFromInterleaved(const Vertex *verts, uint32_t vertCount,
                                             const Face16 *faces, uint32_t faceCount,
                                             bool staticStorage, bool hasUV, bool hasNormals,
                                             uint32_t maxTrisPerChunk = 64);

        struct FromInterleavedTag
        {
        };

        ~Mesh() { cleanup(); }

    public:
        Mesh(const MeshChunk *chunks, uint32_t chunkCount,
             const uint8_t *stream, const Face *faces, uint32_t faceCountIn,
             bool hasUV, bool hasNormals,
             bool staticStorage = true,
             const SubMesh *extSubMeshes = nullptr, uint32_t subMeshCountIn = 0)
            : Mesh(baseFlags(staticStorage, hasUV, hasNormals))
        {
            chunkCount_ = chunkCount;
            faceCount_ = faceCountIn;
            chunks_ = chunks;
            faces_ = faces;
            stream_ = stream;
            initSubMeshes(extSubMeshes, subMeshCountIn);
            for (uint32_t i = 0; i < chunkCount_; ++i)
            {
                vertexCount_ += chunks_[i].posCount;
                attrCount_ += chunks_[i].attrCount;
            }
        }

        Mesh(FromInterleavedTag, const Vertex *verts, uint32_t vertCount,
             const Face16 *faces, uint32_t faceCountIn,
             bool staticStorage, bool hasUV, bool hasNormals,
             uint32_t maxTrisPerChunk = 64)
            : Mesh(baseFlags(staticStorage, hasUV, hasNormals))
        {
            faceCount_ = faceCountIn;
            buildFromInterleaved(verts, vertCount, faces, faceCountIn,
                                 staticStorage, hasUV, hasNormals, maxTrisPerChunk);
        }

        Mesh(const Mesh &) = delete;
        Mesh &operator=(const Mesh &) = delete;

        PIP3D_COLD static void defaultDeleter(Mesh *p) { delete p; }

        template <typename T>
        PIP3D_COLD static void typedDeleter(Mesh *p) { delete static_cast<T *>(p); }

        template <typename T>
        PIP3D_COLD void bindDeleter() { deleter_ = &typedDeleter<T>; }

        PIP3D_COLD static void destroy(Mesh *m)
        {
            if (m)
                m->deleter_(m);
        }

        PIP3D_COLD void autoScale(float size)
        {
            qScale_ = size * kQScaleFactor;
            flags_ &= ~kFlagBoundsValid;
        }

        PIP3D_FORCE_INLINE void finalizeBounds(const Vector3 &boundCenter, float boundRadius)
        {
            boundsCenter_ = boundCenter;
            boundsRadius_ = boundRadius;
            flags_ |= kFlagBoundsValid;
        }

        PIP3D_COLD void finalizeGeometry(uint32_t vCount, uint32_t fCount,
                                         const Vector3 &boundCenter, float boundRadius)
        {
            vertexCount_ = vCount;
            faceCount_ = fCount;
            finalizeBounds(boundCenter, boundRadius);
        }

        [[nodiscard]] PIP3D_FORCE_INLINE uint32_t numFaces() const noexcept { return faceCount_; }
        [[nodiscard]] PIP3D_FORCE_INLINE uint32_t numVertices() const noexcept { return vertexCount_; }
        [[nodiscard]] PIP3D_FORCE_INLINE uint32_t numAttrs() const noexcept { return attrCount_; }
        [[nodiscard]] PIP3D_FORCE_INLINE uint32_t numChunks() const noexcept { return chunkCount_; }
        [[nodiscard]] PIP3D_FORCE_INLINE uint32_t numSubMeshes() const noexcept { return subMeshCount_; }

        [[nodiscard]] PIP3D_FORCE_INLINE bool hasUV() const noexcept { return (flags_ & kFlagHasUV) != 0; }
        [[nodiscard]] PIP3D_FORCE_INLINE bool hasNormals() const noexcept { return (flags_ & kFlagHasNormals) != 0; }

        [[nodiscard]] PIP3D_FORCE_INLINE uint32_t attrRecordSize() const noexcept
        {
            return (hasUV() ? 4u : 0u) + (hasNormals() ? 2u : 0u);
        }

        [[nodiscard]] PIP3D_FORCE_INLINE const MeshChunk *chunkData() const noexcept { return chunks_; }
        [[nodiscard]] PIP3D_FORCE_INLINE const MeshChunk &getChunk(uint32_t i) const noexcept { return chunks_[i]; }
        [[nodiscard]] PIP3D_FORCE_INLINE const Face *faceData() const noexcept { return faces_; }

        [[nodiscard]] PIP3D_FORCE_INLINE static uint32_t posRecordSize(const MeshChunk &c) noexcept
        {
            return (c.flags & kChunkPosDelta8) ? 3u : 6u;
        }
        [[nodiscard]] PIP3D_FORCE_INLINE const uint8_t *chunkPositions(const MeshChunk &c) const noexcept
        {
            return stream_ + c.dataOffset;
        }
        [[nodiscard]] PIP3D_FORCE_INLINE const uint8_t *chunkAttrs(const MeshChunk &c) const noexcept
        {
            const uint32_t posBytes = static_cast<uint32_t>(c.posCount) * posRecordSize(c);
            return stream_ + c.dataOffset + posBytes + (posBytes & 1u);
        }

        [[nodiscard]] PIP3D_FORCE_INLINE bool hasSubMeshes() const noexcept { return (flags_ & kFlagHasSubMeshes) != 0 && subMeshCount_ > 0; }

        [[nodiscard]] PIP3D_FORCE_INLINE uint32_t subMeshFaceEnd(uint32_t i) const noexcept
        {
            return subMeshes_[i].faceOffset + subMeshes_[i].faceCount;
        }
        [[nodiscard]] PIP3D_FORCE_INLINE Color subMeshColor(uint32_t i) const noexcept
        {
            return subMeshes_[i].color;
        }
        [[nodiscard]] PIP3D_FORCE_INLINE bool wantsTexture() const noexcept { return (flags_ & kFlagWantsTexture) != 0; }
        PIP3D_FORCE_INLINE void setWantsTexture(bool e) noexcept
        {
            if (e)
                flags_ |= kFlagWantsTexture;
            else
                flags_ &= ~kFlagWantsTexture;
        }
        [[nodiscard]] PIP3D_FORCE_INLINE float getQScale() const noexcept { return qScale_; }

        [[nodiscard]] PIP3D_FORCE_INLINE uint32_t maxChunkPosCount() const noexcept
        {
            uint32_t m = maxChunkPosCache_;
            if (m == 0xFFFFu)
            {
                m = 0;
                for (uint32_t i = 0; i < chunkCount_; ++i)
                    if (chunks_[i].posCount > m)
                        m = chunks_[i].posCount;
                maxChunkPosCache_ = static_cast<uint16_t>(m);
            }
            return m;
        }
        [[nodiscard]] PIP3D_FORCE_INLINE uint32_t maxChunkAttrCount() const noexcept
        {
            uint32_t m = maxChunkAttrCache_;
            if (m == 0xFFFFu)
            {
                m = 0;
                for (uint32_t i = 0; i < chunkCount_; ++i)
                    if (chunks_[i].attrCount > m)
                        m = chunks_[i].attrCount;
                maxChunkAttrCache_ = static_cast<uint16_t>(m);
            }
            return m;
        }

        [[nodiscard]] PIP3D_FORCE_INLINE uint32_t faceStride() const noexcept
        {
            return (hasUV() || hasNormals()) ? 6u : 3u;
        }

        [[nodiscard]] PIP3D_FORCE_INLINE static ChunkPosDecode chunkPosDecode(const MeshChunk &c, float qScale) noexcept
        {
            ChunkPosDecode d;
            d.baseX = static_cast<float>(c.minX) * qScale;
            d.baseY = static_cast<float>(c.minY) * qScale;
            d.baseZ = static_cast<float>(c.minZ) * qScale;
            d.k = qScale * static_cast<float>(1u << c.posShift);
            return d;
        }

        [[nodiscard]] PIP3D_FORCE_INLINE static Vector3 chunkPosition(const MeshChunk &c,
                                                                      const ChunkPosDecode &d,
                                                                      const uint8_t *rec) noexcept
        {
            if (c.flags & kChunkPosDelta8)
            {
                return Vector3(d.baseX + static_cast<float>(rec[0]) * d.k,
                               d.baseY + static_cast<float>(rec[1]) * d.k,
                               d.baseZ + static_cast<float>(rec[2]) * d.k);
            }
            uint16_t rx, ry, rz;
            memcpy(&rx, rec, 2);
            memcpy(&ry, rec + 2, 2);
            memcpy(&rz, rec + 4, 2);
            return Vector3(d.baseX + static_cast<float>(rx) * d.k,
                           d.baseY + static_cast<float>(ry) * d.k,
                           d.baseZ + static_cast<float>(rz) * d.k);
        }

        PIP3D_FORCE_INLINE void attrUV(const uint8_t *rec, float &u, float &v) const noexcept
        {
            uint16_t ru, rv;
            memcpy(&ru, rec, 2);
            memcpy(&rv, rec + 2, 2);
            u = uvMinU_ + static_cast<float>(ru) * uvKU_;
            v = uvMinV_ + static_cast<float>(rv) * uvKV_;
        }

        PIP3D_FORCE_INLINE void finalizeUVRange(float minU, float spanU,
                                                float minV, float spanV) noexcept
        {
            uvMinU_ = minU;
            uvKU_ = spanU * (1.0f / 65535.0f);
            uvMinV_ = minV;
            uvKV_ = spanV * (1.0f / 65535.0f);
        }

        [[nodiscard]] PIP3D_FORCE_INLINE Vector3 attrNormal(const uint8_t *rec) const noexcept
        {
            uint16_t oct;
            memcpy(&oct, rec + (hasUV() ? 4 : 0), 2);
            return PackedNormal(oct).get();
        }

        template <typename Fn>
        void forEachPosition(Fn &&fn) const
        {
            for (uint32_t ci = 0; ci < chunkCount_; ++ci)
            {
                const MeshChunk &c = chunks_[ci];
                const ChunkPosDecode d = chunkPosDecode(c, qScale_);
                const uint8_t *rec = chunkPositions(c);
                const uint32_t recSize = posRecordSize(c);
                for (uint32_t i = 0; i < c.posCount; ++i, rec += recSize)
                    fn(chunkPosition(c, d, rec));
            }
        }

        template <typename Fn>
        void forEachFace(Fn &&fn) const
        {
            const uint32_t stride = faceStride();
            const uint8_t *PIP3D_RESTRICT fb = reinterpret_cast<const uint8_t *>(faces_);
            uint32_t posBase = 0, attrBase = 0;
            for (uint32_t ci = 0; ci < chunkCount_; ++ci)
            {
                const MeshChunk &c = chunks_[ci];
                const uint8_t *f = fb + static_cast<size_t>(c.faceOffset) * stride;
                if (stride == 6)
                {
                    for (uint32_t i = 0; i < c.faceCount; ++i, f += 6)
                    {
                        fn(c.faceOffset + i,
                           posBase + f[0], posBase + f[1], posBase + f[2],
                           attrBase + f[3], attrBase + f[4], attrBase + f[5]);
                    }
                }
                else
                {
                    for (uint32_t i = 0; i < c.faceCount; ++i, f += 3)
                    {
                        fn(c.faceOffset + i,
                           posBase + f[0], posBase + f[1], posBase + f[2],
                           attrBase, attrBase, attrBase);
                    }
                }
                posBase += c.posCount;
                attrBase += c.attrCount;
            }
        }

        PIP3D_FORCE_INLINE void ensureBounds() const
        {
            if (unlikely(!(flags_ & kFlagBoundsValid)))
                calculateBoundingSphere();
        }

        PIP3D_FORCE_INLINE void getBounds(Vector3 &outCenter, float &outRadius) const
        {
            ensureBounds();
            outCenter = boundsCenter_;
            outRadius = boundsRadius_;
        }

        PIP3D_FORCE_INLINE void getLocalExtents(Vector3 &outHalfExtents) const
        {
            ensureBounds();
            if (boundsHalfExtents_.x == 0.0f && boundsHalfExtents_.y == 0.0f && boundsHalfExtents_.z == 0.0f && boundsRadius_ > 1e-4f)
                recomputeHalfExtents();
            outHalfExtents = boundsHalfExtents_;
        }

        [[nodiscard]] PIP3D_FORCE_INLINE const Vector3 &center() const
        {
            ensureBounds();
            return boundsCenter_;
        }
        [[nodiscard]] PIP3D_FORCE_INLINE float radius() const
        {
            ensureBounds();
            return boundsRadius_;
        }

        [[nodiscard]] PIP3D_FORCE_INLINE bool getCastShadows() const noexcept { return (flags_ & kFlagCastShadows) != 0; }
        PIP3D_FORCE_INLINE void setCastShadows(bool e) noexcept
        {
            if (e)
                flags_ |= kFlagCastShadows;
            else
                flags_ &= ~kFlagCastShadows;
        }

        [[nodiscard]] PIP3D_FORCE_INLINE bool getSingleColorLighting() const noexcept { return (flags_ & kFlagSingleColorLighting) != 0; }
        PIP3D_FORCE_INLINE void setSingleColorLighting(bool e) noexcept
        {
            if (e)
                flags_ |= kFlagSingleColorLighting;
            else
                flags_ &= ~kFlagSingleColorLighting;
        }

        [[nodiscard]] PIP3D_FORCE_INLINE const Texture *getTexture() const noexcept { return meshTexture_; }
        PIP3D_FORCE_INLINE void setTexture(const Texture *t) noexcept { meshTexture_ = t; }
        [[nodiscard]] PIP3D_FORCE_INLINE bool isTextured() const noexcept { return meshTexture_ != nullptr; }
    };
}