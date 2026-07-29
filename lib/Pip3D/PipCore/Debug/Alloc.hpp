#pragma once

#include <PipCore/Features.hpp>

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <atomic>
#include <cstdlib>

namespace pipcore::debug
{
    struct alignas(8) AllocHeader
    {
        AllocHeader *next = nullptr;
        AllocHeader *prev = nullptr;
        uint32_t size = 0;
        const char *tag = nullptr;
        void *caller = nullptr;
        uint32_t magic = 0xDEADBEEF;
    };

    class Tracker
    {
    public:
        AllocHeader *_head = nullptr;
        std::atomic<uint32_t> _totalAllocated{0};
        uint32_t _peakAllocated = 0;
        std::atomic<bool> _lock{false};
        std::atomic<bool> _dirty{true};

    public:
        static Tracker &instance() noexcept
        {
            static Tracker tracker;
            return tracker;
        }

        inline void lock() noexcept
        {
            while (_lock.exchange(true, std::memory_order_acquire))
            {
            }
        }

        inline void unlock() noexcept
        {
            _lock.store(false, std::memory_order_release);
        }

        void *trackMalloc(size_t bytes, const char *tag, void *callerPC = nullptr) noexcept
        {
            if (bytes == 0)
                return nullptr;

            const size_t totalSize = bytes + sizeof(AllocHeader);
            auto *hdr = static_cast<AllocHeader *>(std::malloc(totalSize));
            if (!hdr)
                return nullptr;

            hdr->size = static_cast<uint32_t>(bytes);
            hdr->tag = tag ? tag : "unknown";
            hdr->caller = callerPC;
            hdr->magic = 0xDEADBEEF;

            lock();
            hdr->next = _head;
            hdr->prev = nullptr;
            if (_head)
                _head->prev = hdr;
            _head = hdr;

            _totalAllocated.fetch_add(bytes, std::memory_order_relaxed);
            if (_totalAllocated > _peakAllocated)
                _peakAllocated = _totalAllocated;
            _dirty.store(true, std::memory_order_relaxed);
            unlock();

            return reinterpret_cast<void *>(hdr + 1);
        }

        void trackFree(void *ptr) noexcept
        {
            if (!ptr)
                return;

            auto *hdr = reinterpret_cast<AllocHeader *>(ptr) - 1;

            if (hdr->magic != 0xDEADBEEF)
            {
                std::free(ptr);
                return;
            }

            lock();
            if (hdr->prev)
                hdr->prev->next = hdr->next;
            if (hdr->next)
                hdr->next->prev = hdr->prev;
            if (_head == hdr)
                _head = hdr->next;

            _totalAllocated.fetch_sub(hdr->size, std::memory_order_relaxed);
            _dirty.store(true, std::memory_order_relaxed);
            unlock();

            std::free(hdr);
        }
    };
}