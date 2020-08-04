/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/multi_graphics_allocation.h"

namespace NEO {

MultiGraphicsAllocation::MultiGraphicsAllocation(uint32_t maxRootDeviceIndex) {
    graphicsAllocations.resize(maxRootDeviceIndex + 1);
}

GraphicsAllocation *MultiGraphicsAllocation::getDefaultGraphicsAllocation() const {
    for (auto &allocation : graphicsAllocations) {
        if (allocation) {
            return allocation;
        }
    }
    return nullptr;
}

void MultiGraphicsAllocation::addAllocation(GraphicsAllocation *graphicsAllocation) {
    UNRECOVERABLE_IF(graphicsAllocation == nullptr);
    UNRECOVERABLE_IF(graphicsAllocations.size() < graphicsAllocation->getRootDeviceIndex() + 1);
    graphicsAllocations[graphicsAllocation->getRootDeviceIndex()] = graphicsAllocation;
}

void MultiGraphicsAllocation::removeAllocation(uint32_t rootDeviceIndex) {
    graphicsAllocations[rootDeviceIndex] = nullptr;
}

GraphicsAllocation *MultiGraphicsAllocation::getGraphicsAllocation(uint32_t rootDeviceIndex) const {
    return graphicsAllocations[rootDeviceIndex];
}

GraphicsAllocation::AllocationType MultiGraphicsAllocation::getAllocationType() const {
    return getDefaultGraphicsAllocation()->getAllocationType();
}

bool MultiGraphicsAllocation::isCoherent() const {
    return getDefaultGraphicsAllocation()->isCoherent();
}

StackVec<GraphicsAllocation *, 1> const &MultiGraphicsAllocation::getGraphicsAllocations() const {
    return graphicsAllocations;
}

} // namespace NEO
