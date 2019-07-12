/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/os_interface/windows/wddm_residency_allocations_container.h"
#include "unit_tests/mocks/wddm_mock_helpers.h"

namespace NEO {
class Wddm;

class MockWddmResidentAllocationsContainer : public WddmResidentAllocationsContainer {
  public:
    using WddmResidentAllocationsContainer::resourceHandles;
    using WddmResidentAllocationsContainer::resourcesLock;

    MockWddmResidentAllocationsContainer(Wddm *wddm) : WddmResidentAllocationsContainer(wddm) {}
    virtual ~MockWddmResidentAllocationsContainer() = default;

    bool makeResidentResource(const D3DKMT_HANDLE &handle) override {
        makeResidentResult.called++;
        makeResidentResult.success = WddmResidentAllocationsContainer::makeResidentResource(handle);
        return makeResidentResult.success;
    }

    EvictionStatus evictAllResources() override {
        evictAllResourcesResult.called++;
        evictAllResourcesResult.status = WddmResidentAllocationsContainer::evictAllResources();
        return evictAllResourcesResult.status;
    }

    EvictionStatus evictResource(const D3DKMT_HANDLE &handle) override {
        evictResourceResult.called++;
        evictResourceResult.status = WddmResidentAllocationsContainer::evictResource(handle);
        return evictResourceResult.status;
    }

    std::unique_lock<SpinLock> acquireLock(SpinLock &lock) override {
        acquireLockResult.called++;
        acquireLockResult.uint64ParamPassed = reinterpret_cast<uint64_t>(&lock);
        return WddmResidentAllocationsContainer::acquireLock(lock);
    }

    void removeResource(const D3DKMT_HANDLE &handle) override {
        removeResourceResult.called++;
        WddmResidentAllocationsContainer::removeResource(handle);
    }

    WddmMockHelpers::CallResult makeResidentResult;
    WddmMockHelpers::CallResult acquireLockResult;
    WddmMockHelpers::CallResult removeResourceResult;
    WddmMockHelpers::EvictCallResult evictAllResourcesResult;
    WddmMockHelpers::EvictCallResult evictResourceResult;
};

} // namespace NEO
