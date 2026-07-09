#pragma once

#include "Core/Color.hpp"
#include "Math/Algebra.hpp"
#include "Geometry/Mesh.hpp"

namespace pip3D
{
    class Renderer;
    class ShadowRenderer;

    class MeshInstance
    {
    public:
        explicit MeshInstance(Mesh *mesh = nullptr)
            : sourceMesh(mesh),
              visible(true),
              blobShadow(false),
              transformDirty(true),
              boundsDirty(true),
              instanceColor(Color::WHITE),
              cachedWorldCenter(0, 0, 0),
              cachedWorldRadius(0.0f),
              localTransform(),
              rotation(),
              position(0, 0, 0),
              scale(1, 1, 1),
              cachedWorldVertices(nullptr),
              cachedScreenVertices(nullptr),
              cachedShadowVerts(nullptr),
              cachedProjectionCapacity(0),
              cachedShadowVertCapacity(0),
              cachedScreenVertsFrameStamp(0),
              cachedShadowGen(0),
              cachedWorldVertsValid(false)
        {
        }

        ~MeshInstance()
        {
            freeSafe(cachedWorldVertices);
            freeSafe(cachedScreenVertices);
            freeSafe(cachedShadowVerts);
        }

        MeshInstance(const MeshInstance &) = delete;
        MeshInstance &operator=(const MeshInstance &) = delete;
        MeshInstance(MeshInstance &&) = delete;
        MeshInstance &operator=(MeshInstance &&) = delete;

        void setMesh(Mesh *mesh)
        {
            sourceMesh = mesh;
            invalidateTransformCaches();
        }

        PIP3D_FORCE_INLINE Mesh *getMesh() const { return sourceMesh; }

        void setPosition(const Vector3 &pos)
        {
            position = pos;
            invalidateTransformCaches();
        }

        PIP3D_FORCE_INLINE void setPosition(float x, float y, float z)
        {
            setPosition(Vector3(x, y, z));
        }

        void setRotation(const Quaternion &rot)
        {
            rotation = rot;
            invalidateTransformCaches();
        }

        void setEuler(float pitchDeg, float yawDeg, float rollDeg)
        {
            rotation = Quaternion::fromEuler(pitchDeg * kDegToRad,
                                             yawDeg * kDegToRad,
                                             rollDeg * kDegToRad);
            invalidateTransformCaches();
        }

        void setScale(const Vector3 &scl)
        {
            scale = scl;
            invalidateTransformCaches();
        }

        PIP3D_FORCE_INLINE void setColor(const Color &c) { instanceColor = c; }
        PIP3D_FORCE_INLINE Color color() const { return instanceColor; }

        PIP3D_FORCE_INLINE void setBlobShadow(bool enabled) { blobShadow = enabled; }
        PIP3D_FORCE_INLINE bool getBlobShadow() const { return blobShadow; }

        PIP3D_FORCE_INLINE void setVisible(bool v) { visible = v; }
        PIP3D_FORCE_INLINE bool isVisible() const { return visible && sourceMesh; }

        PIP3D_FORCE_INLINE const Vector3 &pos() const { return position; }

        PIP3D_FORCE_INLINE const Matrix4x4 &transform()
        {
            updateTransform();
            return localTransform;
        }

        Vector3 center()
        {
            if (unlikely(boundsDirty))
                updateBounds();
            return cachedWorldCenter;
        }

        float radius()
        {
            if (unlikely(boundsDirty))
                updateBounds();
            return cachedWorldRadius;
        }

    private:
        Mesh *sourceMesh;
        bool visible;
        bool blobShadow;
        bool transformDirty;
        bool boundsDirty;
        Color instanceColor;
        mutable Vector3 cachedWorldCenter;
        mutable float cachedWorldRadius;
        Matrix4x4 localTransform;
        Quaternion rotation;
        Vector3 position;
        Vector3 scale;
        mutable Vector3 *cachedWorldVertices;
        mutable Vector3 *cachedScreenVertices;
        mutable Vector3 *cachedShadowVerts;
        mutable uint16_t cachedProjectionCapacity;
        mutable uint16_t cachedShadowVertCapacity;
        mutable uint32_t cachedScreenVertsFrameStamp;
        mutable uint32_t cachedShadowGen;
        mutable bool cachedWorldVertsValid;

        friend class Renderer;
        friend class ShadowRenderer;

        PIP3D_FORCE_INLINE void invalidateTransformCaches()
        {
            transformDirty = true;
            boundsDirty = true;
            cachedWorldVertsValid = false;
            cachedScreenVertsFrameStamp = 0;
            cachedShadowGen = 0;
        }

        PIP3D_FORCE_INLINE static void freeSafe(void *p)
        {
            if (p)
                MemUtils::freeData(p);
        }

