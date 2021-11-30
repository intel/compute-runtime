/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/gfx_partition.h"

namespace NEO {

class GfxPartition;
class WddmAllocation;
class Wddm;

bool mapTileInstancedAllocation(WddmAllocation *allocation, const void *requiredPtr, Wddm *wddm, HeapIndex heapIndex, GfxPartition &gfxPartition) {
    return false;
}

} // namespace NEO
