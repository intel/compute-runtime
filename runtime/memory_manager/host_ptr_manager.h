/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <map>
#include <mutex>
#include "runtime/memory_manager/host_ptr_defines.h"

namespace OCLRT {

using HostPtrFragmentsContainer = std::map<const void *, FragmentStorage>;
class MemoryManager;
class HostPtrManager {
  public:
    FragmentStorage *getFragment(const void *inputPtr);
    OsHandleStorage prepareOsStorageForAllocation(MemoryManager &memoryManager, size_t size, const void *ptr);
    void releaseHandleStorage(OsHandleStorage &fragments);
    bool releaseHostPtr(const void *ptr);
    void storeFragment(AllocationStorageData &storageData);
    void storeFragment(FragmentStorage &fragment);

  protected:
    static AllocationRequirements getAllocationRequirements(const void *inputPtr, size_t size);
    OsHandleStorage populateAlreadyAllocatedFragments(AllocationRequirements &requirements, CheckedFragments *checkedFragments);
    FragmentStorage *getFragmentAndCheckForOverlaps(const void *inputPtr, size_t size, OverlapStatus &overlappingStatus);
    RequirementsStatus checkAllocationsForOverlapping(MemoryManager &memoryManager, AllocationRequirements *requirements, CheckedFragments *checkedFragments);

    HostPtrFragmentsContainer::iterator findElement(const void *ptr);
    HostPtrFragmentsContainer partialAllocations;
    std::recursive_mutex allocationsMutex;
};
} // namespace OCLRT
