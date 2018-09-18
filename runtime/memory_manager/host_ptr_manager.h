/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <map>
#include "runtime/helpers/aligned_memory.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/host_ptr_defines.h"

namespace OCLRT {

typedef std::map<const void *, FragmentStorage> HostPtrFragmentsContainer;

class HostPtrManager {
  public:
    static AllocationRequirements getAllocationRequirements(const void *inputPtr, size_t size);
    OsHandleStorage populateAlreadyAllocatedFragments(AllocationRequirements &requirements, CheckedFragments *checkedFragments);
    void storeFragment(FragmentStorage &fragment);
    void storeFragment(AllocationStorageData &storageData);

    void releaseHandleStorage(OsHandleStorage &fragments);
    bool releaseHostPtr(const void *ptr);

    FragmentStorage *getFragment(const void *inputPtr);
    size_t getFragmentCount() { return partialAllocations.size(); }
    FragmentStorage *getFragmentAndCheckForOverlaps(const void *inputPtr, size_t size, OverlapStatus &overlappingStatus);

  private:
    std::map<const void *, FragmentStorage>::iterator findElement(const void *ptr);

    HostPtrFragmentsContainer partialAllocations;
    std::mutex allocationsMutex;
};
} // namespace OCLRT
