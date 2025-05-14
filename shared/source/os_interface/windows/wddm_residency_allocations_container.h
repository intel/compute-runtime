/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/memory_operations_status.h"
#include "shared/source/os_interface/windows/windows_defs.h"
#include "shared/source/utilities/spinlock.h"

#include <mutex>
#include <vector>

namespace NEO {
class Wddm;

class WddmResidentAllocationsContainer {
  public:
    WddmResidentAllocationsContainer(Wddm *wddm) : wddm(wddm) {}
    WddmResidentAllocationsContainer(Wddm *wddm, bool evictionAllowed) : wddm(wddm), isEvictionOnMakeResidentAllowed(evictionAllowed) {}
    MOCKABLE_VIRTUAL ~WddmResidentAllocationsContainer();

    MemoryOperationsStatus isAllocationResident(const D3DKMT_HANDLE &handle);
    MOCKABLE_VIRTUAL MemoryOperationsStatus evictAllResources();
    MOCKABLE_VIRTUAL MemoryOperationsStatus evictResource(const D3DKMT_HANDLE &handle);
    MemoryOperationsStatus evictResources(const D3DKMT_HANDLE *handles, const uint32_t count);
    MOCKABLE_VIRTUAL MemoryOperationsStatus makeResidentResource(const D3DKMT_HANDLE &handle, size_t size);
    MemoryOperationsStatus makeResidentResources(const D3DKMT_HANDLE *handles, const uint32_t count, size_t size, const bool forcePagingFence);
    MOCKABLE_VIRTUAL void removeResource(const D3DKMT_HANDLE &handle);

  protected:
    [[nodiscard]] MOCKABLE_VIRTUAL std::unique_lock<SpinLock> acquireLock(SpinLock &lock) {
        return std::unique_lock<SpinLock>{lock};
    }

    MemoryOperationsStatus evictAllResourcesNoLock();

    Wddm *wddm;
    std::vector<D3DKMT_HANDLE> resourceHandles;
    SpinLock resourcesLock;
    const bool isEvictionOnMakeResidentAllowed{true};
};

} // namespace NEO
