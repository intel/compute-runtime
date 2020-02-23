/*
 * Copyright (C) 2019-2020 Intel Corporation
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

    uint64_t getHeapSize(HeapIndex heapIndex) {
        return getHeap(heapIndex).getSize();
    }

    bool heapInitialized(HeapIndex heapIndex) {
        return getHeapSize(heapIndex) > 0;
    }

    void *getReservedCpuAddressRange() {
        return reservedCpuAddressRange;
    }

    size_t getReservedCpuAddressRangeSize() {
        return reservedCpuAddressRangeSize;
    }

    MOCK_METHOD2(freeGpuAddressRange, void(uint64_t gpuAddress, size_t size));

    static std::array<HeapIndex, static_cast<uint32_t>(HeapIndex::TOTAL_HEAPS)> allHeapNames;
};
