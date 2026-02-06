/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/heap_helper.h"

#include "shared/source/device/device.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"

namespace NEO {

HeapHelper::HeapHelper(Device *device, InternalAllocationStorage *storageForReuse, bool isMultiOsContextCapable)
    : isMultiOsContextCapable(isMultiOsContextCapable),
      storageForReuse(storageForReuse),
      memManager(device->getMemoryManager()),
      device(device) {}

GraphicsAllocation *HeapHelper::getHeapAllocation(uint32_t heapType, size_t heapSize, size_t alignment, uint32_t rootDeviceIndex) {
    auto allocationType = AllocationType::linearStream;
    if (IndirectHeap::Type::indirectObject == heapType) {
        allocationType = AllocationType::internalHeap;
    }

    if (NEO::debugManager.flags.AlignLocalMemoryVaTo2MB.get() == 1) {
        alignment = MemoryConstants::pageSize2M;
    }

    GraphicsAllocation *heapAllocation = nullptr;

    if (allocationType == AllocationType::linearStream && device) {
        auto enablePoolAllocator = NEO::debugManager.flags.EnableLinearStreamPoolAllocator.get();
        if ((enablePoolAllocator == 1) ||
            (enablePoolAllocator == -1 && device->getProductHelper().is2MBLocalMemAlignmentEnabled())) {
            heapAllocation = device->getLinearStreamPoolAllocator().allocate(heapSize);
            if (heapAllocation) {
                return heapAllocation;
            }
        }
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
        if (heapAllocation->isView()) {
            auto *parent = heapAllocation->getParentAllocation();
            if (parent && device && device->getLinearStreamPoolAllocator().isPoolBuffer(parent)) {
                device->getLinearStreamPoolAllocator().free(heapAllocation);
                return;
            }
        }
        this->storageForReuse->storeAllocation(std::unique_ptr<NEO::GraphicsAllocation>(heapAllocation), NEO::AllocationUsage::REUSABLE_ALLOCATION);
    }
}
} // namespace NEO
