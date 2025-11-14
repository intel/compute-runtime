/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/tag_allocator.h"

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/multi_graphics_allocation.h"

namespace NEO {

TagAllocatorBase::TagAllocatorBase(const RootDeviceIndicesContainer &rootDeviceIndices, MemoryManager *memMngr, uint32_t tagCount, size_t tagAlignment, size_t tagSize, bool doNotReleaseNodes, DeviceBitfield deviceBitfield)
    : deviceBitfield(deviceBitfield), rootDeviceIndices(rootDeviceIndices), memoryManager(memMngr), tagCount(tagCount), tagSize(static_cast<uint32_t>(alignUp(tagSize, tagAlignment))), doNotReleaseNodes(doNotReleaseNodes) {

    maxRootDeviceIndex = *std::max_element(std::begin(rootDeviceIndices), std::end(rootDeviceIndices));
}

void TagAllocatorBase::cleanUpResources() {
    for (auto &multiGfxAllocation : gfxAllocations) {
        for (auto &allocation : multiGfxAllocation->getGraphicsAllocations()) {
            memoryManager->freeGraphicsMemory(allocation);
        }
    }

    gfxAllocations.clear();
}

MultiGraphicsAllocation *TagNodeBase::getBaseGraphicsAllocation() const {
    return gfxAllocation;
}

void TagNodeBase::returnTag() {
    allocator->returnTag(this);
}

bool TagNodeBase::canBeReleased() const {
    return !doNotReleaseNodes;
}

} // namespace NEO
