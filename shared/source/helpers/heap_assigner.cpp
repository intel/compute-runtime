/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/heap_assigner.h"

#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/memory_manager/memory_manager.h"

namespace NEO {

HeapAssigner::HeapAssigner(bool allowExternalHeap) {
    apiAllowExternalHeapForSshAndDsh = allowExternalHeap;
}
bool HeapAssigner::useInternal32BitHeap(AllocationType allocType) {
    return GraphicsAllocation::isIsaAllocationType(allocType) ||
           allocType == AllocationType::internalHeap;
}
bool HeapAssigner::use32BitHeap(AllocationType allocType) {
    return useExternal32BitHeap(allocType) || useInternal32BitHeap(allocType);
}
HeapIndex HeapAssigner::get32BitHeapIndex(AllocationType allocType, bool useLocalMem, const HardwareInfo &hwInfo, bool useFrontWindow) {
    if (useInternal32BitHeap(allocType)) {
        return useFrontWindow ? mapInternalWindowIndex(MemoryManager::selectInternalHeap(useLocalMem)) : MemoryManager::selectInternalHeap(useLocalMem);
    }
    return useFrontWindow ? mapExternalWindowIndex(MemoryManager::selectExternalHeap(useLocalMem)) : MemoryManager::selectExternalHeap(useLocalMem);
}
bool HeapAssigner::useExternal32BitHeap(AllocationType allocType) {
    if (apiAllowExternalHeapForSshAndDsh) {
        return allocType == AllocationType::linearStream;
    }
    return false;
}

bool HeapAssigner::heapTypeExternalWithFrontWindowPool(HeapIndex heap) {
    return heap == HeapIndex::heapExternalDeviceMemory || heap == HeapIndex::heapExternal;
}

HeapIndex HeapAssigner::mapExternalWindowIndex(HeapIndex index) {
    auto retIndex = HeapIndex::totalHeaps;
    switch (index) {
    case HeapIndex::heapExternal:
        retIndex = HeapIndex::heapExternalFrontWindow;
        break;
    case HeapIndex::heapExternalDeviceMemory:
        retIndex = HeapIndex::heapExternalDeviceFrontWindow;
        break;
    default:
        UNRECOVERABLE_IF(true);
        break;
    };
    return retIndex;
}

HeapIndex HeapAssigner::mapInternalWindowIndex(HeapIndex index) {
    auto retIndex = HeapIndex::totalHeaps;
    switch (index) {
    case HeapIndex::heapInternal:
        retIndex = HeapIndex::heapInternalFrontWindow;
        break;
    case HeapIndex::heapInternalDeviceMemory:
        retIndex = HeapIndex::heapInternalDeviceFrontWindow;
        break;
    default:
        UNRECOVERABLE_IF(true);
        break;
    };
    return retIndex;
}

bool HeapAssigner::isInternalHeap(HeapIndex heap) {
    return heap == HeapIndex::heapInternalDeviceMemory || heap == HeapIndex::heapInternal;
}

} // namespace NEO