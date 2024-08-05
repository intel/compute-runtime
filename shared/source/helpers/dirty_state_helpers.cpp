/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/dirty_state_helpers.h"

#include "shared/source/indirect_heap/indirect_heap.h"

using namespace NEO;

bool HeapDirtyState::updateAndCheck(const IndirectHeap *heap) {
    if (!heap->getGraphicsAllocation()) {
        sizeInPages = 0llu;
        return true;
    }

    return updateAndCheck(heap, heap->getHeapGpuBase(), heap->getHeapSizeInPages());
}

bool HeapDirtyState::updateAndCheck(const IndirectHeap *heap, const uint64_t comparedGpuAddress, const size_t comparedSize) {
    bool dirty = gpuBaseAddress != comparedGpuAddress || sizeInPages != comparedSize;

    if (dirty) {
        gpuBaseAddress = comparedGpuAddress;
        sizeInPages = comparedSize;
    }

    return dirty;
}