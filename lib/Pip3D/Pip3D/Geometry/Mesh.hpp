#pragma once

#include "Math/Algebra.hpp"
#include "Core/Memory.hpp"
#include "Rendering/Display/Texture.hpp"

#if !defined(PIP3D_PC)
#include <esp_heap_caps.h>
#include <esp_attr.h>
#include <soc/cpu.h>
#endif

#include <string.h>

#define MESH_SIMD_ALIGN __attribute__((aligned(16)))
#define MESH_FORCE_INLINE __attribute__((always_inline)) inline
#define MESH_HOT_PATH __attribute__((hot))
#define MESH_COLD_PATH __attribute__((cold))
#define MESH_PURE __attribute__((pure))

static constexpr float NORMAL_ENCODE_SCALE = 255.0f;
static constexpr float NORMAL_DECODE_SCALE = 0.00784313725490196078f;
static constexpr float NORMAL_DECODE_BIAS = -1.0f;
static constexpr float EPSILON_SQ = 1e-12f;

namespace pip3D
{
    struct Texture;

    struct PackedNormal
    {
        uint16_t data;

        MESH_FORCE_INLINE PackedNormal() : data(0) {}
        constexpr PackedNormal(uint16_t d) : data(d) {}

        MESH_HOT_PATH MESH_FORCE_INLINE void set(const Vector3 &n)
        {
            float l1norm = fabsf(n.x) + fabsf(n.y) + fabsf(n.z);
            if (likely(l1norm > 1e-6f))
            {
                float inv_norm = 1.0f / l1norm;
                float nx = n.x * inv_norm;
                float ny = n.y * inv_norm;

                if (unlikely(n.z < 0.0f))
                {
                    float tx = nx;
                    nx = (1.0f - fabsf(ny)) * (nx >= 0.0f ? 1.0f : -1.0f);
                    ny = (1.0f - fabsf(tx)) * (ny >= 0.0f ? 1.0f : -1.0f);
                }

                uint32_t px = (uint32_t)((nx * 0.5f + 0.5f) * NORMAL_ENCODE_SCALE);
                uint32_t py = (uint32_t)((ny * 0.5f + 0.5f) * NORMAL_ENCODE_SCALE);
                data = (px << 8) | py;
            }
        }

        MESH_HOT_PATH MESH_FORCE_INLINE MESH_PURE Vector3 get() const
        {
            uint32_t px = data >> 8;
            uint32_t py = data & 0xFF;

            float nx = px * NORMAL_DECODE_SCALE + NORMAL_DECODE_BIAS;
            float ny = py * NORMAL_DECODE_SCALE + NORMAL_DECODE_BIAS;
            float nz = 1.0f - fabsf(nx) - fabsf(ny);

            if (unlikely(nz < 0.0f))
            {
                float tx = nx;
                nx = (1.0f - fabsf(ny)) * (nx >= 0.0f ? 1.0f : -1.0f);
                ny = (1.0f - fabsf(tx)) * (ny >= 0.0f ? 1.0f : -1.0f);
            }

            Vector3 result(nx, ny, nz);
            return result;
        }
    };

    struct Vertex
    {
        int16_t px, py, pz;
        PackedNormal normal;
        float tu, tv;

        MESH_FORCE_INLINE Vertex() : px(0), py(0), pz(0), normal(), tu(0.0f), tv(0.0f) {}
        constexpr Vertex(int16_t x, int16_t y, int16_t z, uint16_t norm, float u = 0.0f, float v = 0.0f)
            : px(x), py(y), pz(z), normal(norm), tu(u), tv(v) {}
    };

    struct Face
    {
        uint16_t v0, v1, v2;

        constexpr Face() : v0(0), v1(0), v2(0) {}
        constexpr Face(uint16_t a, uint16_t b, uint16_t c) : v0(a), v1(b), v2(c) {}
    };

