/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/heap_assigner.h"

#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/memory_manager.h"

namespace NEO {

HeapAssigner::HeapAssigner() {
    apiAllowExternalHeapForSshAndDsh = ApiSpecificConfig::getHeapConfiguration();
}
bool HeapAssigner::useInternal32BitHeap(AllocationType allocType) {
    return GraphicsAllocation::isIsaAllocationType(allocType) ||
           allocType == AllocationType::INTERNAL_HEAP;
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
        return allocType == AllocationType::LINEAR_STREAM;
    }
    return false;
}

bool HeapAssigner::heapTypeExternalWithFrontWindowPool(HeapIndex heap) {
    return heap == HeapIndex::HEAP_EXTERNAL_DEVICE_MEMORY || heap == HeapIndex::HEAP_EXTERNAL;
}

HeapIndex HeapAssigner::mapExternalWindowIndex(HeapIndex index) {
    auto retIndex = HeapIndex::TOTAL_HEAPS;
    switch (index) {
    case HeapIndex::HEAP_EXTERNAL:
        retIndex = HeapIndex::HEAP_EXTERNAL_FRONT_WINDOW;
        break;
    case HeapIndex::HEAP_EXTERNAL_DEVICE_MEMORY:
        retIndex = HeapIndex::HEAP_EXTERNAL_DEVICE_FRONT_WINDOW;
        break;
    default:
        UNRECOVERABLE_IF(true);
        break;
    };
    return retIndex;
}

HeapIndex HeapAssigner::mapInternalWindowIndex(HeapIndex index) {
    auto retIndex = HeapIndex::TOTAL_HEAPS;
    switch (index) {
    case HeapIndex::HEAP_INTERNAL:
        retIndex = HeapIndex::HEAP_INTERNAL_FRONT_WINDOW;
        break;
    case HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY:
        retIndex = HeapIndex::HEAP_INTERNAL_DEVICE_FRONT_WINDOW;
        break;
    default:
        UNRECOVERABLE_IF(true);
        break;
    };
    return retIndex;
}

bool HeapAssigner::isInternalHeap(HeapIndex heap) {
    return heap == HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY || heap == HeapIndex::HEAP_INTERNAL;
}

} // namespace NEO