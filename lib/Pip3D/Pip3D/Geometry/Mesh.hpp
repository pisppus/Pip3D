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
        Matrix4x4 transform;
        float maxScale;
        uint32_t transformHash;
        bool transformValid;
        bool boundsValid;

        MESH_FORCE_INLINE MeshCache() : boundingRadius(0), maxScale(1), transformHash(0),
                                        transformValid(false), boundsValid(false) {}
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

        Vector3 position;
        Vector3 rotation;
        Vector3 scale;

        Color meshColor;
        bool visible;
        bool castShadows;
        bool blobShadow;
        mutable bool transformDirty;

        bool singleColorLighting;
        bool isStaticStorage;
        float qScale;
        const Texture *meshTexture = nullptr;

        Mesh *shadowProxy = nullptr;

        mutable MeshCache cache;
        mutable Vector3 *cachedLocalVertices;
        mutable uint16_t cachedLocalVertexCapacity;
        mutable bool cachedLocalVerticesValid;
        mutable Vector3 *cachedWorldVertices;
        mutable Vector3 *cachedScreenVertices;
        mutable uint16_t cachedProjectionCapacity;
        mutable uint32_t cachedProjectionFrameStamp;

        mutable Vector3 *cachedShadowVerts;
        mutable uint16_t cachedShadowVertCapacity;
        mutable uint32_t cachedShadowGen;

    public:
        MESH_HOT_PATH Mesh(uint16_t maxVerts = 64, uint16_t maxFcs = 128, const Color &color = Color::WHITE)
            : vertexCount(0), faceCount(0), maxVertices(maxVerts), maxFaces(maxFcs),
              position(0, 0, 0), rotation(0, 0, 0), scale(1, 1, 1),
              meshColor(color), visible(true), castShadows(true), blobShadow(false), transformDirty(true),
              isStaticStorage(false), qScale(1.0f),
              cachedLocalVertices(nullptr), singleColorLighting(false), cachedLocalVertexCapacity(0),
              cachedLocalVerticesValid(false), cachedWorldVertices(nullptr),
              cachedScreenVertices(nullptr), cachedProjectionCapacity(0), cachedProjectionFrameStamp(0),
              cachedShadowVerts(nullptr), cachedShadowVertCapacity(0), cachedShadowGen(0)
        {

            const size_t vertexSize = maxVertices * sizeof(Vertex);
            const size_t faceSize = maxFaces * sizeof(Face);

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

            cache.transform.identity();
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
              position(0, 0, 0), rotation(0, 0, 0), scale(1, 1, 1),
              meshColor(color), visible(true), castShadows(true), blobShadow(false), transformDirty(true),
              isStaticStorage(staticStorage), qScale(1.0f), singleColorLighting(false),
              cachedLocalVertices(nullptr), cachedLocalVertexCapacity(0), cachedLocalVerticesValid(false),
              cachedWorldVertices(nullptr), cachedScreenVertices(nullptr),
              cachedProjectionCapacity(0), cachedProjectionFrameStamp(0),
              cachedShadowVerts(nullptr), cachedShadowVertCapacity(0), cachedShadowGen(0)
        {
            cache.transform.identity();
        }

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
            {
                return 0;
            }
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
            return Vector3(
                static_cast<float>(v.px) * qScale,
                static_cast<float>(v.py) * qScale,
                static_cast<float>(v.pz) * qScale);
        }

        MESH_COLD_PATH void cleanup()
        {
            if (vertices && !isStaticStorage)
            {
                MemUtils::freeData(vertices);
                vertices = nullptr;
            }
            if (faces && !isStaticStorage)
            {
                MemUtils::freeData(faces);
                faces = nullptr;
            }
            if (cachedLocalVertices)
            {
                MemUtils::freeData(cachedLocalVertices);
                cachedLocalVertices = nullptr;
            }
            if (cachedWorldVertices)
            {
                MemUtils::freeData(cachedWorldVertices);
                cachedWorldVertices = nullptr;
            }
            if (cachedScreenVertices)
            {
                MemUtils::freeData(cachedScreenVertices);
                cachedScreenVertices = nullptr;
            }
            if (cachedShadowVerts)
            {
                MemUtils::freeData(cachedShadowVerts);
                cachedShadowVerts = nullptr;
            }
            vertexCount = 0;
            faceCount = 0;
            maxVertices = 0;
            maxFaces = 0;
            cache.boundsValid = false;
            cachedLocalVertexCapacity = 0;
            cachedLocalVerticesValid = false;
            cachedProjectionCapacity = 0;
            cachedProjectionFrameStamp = 0;
            cachedShadowVertCapacity = 0;
            cachedShadowGen = 0;
        }

        MESH_COLD_PATH virtual ~Mesh()
        {
            cleanup();
        }

        Mesh(const Mesh &) = delete;
        Mesh &operator=(const Mesh &) = delete;

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
            if (unlikely(vertexCount == 0))
            {
                return;
            }

            const size_t normalSize = vertexCount * sizeof(Vector3);
            Vector3 *vertexNormals = (Vector3 *)pip3D::MemUtils::allocAligned(normalSize, 16, pipcore::AllocCaps::PreferInternal);

            if (unlikely(!vertexNormals))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES,
                     "Mesh::finalizeNormals: failed to allocate %u bytes for temporary normals (vertexCount=%u)",
                     static_cast<unsigned int>(normalSize),
                     static_cast<unsigned int>(vertexCount));
                return;
            }

            const Vertex *__restrict vertPtr = vertices;
            const Face *__restrict facePtr = faces;
            const Vector3 *vertexPositions = nullptr;
            if (ensureDecodedVertexCache())
            {
                for (uint16_t i = 0; i < vertexCount; i++)
                {
                    vertexNormals[i] = Vector3(0.0f, 0.0f, 0.0f);
                }
                vertexPositions = getCachedLocalVertices();

                for (uint16_t f = 0; f < faceCount; f++)
                {
                    const Face &face = facePtr[f];

                    const Vector3 &v0 = vertexPositions[face.v0];
                    const Vector3 &v1 = vertexPositions[face.v1];
                    const Vector3 &v2 = vertexPositions[face.v2];

                    Vector3 edge1 = v1 - v0;
                    Vector3 edge2 = v2 - v0;
                    Vector3 normal = edge1.cross(edge2);

                    float lenSq = normal.lengthSquared();
                    if (likely(lenSq > EPSILON_SQ))
                    {
                        vertexNormals[face.v0] += normal;
                        vertexNormals[face.v1] += normal;
                        vertexNormals[face.v2] += normal;
                    }
                }
            }
            else
            {
                memset(vertexNormals, 0, normalSize);

                for (uint16_t f = 0; f < faceCount; f++)
                {
                    const Face &face = facePtr[f];

                    Vector3 v0 = decodePosition(vertPtr[face.v0]);
                    Vector3 v1 = decodePosition(vertPtr[face.v1]);
                    Vector3 v2 = decodePosition(vertPtr[face.v2]);

                    Vector3 edge1 = v1 - v0;
                    Vector3 edge2 = v2 - v0;
                    Vector3 normal = edge1.cross(edge2);

                    float lenSq = normal.lengthSquared();
                    if (likely(lenSq > EPSILON_SQ))
                    {
                        vertexNormals[face.v0] += normal;
                        vertexNormals[face.v1] += normal;
                        vertexNormals[face.v2] += normal;
                    }
                }
            }

            for (uint16_t i = 0; i < vertexCount; i++)
            {
                vertexNormals[i].normalize();
                vertices[i].normal.set(vertexNormals[i]);
            }

            pip3D::MemUtils::freeAligned(vertexNormals);
        }

        MESH_HOT_PATH MESH_FORCE_INLINE void setPosition(float x, float y, float z)
        {
            position.x = x;
            position.y = y;
            position.z = z;
            invalidateTransform();
        }

        MESH_HOT_PATH MESH_FORCE_INLINE void setRotation(float x, float y, float z)
        {
            rotation.x = x;
            rotation.y = y;
            rotation.z = z;
            invalidateTransform();
        }

        MESH_HOT_PATH MESH_FORCE_INLINE void setScale(float x, float y, float z)
        {
            scale.x = x;
            scale.y = y;
            scale.z = z;
            invalidateTransform();
        }

        MESH_HOT_PATH MESH_FORCE_INLINE void rotate(float x, float y, float z)
        {
            rotation.x += x;
            rotation.y += y;
            rotation.z += z;
            invalidateTransform();
        }

        MESH_HOT_PATH MESH_FORCE_INLINE void translate(float x, float y, float z)
        {
            position.x += x;
            position.y += y;
            position.z += z;
            invalidateTransform();
        }

        MESH_FORCE_INLINE void invalidateTransform()
        {
            transformDirty = true;
            cache.transformValid = false;
        }

        MESH_FORCE_INLINE void finalizeTransform()
        {
            cache.transform.identity();
            cache.maxScale = 1.0f;
            cache.transformValid = true;
            transformDirty = false;
            cache.transformHash = computeTransformHash();
        }

        MESH_FORCE_INLINE void finalizeGeometry(uint16_t vCount, uint16_t fCount, const Vector3 &boundCenter, float boundRadius)
        {
            vertexCount = vCount;
            faceCount = fCount;
            cache.boundingCenter = boundCenter;
            cache.boundingRadius = boundRadius;
            cache.boundsValid = true;
            finalizeTransform();
        }

        MESH_PURE MESH_FORCE_INLINE Vector3 pos() const { return position; }
        MESH_PURE MESH_FORCE_INLINE Vector3 rot() const { return rotation; }

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

            for (uint16_t i = 0; i < vertexCount; i++)
            {
                center += localVerts ? localVerts[i] : decodePosition(vertices[i]);
            }

            float invCount = 1.0f / vertexCount;
            center *= invCount;

            float maxDistSq = 0;
            for (uint16_t i = 0; i < vertexCount; i++)
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

        MESH_HOT_PATH void updateTransform() const
        {
            if (likely(!transformDirty && cache.transformValid))
                return;

            uint32_t newHash = computeTransformHash();
            if (cache.transformHash == newHash && cache.transformValid)
                return;

            float radX = rotation.x * kDegToRad;
            float radY = rotation.y * kDegToRad;
            float radZ = rotation.z * kDegToRad;

            float cx = FastMath::fastCos(radX), sx = FastMath::fastSin(radX);
            float cy = FastMath::fastCos(radY), sy = FastMath::fastSin(radY);
            float cz = FastMath::fastCos(radZ), sz = FastMath::fastSin(radZ);

            cache.transform.identity();

            const float scaleX = scale.x;
            const float scaleY = scale.y;
            const float scaleZ = scale.z;

            cache.transform.m[0] = cy * cz * scaleX;
            cache.transform.m[1] = cy * sz * scaleX;
            cache.transform.m[2] = -sy * scaleX;

            cache.transform.m[4] = (sx * sy * cz - cx * sz) * scaleY;
            cache.transform.m[5] = (sx * sy * sz + cx * cz) * scaleY;
            cache.transform.m[6] = sx * cy * scaleY;

            cache.transform.m[8] = (cx * sy * cz + sx * sz) * scaleZ;
            cache.transform.m[9] = (cx * sy * sz - sx * cz) * scaleZ;
            cache.transform.m[10] = cx * cy * scaleZ;

            cache.transform.m[12] = position.x;
            cache.transform.m[13] = position.y;
            cache.transform.m[14] = position.z;

            cache.maxScale = fmaxf(fmaxf(scaleX, scaleY), scaleZ);
            cache.transformHash = newHash;
            cache.transformValid = true;

            transformDirty = false;
        }

        MESH_PURE MESH_FORCE_INLINE uint32_t computeTransformHash() const
        {
            uint32_t hash = 2166136261u;
            const float *data = &position.x;
            for (int i = 0; i < 9; i++)
            {
                uint32_t val;
                memcpy(&val, &data[i], sizeof(uint32_t));
                hash ^= val;
                hash *= 16777619u;
            }
            return hash;
        }

        MESH_HOT_PATH Vector3 center() const
        {
            updateTransform();
            if (unlikely(!cache.boundsValid))
            {
                const_cast<Mesh *>(this)->calculateBoundingSphere();
            }
            return cache.transform.transformNoDiv(cache.boundingCenter);
        }

        MESH_HOT_PATH float radius() const
        {
            updateTransform();
            if (unlikely(!cache.boundsValid))
            {
                const_cast<Mesh *>(this)->calculateBoundingSphere();
            }
            return cache.boundingRadius * cache.maxScale * 1.2f;
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
            updateTransform();
            const Vector3 *localVerts = nullptr;
            if (ensureDecodedVertexCache())
                localVerts = getCachedLocalVertices();
            Vector3 local = localVerts ? localVerts[index] : decodePosition(vertices[index]);
            return cache.transform.transformNoDiv(local);
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
            updateTransform();
            Vector3 n = vertices[index].normal.get();
            return cache.transform.transformNormal(n);
        }

        MESH_HOT_PATH MESH_FORCE_INLINE void clear()
        {
            vertexCount = faceCount = 0;
            cache.boundsValid = false;
            cachedLocalVerticesValid = false;
        }

        MESH_PURE MESH_FORCE_INLINE uint16_t numFaces() const { return faceCount; }
        MESH_PURE MESH_FORCE_INLINE uint16_t numVertices() const { return vertexCount; }
        MESH_PURE MESH_FORCE_INLINE const Face &face(uint16_t i) const { return faces[i]; }
        MESH_PURE MESH_FORCE_INLINE const Vertex &vert(uint16_t i) const { return vertices[i]; }
        MESH_PURE MESH_FORCE_INLINE Color color() const { return meshColor; }
        MESH_FORCE_INLINE void color(const Color &c) { meshColor = c; }
        MESH_FORCE_INLINE void show() { visible = true; }
        MESH_FORCE_INLINE void hide() { visible = false; }
        MESH_PURE MESH_FORCE_INLINE bool isVisible() const { return visible; }
        MESH_FORCE_INLINE void setCastShadows(bool enabled) { castShadows = enabled; }
        MESH_PURE MESH_FORCE_INLINE bool getCastShadows() const { return castShadows; }
        MESH_FORCE_INLINE void setBlobShadow(bool enabled) { blobShadow = enabled; if (enabled) castShadows = false; }
        MESH_PURE MESH_FORCE_INLINE bool getBlobShadow() const { return blobShadow; }
        MESH_FORCE_INLINE Vector3 *getCachedShadowVerts(uint32_t expectedGen, uint16_t expectedCount) const
        {
            if (cachedShadowGen != expectedGen || cachedShadowVertCapacity < expectedCount)
                return nullptr;
            return cachedShadowVerts;
        }
        Vector3 *ensureShadowVertCapacity(uint16_t count) const
        {
            if (cachedShadowVertCapacity >= count)
                return cachedShadowVerts;
            if (cachedShadowVerts)
                MemUtils::freeData(cachedShadowVerts);
            cachedShadowVerts = (Vector3 *)MemUtils::allocData(count * sizeof(Vector3), 16);
            cachedShadowVertCapacity = cachedShadowVerts ? count : 0;
            return cachedShadowVerts;
        }
        MESH_FORCE_INLINE void storeCachedShadowVerts(uint32_t gen) const { cachedShadowGen = gen; }
        MESH_FORCE_INLINE void invalidateShadowCache() const { cachedShadowGen = 0; }
        MESH_FORCE_INLINE void setSingleColorLighting(bool enabled) { singleColorLighting = enabled; }
        MESH_PURE MESH_FORCE_INLINE bool getSingleColorLighting() const { return singleColorLighting; }
        MESH_FORCE_INLINE void setTexture(const Texture *tex) { meshTexture = tex; }
        MESH_PURE MESH_FORCE_INLINE const Texture *getTexture() const { return meshTexture; }
        MESH_PURE MESH_FORCE_INLINE bool isTextured() const { return meshTexture != nullptr; }
        MESH_FORCE_INLINE void setShadowProxy(Mesh *proxy) { shadowProxy = proxy; }
        MESH_PURE MESH_FORCE_INLINE Mesh* getShadowProxy() const { return shadowProxy; }

        MESH_HOT_PATH const Matrix4x4 &getTransform() const
        {
            updateTransform();
            return cache.transform;
        }

        bool ensureProjectionCache(uint16_t required) const
        {
            if (required == 0)
                return false;

            if (cachedProjectionCapacity >= required && cachedWorldVertices && cachedScreenVertices)
                return true;

            if (cachedWorldVertices)
            {
                MemUtils::freeData(cachedWorldVertices);
                cachedWorldVertices = nullptr;
            }
            if (cachedScreenVertices)
            {
                MemUtils::freeData(cachedScreenVertices);
                cachedScreenVertices = nullptr;
            }

            cachedWorldVertices = static_cast<Vector3 *>(MemUtils::allocData(static_cast<size_t>(required) * sizeof(Vector3), 16));
            cachedScreenVertices = static_cast<Vector3 *>(MemUtils::allocData(static_cast<size_t>(required) * sizeof(Vector3), 16));

            if (!cachedWorldVertices || !cachedScreenVertices)
            {
                if (cachedWorldVertices)
                {
                    MemUtils::freeData(cachedWorldVertices);
                    cachedWorldVertices = nullptr;
                }
                if (cachedScreenVertices)
                {
                    MemUtils::freeData(cachedScreenVertices);
                    cachedScreenVertices = nullptr;
                }
                cachedProjectionCapacity = 0;
                return false;
            }

            cachedProjectionCapacity = required;
            cachedProjectionFrameStamp = 0;
            return true;
        }

        bool ensureDecodedVertexCache() const
        {
            if (vertexCount == 0)
                return false;

            if ((!cachedLocalVertices || cachedLocalVertexCapacity < vertexCount))
            {
                if (cachedLocalVertices)
                {
                    MemUtils::freeData(cachedLocalVertices);
                    cachedLocalVertices = nullptr;
                }

                cachedLocalVertices = static_cast<Vector3 *>(MemUtils::allocData(static_cast<size_t>(vertexCount) * sizeof(Vector3), 16));
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
                {
                    cachedLocalVertices[i] = decodePosition(vertices[i]);
                }
                cachedLocalVerticesValid = true;
            }

            return true;
        }

        MESH_FORCE_INLINE const Vector3 *getCachedLocalVertices() const { return cachedLocalVertices; }
        MESH_FORCE_INLINE Vector3 *getCachedWorldVertices() const { return cachedWorldVertices; }
        MESH_FORCE_INLINE Vector3 *getCachedScreenVertices() const { return cachedScreenVertices; }
        MESH_FORCE_INLINE uint32_t getCachedProjectionFrameStamp() const { return cachedProjectionFrameStamp; }
        MESH_FORCE_INLINE void setCachedProjectionFrameStamp(uint32_t stamp) const { cachedProjectionFrameStamp = stamp; }
    };

}
