/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/memory_manager/memory_operations_status.h"
#include "core/os_interface/windows/windows_defs.h"
#include "core/utilities/spinlock.h"

#include <mutex>
#include <vector>

namespace NEO {
class Wddm;

class WddmResidentAllocationsContainer {
  public:
    WddmResidentAllocationsContainer(Wddm *wddm) : wddm(wddm) {}
    virtual ~WddmResidentAllocationsContainer();

    MemoryOperationsStatus isAllocationResident(const D3DKMT_HANDLE &handle);
    MOCKABLE_VIRTUAL MemoryOperationsStatus evictAllResources();
    MOCKABLE_VIRTUAL MemoryOperationsStatus evictResource(const D3DKMT_HANDLE &handle);
    MemoryOperationsStatus evictResources(const D3DKMT_HANDLE *handles, const uint32_t count);
    MOCKABLE_VIRTUAL MemoryOperationsStatus makeResidentResource(const D3DKMT_HANDLE &handle);
    MemoryOperationsStatus makeResidentResources(const D3DKMT_HANDLE *handles, const uint32_t count);
    MOCKABLE_VIRTUAL void removeResource(const D3DKMT_HANDLE &handle);

  protected:
    MOCKABLE_VIRTUAL std::unique_lock<SpinLock> acquireLock(SpinLock &lock) {
        return std::unique_lock<SpinLock>{lock};
    }

    Wddm *wddm;
    std::vector<D3DKMT_HANDLE> resourceHandles;
    SpinLock resourcesLock;
};

} // namespace NEO
