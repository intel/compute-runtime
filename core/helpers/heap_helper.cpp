/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/heap_helper.h"

#include "core/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/internal_allocation_storage.h"
#include "runtime/memory_manager/memory_manager.h"

namespace NEO {
GraphicsAllocation *HeapHelper::getHeapAllocation(size_t heapSize, size_t alignment, uint32_t rootDeviceIndex) {
    auto allocation = this->storageForReuse->obtainReusableAllocation(heapSize, GraphicsAllocation::AllocationType::INTERNAL_HEAP);
    if (allocation) {
        return allocation.release();
    }
    NEO::AllocationProperties properties{rootDeviceIndex, true /* allocateMemory*/, alignment,
                                         GraphicsAllocation::AllocationType::INTERNAL_HEAP,
                                         isMultiOsContextCapable /* multiOsContextCapable */,
                                         false,
                                         NEO::SubDevice::unspecifiedSubDeviceIndex};

    return this->memManager->allocateGraphicsMemoryWithProperties(properties);
}
void HeapHelper::storeHeapAllocation(GraphicsAllocation *heapAllocation) {
    this->storageForReuse->storeAllocation(std::unique_ptr<NEO::GraphicsAllocation>(heapAllocation), NEO::AllocationUsage::REUSABLE_ALLOCATION);
}
} // namespace NEO