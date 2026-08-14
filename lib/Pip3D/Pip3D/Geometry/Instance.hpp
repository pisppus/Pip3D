#pragma once

#include "Core/Color.hpp"
#include "Math/Algebra.hpp"
#include "Geometry/Mesh.hpp"
#include "Rendering/Pipeline/DrawCache.hpp"
#include "Rendering/Pipeline/Shading.hpp"

namespace pip3D
{
    class MeshInstance
    {
    public:
        explicit MeshInstance(Mesh *mesh = nullptr)
            : sourceMesh(mesh),
              instanceColor(Color::WHITE),
              cacheFlags(kFlagTransformDirty | kFlagBoundsDirty),
              stateFlags(kFlagVisible),
              position(0.0f, 0.0f, 0.0f),
              transformVersion(1),
              cachedWorldRadius(0.0f),
              cachedMaxAbsScale(1.0f),
              cachedWorldCenter(0.0f, 0.0f, 0.0f),
              scale(1.0f, 1.0f, 1.0f),
              rotation(0.0f, 0.0f, 0.0f, 1.0f),
              rotationValid(true),
              shadingOverride_(-1),
              emissiveColor_(Color::WHITE),
              emissiveIntensity_(1.0f),
              emissiveShape_(255),
              emissiveBoxSize_(0.0f, 0.0f, 0.0f)
        {
            localTransform.reset();
        }

        ~MeshInstance() = default;
        MeshInstance(const MeshInstance &) = delete;
        MeshInstance &operator=(const MeshInstance &) = delete;
        MeshInstance(MeshInstance &&) = delete;
        MeshInstance &operator=(MeshInstance &&) = delete;

        void setMesh(Mesh *mesh)
        {
            sourceMesh = mesh;
            invalidateTransform();
        }
        PIP3D_FORCE_INLINE Mesh *getMesh() const { return sourceMesh; }
        PIP3D_FORCE_INLINE void setColor(const Color &c) { instanceColor = c; }
        PIP3D_FORCE_INLINE Color color() const { return instanceColor; }

        PIP3D_FORCE_INLINE void setVisible(bool v)
        {
            if (v)
                stateFlags |= kFlagVisible;
            else
                stateFlags &= ~kFlagVisible;
        }
        PIP3D_FORCE_INLINE bool isVisible() const
        {
            return (stateFlags & kFlagVisible) != 0 && sourceMesh;
        }

        PIP3D_FORCE_INLINE void setBlobShadow(bool enabled)
        {
            if (enabled)
                stateFlags |= kFlagBlobShadow;
            else
                stateFlags &= ~kFlagBlobShadow;
        }
        PIP3D_FORCE_INLINE bool getBlobShadow() const
        {
            return (stateFlags & kFlagBlobShadow) != 0;
        }

        PIP3D_FORCE_INLINE void setEmissive(bool enabled,
                                            Color glowColor = Color::WHITE,
                                            float intensity = 1.2f,
                                            uint8_t shapeMode = 255)
        {
            if (enabled)
                stateFlags |= kFlagEmissive;
            else
                stateFlags &= ~kFlagEmissive;

            emissiveColor_ = glowColor;
            emissiveIntensity_ = intensity;
            emissiveShape_ = shapeMode;
        }

        PIP3D_FORCE_INLINE void setEmissiveBoxSize(float w, float h, float d = 0.0f)
        {
            emissiveBoxSize_ = Vector3(w, h, d);
        }
        PIP3D_FORCE_INLINE void setEmissiveBoxSize(const Vector3 &whd)
        {
            emissiveBoxSize_ = whd;
        }
        PIP3D_FORCE_INLINE const Vector3 &emissiveBoxSize() const
        {
            return emissiveBoxSize_;
        }

        PIP3D_FORCE_INLINE bool isEmissive() const
        {
            return (stateFlags & kFlagEmissive) != 0 && sourceMesh && isVisible();
        }

        PIP3D_FORCE_INLINE Color emissiveColor() const { return emissiveColor_; }
        PIP3D_FORCE_INLINE float emissiveIntensity() const { return emissiveIntensity_; }
        PIP3D_FORCE_INLINE uint8_t emissiveShape() const { return emissiveShape_; }

        void setPosition(const Vector3 &pos)
        {
            position = pos;
            invalidateTransform();
        }
        PIP3D_FORCE_INLINE void setPosition(float x, float y, float z)
        {
            setPosition(Vector3(x, y, z));
        }
        void setRotation(const Quaternion &rot)
        {
            rotation = rot;
            rotationValid = true;
            invalidateTransform();
        }
        void setEuler(float pitchDeg, float yawDeg, float rollDeg)
        {
            rotation = Quaternion::fromEuler(pitchDeg * kDegToRad,
                                             yawDeg * kDegToRad,
                                             rollDeg * kDegToRad);
            rotationValid = true;
            invalidateTransform();
        }
        void setScale(const Vector3 &scl)
        {
            scale = scl;
            cachedMaxAbsScale = computeMaxAbsScale(scl);
            invalidateTransform();
        }
        PIP3D_FORCE_INLINE void setTransform(const Vector3 &pos,
                                             const Quaternion &rot,
                                             const Vector3 &scl)
        {
            position = pos;
            rotation = rot;
            rotationValid = true;
            scale = scl;
            cachedMaxAbsScale = computeMaxAbsScale(scl);
            invalidateTransform();
        }
        PIP3D_FORCE_INLINE void setTransformPRS(const Vector3 &pos,
                                                float pitchDeg,
                                                float yawDeg,
                                                float rollDeg,
                                                const Vector3 &scl)
        {
            position = pos;
            rotation = Quaternion::fromEuler(pitchDeg * kDegToRad,
                                             yawDeg * kDegToRad,
                                             rollDeg * kDegToRad);
            rotationValid = true;
            scale = scl;
            cachedMaxAbsScale = computeMaxAbsScale(scl);
            invalidateTransform();
        }

