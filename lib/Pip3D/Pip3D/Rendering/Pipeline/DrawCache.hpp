#pragma once

#include "Core/Memory.hpp"
#include "Core/Platform.hpp"

namespace pip3D
{

    class Vector3;

    class DrawCache
    {
    public:
        DrawCache() noexcept = default;
        ~DrawCache() noexcept;

        DrawCache(DrawCache &&other) noexcept
            : worldVerts_(other.worldVerts_),
              screenVerts_(other.screenVerts_),
              shadowVerts_(other.shadowVerts_),
              capacity_(other.capacity_),
              screenVertsFrameStamp_(other.screenVertsFrameStamp_),
              shadowGen_(other.shadowGen_),
              cachedTransformVersion_(other.cachedTransformVersion_)
        {
            other.worldVerts_ = nullptr;
            other.screenVerts_ = nullptr;
            other.shadowVerts_ = nullptr;
            other.capacity_ = 0;
            other.cachedTransformVersion_ = 0;
        }

        DrawCache &operator=(DrawCache &&other) noexcept
        {
            if (this != &other)
            {

                safeFree(worldVerts_);
                screenVerts_ = nullptr;
                safeFree(shadowVerts_);

                worldVerts_ = other.worldVerts_;
                screenVerts_ = other.screenVerts_;
                shadowVerts_ = other.shadowVerts_;
                capacity_ = other.capacity_;
                screenVertsFrameStamp_ = other.screenVertsFrameStamp_;
                shadowGen_ = other.shadowGen_;
                cachedTransformVersion_ = other.cachedTransformVersion_;

                other.worldVerts_ = nullptr;
                other.screenVerts_ = nullptr;
                other.shadowVerts_ = nullptr;
                other.capacity_ = 0;
                other.cachedTransformVersion_ = 0;
            }
            return *this;
        }

        DrawCache(const DrawCache &) = delete;
        DrawCache &operator=(const DrawCache &) = delete;

        PIP3D_HOT bool ensureProjectionCapacity(uint16_t required) noexcept;

        enum class ProjState : uint8_t
        {
            Cached = 0,
            NeedsReproject = 1,
            NeedsTransformAndProject = 2
        };

        PIP3D_HOT ProjState beginProjection(uint32_t frameStamp,
                                            uint32_t instanceVersion) const noexcept
        {

            if (cachedTransformVersion_ != instanceVersion)
                return ProjState::NeedsTransformAndProject;

            if (screenVertsFrameStamp_ != frameStamp)
                return ProjState::NeedsReproject;

            return ProjState::Cached;
        }

        PIP3D_HOT void commitProjection(uint32_t frameStamp,
                                        uint32_t instanceVersion) noexcept
        {
            cachedTransformVersion_ = instanceVersion;
            screenVertsFrameStamp_ = frameStamp;
        }

        PIP3D_FORCE_INLINE Vector3 *worldVerts() noexcept { return worldVerts_; }
        PIP3D_FORCE_INLINE const Vector3 *worldVerts() const noexcept { return worldVerts_; }

        PIP3D_FORCE_INLINE Vector3 *screenVerts() noexcept { return screenVerts_; }
        PIP3D_FORCE_INLINE const Vector3 *screenVerts() const noexcept { return screenVerts_; }

        PIP3D_HOT Vector3 *acquireShadowVerts(uint32_t gen, uint16_t count,
                                              bool &needsCompute) noexcept;

        PIP3D_HOT void commitShadowVerts(uint32_t gen) noexcept { shadowGen_ = gen; }

    private:
        PIP3D_FORCE_INLINE static void safeFree(Vector3 *&ptr) noexcept
        {
            if (ptr)
            {
                MemUtils::freeData(ptr);
                ptr = nullptr;
            }
        }

        PIP3D_FORCE_INLINE void freeProjectionBuffer() noexcept
        {
            safeFree(worldVerts_);
            screenVerts_ = nullptr;
            capacity_ = 0;
        }

    private:
        Vector3 *worldVerts_ = nullptr;
        Vector3 *screenVerts_ = nullptr;
        Vector3 *shadowVerts_ = nullptr;
        uint16_t capacity_ = 0;
        uint16_t padding_ = 0;
        uint32_t screenVertsFrameStamp_ = 0;
        uint32_t shadowGen_ = 0;
        uint32_t cachedTransformVersion_ = 0;
    };

}
