/*
 * Copyright (C) 2020 Intel Corporation
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
bool HeapAssigner::useInternal32BitHeap(GraphicsAllocation::AllocationType allocType) {
    return allocType == GraphicsAllocation::AllocationType::KERNEL_ISA ||
           allocType == GraphicsAllocation::AllocationType::INTERNAL_HEAP;
}
bool HeapAssigner::use32BitHeap(GraphicsAllocation::AllocationType allocType) {
    return useExternal32BitHeap(allocType) || useInternal32BitHeap(allocType);
}
HeapIndex HeapAssigner::get32BitHeapIndex(GraphicsAllocation::AllocationType allocType, bool useLocalMem, const HardwareInfo &hwInfo, bool useExternalWindow) {
    if (useInternal32BitHeap(allocType)) {
        return MemoryManager::selectInternalHeap(useLocalMem);
    }
    return useExternalWindow ? mapExternalWindowIndex(MemoryManager::selectExternalHeap(useLocalMem)) : MemoryManager::selectExternalHeap(useLocalMem);
}
bool HeapAssigner::useExternal32BitHeap(GraphicsAllocation::AllocationType allocType) {
    if (apiAllowExternalHeapForSshAndDsh) {
        return allocType == GraphicsAllocation::AllocationType::LINEAR_STREAM;
    }
    return false;
}

bool HeapAssigner::heapTypeWithFrontWindowPool(HeapIndex heap) {
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

} // namespace NEO