/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/host_ptr_defines.h"

#include <map>
#include <mutex>

namespace NEO {

struct HostPtrEntryKey {
    const void *ptr = nullptr;
    uint32_t rootDeviceIndex = std::numeric_limits<uint32_t>::max();

    bool operator<(const HostPtrEntryKey &key) const {
        return rootDeviceIndex < key.rootDeviceIndex ||
               ((ptr < key.ptr) && (rootDeviceIndex == key.rootDeviceIndex));
    }
};

using HostPtrFragmentsContainer = std::map<HostPtrEntryKey, FragmentStorage>;
class MemoryManager;
class HostPtrManager {
  public:
    FragmentStorage *getFragment(HostPtrEntryKey key);
    OsHandleStorage prepareOsStorageForAllocation(MemoryManager &memoryManager, size_t size, const void *ptr, uint32_t rootDeviceIndex);
    void releaseHandleStorage(uint32_t rootDeviceIndex, OsHandleStorage &fragments);
    bool releaseHostPtr(uint32_t rootDeviceIndex, const void *ptr);
    void storeFragment(uint32_t rootDeviceIndex, AllocationStorageData &storageData);
    void storeFragment(uint32_t rootDeviceIndex, FragmentStorage &fragment);
    [[nodiscard]] std::unique_lock<std::recursive_mutex> obtainOwnership();

  protected:
    static AllocationRequirements getAllocationRequirements(uint32_t rootDeviceIndex, const void *inputPtr, size_t size);
    OsHandleStorage populateAlreadyAllocatedFragments(AllocationRequirements &requirements);
    FragmentStorage *getFragmentAndCheckForOverlaps(uint32_t rootDeviceIndex, const void *inputPtr, size_t size, OverlapStatus &overlappingStatus);
    RequirementsStatus checkAllocationsForOverlapping(MemoryManager &memoryManager, AllocationRequirements *requirements);

    HostPtrFragmentsContainer::iterator findElement(HostPtrEntryKey key);
    HostPtrFragmentsContainer partialAllocations;
    std::recursive_mutex allocationsMutex;
};
} // namespace NEO
