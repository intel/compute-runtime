/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_memory_operations_handler_default.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include <algorithm>

namespace NEO {

DrmMemoryOperationsHandlerDefault::DrmMemoryOperationsHandlerDefault() = default;
DrmMemoryOperationsHandlerDefault::~DrmMemoryOperationsHandlerDefault() = default;

MemoryOperationsStatus DrmMemoryOperationsHandlerDefault::makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable) {
    std::lock_guard<std::mutex> lock(mutex);
    this->residency.insert(gfxAllocations.begin(), gfxAllocations.end());
    return MemoryOperationsStatus::SUCCESS;
}

MemoryOperationsStatus DrmMemoryOperationsHandlerDefault::makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations) {
    OsContext *osContext = nullptr;
    return this->makeResidentWithinOsContext(osContext, gfxAllocations, false);
}

MemoryOperationsStatus DrmMemoryOperationsHandlerDefault::evictWithinOsContext(OsContext *osContext, GraphicsAllocation &gfxAllocation) {
    std::lock_guard<std::mutex> lock(mutex);
    this->residency.erase(&gfxAllocation);
    return MemoryOperationsStatus::SUCCESS;
}

MemoryOperationsStatus DrmMemoryOperationsHandlerDefault::evict(Device *device, GraphicsAllocation &gfxAllocation) {
    OsContext *osContext = nullptr;
    return this->evictWithinOsContext(osContext, gfxAllocation);
}

MemoryOperationsStatus DrmMemoryOperationsHandlerDefault::isResident(Device *device, GraphicsAllocation &gfxAllocation) {
    std::lock_guard<std::mutex> lock(mutex);
    auto ret = this->residency.find(&gfxAllocation);
    if (ret == this->residency.end()) {
        return MemoryOperationsStatus::MEMORY_NOT_FOUND;
    }
    return MemoryOperationsStatus::SUCCESS;
}

void DrmMemoryOperationsHandlerDefault::mergeWithResidencyContainer(OsContext *osContext, ResidencyContainer &residencyContainer) {
    for (auto gfxAllocation = this->residency.begin(); gfxAllocation != this->residency.end(); gfxAllocation++) {
        auto ret = std::find(residencyContainer.begin(), residencyContainer.end(), *gfxAllocation);
        if (ret == residencyContainer.end()) {
            residencyContainer.push_back(*gfxAllocation);
        }
    }
}

std::unique_lock<std::mutex> DrmMemoryOperationsHandlerDefault::lockHandlerIfUsed() {
    std::unique_lock<std::mutex> lock(this->mutex);
    if (this->residency.size()) {
        return lock;
    }
    return std::unique_lock<std::mutex>();
}

void DrmMemoryOperationsHandlerDefault::evictUnusedAllocations(bool waitForCompletion, bool isLockNeeded) {
}

} // namespace NEO
