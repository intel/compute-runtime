/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/host_ptr_manager.h"

namespace NEO {
class MockHostPtrManager : public HostPtrManager {
  public:
    using HostPtrManager::checkAllocationsForOverlapping;
    using HostPtrManager::getAllocationRequirements;
    using HostPtrManager::getFragmentAndCheckForOverlaps;
    using HostPtrManager::populateAlreadyAllocatedFragments;
    size_t getFragmentCount() { return partialAllocations.size(); }
};
} // namespace NEO
