/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/gfx_partition.h"

#include "gmock/gmock.h"

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

    void heapFree(HeapIndex heapIndex, uint64_t ptr, size_t size) override {
        if (callHeapAllocate) {
            getHeap(heapIndex).free(ptr, size);
        }
    }

    static std::array<HeapIndex, static_cast<uint32_t>(HeapIndex::TOTAL_HEAPS)> allHeapNames;

    OSMemory::ReservedCpuAddressRange reservedCpuAddressRange;
    bool callHeapAllocate = true;
    HeapIndex heapAllocateIndex = HeapIndex::TOTAL_HEAPS;
    const uint64_t mockGpuVa = std::numeric_limits<uint64_t>::max();
};

struct GmockGfxPartition : MockGfxPartition {
    using MockGfxPartition::MockGfxPartition;
    MOCK_METHOD(void, freeGpuAddressRange, (uint64_t gpuAddress, size_t size), (override));
};

class MockGfxPartitionBasic : public GfxPartition {
  public:
    MockGfxPartitionBasic() : GfxPartition(reservedCpuAddressRange) {}

    OSMemory::ReservedCpuAddressRange reservedCpuAddressRange;
};

class FailedInitGfxPartition : public MockGfxPartition {
  public:
    virtual bool init(uint64_t gpuAddressSpace, size_t cpuAddressRangeSizeToReserve, uint32_t rootDeviceIndex, size_t numRootDevices, bool useFrontWindowPool) override {
        return false;
    }
};
