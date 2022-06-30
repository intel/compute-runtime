/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/memory_operations_status.h"
#include "shared/source/os_interface/windows/wddm_residency_allocations_container.h"
#include "shared/test/common/mocks/wddm_mock_helpers.h"

namespace NEO {
class Wddm;

class MockWddmResidentAllocationsContainer : public WddmResidentAllocationsContainer {
  public:
    using WddmResidentAllocationsContainer::resourceHandles;
    using WddmResidentAllocationsContainer::resourcesLock;

    MockWddmResidentAllocationsContainer(Wddm *wddm) : WddmResidentAllocationsContainer(wddm) {}

    MemoryOperationsStatus makeResidentResource(const D3DKMT_HANDLE &handle, size_t size) override {
        makeResidentResult.called++;
        makeResidentResult.operationSuccess = WddmResidentAllocationsContainer::makeResidentResource(handle, size);
        return makeResidentResult.operationSuccess;
    }

    MemoryOperationsStatus evictAllResources() override {
        evictAllResourcesResult.called++;
        evictAllResourcesResult.operationSuccess = WddmResidentAllocationsContainer::evictAllResources();
        return evictAllResourcesResult.operationSuccess;
    }

    MemoryOperationsStatus evictResource(const D3DKMT_HANDLE &handle) override {
        evictResourceResult.called++;
        evictResourceResult.operationSuccess = WddmResidentAllocationsContainer::evictResource(handle);
        return evictResourceResult.operationSuccess;
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

    WddmMockHelpers::MemoryOperationResult makeResidentResult;
    WddmMockHelpers::MemoryOperationResult acquireLockResult;
    WddmMockHelpers::MemoryOperationResult removeResourceResult;
    WddmMockHelpers::MemoryOperationResult evictAllResourcesResult;
    WddmMockHelpers::MemoryOperationResult evictResourceResult;
};

} // namespace NEO
