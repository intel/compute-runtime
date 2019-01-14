/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <memory>
#include "runtime/os_interface/32bit_memory.h"
#include "runtime/os_interface/linux/allocator_helper.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/os_interface/debug_settings_manager.h"

#include <sys/mman.h>
using namespace OCLRT;
constexpr uintptr_t maxMmap32BitAddress = 0x80000000;
constexpr uintptr_t lowerRangeStart = 0x10000000;

class Allocator32bit::OsInternals {
  public:
    uintptr_t upperRangeAddress = maxMmap32BitAddress;
    uintptr_t lowerRangeAddress = lowerRangeStart;
    decltype(&mmap) mmapFunction = mmap;
    decltype(&munmap) munmapFunction = munmap;
    void *heapBasePtr = nullptr;
    size_t heapSize = 0;

    class Drm32BitAllocator {
      protected:
        Allocator32bit::OsInternals &outer;

      public:
        Drm32BitAllocator(Allocator32bit::OsInternals &outer) : outer(outer) {
        }

        void *allocate(size_t size) {
            auto ptr = outer.mmapFunction(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);

            // In case we failed, retry with address provided as a hint
            if (ptr == MAP_FAILED) {
                ptr = outer.mmapFunction((void *)outer.upperRangeAddress, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                if (((uintptr_t)ptr + alignUp(size, 4096)) >= max32BitAddress || ptr == MAP_FAILED) {
                    outer.munmapFunction(ptr, size);

                    // Try to use lower range
                    ptr = outer.mmapFunction((void *)outer.lowerRangeAddress, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                    if ((uintptr_t)ptr >= max32BitAddress) {
                        outer.munmapFunction(ptr, size);
                        return nullptr;
                    }

                    outer.lowerRangeAddress = (uintptr_t)ptr + alignUp(size, 4096);
                    return ptr;
                }

                outer.upperRangeAddress = (uintptr_t)ptr + alignUp(size, 4096);
            }
            return ptr;
        }

        int free(void *ptr, uint64_t size) {
            auto alignedSize = alignUp(size, 4096);
            auto offsetedPtr = (uintptr_t)ptrOffset(ptr, alignedSize);

            if (offsetedPtr == outer.upperRangeAddress) {
                outer.upperRangeAddress -= alignedSize;
            } else if (offsetedPtr == outer.lowerRangeAddress) {
                outer.lowerRangeAddress -= alignedSize;
            }
            return outer.munmapFunction(ptr, size);
        }
    };
    Drm32BitAllocator *drmAllocator = nullptr;
};

bool OCLRT::is32BitOsAllocatorAvailable = true;

Allocator32bit::Allocator32bit(uint64_t base, uint64_t size) : base(base), size(size) {
    heapAllocator = std::make_unique<HeapAllocator>(base, size);
}

OCLRT::Allocator32bit::Allocator32bit() : Allocator32bit(new OsInternals) {
}

OCLRT::Allocator32bit::Allocator32bit(Allocator32bit::OsInternals *osInternalsIn) : osInternals(osInternalsIn) {

    if (DebugManager.flags.UseNewHeapAllocator.get()) {
        size_t sizeToMap = getSizeToMap();
        void *ptr = this->osInternals->mmapFunction(nullptr, sizeToMap, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);

        if (ptr == MAP_FAILED) {
            sizeToMap -= sizeToMap / 4;
            ptr = this->osInternals->mmapFunction(nullptr, sizeToMap, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);

            DebugManager.log(DebugManager.flags.PrintDebugMessages.get(), __FUNCTION__, " Allocator RETRY ptr == ", ptr);

            if (ptr == MAP_FAILED) {
                ptr = nullptr;
                sizeToMap = 0;
            }
        }

        DebugManager.log(DebugManager.flags.PrintDebugMessages.get(), __FUNCTION__, "Allocator ptr == ", ptr);

        osInternals->heapBasePtr = ptr;
        osInternals->heapSize = sizeToMap;
        base = reinterpret_cast<uint64_t>(ptr);
        size = sizeToMap;

        heapAllocator = std::unique_ptr<HeapAllocator>(new HeapAllocator(base, sizeToMap));
    } else {
        this->osInternals->drmAllocator = new Allocator32bit::OsInternals::Drm32BitAllocator(*this->osInternals);
    }
}

OCLRT::Allocator32bit::~Allocator32bit() {
    if (this->osInternals.get() != nullptr) {
        if (this->osInternals->heapBasePtr != nullptr)
            this->osInternals->munmapFunction(this->osInternals->heapBasePtr, this->osInternals->heapSize);

        if (this->osInternals->drmAllocator != nullptr)
            delete this->osInternals->drmAllocator;
    }
}

uint64_t OCLRT::Allocator32bit::allocate(size_t &size) {
    uint64_t ptr = 0llu;
    if (DebugManager.flags.UseNewHeapAllocator.get()) {
        ptr = this->heapAllocator->allocate(size);
    } else {
        ptr = reinterpret_cast<uint64_t>(this->osInternals->drmAllocator->allocate(size));
    }
    return ptr;
}

int Allocator32bit::free(uint64_t ptr, size_t size) {
    if (ptr == reinterpret_cast<uint64_t>(MAP_FAILED) || ptr == 0llu)
        return 0;

    if (DebugManager.flags.UseNewHeapAllocator.get()) {
        this->heapAllocator->free(ptr, size);
    } else {
        return this->osInternals->drmAllocator->free(reinterpret_cast<void *>(ptr), size);
    }
    return 0;
}

uintptr_t Allocator32bit::getBase() const {
    return (uintptr_t)base;
}
