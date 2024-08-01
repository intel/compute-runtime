/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/heap_helper.h"

#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"

namespace NEO {

GraphicsAllocation *HeapHelper::getHeapAllocation(uint32_t heapType, size_t heapSize, size_t alignment, uint32_t rootDeviceIndex) {
    auto allocationType = AllocationType::linearStream;
    if (IndirectHeap::Type::indirectObject == heapType) {
        allocationType = AllocationType::internalHeap;
    }

    if (NEO::debugManager.flags.AlignLocalMemoryVaTo2MB.get() == 1) {
        alignment = MemoryConstants::pageSize2M;
    }

    auto allocation = this->storageForReuse->obtainReusableAllocation(heapSize, allocationType);
    if (allocation) {
        return allocation.release();
    }
    NEO::AllocationProperties properties{rootDeviceIndex, true, heapSize, allocationType, isMultiOsContextCapable, false, storageForReuse->getDeviceBitfield()};
    properties.alignment = alignment;

    return this->memManager->allocateGraphicsMemoryWithProperties(properties);
}
void HeapHelper::storeHeapAllocation(GraphicsAllocation *heapAllocation) {
    if (heapAllocation) {
        this->storageForReuse->storeAllocation(std::unique_ptr<NEO::GraphicsAllocation>(heapAllocation), NEO::AllocationUsage::REUSABLE_ALLOCATION);
    }
}
} // namespace NEO
