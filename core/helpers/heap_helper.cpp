/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/heap_helper.h"

#include "core/indirect_heap/indirect_heap.h"
#include "core/memory_manager/graphics_allocation.h"
#include "core/memory_manager/internal_allocation_storage.h"
#include "core/memory_manager/memory_manager.h"

namespace NEO {

GraphicsAllocation *HeapHelper::getHeapAllocation(uint32_t heapType, size_t heapSize, size_t alignment, uint32_t rootDeviceIndex) {
    auto allocationType = GraphicsAllocation::AllocationType::LINEAR_STREAM;
    if (IndirectHeap::Type::INDIRECT_OBJECT == heapType) {
        allocationType = GraphicsAllocation::AllocationType::INTERNAL_HEAP;
    }

    auto allocation = this->storageForReuse->obtainReusableAllocation(heapSize, allocationType);
    if (allocation) {
        return allocation.release();
    }
    NEO::AllocationProperties properties{rootDeviceIndex, true, heapSize, allocationType, isMultiOsContextCapable, false, {}};
    properties.alignment = alignment;

    return this->memManager->allocateGraphicsMemoryWithProperties(properties);
}
void HeapHelper::storeHeapAllocation(GraphicsAllocation *heapAllocation) {
    this->storageForReuse->storeAllocation(std::unique_ptr<NEO::GraphicsAllocation>(heapAllocation), NEO::AllocationUsage::REUSABLE_ALLOCATION);
}
} // namespace NEO
