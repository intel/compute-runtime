/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_memory_operations_handler_default.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"

#include <algorithm>

namespace NEO {

DrmMemoryOperationsHandlerDefault::DrmMemoryOperationsHandlerDefault(uint32_t rootDeviceIndex)
    : DrmMemoryOperationsHandler(rootDeviceIndex){};
;
DrmMemoryOperationsHandlerDefault::~DrmMemoryOperationsHandlerDefault() = default;

MemoryOperationsStatus DrmMemoryOperationsHandlerDefault::makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable) {
    std::lock_guard<std::mutex> lock(mutex);
    this->residency.insert(gfxAllocations.begin(), gfxAllocations.end());
    return MemoryOperationsStatus::success;
}

MemoryOperationsStatus DrmMemoryOperationsHandlerDefault::makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations) {
    OsContext *osContext = nullptr;
    return this->makeResidentWithinOsContext(osContext, gfxAllocations, false);
}

MemoryOperationsStatus DrmMemoryOperationsHandlerDefault::lock(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations) {
    OsContext *osContext = nullptr;
    for (auto gfxAllocation = gfxAllocations.begin(); gfxAllocation != gfxAllocations.end(); gfxAllocation++) {
        auto drmAllocation = static_cast<DrmAllocation *>(*gfxAllocation);
        drmAllocation->setLockedMemory(true);
        for (auto bo : drmAllocation->getBOs()) {
            bo->requireExplicitLockedMemory(true);
        }
    }
    return this->makeResidentWithinOsContext(osContext, gfxAllocations, false);
}

MemoryOperationsStatus DrmMemoryOperationsHandlerDefault::evictWithinOsContext(OsContext *osContext, GraphicsAllocation &gfxAllocation) {
    std::lock_guard<std::mutex> lock(mutex);
    this->residency.erase(&gfxAllocation);
    return MemoryOperationsStatus::success;
}

MemoryOperationsStatus DrmMemoryOperationsHandlerDefault::evict(Device *device, GraphicsAllocation &gfxAllocation) {
    OsContext *osContext = nullptr;
    auto drmAllocation = static_cast<DrmAllocation *>(&gfxAllocation);
    drmAllocation->setLockedMemory(false);
    if (drmAllocation->storageInfo.isChunked || drmAllocation->storageInfo.getNumBanks() == 1) {
        auto bo = drmAllocation->getBO();
        bo->requireExplicitLockedMemory(false);
    } else {
        for (auto bo : drmAllocation->getBOs()) {
            bo->requireExplicitLockedMemory(false);
        }
    }
    return this->evictWithinOsContext(osContext, gfxAllocation);
}

MemoryOperationsStatus DrmMemoryOperationsHandlerDefault::isResident(Device *device, GraphicsAllocation &gfxAllocation) {
    std::lock_guard<std::mutex> lock(mutex);
    auto ret = this->residency.find(&gfxAllocation);
    if (ret == this->residency.end()) {
        return MemoryOperationsStatus::memoryNotFound;
    }
    return MemoryOperationsStatus::success;
}

MemoryOperationsStatus DrmMemoryOperationsHandlerDefault::mergeWithResidencyContainer(OsContext *osContext, ResidencyContainer &residencyContainer) {
    for (auto gfxAllocation = this->residency.begin(); gfxAllocation != this->residency.end(); gfxAllocation++) {
        auto ret = std::find(residencyContainer.begin(), residencyContainer.end(), *gfxAllocation);
        if (ret == residencyContainer.end()) {
            residencyContainer.push_back(*gfxAllocation);
        }
    }
    return MemoryOperationsStatus::success;
}

std::unique_lock<std::mutex> DrmMemoryOperationsHandlerDefault::lockHandlerIfUsed() {
    std::unique_lock<std::mutex> lock(this->mutex);
    if (this->residency.size()) {
        return lock;
    }
    return std::unique_lock<std::mutex>();
}

MemoryOperationsStatus DrmMemoryOperationsHandlerDefault::evictUnusedAllocations(bool waitForCompletion, bool isLockNeeded) {
    return MemoryOperationsStatus::success;
}

} // namespace NEO
