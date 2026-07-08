#pragma once

#include <cstddef>

#include <PipCore/Platform.hpp>
#include <PipCore/Platforms/Select.hpp>
#include "Core/Platform.hpp"

namespace pip3D
{
    struct MemUtils
    {
        PIP3D_FORCE_INLINE static void *allocAligned(
            size_t size,
            size_t align = 16,
            pipcore::AllocCaps caps = pipcore::AllocCaps::PreferInternal)
        {
            return pipcore::GetPlatform()->allocAligned(size, align, caps);
        }

        PIP3D_FORCE_INLINE static void freeAligned(void *alignedPtr)
        {
            pipcore::GetPlatform()->freeAligned(alignedPtr);
        }

        PIP3D_FORCE_INLINE static void *allocData(size_t size, size_t align = 16)
        {
            pipcore::AllocCaps caps = pipcore::AllocCaps::PreferInternal;
            return allocAligned(size, align, caps);
        }

        PIP3D_FORCE_INLINE static void freeData(void *ptr)
        {
            freeAligned(ptr);
        }

        PIP3D_FORCE_INLINE static size_t getFreeHeap()
        {
            return pipcore::GetPlatform()->freeHeapTotal();
        }

        PIP3D_FORCE_INLINE static size_t getLargestFreeBlock()
        {
            return pipcore::GetPlatform()->largestFreeBlock();
        }
    };
}