/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/wddm_residency_allocations_container.h"

#include "runtime/os_interface/windows/wddm/wddm.h"
#include "runtime/os_interface/windows/wddm_allocation.h"

#include <algorithm>

namespace NEO {
WddmResidentAllocationsContainer::~WddmResidentAllocationsContainer() {
    evictAllResources();
}

bool WddmResidentAllocationsContainer::isAllocationResident(const D3DKMT_HANDLE &handle) {
    auto lock = acquireLock(resourcesLock);
    auto position = std::find(resourceHandles.begin(), resourceHandles.end(), handle);
    return position != resourceHandles.end();
}

EvictionStatus WddmResidentAllocationsContainer::evictAllResources() {
    decltype(resourceHandles) resourcesToEvict;
    auto lock = acquireLock(resourcesLock);
    resourceHandles.swap(resourcesToEvict);
    if (resourcesToEvict.empty()) {
        return EvictionStatus::NOT_APPLIED;
    }
    uint64_t sizeToTrim = 0;
    uint32_t evictedResources = static_cast<uint32_t>(resourcesToEvict.size());
    bool success = wddm->evict(resourcesToEvict.data(), evictedResources, sizeToTrim);
    return success ? EvictionStatus::SUCCESS : EvictionStatus::FAILED;
}

EvictionStatus WddmResidentAllocationsContainer::evictResource(const D3DKMT_HANDLE &handle) {
    return evictResources(&handle, 1u);
}

EvictionStatus WddmResidentAllocationsContainer::evictResources(const D3DKMT_HANDLE *handles, const uint32_t count) {
    auto lock = acquireLock(resourcesLock);
    auto position = std::find(resourceHandles.begin(), resourceHandles.end(), handles[0]);
    if (position == resourceHandles.end()) {
        return EvictionStatus::NOT_APPLIED;
    }
    auto distance = static_cast<size_t>(std::distance(resourceHandles.begin(), position));
    UNRECOVERABLE_IF(distance + count > resourceHandles.size());
    resourceHandles.erase(position, position + count);
    uint64_t sizeToTrim = 0;
    if (!wddm->evict(handles, count, sizeToTrim)) {
        return EvictionStatus::FAILED;
    }
    return EvictionStatus::SUCCESS;
}

bool WddmResidentAllocationsContainer::makeResidentResource(const D3DKMT_HANDLE &handle) {
    return makeResidentResources(&handle, 1u);
}

bool WddmResidentAllocationsContainer::makeResidentResources(const D3DKMT_HANDLE *handles, const uint32_t count) {
    bool madeResident = false;
    while (!(madeResident = wddm->makeResident(handles, count, false, nullptr))) {
        if (evictAllResources() == EvictionStatus::SUCCESS) {
            continue;
        }
        if (!wddm->makeResident(handles, count, false, nullptr)) {
            DEBUG_BREAK_IF(true);
            return false;
        };
        break;
    }
    DEBUG_BREAK_IF(!madeResident);
    auto lock = acquireLock(resourcesLock);
    for (uint32_t i = 0; i < count; i++) {
        resourceHandles.push_back(handles[i]);
    }
    lock.unlock();
    wddm->waitOnPagingFenceFromCpu();
    return madeResident;
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
