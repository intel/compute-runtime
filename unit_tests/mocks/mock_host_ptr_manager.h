/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/memory_manager/host_ptr_manager.h"

namespace OCLRT {
class MockHostPtrManager : public HostPtrManager {
  public:
    using HostPtrManager::checkAllocationsForOverlapping;
    using HostPtrManager::getAllocationRequirements;
    using HostPtrManager::getFragmentAndCheckForOverlaps;
    using HostPtrManager::populateAlreadyAllocatedFragments;
    size_t getFragmentCount() { return partialAllocations.size(); }
};
} // namespace OCLRT
