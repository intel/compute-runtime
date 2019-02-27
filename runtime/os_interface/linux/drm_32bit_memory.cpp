/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/os_interface/32bit_memory.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/os_interface/linux/allocator_helper.h"

#include <memory>
#include <sys/mman.h>
using namespace OCLRT;

class Allocator32bit::OsInternals {
  public:
    decltype(&mmap) mmapFunction = mmap;
    decltype(&munmap) munmapFunction = munmap;
    void *heapBasePtr = nullptr;
    size_t heapSize = 0;
};

bool OCLRT::is32BitOsAllocatorAvailable = true;

Allocator32bit::Allocator32bit(uint64_t base, uint64_t size) : base(base), size(size) {
    heapAllocator = std::make_unique<HeapAllocator>(base, size);
}

OCLRT::Allocator32bit::Allocator32bit() : Allocator32bit(new OsInternals) {
}

OCLRT::Allocator32bit::Allocator32bit(Allocator32bit::OsInternals *osInternalsIn) : osInternals(osInternalsIn) {
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
}

OCLRT::Allocator32bit::~Allocator32bit() {
    if (this->osInternals.get() != nullptr) {
        if (this->osInternals->heapBasePtr != nullptr)
            this->osInternals->munmapFunction(this->osInternals->heapBasePtr, this->osInternals->heapSize);
    }
}

uint64_t OCLRT::Allocator32bit::allocate(size_t &size) {
    return this->heapAllocator->allocate(size);
}

int Allocator32bit::free(uint64_t ptr, size_t size) {
    if (ptr == reinterpret_cast<uint64_t>(MAP_FAILED))
        return 0;

    this->heapAllocator->free(ptr, size);

    return 0;
}

uintptr_t Allocator32bit::getBase() const {
    return (uintptr_t)base;
}