        void setWorldMatrix(const Matrix4x4 &m)
        {
            localTransform = m;
            position = Vector3(m.m[12], m.m[13], m.m[14]);

            const float sx = sqrtf(m.m[0] * m.m[0] + m.m[1] * m.m[1] + m.m[2] * m.m[2]);
            const float sy = sqrtf(m.m[4] * m.m[4] + m.m[5] * m.m[5] + m.m[6] * m.m[6]);
            const float sz = sqrtf(m.m[8] * m.m[8] + m.m[9] * m.m[9] + m.m[10] * m.m[10]);
            scale = Vector3(sx, sy, sz);
            cachedMaxAbsScale = fmaxf(fmaxf(sx, sy), sz);

            rotationValid = false;
            cacheFlags = (cacheFlags & ~kFlagTransformDirty) | kFlagBoundsDirty;
            ++transformVersion;
        }

        PIP3D_FORCE_INLINE void setShadingOverride(ShadingMode mode)
        {
            shadingOverride_ = static_cast<int8_t>(mode);
        }
        PIP3D_FORCE_INLINE void clearShadingOverride()
        {
            shadingOverride_ = -1;
        }
        PIP3D_FORCE_INLINE bool hasShadingOverride() const
        {
            return shadingOverride_ >= 0;
        }
        PIP3D_FORCE_INLINE ShadingMode getEffectiveShadingMode(ShadingMode globalMode) const
        {
            return (shadingOverride_ >= 0)
                       ? static_cast<ShadingMode>(shadingOverride_)
                       : globalMode;
        }

        PIP3D_FORCE_INLINE const Vector3 &pos() const { return position; }
        PIP3D_FORCE_INLINE const Vector3 &getScale() const { return scale; }
        PIP3D_FORCE_INLINE const Quaternion &getRotation() const { return rotation; }
        PIP3D_FORCE_INLINE bool isRotationValid() const { return rotationValid; }
        PIP3D_FORCE_INLINE uint32_t version() const { return transformVersion; }
        PIP3D_FORCE_INLINE DrawCache &drawCache() noexcept { return drawCache_; }
        PIP3D_FORCE_INLINE const DrawCache &drawCache() const noexcept { return drawCache_; }

        PIP3D_FORCE_INLINE const Matrix4x4 &transform() const
        {
            if (unlikely(cacheFlags & kFlagTransformDirty))
                updateTransform();
            return localTransform;
        }
        PIP3D_FORCE_INLINE const Vector3 &center() const
        {
            if (unlikely(cacheFlags & kFlagBoundsDirty))
                updateBounds();
            return cachedWorldCenter;
        }
        PIP3D_FORCE_INLINE float radius() const
        {
            if (unlikely(cacheFlags & kFlagBoundsDirty))
                updateBounds();
            return cachedWorldRadius;
        }

    private:
        static constexpr uint8_t kFlagTransformDirty = 0x01;
        static constexpr uint8_t kFlagBoundsDirty = 0x02;
        static constexpr uint8_t kFlagVisible = 0x10;
        static constexpr uint8_t kFlagBlobShadow = 0x20;
        static constexpr uint8_t kFlagEmissive = 0x40;

        Mesh *sourceMesh;
        Color instanceColor;
        mutable uint8_t cacheFlags;
        uint8_t stateFlags;
        Vector3 position;
        uint32_t transformVersion;
        mutable float cachedWorldRadius;
        mutable float cachedMaxAbsScale;
        mutable Vector3 cachedWorldCenter;
        Vector3 scale;
        Quaternion rotation;
        bool rotationValid;
        mutable Matrix4x4 localTransform;
        DrawCache drawCache_;
        int8_t shadingOverride_;

        Color emissiveColor_;
        float emissiveIntensity_;
        uint8_t emissiveShape_;
        Vector3 emissiveBoxSize_;

        PIP3D_FORCE_INLINE void invalidateTransform()
        {
            cacheFlags |= (kFlagTransformDirty | kFlagBoundsDirty);
            ++transformVersion;
        }

        PIP3D_FORCE_INLINE static float computeMaxAbsScale(const Vector3 &scl)
        {
            return fmaxf(fmaxf(fabsf(scl.x), fabsf(scl.y)), fabsf(scl.z));
        }

        PIP3D_HOT PIP3D_FORCE_INLINE void updateTransform() const
        {
            cacheFlags &= ~kFlagTransformDirty;

            const float x = rotation.x, y = rotation.y, z = rotation.z, w = rotation.w;
            const float sx = scale.x, sy = scale.y, sz = scale.z;

            const float qx = x + x, qy = y + y, qz = z + z;
            const float qxx = x * qx, qyy = y * qy, qzz = z * qz;
            const float qxy = x * qy, qxz = x * qz, qyz = y * qz;
            const float qwx = w * qx, qwy = w * qy, qwz = w * qz;

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

        PIP3D_COLD void updateBounds() const
        {
            cacheFlags &= ~kFlagBoundsDirty;

            if (!sourceMesh)
            {
                cachedWorldCenter = position;
                cachedWorldRadius = 0.0f;
                return;
            }

            if (cacheFlags & kFlagTransformDirty)
                updateTransform();

            Vector3 localMeshCenter;
            float localMeshRadius;
            sourceMesh->getBounds(localMeshCenter, localMeshRadius);
            cachedWorldCenter = localTransform.transformNoDiv(localMeshCenter);
            cachedWorldRadius = localMeshRadius * cachedMaxAbsScale;
        }
    };
}
