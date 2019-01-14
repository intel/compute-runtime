/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <limits>
#include "runtime/os_interface/32bit_memory.h"
#include "runtime/os_interface/linux/drm_32bit_memory.cpp"
#include "runtime/helpers/aligned_memory.h"
namespace OCLRT {

constexpr uintptr_t startOf32MmapRegion = 0x40000000;
static bool failMmap = false;
static bool fail32BitMmap = false;
static bool failUpperRange = false;
static bool failLowerRanger = false;
static size_t maxMmapLength = std::numeric_limits<size_t>::max();

static uintptr_t startUpperHeap = maxMmap32BitAddress;
static uintptr_t lowerRangeHeapStart = lowerRangeStart;

static uintptr_t offsetIn32BitRange = 0;
static uint32_t mmapCallCount = 0u;
static uint32_t unmapCallCount = 0u;
static uint32_t mmapFailCount = 0u;

void *MockMmap(void *addr, size_t length, int prot, int flags,
               int fd, off_t offset) noexcept {

    bool return32bitRange = true;
    bool returnUpperRange = false;
    bool returnLowerRange = false;
    mmapCallCount++;

    if (failMmap || length > maxMmapLength) {
        return MAP_FAILED;
    }

    if (mmapFailCount > 0) {
        mmapFailCount--;
        return MAP_FAILED;
    }

    if (addr) {
        return32bitRange = false;
        if ((uintptr_t)addr >= maxMmap32BitAddress) {
            if (failUpperRange) {
                return MAP_FAILED;
            }
            returnUpperRange = true;
        }
        if ((uintptr_t)addr >= lowerRangeStart) {
            if (failLowerRanger) {
                return MAP_FAILED;
            }
            returnLowerRange = true;
        }
    }

    if (flags & MAP_32BIT) {
        if (fail32BitMmap) {
            return MAP_FAILED;
        }
        return32bitRange = true;
    }

    uintptr_t ptrToReturn = (uintptr_t)addr;
    if (return32bitRange) {
        ptrToReturn = startOf32MmapRegion + offsetIn32BitRange;
        offsetIn32BitRange += alignUp(length, MemoryConstants::pageSize);
    } else if (returnUpperRange) {
        ptrToReturn = (uintptr_t)addr;
    } else if (returnLowerRange) {
        ptrToReturn = (uintptr_t)addr;
    } else {
        ptrToReturn = (uintptr_t)MAP_FAILED;
    }

    return (void *)ptrToReturn;
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
        fail32BitMmap = false;
        failUpperRange = false;
        failLowerRanger = false;
        failMmap = false;
        maxMmapLength = std::numeric_limits<size_t>::max();
        startUpperHeap = maxMmap32BitAddress;
        lowerRangeHeapStart = lowerRangeStart;
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