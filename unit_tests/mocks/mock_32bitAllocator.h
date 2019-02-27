/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/aligned_memory.h"
#include "runtime/os_interface/32bit_memory.h"
#include "runtime/os_interface/linux/drm_32bit_memory.cpp"

#include <limits>
namespace OCLRT {

constexpr uintptr_t startOf32MmapRegion = 0x40000000;
static bool failMmap = false;
static size_t maxMmapLength = std::numeric_limits<size_t>::max();

static uintptr_t offsetIn32BitRange = 0;
static uint32_t mmapCallCount = 0u;
static uint32_t unmapCallCount = 0u;
static uint32_t mmapFailCount = 0u;

void *MockMmap(void *addr, size_t length, int prot, int flags,
               int fd, off_t offset) noexcept {

    mmapCallCount++;
    UNRECOVERABLE_IF(addr);

    if (failMmap || length > maxMmapLength) {
        return MAP_FAILED;
    }

    if (mmapFailCount > 0) {
        mmapFailCount--;
        return MAP_FAILED;
    }

    uintptr_t ptrToReturn = startOf32MmapRegion + offsetIn32BitRange;
    offsetIn32BitRange += alignUp(length, MemoryConstants::pageSize);

    return reinterpret_cast<void *>(ptrToReturn);
}
int MockMunmap(void *addr, size_t length) noexcept {
    unmapCallCount++;
    return 0;
}

class MockAllocator32Bit : public Allocator32bit {
  public:
    class OsInternalsPublic : public Allocator32bit::OsInternals {
    };

    MockAllocator32Bit(Allocator32bit::OsInternals *osInternalsIn) : Allocator32bit(osInternalsIn) {
    }

    MockAllocator32Bit() {
        this->osInternals->mmapFunction = MockMmap;
        this->osInternals->munmapFunction = MockMunmap;
        resetState();
    }
    ~MockAllocator32Bit() {
        resetState();
    }
    static void resetState() {
        failMmap = false;
        maxMmapLength = std::numeric_limits<size_t>::max();
        offsetIn32BitRange = 0u;
        mmapCallCount = 0u;
        unmapCallCount = 0u;
        mmapFailCount = 0u;
    }

    static OsInternalsPublic *createOsInternals() {
        return new OsInternalsPublic;
    }

    OsInternals *getOsInternals() const { return this->osInternals.get(); }
};
} // namespace OCLRT