    struct MESH_SIMD_ALIGN MeshCache
    {
        Vector3 boundingCenter;
        float boundingRadius;
        bool boundsValid;

        MESH_FORCE_INLINE MeshCache() : boundingRadius(0), boundsValid(false) {}
    };

    class MESH_SIMD_ALIGN Mesh
    {
    protected:
        Vertex *__restrict__ vertices;
        Face *__restrict__ faces;

        uint16_t vertexCount;
        uint16_t faceCount;
        uint16_t maxVertices;
        uint16_t maxFaces;

        Color defaultColor;
        bool castShadows;
        bool singleColorLighting;
        bool isStaticStorage;
        float qScale;
        const Texture *meshTexture;
        Mesh *shadowProxy;

        mutable MeshCache cache;

        mutable Vector3 *cachedLocalVertices;
        mutable uint16_t cachedLocalVertexCapacity;
        mutable bool cachedLocalVerticesValid;

        void freeLocalVertexCache()
        {
            if (cachedLocalVertices)
            {
                MemUtils::freeData(cachedLocalVertices);
                cachedLocalVertices = nullptr;
            }
            cachedLocalVertexCapacity = 0;
            cachedLocalVerticesValid = false;
        }

    public:
        MESH_HOT_PATH Mesh(uint16_t maxVerts = 64, uint16_t maxFcs = 128, const Color &color = Color::WHITE)
            : vertices(nullptr), faces(nullptr),
              vertexCount(0), faceCount(0), maxVertices(maxVerts), maxFaces(maxFcs),
              defaultColor(color), castShadows(true),
              singleColorLighting(false), isStaticStorage(false), qScale(1.0f),
              meshTexture(nullptr), shadowProxy(nullptr),
              cachedLocalVertices(nullptr), cachedLocalVertexCapacity(0),
              cachedLocalVerticesValid(false)
        {
            const size_t vertexSize = static_cast<size_t>(maxVertices) * sizeof(Vertex);
            const size_t faceSize = static_cast<size_t>(maxFaces) * sizeof(Face);

            vertices = (Vertex *)MemUtils::allocData(vertexSize, 16);
            faces = (Face *)MemUtils::allocData(faceSize, 16);

            if (unlikely(!vertices || !faces))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES,
                     "Mesh ctor: failed to allocate geometry buffers (maxVertices=%u, maxFaces=%u, vertexSize=%u, faceSize=%u)",
                     static_cast<unsigned int>(maxVertices),
                     static_cast<unsigned int>(maxFaces),
                     static_cast<unsigned int>(vertexSize),
                     static_cast<unsigned int>(faceSize));
                cleanup();
                return;
            }
        }

        MESH_HOT_PATH Mesh(const Vertex *externalVertices,
                           uint16_t vertCount,
                           const Face *externalFaces,
                           uint16_t faceCountIn,
                           const Color &color = Color::WHITE,
                           bool staticStorage = true)
            : vertices(const_cast<Vertex *>(externalVertices)),
              faces(const_cast<Face *>(externalFaces)),
              vertexCount(vertCount), faceCount(faceCountIn),
              maxVertices(vertCount), maxFaces(faceCountIn),
              defaultColor(color), castShadows(true),
              singleColorLighting(false), isStaticStorage(staticStorage), qScale(1.0f),
              meshTexture(nullptr), shadowProxy(nullptr),
              cachedLocalVertices(nullptr), cachedLocalVertexCapacity(0),
              cachedLocalVerticesValid(false)
        {
        }

        MESH_COLD_PATH void cleanup()
        {
            if (!isStaticStorage)
            {
                if (vertices)
                {
                    MemUtils::freeData(vertices);
                    vertices = nullptr;
                }
                if (faces)
                {
                    MemUtils::freeData(faces);
                    faces = nullptr;
                }
            }
            freeLocalVertexCache();
            vertexCount = 0;
            faceCount = 0;
            maxVertices = 0;
            maxFaces = 0;
            cache.boundsValid = false;
        }

        MESH_COLD_PATH virtual ~Mesh()
        {
            cleanup();
        }

        Mesh(const Mesh &) = delete;
        Mesh &operator=(const Mesh &) = delete;

        MESH_FORCE_INLINE void autoScale(float size)
        {
            if (size <= 0.0f)
            {
                qScale = 1.0f;
                cachedLocalVerticesValid = false;
                return;
            }
            const float half = size * 0.5f;
            const float denom = 32767.0f;
            qScale = half / denom;
            cachedLocalVerticesValid = false;
        }

        MESH_FORCE_INLINE int16_t quantizeCoord(float x) const
        {
            if (qScale <= 0.0f)
                return 0;
            float q = x / qScale;
            float clamped = fminf(fmaxf(q, -32768.0f), 32767.0f);
            if (clamped >= 0.0f)
                clamped += 0.5f;
            else
                clamped -= 0.5f;
            return static_cast<int16_t>(clamped);
        }

        MESH_PURE MESH_FORCE_INLINE Vector3 decodePosition(const Vertex &v) const
        {
            return Vector3(static_cast<float>(v.px) * qScale,
                           static_cast<float>(v.py) * qScale,
                           static_cast<float>(v.pz) * qScale);
        }

        MESH_HOT_PATH MESH_FORCE_INLINE uint16_t addVertex(const Vector3 &pos)
        {
            if (unlikely(isStaticStorage))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES,
                     "Mesh::addVertex called on static-storage mesh");
                return 0xFFFF;
            }
            if (unlikely(!vertices || vertexCount >= maxVertices))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES,
                     "Mesh::addVertex failed: vertices=%p, vertexCount=%u, maxVertices=%u",
                     static_cast<void *>(vertices),
                     static_cast<unsigned int>(vertexCount),
                     static_cast<unsigned int>(maxVertices));
                return 0xFFFF;
            }

            Vertex &v = vertices[vertexCount];
            v.px = quantizeCoord(pos.x);
            v.py = quantizeCoord(pos.y);
            v.pz = quantizeCoord(pos.z);
            v.normal.data = 0;

            cache.boundsValid = false;
            cachedLocalVerticesValid = false;
            return vertexCount++;
        }

        MESH_HOT_PATH MESH_FORCE_INLINE bool addFace(uint16_t v0, uint16_t v1, uint16_t v2)
        {
            if (unlikely(isStaticStorage))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES,
                     "Mesh::addFace called on static-storage mesh");
                return false;
            }
            if (unlikely(!faces || !vertices))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES,
                     "Mesh::addFace failed: vertices=%p, faces=%p",
                     static_cast<void *>(vertices),
                     static_cast<void *>(faces));
                return false;
            }
            if (unlikely(faceCount >= maxFaces ||
                         v0 >= vertexCount ||
                         v1 >= vertexCount ||
                         v2 >= vertexCount))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES,
                     "Mesh::addFace invalid indices or overflow: faceCount=%u, maxFaces=%u, v=(%u,%u,%u), vertexCount=%u",
                     static_cast<unsigned int>(faceCount),
                     static_cast<unsigned int>(maxFaces),
                     static_cast<unsigned int>(v0),
                     static_cast<unsigned int>(v1),
                     static_cast<unsigned int>(v2),
                     static_cast<unsigned int>(vertexCount));
                return false;
            }

            Face &f = faces[faceCount++];
            f.v0 = v0;
            f.v1 = v1;
            f.v2 = v2;
            return true;
        }

        MESH_HOT_PATH void finalizeNormals()
        {
            if (unlikely(isStaticStorage))
            {
                LOGW(::pip3D::Debug::LOG_MODULE_RESOURCES,
                     "Mesh::finalizeNormals skipped for static-storage mesh");
                return;
            }
            if (unlikely(!vertices || !faces))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES,
                     "Mesh::finalizeNormals: missing geometry buffers (vertices=%p, faces=%p, vertexCount=%u, faceCount=%u)",
                     static_cast<void *>(vertices),
                     static_cast<void *>(faces),
                     static_cast<unsigned int>(vertexCount),
                     static_cast<unsigned int>(faceCount));
                return;
            }

            const size_t scratchBytes = static_cast<size_t>(vertexCount) * sizeof(Vector3);
            Vector3 *vertexNormals = static_cast<Vector3 *>(MemUtils::allocData(scratchBytes, 16));
            if (unlikely(!vertexNormals))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES,
                     "Mesh::finalizeNormals: alloc failed (vertexCount=%u)",
                     static_cast<unsigned int>(vertexCount));
                return;
            }
            memset(vertexNormals, 0, scratchBytes);

            for (uint16_t i = 0; i < faceCount; ++i)
            {
                const Face &f = faces[i];
                if (f.v0 >= vertexCount || f.v1 >= vertexCount || f.v2 >= vertexCount)
                    continue;

                Vector3 v0 = decodePosition(vertices[f.v0]);
                Vector3 v1 = decodePosition(vertices[f.v1]);
                Vector3 v2 = decodePosition(vertices[f.v2]);

                Vector3 edge1 = v1 - v0;
                Vector3 edge2 = v2 - v0;
                Vector3 faceNormal = edge1.cross(edge2);

                vertexNormals[f.v0] += faceNormal;
                vertexNormals[f.v1] += faceNormal;
                vertexNormals[f.v2] += faceNormal;
            }

            for (uint16_t i = 0; i < vertexCount; ++i)
            {
                vertexNormals[i].normalize();
                vertices[i].normal.set(vertexNormals[i]);
            }

            MemUtils::freeData(vertexNormals);
        }

        MESH_FORCE_INLINE void finalizeGeometry(uint16_t vCount, uint16_t fCount,
                                                const Vector3 &boundCenter, float boundRadius)
        {
            vertexCount = vCount;
            faceCount = fCount;
            cache.boundingCenter = boundCenter;
            cache.boundingRadius = boundRadius;
            cache.boundsValid = true;
        }

        MESH_HOT_PATH void calculateBoundingSphere()
        {
            if (unlikely(!vertices || vertexCount == 0))
            {
                cache.boundingCenter = Vector3(0, 0, 0);
                cache.boundingRadius = 0;
                cache.boundsValid = true;
                return;
            }

            Vector3 center(0, 0, 0);
            const Vector3 *localVerts = nullptr;
            if (ensureDecodedVertexCache())
                localVerts = getCachedLocalVertices();

            for (uint16_t i = 0; i < vertexCount; ++i)
                center += localVerts ? localVerts[i] : decodePosition(vertices[i]);

            const float invCount = 1.0f / static_cast<float>(vertexCount);
            center *= invCount;

            float maxDistSq = 0;
            for (uint16_t i = 0; i < vertexCount; ++i)
            {
                Vector3 diff = (localVerts ? localVerts[i] : decodePosition(vertices[i])) - center;
                float distSq = diff.lengthSquared();
                if (distSq > maxDistSq)
                    maxDistSq = distSq;
            }

            cache.boundingCenter = center;
            cache.boundingRadius = sqrtf(maxDistSq);
            cache.boundsValid = true;
        }

        bool ensureDecodedVertexCache() const
        {
            if (vertexCount == 0)
                return false;

            if (!cachedLocalVertices || cachedLocalVertexCapacity < vertexCount)
            {
                if (cachedLocalVertices)
                {
                    MemUtils::freeData(cachedLocalVertices);
                    cachedLocalVertices = nullptr;
                }

                cachedLocalVertices = static_cast<Vector3 *>(
                    MemUtils::allocData(static_cast<size_t>(vertexCount) * sizeof(Vector3), 16));
                if (!cachedLocalVertices)
                {
                    cachedLocalVertexCapacity = 0;
                    cachedLocalVerticesValid = false;
                    return false;
                }
                cachedLocalVertexCapacity = vertexCount;
                cachedLocalVerticesValid = false;
            }

            if (!cachedLocalVerticesValid)
            {
                for (uint16_t i = 0; i < vertexCount; ++i)
                    cachedLocalVertices[i] = decodePosition(vertices[i]);
                cachedLocalVerticesValid = true;
            }
            return true;
        }

        MESH_FORCE_INLINE const Vector3 *getCachedLocalVertices() const { return cachedLocalVertices; }

        MESH_PURE MESH_FORCE_INLINE uint16_t numFaces() const { return faceCount; }
        MESH_PURE MESH_FORCE_INLINE uint16_t numVertices() const { return vertexCount; }
        MESH_PURE MESH_FORCE_INLINE const Face &face(uint16_t i) const { return faces[i]; }
        MESH_PURE MESH_FORCE_INLINE const Vertex &vert(uint16_t i) const { return vertices[i]; }

        MESH_HOT_PATH Vector3 center() const
        {
            if (unlikely(!cache.boundsValid))
                const_cast<Mesh *>(this)->calculateBoundingSphere();
            return cache.boundingCenter;
        }
        MESH_HOT_PATH float radius() const
        {
            if (unlikely(!cache.boundsValid))
                const_cast<Mesh *>(this)->calculateBoundingSphere();
            return cache.boundingRadius;
        }

        MESH_HOT_PATH Vector3 vertex(uint16_t index) const
        {
            if (unlikely(index >= vertexCount))
            {
                LOGW(::pip3D::Debug::LOG_MODULE_SCENE,
                     "Mesh::vertex index out of range (index=%u, vertexCount=%u)",
                     static_cast<unsigned int>(index),
                     static_cast<unsigned int>(vertexCount));
                return Vector3();
            }
            const Vector3 *localVerts = nullptr;
            if (ensureDecodedVertexCache())
                localVerts = getCachedLocalVertices();
            return localVerts ? localVerts[index] : decodePosition(vertices[index]);
        }

        MESH_HOT_PATH Vector3 normal(uint16_t index) const
        {
            if (unlikely(index >= vertexCount))
            {
                LOGW(::pip3D::Debug::LOG_MODULE_SCENE,
                     "Mesh::normal index out of range (index=%u, vertexCount=%u)",
                     static_cast<unsigned int>(index),
                     static_cast<unsigned int>(vertexCount));
                return Vector3(0.0f, 1.0f, 0.0f);
            }
            return vertices[index].normal.get();
        }

        MESH_HOT_PATH MESH_FORCE_INLINE void clear()
        {
            vertexCount = faceCount = 0;
            cache.boundsValid = false;
            cachedLocalVerticesValid = false;
        }

        MESH_PURE MESH_FORCE_INLINE Color color() const { return defaultColor; }
        MESH_PURE MESH_FORCE_INLINE bool getCastShadows() const { return castShadows; }
        MESH_FORCE_INLINE void setCastShadows(bool e) { castShadows = e; }
        MESH_PURE MESH_FORCE_INLINE bool getSingleColorLighting() const { return singleColorLighting; }
        MESH_FORCE_INLINE void setSingleColorLighting(bool e) { singleColorLighting = e; }
        MESH_PURE MESH_FORCE_INLINE const Texture *getTexture() const { return meshTexture; }
        MESH_FORCE_INLINE void setTexture(const Texture *t) { meshTexture = t; }
        MESH_PURE MESH_FORCE_INLINE bool isTextured() const { return meshTexture != nullptr; }
        MESH_FORCE_INLINE void setShadowProxy(Mesh *p) { shadowProxy = p; }
        MESH_PURE MESH_FORCE_INLINE Mesh *getShadowProxy() const { return shadowProxy; }
    };

}
