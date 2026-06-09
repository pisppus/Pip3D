#pragma once

#include "Core/Platform.hpp"

#if defined(PIP3D_PC)
inline bool psramFound()
{
    return false;
}

inline void *ps_malloc(size_t size)
{
    return std::malloc(size);
}

#ifndef MALLOC_CAP_DMA
#define MALLOC_CAP_DMA 0
#define MALLOC_CAP_INTERNAL 0
#define MALLOC_CAP_SPIRAM 0
#define MALLOC_CAP_8BIT 0
#endif

inline void *heap_caps_aligned_alloc(size_t align, size_t size, unsigned int)
{
#if defined(_MSC_VER)
    return _aligned_malloc(size, align);
#else
    const size_t alignedSize = (size + align - 1u) / align * align;
    return std::aligned_alloc(align, alignedSize);
#endif
}

inline void *heap_caps_malloc(size_t size, unsigned int)
{
#if defined(_MSC_VER)
    const size_t align = 16u;
    return _aligned_malloc(size, align);
#else
    return std::malloc(size);
#endif
}

inline void heap_caps_free(void *ptr)
{
#if defined(_MSC_VER)
    _aligned_free(ptr);
#else
    std::free(ptr);
#endif
}
#else
#include <esp_heap_caps.h>
#endif

namespace pip3D
{
    struct MemUtils
    {
        static size_t getFreeHeap()
        {
#if defined(PIP3D_PC)
            return 0;
#else
            return ESP.getFreeHeap();
#endif
        }

        static size_t getFreePSRAM()
        {
#if defined(PIP3D_PC)
            return 0;
#else
            return ESP.getFreePsram();
#endif
        }

        static size_t getLargestFreeBlock()
        {
#if defined(PIP3D_PC)
            return 0;
#else
            return ESP.getMaxAllocHeap();
#endif
        }

        static void *allocAligned(size_t size, size_t align = 4)
        {
            (void)align;

            if (size > 1024)
            {
                if (psramFound())
                {
                    return ps_malloc(size);
                }
            }
            return malloc(size);
        }

        static void freeAligned(void *ptr)
        {
            if (ptr)
                free(ptr);
        }

        static void *allocData(size_t size, size_t align = 16)
        {
            if (size == 0)
            {
                return nullptr;
            }

#ifdef PIP3D_USE_PSRAM
            if (psramFound())
            {
                void *ptr = heap_caps_aligned_alloc(align, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                if (ptr)
                {
                    return ptr;
                }
            }
#endif

            return heap_caps_aligned_alloc(align, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        }

        static void freeData(void *ptr)
        {
            if (!ptr)
            {
                return;
            }
            heap_caps_free(ptr);
        }

        static bool isInPSRAM(void *ptr)
        {
#if defined(PIP3D_PC)
            (void)ptr;
            return false;
#else
            return ((uint32_t)ptr >= 0x3F800000 && (uint32_t)ptr < 0x3FC00000);
#endif
        }
    };
}