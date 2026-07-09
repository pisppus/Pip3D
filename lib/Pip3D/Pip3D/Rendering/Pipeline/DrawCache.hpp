#pragma once

#include "Core/Memory.hpp"
#include "Core/Platform.hpp"
#include "Math/Algebra.hpp"

namespace pip3D
{
    class DrawCache
    {
    public:
        constexpr DrawCache() noexcept = default;

        ~DrawCache()
        {
            if (worldVerts_)
                MemUtils::freeData(worldVerts_);
            if (screenVerts_)
                MemUtils::freeData(screenVerts_);
            if (shadowVerts_)
                MemUtils::freeData(shadowVerts_);
        }

        DrawCache(const DrawCache &) = delete;
        DrawCache &operator=(const DrawCache &) = delete;
        DrawCache(DrawCache &&) = delete;
        DrawCache &operator=(DrawCache &&) = delete;

        PIP3D_HOT bool ensureProjectionCapacity(uint16_t required)
        {
            if (required == 0)
                return false;
            if (likely(worldCapacity_ >= required && worldVerts_ && screenVerts_))
                return true;

            if (worldVerts_)
            {
                MemUtils::freeData(worldVerts_);
                worldVerts_ = nullptr;
            }
            if (screenVerts_)
            {
                MemUtils::freeData(screenVerts_);
                screenVerts_ = nullptr;
            }

            worldVerts_ = static_cast<Vector3 *>(
                MemUtils::allocData(static_cast<size_t>(required) * sizeof(Vector3), 16));
            screenVerts_ = static_cast<Vector3 *>(
                MemUtils::allocData(static_cast<size_t>(required) * sizeof(Vector3), 16));

            if (unlikely(!worldVerts_ || !screenVerts_))
            {
                if (worldVerts_)
                {
                    MemUtils::freeData(worldVerts_);
                    worldVerts_ = nullptr;
                }
                if (screenVerts_)
                {
                    MemUtils::freeData(screenVerts_);
                    screenVerts_ = nullptr;
                }
                worldCapacity_ = 0;
                worldVertsValid_ = false;
                return false;
            }

            worldCapacity_ = required;
            worldVertsValid_ = false;
            screenVertsFrameStamp_ = 0;
            return true;
        }

        PIP3D_HOT int beginProjection(uint32_t frameStamp,
                                      uint32_t instanceVersion) const
        {
            if (cachedTransformVersion_ != instanceVersion || !worldVertsValid_)
                return 2;
            if (screenVertsFrameStamp_ != frameStamp)
                return 1;
            return 0;
        }

        PIP3D_HOT void commitProjection(uint32_t frameStamp,
                                        uint32_t instanceVersion)
        {
            worldVertsValid_ = true;
            cachedTransformVersion_ = instanceVersion;
            screenVertsFrameStamp_ = frameStamp;
        }

        PIP3D_FORCE_INLINE Vector3 *worldVerts() const { return worldVerts_; }
        PIP3D_FORCE_INLINE Vector3 *screenVerts() const { return screenVerts_; }

        PIP3D_HOT Vector3 *acquireShadowVerts(uint32_t gen, uint16_t count,
                                              bool &needsCompute)
        {
            if (shadowGen_ == gen && shadowCapacity_ >= count && shadowVerts_)
            {
                needsCompute = false;
                return shadowVerts_;
            }

            if (shadowCapacity_ < count || !shadowVerts_)
            {
                if (shadowVerts_)
                {
                    MemUtils::freeData(shadowVerts_);
                    shadowVerts_ = nullptr;
                }
                shadowVerts_ = static_cast<Vector3 *>(
                    MemUtils::allocData(static_cast<size_t>(count) * sizeof(Vector3), 16));
                shadowCapacity_ = shadowVerts_ ? count : 0;
                if (unlikely(!shadowVerts_))
                {
                    shadowGen_ = 0;
                    needsCompute = true;
                    return nullptr;
                }
            }

            needsCompute = true;
            return shadowVerts_;
        }

        PIP3D_HOT void commitShadowVerts(uint32_t gen) { shadowGen_ = gen; }

    private:
        Vector3 *worldVerts_ = nullptr;
        Vector3 *screenVerts_ = nullptr;
        Vector3 *shadowVerts_ = nullptr;
        uint16_t worldCapacity_ = 0;
        uint16_t shadowCapacity_ = 0;
        uint32_t screenVertsFrameStamp_ = 0;
        uint32_t shadowGen_ = 0;
        uint32_t cachedTransformVersion_ = 0;
        bool worldVertsValid_ = false;
    };
}
