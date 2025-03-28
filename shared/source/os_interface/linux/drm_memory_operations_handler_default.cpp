/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_memory_operations_handler_default.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/os_context.h"

#include <algorithm>
#include <iostream>

namespace NEO {

DrmMemoryOperationsHandlerDefault::DrmMemoryOperationsHandlerDefault(uint32_t rootDeviceIndex)
    : DrmMemoryOperationsHandler(rootDeviceIndex){};
;
DrmMemoryOperationsHandlerDefault::~DrmMemoryOperationsHandlerDefault() = default;

MemoryOperationsStatus DrmMemoryOperationsHandlerDefault::makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable, const bool forcePagingFence, const bool acquireLock) {
    std::unique_lock<std::mutex> lock;
    if (acquireLock) {
        lock = std::unique_lock<std::mutex>(this->mutex);
    }

    this->residency.insert(this->residency.end(), gfxAllocations.begin(), gfxAllocations.end());
    this->newResourcesSinceLastRingSubmit = true;
    return MemoryOperationsStatus::success;
}

MemoryOperationsStatus DrmMemoryOperationsHandlerDefault::makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations, bool isDummyExecNeeded, const bool forcePagingFence) {
    OsContext *osContext = nullptr;
    auto ret = this->makeResidentWithinOsContext(osContext, gfxAllocations, false, forcePagingFence, true);
    if (!isDummyExecNeeded || ret != MemoryOperationsStatus::success) {
        return ret;
    }
    ret = flushDummyExec(device, gfxAllocations);
    if (ret != MemoryOperationsStatus::success) {
        for (auto &alloc : gfxAllocations) {
            evictWithinOsContext(osContext, *alloc);
        }
    }
    return ret;
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
    return this->makeResidentWithinOsContext(osContext, gfxAllocations, false, false, true);
}

MemoryOperationsStatus DrmMemoryOperationsHandlerDefault::evictWithinOsContext(OsContext *osContext, GraphicsAllocation &gfxAllocation) {
    std::lock_guard<std::mutex> lock(mutex);
    auto ret = std::find(this->residency.begin(), this->residency.end(), &gfxAllocation);
    if (ret != this->residency.end()) {
        this->residency.erase(ret);
        this->newResourcesSinceLastRingSubmit = true;
    }
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
    auto ret = std::find(this->residency.begin(), this->residency.end(), &gfxAllocation);
    if (ret == this->residency.end()) {
        return MemoryOperationsStatus::memoryNotFound;
    }
    return MemoryOperationsStatus::success;
}

MemoryOperationsStatus DrmMemoryOperationsHandlerDefault::mergeWithResidencyContainer(OsContext *osContext, ResidencyContainer &residencyContainer) {
    if (osContext->isDirectSubmissionLightActive()) {
        if (this->newResourcesSinceLastRingSubmit) {
            residencyContainer.insert(residencyContainer.end(), this->residency.begin(), this->residency.end());
        }
    } else {
        for (auto gfxAllocation = this->residency.begin(); gfxAllocation != this->residency.end(); gfxAllocation++) {
            auto ret = std::find(residencyContainer.begin(), residencyContainer.end(), *gfxAllocation);
            if (ret == residencyContainer.end()) {
                residencyContainer.push_back(*gfxAllocation);
            }
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

bool DrmMemoryOperationsHandlerDefault::obtainAndResetNewResourcesSinceLastRingSubmit() {
    auto ret = this->newResourcesSinceLastRingSubmit;
    this->newResourcesSinceLastRingSubmit = false;
    return ret;
}

MemoryOperationsStatus DrmMemoryOperationsHandlerDefault::flushDummyExec(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations) {
    std::lock_guard<std::mutex> lock(mutex);
    auto memoryManager = reinterpret_cast<DrmMemoryManager *>(device->getMemoryManager());

    std::vector<BufferObject *> boArray;
    uint32_t boCount = 0;
    for (auto &alloc : this->residency) {
        for (auto &bo : reinterpret_cast<DrmAllocation *>(alloc)->getBOs()) {
            if (bo != 0) {
                boArray.push_back(bo);
                boCount++;
            }
        }
    }

    auto submissionStatus = memoryManager->emitPinningRequestForBoContainer(boArray.data(), boCount, rootDeviceIndex);
    if (submissionStatus != SubmissionStatus::success) {
        return MemoryOperationsStatus::outOfMemory;
    }
    return MemoryOperationsStatus::success;
}
} // namespace NEO
