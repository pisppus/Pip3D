#include <cstring>

#include "Rendering/Pipeline/DrawCache.hpp"
#include "Math/Algebra.hpp"

namespace pip3D
{
    DrawCache::~DrawCache() noexcept
    {
        safeFree(storage_);

        if (probeData_)
            MemUtils::freeData(probeData_);
    }

    bool DrawCache::ensureCapacity(uint16_t posRequired, uint16_t attrRequired,
                                   bool withNormals) noexcept
    {
        if (posRequired == 0)
            return false;

        const bool normalsOk = !withNormals || (worldNormals_ && normCapacity_ >= attrRequired);

        if (likely(capacity_ >= posRequired && storage_ && normalsOk))
            return true;

        constexpr size_t kAlign = 16;
        const size_t posBytes = static_cast<size_t>(posRequired) * sizeof(Vector3);
        const size_t alignedPosBytes = (posBytes + kAlign - 1) & ~(kAlign - 1);
        const size_t normBytes = withNormals
                                     ? ((static_cast<size_t>(attrRequired ? attrRequired : posRequired) * sizeof(Vector3) +
                                         kAlign - 1) &
                                        ~(kAlign - 1))
                                     : 0;
        const size_t totalBytes = 2 * alignedPosBytes + normBytes;

        Vector3 *block = static_cast<Vector3 *>(
            MemUtils::allocData(totalBytes, static_cast<uint8_t>(kAlign)));

        if (unlikely(!block))
        {
            safeFree(storage_);
            worldNormals_ = nullptr;
            screenVerts_ = nullptr;
            capacity_ = 0;
            normCapacity_ = 0;
            cachedTransformVersion_ = 0;
            screenVertsFrameStamp_ = 0;
            shadowGen_ = 0;
            shadowVertsValid_ = false;
            probeStateVersion_ = 0;
            return false;
        }

        safeFree(storage_);

        uint8_t *base = reinterpret_cast<uint8_t *>(block);
        storage_ = block;
        screenVerts_ = reinterpret_cast<Vector3 *>(base + alignedPosBytes);
        if (withNormals)
        {
            worldNormals_ = reinterpret_cast<Vector3 *>(base + 2 * alignedPosBytes);
            normCapacity_ = static_cast<uint16_t>(attrRequired ? attrRequired : posRequired);
        }
        else
        {
            worldNormals_ = nullptr;
            normCapacity_ = 0;
        }
        capacity_ = posRequired;
        cachedTransformVersion_ = 0;
        screenVertsFrameStamp_ = 0;
        shadowVertsValid_ = false;
        shadowGen_ = 0;
        probeStateVersion_ = 0;
        return true;
    }

    Vector3 *DrawCache::acquireShadowVerts(uint32_t gen, uint16_t count,
                                           bool &needsCompute) noexcept
    {
        if (!storage_ || capacity_ < count)
        {
            if (!ensureCapacity(count, 0))
            {
                needsCompute = true;
                return nullptr;
            }
        }

        if (shadowVertsValid_ && shadowGen_ == gen)
        {
            needsCompute = false;
            return screenVerts_;
        }

        needsCompute = true;
        return screenVerts_;
    }

    uint8_t *DrawCache::ensureProbePlanes(uint16_t verts) noexcept
    {
        if (probeData_ && probeVerts_ >= verts)
            return probeData_;
        if (probeAllocCooldown_ && --probeAllocCooldown_)
            return nullptr;

        uint8_t *grown = static_cast<uint8_t *>(
            MemUtils::allocData(static_cast<size_t>(verts) * 4, 4));
        if (unlikely(!grown))
        {
            probeAllocCooldown_ = 256;
            return nullptr;
        }
        if (probeData_)
            MemUtils::freeData(probeData_);
        probeData_ = grown;
        probeVerts_ = verts;
        return probeData_;
    }
}