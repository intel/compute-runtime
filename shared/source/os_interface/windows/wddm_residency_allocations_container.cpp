/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm_residency_allocations_container.h"

#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm_allocation.h"

#include <algorithm>

namespace NEO {
WddmResidentAllocationsContainer::~WddmResidentAllocationsContainer() {
    evictAllResourcesNoLock();
}

MemoryOperationsStatus WddmResidentAllocationsContainer::isAllocationResident(const D3DKMT_HANDLE &handle) {
    auto lock = acquireLock(resourcesLock);
    auto position = std::find(resourceHandles.begin(), resourceHandles.end(), handle);
    return position != resourceHandles.end() ? MemoryOperationsStatus::success : MemoryOperationsStatus::memoryNotFound;
}

MemoryOperationsStatus WddmResidentAllocationsContainer::evictAllResources() {
    auto lock = acquireLock(resourcesLock);
    return evictAllResourcesNoLock();
}

MemoryOperationsStatus WddmResidentAllocationsContainer::evictAllResourcesNoLock() {
    decltype(resourceHandles) resourcesToEvict;
    resourceHandles.swap(resourcesToEvict);
    if (resourcesToEvict.empty()) {
        return MemoryOperationsStatus::memoryNotFound;
    }
    uint64_t sizeToTrim = 0;
    uint32_t evictedResources = static_cast<uint32_t>(resourcesToEvict.size());
    bool success = wddm->evict(resourcesToEvict.data(), evictedResources, sizeToTrim, true);
    return success ? MemoryOperationsStatus::success : MemoryOperationsStatus::failed;
}

MemoryOperationsStatus WddmResidentAllocationsContainer::evictResource(const D3DKMT_HANDLE &handle) {
    return evictResources(&handle, 1u);
}

MemoryOperationsStatus WddmResidentAllocationsContainer::evictResources(const D3DKMT_HANDLE *handles, const uint32_t count) {
    auto lock = acquireLock(resourcesLock);
    auto position = std::find(resourceHandles.begin(), resourceHandles.end(), handles[0]);
    if (position == resourceHandles.end()) {
        return MemoryOperationsStatus::memoryNotFound;
    }
    auto distance = static_cast<size_t>(std::distance(resourceHandles.begin(), position));
    UNRECOVERABLE_IF(distance + count > resourceHandles.size());
    resourceHandles.erase(position, position + count);
    uint64_t sizeToTrim = 0;
    if (!wddm->evict(handles, count, sizeToTrim, true)) {
        return MemoryOperationsStatus::failed;
    }
    return MemoryOperationsStatus::success;
}

MemoryOperationsStatus WddmResidentAllocationsContainer::makeResidentResource(const D3DKMT_HANDLE &handle, size_t size) {
    return makeResidentResources(&handle, 1u, size, false);
}

MemoryOperationsStatus WddmResidentAllocationsContainer::makeResidentResources(const D3DKMT_HANDLE *handles, const uint32_t count, size_t size, const bool forcePagingFence) {
    while (!wddm->makeResident(handles, count, false, nullptr, size)) {
        if (!isEvictionOnMakeResidentAllowed) {
            DEBUG_BREAK_IF(true);
            return MemoryOperationsStatus::outOfMemory;
        }
        if (evictAllResources() == MemoryOperationsStatus::success) {
            continue;
        }
        if (!wddm->makeResident(handles, count, true, nullptr, size)) {
            DEBUG_BREAK_IF(true);
            return MemoryOperationsStatus::outOfMemory;
        };
        break;
    }
    auto lock = acquireLock(resourcesLock);
    for (uint32_t i = 0; i < count; i++) {
        resourceHandles.push_back(handles[i]);
    }
    lock.unlock();
    wddm->waitOnPagingFenceFromCpu(false);
    return MemoryOperationsStatus::success;
}

void WddmResidentAllocationsContainer::removeResource(const D3DKMT_HANDLE &handle) {
    auto lock = acquireLock(resourcesLock);
    auto position = std::find(resourceHandles.begin(), resourceHandles.end(), handle);
    if (position == resourceHandles.end()) {
        return;
    }
    *position = resourceHandles.back();
    resourceHandles.pop_back();
}

} // namespace NEO
