/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/multi_graphics_allocation.h"

#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/migration_sync_data.h"

namespace NEO {

MultiGraphicsAllocation::MultiGraphicsAllocation(uint32_t maxRootDeviceIndex) {
    graphicsAllocations.resize(maxRootDeviceIndex + 1);
    for (auto &allocation : graphicsAllocations) {
        allocation = nullptr;
    }
}

MultiGraphicsAllocation::MultiGraphicsAllocation(const MultiGraphicsAllocation &multiGraphicsAllocation) {
    this->graphicsAllocations = multiGraphicsAllocation.graphicsAllocations;
    this->migrationSyncData = multiGraphicsAllocation.migrationSyncData;
    this->isMultiStorage = multiGraphicsAllocation.isMultiStorage;
    if (migrationSyncData) {
        migrationSyncData->incRefInternal();
    }
}
MultiGraphicsAllocation::MultiGraphicsAllocation(MultiGraphicsAllocation &&multiGraphicsAllocation) noexcept {
    this->graphicsAllocations = std::move(multiGraphicsAllocation.graphicsAllocations);
    std::swap(this->migrationSyncData, multiGraphicsAllocation.migrationSyncData);
    this->isMultiStorage = multiGraphicsAllocation.isMultiStorage;
};

MultiGraphicsAllocation::~MultiGraphicsAllocation() {
    if (migrationSyncData) {
        migrationSyncData->decRefInternal();
    }
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
    if (rootDeviceIndex >= graphicsAllocations.size()) {
        return nullptr;
    }
    return graphicsAllocations[rootDeviceIndex];
}

AllocationType MultiGraphicsAllocation::getAllocationType() const {
    return getDefaultGraphicsAllocation()->getAllocationType();
}

bool MultiGraphicsAllocation::isCoherent() const {
    return getDefaultGraphicsAllocation()->isCoherent();
}

StackVec<GraphicsAllocation *, 1> const &MultiGraphicsAllocation::getGraphicsAllocations() const {
    return graphicsAllocations;
}

void MultiGraphicsAllocation::setMultiStorage(bool value) {
    isMultiStorage = value;
    if (isMultiStorage && !migrationSyncData) {
        auto graphicsAllocation = getDefaultGraphicsAllocation();
        UNRECOVERABLE_IF(!graphicsAllocation);
        migrationSyncData = createMigrationSyncDataFunc(graphicsAllocation->getUnderlyingBufferSize());
        migrationSyncData->incRefInternal();
    }
}

bool MultiGraphicsAllocation::requiresMigrations() const {
    if (migrationSyncData && migrationSyncData->isMigrationInProgress()) {
        return false;
    }
    return isMultiStorage;
}

decltype(MultiGraphicsAllocation::createMigrationSyncDataFunc) MultiGraphicsAllocation::createMigrationSyncDataFunc = [](size_t size) -> MigrationSyncData * {
    return new MigrationSyncData(size);
};
} // namespace NEO
