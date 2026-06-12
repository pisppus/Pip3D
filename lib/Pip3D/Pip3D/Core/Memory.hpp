#pragma once

#include "Core/Platform.hpp"
#include <PipCore/Platform.hpp>
#include <PipCore/Platforms/Select.hpp>

namespace pip3D
{
    struct MemUtils
    {
        static size_t getFreeHeap()
        {
            return pipcore::GetPlatform()->freeHeapTotal();
        }

        static size_t getFreePSRAM()
        {
            uint32_t total = pipcore::GetPlatform()->freeHeapTotal();
            uint32_t internal = pipcore::GetPlatform()->freeHeapInternal();
            return (total > internal) ? (total - internal) : 0;
        }

        static size_t getLargestFreeBlock()
        {
            return pipcore::GetPlatform()->largestFreeBlock();
        }

        static void *allocAligned(size_t size, size_t align = 16, pipcore::AllocCaps caps = pipcore::AllocCaps::Default)
        {
            return pipcore::GetPlatform()->allocAligned(size, align, caps);
        }

        static void freeAligned(void *alignedPtr)
        {
            pipcore::GetPlatform()->freeAligned(alignedPtr);
        }

        static void *allocData(size_t size, size_t align = 16)
        {
            pipcore::AllocCaps caps = pipcore::AllocCaps::Default;

#ifndef PIP3D_USE_PSRAM
            caps = pipcore::AllocCaps::PreferInternal;
#else
            if (size < 1024)
            {
                caps = pipcore::AllocCaps::PreferInternal;
            }
#endif

            return allocAligned(size, align, caps);
        }

        static void freeData(void *ptr)
        {
            freeAligned(ptr);
        }

        static bool isInPSRAM(void *ptr)
        {
#if defined(PIP3D_PC)
            (void)ptr;
            return false;
#else
            uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
            return (addr >= 0x3F800000 && addr < 0x3FC00000);
#endif
        }
    };
}