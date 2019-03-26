/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/dirty_state_helpers.h"

#include "runtime/indirect_heap/indirect_heap.h"

using namespace NEO;

bool HeapDirtyState::updateAndCheck(const IndirectHeap *heap) {
    if (!heap->getGraphicsAllocation()) {
        sizeInPages = 0llu;
        return true;
    }
    bool dirty = gpuBaseAddress != heap->getHeapGpuBase() || sizeInPages != heap->getHeapSizeInPages();
    if (dirty) {
        gpuBaseAddress = heap->getHeapGpuBase();
        sizeInPages = heap->getHeapSizeInPages();
    }
    return dirty;
}
