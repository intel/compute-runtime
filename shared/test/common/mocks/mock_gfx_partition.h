/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/gfx_partition.h"

using namespace NEO;

class MockGfxPartition : public GfxPartition {
  public:
    using GfxPartition::osMemory;

    MockGfxPartition() : GfxPartition(reservedCpuAddressRange) {}
    MockGfxPartition(OSMemory::ReservedCpuAddressRange &sharedReservedCpuAddressRange) : GfxPartition(sharedReservedCpuAddressRange) {}

    uint64_t getHeapSize(HeapIndex heapIndex) {
        return getHeap(heapIndex).getSize();
    }

    bool heapInitialized(HeapIndex heapIndex) {
        return getHeapSize(heapIndex) > 0;
    }

    void *getReservedCpuAddressRange() {
        return reservedCpuAddressRange.alignedPtr;
    }

    size_t getReservedCpuAddressRangeSize() {
        return reservedCpuAddressRange.actualReservedSize - GfxPartition::heapGranularity;
    }

    uint64_t heapAllocate(HeapIndex heapIndex, size_t &size) override {
        if (callHeapAllocate) {
            return getHeap(heapIndex).allocate(size);
        }
        heapAllocateIndex = heapIndex;
        return mockGpuVa;
    }

    uint64_t heapAllocateWithCustomAlignment(HeapIndex heapIndex, size_t &size, size_t alignment) override {
        if (callHeapAllocate) {
            return getHeap(heapIndex).allocateWithCustomAlignment(size, alignment);
        }
        heapAllocateIndex = heapIndex;
        return mockGpuVa;
    }

    uint64_t heapGetLimit(HeapIndex heapIndex) {
        return getHeap(heapIndex).getLimit();
    }

    void heapFree(HeapIndex heapIndex, uint64_t ptr, size_t size) override {
        if (callHeapAllocate) {
            getHeap(heapIndex).free(ptr, size);
        }
        heapFreePtr = ptr;
    }

    void freeGpuAddressRange(uint64_t gpuAddress, size_t size) override {
        freeGpuAddressRangeCalled++;
        if (callBasefreeGpuAddressRange) {
            GfxPartition::freeGpuAddressRange(gpuAddress, size);
        }
    }
    void initHeap(HeapIndex heapIndex, uint64_t base, uint64_t size, size_t allocationAlignment) {
        getHeap(heapIndex).init(base, size, allocationAlignment);
    }

    uint32_t freeGpuAddressRangeCalled = 0u;
    bool callBasefreeGpuAddressRange = false;

    static std::array<HeapIndex, static_cast<uint32_t>(HeapIndex::totalHeaps)> allHeapNames;

    OSMemory::ReservedCpuAddressRange reservedCpuAddressRange;
    bool callHeapAllocate = true;
    HeapIndex heapAllocateIndex = HeapIndex::totalHeaps;
    const uint64_t mockGpuVa = std::numeric_limits<uint64_t>::max();
    uint64_t heapFreePtr = 0;
};

class MockGfxPartitionBasic : public GfxPartition {
  public:
    MockGfxPartitionBasic() : GfxPartition(reservedCpuAddressRange) {}

    OSMemory::ReservedCpuAddressRange reservedCpuAddressRange;
};

class FailedInitGfxPartition : public MockGfxPartition {
  public:
    bool init(uint64_t gpuAddressSpace, size_t cpuAddressRangeSizeToReserve, uint32_t rootDeviceIndex, size_t numRootDevices, bool useFrontWindowPool, uint64_t systemMemorySize, uint64_t gfxTop) override {
        return false;
    }
};
