/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/heap_helper.h"

#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"

namespace NEO {

GraphicsAllocation *HeapHelper::getHeapAllocation(uint32_t heapType, size_t heapSize, size_t alignment, uint32_t rootDeviceIndex) {
    auto allocationType = AllocationType::LINEAR_STREAM;
    if (IndirectHeap::Type::INDIRECT_OBJECT == heapType) {
        allocationType = AllocationType::INTERNAL_HEAP;
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
