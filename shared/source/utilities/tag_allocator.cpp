/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/tag_allocator.h"

namespace NEO {

TagAllocatorBase::TagAllocatorBase(uint32_t rootDeviceIndex, MemoryManager *memMngr, size_t tagCount, size_t tagAlignment, size_t tagSize, bool doNotReleaseNodes, DeviceBitfield deviceBitfield)
    : deviceBitfield(deviceBitfield), rootDeviceIndex(rootDeviceIndex), memoryManager(memMngr), tagCount(tagCount), tagSize(tagSize), doNotReleaseNodes(doNotReleaseNodes) {

    this->tagSize = alignUp(tagSize, tagAlignment);
}

void TagAllocatorBase::cleanUpResources() {
    for (auto gfxAllocation : gfxAllocations) {
        memoryManager->freeGraphicsMemory(gfxAllocation);
    }
    gfxAllocations.clear();
}

void TagNodeBase::returnTag() {
    allocator->returnTag(this);
}

bool TagNodeBase::canBeReleased() const {
    return (!doNotReleaseNodes) &&
           (isCompleted()) &&
           (getImplicitGpuDependenciesCount() == getImplicitCpuDependenciesCount());
}

} // namespace NEO
