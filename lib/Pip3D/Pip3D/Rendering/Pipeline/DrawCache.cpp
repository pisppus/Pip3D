#include "Rendering/Pipeline/DrawCache.hpp"
#include "Math/Algebra.hpp"

namespace pip3D
{
    DrawCache::~DrawCache() noexcept
    {
        safeFree(worldVerts_);
        screenVerts_ = nullptr;
        safeFree(shadowVerts_);
    }

    bool DrawCache::ensureProjectionCapacity(uint16_t required) noexcept
    {
        if (required == 0)
            return false;

        if (likely(capacity_ >= required && worldVerts_))
            return true;

        const size_t vertsBytes = static_cast<size_t>(required) * sizeof(Vector3);
        Vector3 *block = static_cast<Vector3 *>(
            MemUtils::allocData(2 * vertsBytes, 16));

        if (unlikely(!block))
        {
            freeProjectionBuffer();
            cachedTransformVersion_ = 0;
            return false;
        }

        safeFree(worldVerts_);

        worldVerts_ = block;
        screenVerts_ = block + required;
        capacity_ = required;

        cachedTransformVersion_ = 0;

        return true;
    }

    Vector3 *DrawCache::acquireShadowVerts(uint32_t gen, uint16_t count,
                                           bool &needsCompute) noexcept
    {

        if (shadowGen_ == gen && capacity_ >= count && shadowVerts_)
        {
            needsCompute = false;
            return shadowVerts_;
        }

        if (capacity_ < count || !shadowVerts_)
        {
            safeFree(shadowVerts_);

            const size_t bytes = static_cast<size_t>(count) * sizeof(Vector3);
            shadowVerts_ = static_cast<Vector3 *>(
                MemUtils::allocData(bytes, 16));

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
}
