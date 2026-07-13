#pragma once

#include "Math/Algebra.hpp"
#include "Math/Quant.hpp"
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

    struct Face
    {
        uint16_t v0, v1, v2;
        constexpr Face() : v0(0), v1(0), v2(0) {}
        constexpr Face(uint16_t a, uint16_t b, uint16_t c) : v0(a), v1(b), v2(c) {}
    };
    static_assert(sizeof(Face) == 6);

    class Mesh
    {
    private:
        enum FlagBits : uint8_t
        {
            kFlagCastShadows = 1u << 0,
            kFlagSingleColorLighting = 1u << 1,
            kFlagStaticStorage = 1u << 2,
            kFlagBoundsValid = 1u << 3
        };

        static constexpr float kQScaleFactor = 0.5f * (1.0f / 32767.0f);

        PIP3D_COLD void calculateBoundingSphere() const;

        PIP3D_COLD void cleanup()
        {
            if (!(flags_ & kFlagStaticStorage) && vertices_)
                MemUtils::freeData(vertices_);
        }

    protected:
        Vertex *PIP3D_RESTRICT vertices_;
        Face *PIP3D_RESTRICT faces_;
        float qScale_;
        uint16_t vertexCount_;
        uint16_t faceCount_;
        mutable uint8_t flags_;
        mutable Vector3 boundsCenter_;
        mutable float boundsRadius_;
        const Texture *meshTexture_;
        uint16_t maxVertices_;
        uint16_t maxFaces_;
        void (*deleter_)(Mesh *);

        ~Mesh() { cleanup(); }

    public:
        explicit Mesh(uint16_t maxVerts, uint16_t maxFaces)
            : vertices_(nullptr), faces_(nullptr),
              qScale_(1.0f),
              vertexCount_(0), faceCount_(0),
              flags_(kFlagCastShadows),
              boundsCenter_(0.0f, 0.0f, 0.0f),
              boundsRadius_(0.0f),
              meshTexture_(nullptr),
              maxVertices_(maxVerts), maxFaces_(maxFaces),
              deleter_(&defaultDeleter)
        {
            const size_t vBytes = static_cast<size_t>(maxVerts) * sizeof(Vertex);
            const size_t fBytes = static_cast<size_t>(maxFaces) * sizeof(Face);
            Vertex *buf = static_cast<Vertex *>(
                MemUtils::allocData(vBytes + fBytes, 16));
            vertices_ = buf;
            faces_ = (buf != nullptr)
                         ? reinterpret_cast<Face *>(buf + maxVerts)
                         : nullptr;

            if (unlikely(!buf))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES,
                     "Mesh ctor: alloc failed (maxVerts=%u maxFaces=%u)",
                     static_cast<unsigned>(maxVerts),
                     static_cast<unsigned>(maxFaces));
            }
        }

        Mesh(const Vertex *extVerts, uint16_t vertCount,
             const Face *extFaces, uint16_t faceCountIn,
             bool staticStorage = true)
            : vertices_(const_cast<Vertex *>(extVerts)),
              faces_(const_cast<Face *>(extFaces)),
              qScale_(1.0f),
              vertexCount_(vertCount), faceCount_(faceCountIn),
              flags_(kFlagCastShadows | (staticStorage ? kFlagStaticStorage : 0u)),
              boundsCenter_(0.0f, 0.0f, 0.0f),
              boundsRadius_(0.0f),
              meshTexture_(nullptr),
              maxVertices_(vertCount), maxFaces_(faceCountIn),
              deleter_(&defaultDeleter)
        {
        }

        Mesh(const Mesh &) = delete;
        Mesh &operator=(const Mesh &) = delete;

        PIP3D_COLD static void defaultDeleter(Mesh *p) { delete p; }

        template <typename T>
        PIP3D_COLD static void typedDeleter(Mesh *p)
        {
            delete static_cast<T *>(p);
        }

        template <typename T>
        PIP3D_COLD void bindDeleter()
        {
            deleter_ = &typedDeleter<T>;
        }

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

        PIP3D_COLD void finalizeGeometry(uint16_t vCount, uint16_t fCount,
                                         const Vector3 &boundCenter,
                                         float boundRadius)
        {
            vertexCount_ = vCount;
            faceCount_ = fCount;
            boundsCenter_ = boundCenter;
            boundsRadius_ = boundRadius;
            flags_ |= kFlagBoundsValid;
        }

        [[nodiscard]] PIP3D_FORCE_INLINE uint16_t numFaces() const { return faceCount_; }
        [[nodiscard]] PIP3D_FORCE_INLINE uint16_t numVertices() const { return vertexCount_; }

        [[nodiscard]] PIP3D_FORCE_INLINE const Vertex *PIP3D_RESTRICT vertexData() const { return vertices_; }
        [[nodiscard]] PIP3D_FORCE_INLINE const Face *PIP3D_RESTRICT faceData() const { return faces_; }

        [[nodiscard]] PIP3D_FORCE_INLINE Vector3 decodePosition(const Vertex &v) const
        {
            return Vector3(static_cast<float>(v.px) * qScale_,
                           static_cast<float>(v.py) * qScale_,
                           static_cast<float>(v.pz) * qScale_);
        }

        PIP3D_HOT void decodePositions(Vector3 *PIP3D_RESTRICT out, uint16_t count) const
        {
            const Vertex *PIP3D_RESTRICT src = vertices_;
            const float s = qScale_;
            for (uint16_t i = 0; i < count; ++i)
            {
                out[i].x = static_cast<float>(src[i].px) * s;
                out[i].y = static_cast<float>(src[i].py) * s;
                out[i].z = static_cast<float>(src[i].pz) * s;
            }
        }

        PIP3D_FORCE_INLINE void getBounds(Vector3 &outCenter, float &outRadius) const
        {
            if (unlikely(!(flags_ & kFlagBoundsValid)))
                calculateBoundingSphere();
            outCenter = boundsCenter_;
            outRadius = boundsRadius_;
        }

        [[nodiscard]] PIP3D_FORCE_INLINE const Vector3 &center() const
        {
            if (unlikely(!(flags_ & kFlagBoundsValid)))
                calculateBoundingSphere();
            return boundsCenter_;
        }
        [[nodiscard]] PIP3D_FORCE_INLINE float radius() const
        {
            if (unlikely(!(flags_ & kFlagBoundsValid)))
                calculateBoundingSphere();
            return boundsRadius_;
        }

        [[nodiscard]] PIP3D_FORCE_INLINE bool getCastShadows() const { return (flags_ & kFlagCastShadows) != 0; }
        PIP3D_FORCE_INLINE void setCastShadows(bool e)
        {
            if (e)
                flags_ |= kFlagCastShadows;
            else
                flags_ &= ~kFlagCastShadows;
        }

        [[nodiscard]] PIP3D_FORCE_INLINE bool getSingleColorLighting() const { return (flags_ & kFlagSingleColorLighting) != 0; }
        PIP3D_FORCE_INLINE void setSingleColorLighting(bool e)
        {
            if (e)
                flags_ |= kFlagSingleColorLighting;
            else
                flags_ &= ~kFlagSingleColorLighting;
        }

        [[nodiscard]] PIP3D_FORCE_INLINE const Texture *getTexture() const { return meshTexture_; }
        PIP3D_FORCE_INLINE void setTexture(const Texture *t) { meshTexture_ = t; }
        [[nodiscard]] PIP3D_FORCE_INLINE bool isTextured() const { return meshTexture_ != nullptr; }
    };

    static_assert(sizeof(void*) == 4 ? sizeof(Mesh) == 48 : true);

    PIP3D_COLD inline void Mesh::calculateBoundingSphere() const
    {
        if (unlikely(!vertices_ || vertexCount_ == 0))
        {
            boundsCenter_ = Vector3(0.0f, 0.0f, 0.0f);
            boundsRadius_ = 0.0f;
            flags_ |= kFlagBoundsValid;
            return;
        }

        const Vertex *PIP3D_RESTRICT vPtr = vertices_;
        const uint16_t vN = vertexCount_;
        const float scale = qScale_;

        uint16_t iMinX = 0, iMaxX = 0;
        uint16_t iMinY = 0, iMaxY = 0;
        uint16_t iMinZ = 0, iMaxZ = 0;
        for (uint16_t i = 1; i < vN; ++i)
        {
            const int16_t x = vPtr[i].px;
            const int16_t y = vPtr[i].py;
            const int16_t z = vPtr[i].pz;
            if (x < vPtr[iMinX].px)
                iMinX = i;
            if (x > vPtr[iMaxX].px)
                iMaxX = i;
            if (y < vPtr[iMinY].py)
                iMinY = i;
            if (y > vPtr[iMaxY].py)
                iMaxY = i;
            if (z < vPtr[iMinZ].pz)
                iMinZ = i;
            if (z > vPtr[iMaxZ].pz)
                iMaxZ = i;
        }

        const Vector3 extremes[6] = {
            Vector3(float(vPtr[iMinX].px) * scale, float(vPtr[iMinX].py) * scale, float(vPtr[iMinX].pz) * scale),
            Vector3(float(vPtr[iMaxX].px) * scale, float(vPtr[iMaxX].py) * scale, float(vPtr[iMaxX].pz) * scale),
            Vector3(float(vPtr[iMinY].px) * scale, float(vPtr[iMinY].py) * scale, float(vPtr[iMinY].pz) * scale),
            Vector3(float(vPtr[iMaxY].px) * scale, float(vPtr[iMaxY].py) * scale, float(vPtr[iMaxY].pz) * scale),
            Vector3(float(vPtr[iMinZ].px) * scale, float(vPtr[iMinZ].py) * scale, float(vPtr[iMinZ].pz) * scale),
            Vector3(float(vPtr[iMaxZ].px) * scale, float(vPtr[iMaxZ].py) * scale, float(vPtr[iMaxZ].pz) * scale)};

        float bestDistSq = 0.0f;
        Vector3 p1 = extremes[0], p2 = extremes[1];
        for (int i = 0; i < 6; ++i)
            for (int j = i + 1; j < 6; ++j)
            {
                const float dx = extremes[i].x - extremes[j].x;
                const float dy = extremes[i].y - extremes[j].y;
                const float dz = extremes[i].z - extremes[j].z;
                const float d = dx * dx + dy * dy + dz * dz;
                if (d > bestDistSq)
                {
                    bestDistSq = d;
                    p1 = extremes[i];
                    p2 = extremes[j];
                }
            }

        Vector3 center = (p1 + p2) * 0.5f;
        float radius = (bestDistSq > 0.0f)
                           ? 0.5f * bestDistSq * FastMath::fastInvSqrt(bestDistSq)
                           : 0.0f;
        float radiusSq = radius * radius;

        for (uint16_t i = 0; i < vN; ++i)
        {
            const float px = float(vPtr[i].px) * scale;
            const float py = float(vPtr[i].py) * scale;
            const float pz = float(vPtr[i].pz) * scale;
            const float dx = px - center.x;
            const float dy = py - center.y;
            const float dz = pz - center.z;
            const float dSq = dx * dx + dy * dy + dz * dz;
            if (dSq <= radiusSq)
                continue;
            const float d = dSq * FastMath::fastInvSqrt(dSq);
            const float newRadius = (radius + d) * 0.5f;
            const float k = (d - radius) * 0.5f * FastMath::fastReciprocal(d);
            center.x += dx * k;
            center.y += dy * k;
            center.z += dz * k;
            radius = newRadius;
            radiusSq = radius * radius;
        }

        boundsCenter_ = center;
        boundsRadius_ = radius;
        flags_ |= kFlagBoundsValid;
    }

}