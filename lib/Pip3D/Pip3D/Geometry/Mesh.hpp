#pragma once

#include "Math/Algebra.hpp"
#include "Math/Quant.hpp"
#include "Core/Memory.hpp"
#include "Rendering/Display/Texture.hpp"

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
    static_assert(sizeof(Vertex) == 16, "Vertex must be 16 bytes");

    struct Face
    {
        uint16_t v0, v1, v2;
        constexpr Face() : v0(0), v1(0), v2(0) {}
        constexpr Face(uint16_t a, uint16_t b, uint16_t c) : v0(a), v1(b), v2(c) {}
    };

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

        void (*deleter_)(Mesh *);
        mutable Vector3 boundsCenter_;
        mutable float boundsRadius_;

    protected:
        Vertex *PIP3D_RESTRICT vertices_;
        Face *PIP3D_RESTRICT faces_;
        uint16_t vertexCount_;
        uint16_t faceCount_;
        uint16_t maxVertices_;
        uint16_t maxFaces_;
        const Texture *meshTexture_;
        Mesh *shadowProxy_;
        float qScale_;
        mutable uint8_t flags_;

    public:
        explicit Mesh(uint16_t maxVerts, uint16_t maxFaces)
            : deleter_(&defaultDeleter),
              boundsCenter_(0.0f, 0.0f, 0.0f),
              boundsRadius_(-1.0f),
              vertices_(static_cast<Vertex *>(MemUtils::allocData(
                  static_cast<size_t>(maxVerts) * sizeof(Vertex), 16))),
              faces_(static_cast<Face *>(MemUtils::allocData(
                  static_cast<size_t>(maxFaces) * sizeof(Face), 16))),
              vertexCount_(0), faceCount_(0),
              maxVertices_(maxVerts), maxFaces_(maxFaces),
              meshTexture_(nullptr), shadowProxy_(nullptr),
              qScale_(1.0f),
              flags_(kFlagCastShadows)
        {
            if (unlikely(!vertices_ || !faces_))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES,
                     "Mesh ctor: alloc failed (maxVerts=%u maxFaces=%u)",
                     static_cast<unsigned>(maxVerts),
                     static_cast<unsigned>(maxFaces));
                cleanup();
            }
        }

        Mesh(const Vertex *extVerts, uint16_t vertCount,
             const Face *extFaces, uint16_t faceCountIn,
             bool staticStorage = true)
            : deleter_(&defaultDeleter),
              boundsCenter_(0.0f, 0.0f, 0.0f),
              boundsRadius_(-1.0f),
              vertices_(const_cast<Vertex *>(extVerts)),
              faces_(const_cast<Face *>(extFaces)),
              vertexCount_(vertCount), faceCount_(faceCountIn),
              maxVertices_(vertCount), maxFaces_(faceCountIn),
              meshTexture_(nullptr), shadowProxy_(nullptr),
              qScale_(1.0f),
              flags_(kFlagCastShadows | (staticStorage ? kFlagStaticStorage : 0u))
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
        PIP3D_FORCE_INLINE void bindDeleter()
        {
            deleter_ = &typedDeleter<T>;
        }

        PIP3D_COLD static void destroy(Mesh *m)
        {
            if (m)
                m->deleter_(m);
        }

        PIP3D_COLD void cleanup()
        {
            if (!(flags_ & kFlagStaticStorage))
            {
                if (vertices_)
                {
                    MemUtils::freeData(vertices_);
                    vertices_ = nullptr;
                }
                if (faces_)
                {
                    MemUtils::freeData(faces_);
                    faces_ = nullptr;
                }
            }
            vertexCount_ = 0;
            faceCount_ = 0;
            maxVertices_ = 0;
            maxFaces_ = 0;
            flags_ &= ~kFlagBoundsValid;
            boundsRadius_ = -1.0f;
        }

        ~Mesh() { cleanup(); }

        PIP3D_FORCE_INLINE uint16_t maxVertices() const { return maxVertices_; }
        PIP3D_FORCE_INLINE uint16_t maxFaces() const { return maxFaces_; }

        PIP3D_FORCE_INLINE void autoScale(float size)
        {
            qScale_ = size * kQScaleFactor;
            flags_ &= ~kFlagBoundsValid;
        }

        PIP3D_FORCE_INLINE void finalizeGeometry(uint16_t vCount, uint16_t fCount,
                                                 const Vector3 &boundCenter,
                                                 float boundRadius)
        {
            vertexCount_ = vCount;
            faceCount_ = fCount;
            boundsCenter_ = boundCenter;
            boundsRadius_ = boundRadius;
            flags_ |= kFlagBoundsValid;
        }

        PIP3D_COLD void finalizeNormals()
        {
            if (unlikely(!vertices_ || !faces_ || vertexCount_ == 0))
                return;

            const size_t scratchBytes = static_cast<size_t>(vertexCount_) * sizeof(Vector3);
            Vector3 *PIP3D_RESTRICT normals = static_cast<Vector3 *>(
                MemUtils::allocData(scratchBytes, 16));
            if (unlikely(!normals))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES,
                     "Mesh::finalizeNormals: alloc failed (vertexCount=%u)",
                     static_cast<unsigned>(vertexCount_));
                return;
            }
            memset(normals, 0, scratchBytes);

            const float scale = qScale_;
            const Face *PIP3D_RESTRICT fPtr = faces_;
            Vertex *PIP3D_RESTRICT vPtr = vertices_;
            const uint16_t fN = faceCount_;
            const uint16_t vN = vertexCount_;

            for (uint16_t i = 0; i < fN; ++i)
            {
                const Face &f = fPtr[i];
                const Vertex &a = vPtr[f.v0];
                const Vertex &b = vPtr[f.v1];
                const Vertex &c = vPtr[f.v2];
                const float ax = static_cast<float>(a.px) * scale;
                const float ay = static_cast<float>(a.py) * scale;
                const float az = static_cast<float>(a.pz) * scale;
                const float bx = static_cast<float>(b.px) * scale;
                const float by = static_cast<float>(b.py) * scale;
                const float bz = static_cast<float>(b.pz) * scale;
                const float cx = static_cast<float>(c.px) * scale;
                const float cy = static_cast<float>(c.py) * scale;
                const float cz = static_cast<float>(c.pz) * scale;

                const float ex = bx - ax, ey = by - ay, ez = bz - az;
                const float fx = cx - ax, fy = cy - ay, fz = cz - az;
                const float nx = ey * fz - ez * fy;
                const float ny = ez * fx - ex * fz;
                const float nz = ex * fy - ey * fx;

                normals[f.v0].x += nx;
                normals[f.v0].y += ny;
                normals[f.v0].z += nz;
                normals[f.v1].x += nx;
                normals[f.v1].y += ny;
                normals[f.v1].z += nz;
                normals[f.v2].x += nx;
                normals[f.v2].y += ny;
                normals[f.v2].z += nz;
            }

            for (uint16_t i = 0; i < vN; ++i)
            {
                normals[i].normalize();
                vPtr[i].normal.set(normals[i]);
            }

            MemUtils::freeData(normals);
        }

        PIP3D_COLD void calculateBoundingSphere() const
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

            auto decodeV = [&](uint16_t i) -> Vector3
            {
                const Vertex &vv = vPtr[i];
                return Vector3(static_cast<float>(vv.px) * scale,
                               static_cast<float>(vv.py) * scale,
                               static_cast<float>(vv.pz) * scale);
            };

            const Vector3 extremes[6] = {
                decodeV(iMinX), decodeV(iMaxX),
                decodeV(iMinY), decodeV(iMaxY),
                decodeV(iMinZ), decodeV(iMaxZ)};

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
                const Vector3 p = decodeV(i);
                const float dx = p.x - center.x;
                const float dy = p.y - center.y;
                const float dz = p.z - center.z;
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

            const float boundaryThresholdSq = radiusSq * 0.9025f;
            Vector3 centroid(0.0f, 0.0f, 0.0f);
            uint16_t boundaryCount = 0;
            for (uint16_t i = 0; i < vN; ++i)
            {
                const Vector3 p = decodeV(i);
                const float dx = p.x - center.x;
                const float dy = p.y - center.y;
                const float dz = p.z - center.z;
                const float dSq = dx * dx + dy * dy + dz * dz;
                if (dSq >= boundaryThresholdSq)
                {
                    centroid.x += p.x;
                    centroid.y += p.y;
                    centroid.z += p.z;
                    ++boundaryCount;
                }
            }
            if (boundaryCount > 0)
            {
                const float invCount = FastMath::fastReciprocal(
                    static_cast<float>(boundaryCount));
                centroid.x *= invCount;
                centroid.y *= invCount;
                centroid.z *= invCount;

                float newRadiusSq = 0.0f;
                for (uint16_t i = 0; i < vN; ++i)
                {
                    const Vector3 p = decodeV(i);
                    const float dx = p.x - centroid.x;
                    const float dy = p.y - centroid.y;
                    const float dz = p.z - centroid.z;
                    const float dSq = dx * dx + dy * dy + dz * dz;
                    if (dSq > newRadiusSq)
                        newRadiusSq = dSq;
                }
                if (newRadiusSq < radiusSq)
                {
                    center = centroid;
                    radiusSq = newRadiusSq;
                    radius = radiusSq * FastMath::fastInvSqrt(radiusSq);
                }
            }

            boundsCenter_ = center;
            boundsRadius_ = radius;
            flags_ |= kFlagBoundsValid;
        }

        PIP3D_FORCE_INLINE uint16_t numFaces() const { return faceCount_; }
        PIP3D_FORCE_INLINE uint16_t numVertices() const { return vertexCount_; }

        PIP3D_FORCE_INLINE const Face &face(uint16_t i) const { return faces_[i]; }
        PIP3D_FORCE_INLINE const Vertex &vert(uint16_t i) const { return vertices_[i]; }

        PIP3D_FORCE_INLINE const Vertex *PIP3D_RESTRICT vertexData() const { return vertices_; }
        PIP3D_FORCE_INLINE const Face *PIP3D_RESTRICT faceData() const { return faces_; }

        PIP3D_FORCE_INLINE Vector3 decodePosition(const Vertex &v) const
        {
            return Vector3(static_cast<float>(v.px) * qScale_,
                           static_cast<float>(v.py) * qScale_,
                           static_cast<float>(v.pz) * qScale_);
        }

        PIP3D_FORCE_INLINE Vector3 decodePosition(uint16_t i) const
        {
            const Vertex &v = vertices_[i];
            return Vector3(static_cast<float>(v.px) * qScale_,
                           static_cast<float>(v.py) * qScale_,
                           static_cast<float>(v.pz) * qScale_);
        }

        PIP3D_FORCE_INLINE const Vector3 &center() const
        {
            if (unlikely(!(flags_ & kFlagBoundsValid)))
                calculateBoundingSphere();
            return boundsCenter_;
        }
        PIP3D_FORCE_INLINE float radius() const
        {
            if (unlikely(!(flags_ & kFlagBoundsValid)))
                calculateBoundingSphere();
            return boundsRadius_;
        }

        PIP3D_FORCE_INLINE bool getCastShadows() const { return (flags_ & kFlagCastShadows) != 0; }
        PIP3D_FORCE_INLINE void setCastShadows(bool e)
        {
            if (e)
                flags_ |= kFlagCastShadows;
            else
                flags_ &= ~kFlagCastShadows;
        }

        PIP3D_FORCE_INLINE bool getSingleColorLighting() const { return (flags_ & kFlagSingleColorLighting) != 0; }
        PIP3D_FORCE_INLINE void setSingleColorLighting(bool e)
        {
            if (e)
                flags_ |= kFlagSingleColorLighting;
            else
                flags_ &= ~kFlagSingleColorLighting;
        }

        PIP3D_FORCE_INLINE const Texture *getTexture() const { return meshTexture_; }
        PIP3D_FORCE_INLINE void setTexture(const Texture *t) { meshTexture_ = t; }
        PIP3D_FORCE_INLINE bool isTextured() const { return meshTexture_ != nullptr; }

        PIP3D_FORCE_INLINE void setShadowProxy(Mesh *p) { shadowProxy_ = p; }
        PIP3D_FORCE_INLINE Mesh *getShadowProxy() const { return shadowProxy_; }
    };

}