        PIP3D_FORCE_INLINE void updateTransform()
        {
            if (likely(!transformDirty))
                return;
            transformDirty = false;

            const float qx2 = rotation.x + rotation.x;
            const float qy2 = rotation.y + rotation.y;
            const float qz2 = rotation.z + rotation.z;
            const float qxx = rotation.x * qx2;
            const float qyy = rotation.y * qy2;
            const float qzz = rotation.z * qz2;
            const float qxy = rotation.x * qy2;
            const float qxz = rotation.x * qz2;
            const float qyz = rotation.y * qz2;
            const float qwx = rotation.w * qx2;
            const float qwy = rotation.w * qy2;
            const float qwz = rotation.w * qz2;

            const float sx = scale.x, sy = scale.y, sz = scale.z;

            float *PIP3D_RESTRICT m = localTransform.m;
            m[0] = (1.0f - qyy - qzz) * sx;
            m[1] = (qxy + qwz) * sx;
            m[2] = (qxz - qwy) * sx;
            m[3] = 0.0f;
            m[4] = (qxy - qwz) * sy;
            m[5] = (1.0f - qxx - qzz) * sy;
            m[6] = (qyz + qwx) * sy;
            m[7] = 0.0f;
            m[8] = (qxz + qwy) * sz;
            m[9] = (qyz - qwx) * sz;
            m[10] = (1.0f - qxx - qyy) * sz;
            m[11] = 0.0f;
            m[12] = position.x;
            m[13] = position.y;
            m[14] = position.z;
            m[15] = 1.0f;
        }

        void updateBounds()
        {
            if (!sourceMesh)
            {
                cachedWorldCenter = position;
                cachedWorldRadius = 0.0f;
                boundsDirty = false;
                return;
            }

            updateTransform();

            const Vector3 localMeshCenter = sourceMesh->center();
            const float localMeshRadius = sourceMesh->radius();
            cachedWorldCenter = localTransform.transformNoDiv(localMeshCenter);

            const float maxAbsScale = fmaxf(fmaxf(fabsf(scale.x), fabsf(scale.y)), fabsf(scale.z));
            cachedWorldRadius = localMeshRadius * maxAbsScale;
            boundsDirty = false;
        }

        bool ensureProjectionCache(uint16_t required) const
        {
            if (required == 0)
                return false;

            if (likely(cachedProjectionCapacity >= required &&
                       cachedWorldVertices && cachedScreenVertices))
                return true;

            freeSafe(cachedWorldVertices);
            cachedWorldVertices = nullptr;
            freeSafe(cachedScreenVertices);
            cachedScreenVertices = nullptr;

            cachedWorldVertices = static_cast<Vector3 *>(
                MemUtils::allocData(static_cast<size_t>(required) * sizeof(Vector3), 16));
            cachedScreenVertices = static_cast<Vector3 *>(
                MemUtils::allocData(static_cast<size_t>(required) * sizeof(Vector3), 16));

            if (!cachedWorldVertices || !cachedScreenVertices)
            {
                freeSafe(cachedWorldVertices);
                cachedWorldVertices = nullptr;
                freeSafe(cachedScreenVertices);
                cachedScreenVertices = nullptr;
                cachedProjectionCapacity = 0;
                return false;
            }

            cachedProjectionCapacity = required;
            cachedWorldVertsValid = false;
            cachedScreenVertsFrameStamp = 0;
            return true;
        }

        PIP3D_FORCE_INLINE Vector3 *getCachedWorldVertices() const { return cachedWorldVertices; }
        PIP3D_FORCE_INLINE bool areWorldVertsValid() const { return cachedWorldVertsValid; }
        PIP3D_FORCE_INLINE void markWorldVertsValid() const { cachedWorldVertsValid = true; }

        PIP3D_FORCE_INLINE Vector3 *getCachedScreenVertices() const { return cachedScreenVertices; }
        PIP3D_FORCE_INLINE uint32_t getCachedScreenVertsFrameStamp() const { return cachedScreenVertsFrameStamp; }
        PIP3D_FORCE_INLINE void setCachedScreenVertsFrameStamp(uint32_t stamp) const { cachedScreenVertsFrameStamp = stamp; }

        PIP3D_FORCE_INLINE Vector3 *getCachedShadowVerts(uint32_t expectedGen, uint16_t expectedCount) const
        {
            if (cachedShadowGen != expectedGen || cachedShadowVertCapacity < expectedCount)
                return nullptr;
            return cachedShadowVerts;
        }

        Vector3 *ensureShadowVertCapacity(uint16_t count) const
        {
            if (likely(cachedShadowVertCapacity >= count))
                return cachedShadowVerts;
            freeSafe(cachedShadowVerts);
            cachedShadowVerts = static_cast<Vector3 *>(
                MemUtils::allocData(static_cast<size_t>(count) * sizeof(Vector3), 16));
            cachedShadowVertCapacity = cachedShadowVerts ? count : 0;
            return cachedShadowVerts;
        }

        PIP3D_FORCE_INLINE void storeCachedShadowVerts(uint32_t gen) const { cachedShadowGen = gen; }
    };
}
