/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/memory_manager/host_ptr_defines.h"

#include <map>
#include <mutex>

namespace NEO {

using HostPtrFragmentsContainer = std::map<const void *, FragmentStorage>;
class MemoryManager;
class HostPtrManager {
  public:
    FragmentStorage *getFragment(const void *inputPtr);
    OsHandleStorage prepareOsStorageForAllocation(MemoryManager &memoryManager, size_t size, const void *ptr, uint32_t rootDeviceIndex);
    void releaseHandleStorage(OsHandleStorage &fragments);
    bool releaseHostPtr(const void *ptr);
    void storeFragment(AllocationStorageData &storageData);
    void storeFragment(FragmentStorage &fragment);
    std::unique_lock<std::recursive_mutex> obtainOwnership();

  protected:
    static AllocationRequirements getAllocationRequirements(const void *inputPtr, size_t size);
    OsHandleStorage populateAlreadyAllocatedFragments(AllocationRequirements &requirements);
    FragmentStorage *getFragmentAndCheckForOverlaps(const void *inputPtr, size_t size, OverlapStatus &overlappingStatus);
    RequirementsStatus checkAllocationsForOverlapping(MemoryManager &memoryManager, AllocationRequirements *requirements);

    HostPtrFragmentsContainer::iterator findElement(const void *ptr);
    HostPtrFragmentsContainer partialAllocations;
    std::recursive_mutex allocationsMutex;
};
} // namespace NEO
