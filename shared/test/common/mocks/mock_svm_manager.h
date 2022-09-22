/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/unified_memory_manager.h"
namespace NEO {
struct MockSVMAllocsManager : public SVMAllocsManager {
  public:
    using SVMAllocsManager::memoryManager;
    using SVMAllocsManager::mtxForIndirectAccess;
    using SVMAllocsManager::multiOsContextSupport;
    using SVMAllocsManager::SVMAllocs;
    using SVMAllocsManager::SVMAllocsManager;
    using SVMAllocsManager::svmMapOperations;
    using SVMAllocsManager::usmDeviceAllocationsCache;
    using SVMAllocsManager::usmDeviceAllocationsCacheEnabled;
};
} // namespace NEO
