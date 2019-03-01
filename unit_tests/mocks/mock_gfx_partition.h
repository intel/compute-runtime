/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/gfx_partition.h"

using namespace OCLRT;

class MockGfxPartition : public GfxPartition {
  public:
    uint64_t getHeapSize(HeapIndex heapIndex) {
        return getHeap(heapIndex).getSize();
    }

    bool heapInitialized(HeapIndex heapIndex) {
        return getHeapBase(heapIndex) && getHeapSize(heapIndex);
    }

    static std::array<HeapIndex, static_cast<uint32_t>(HeapIndex::TOTAL_HEAPS)> allHeapNames;
};
