/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/memory_manager/eviction_status.h"
#include "core/utilities/spinlock.h"
#include "runtime/os_interface/windows/windows_defs.h"

#include <mutex>
#include <vector>

namespace NEO {
class Wddm;

class WddmResidentAllocationsContainer {
  public:
    WddmResidentAllocationsContainer(Wddm *wddm) : wddm(wddm) {}
    virtual ~WddmResidentAllocationsContainer();

    bool isAllocationResident(const D3DKMT_HANDLE &handle);
    MOCKABLE_VIRTUAL EvictionStatus evictAllResources();
    MOCKABLE_VIRTUAL EvictionStatus evictResource(const D3DKMT_HANDLE &handle);
    EvictionStatus evictResources(const D3DKMT_HANDLE *handles, const uint32_t count);
    MOCKABLE_VIRTUAL bool makeResidentResource(const D3DKMT_HANDLE &handle);
    bool makeResidentResources(const D3DKMT_HANDLE *handles, const uint32_t count);
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
