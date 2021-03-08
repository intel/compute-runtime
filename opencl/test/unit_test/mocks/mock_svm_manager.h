/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/unified_memory_manager.h"
namespace NEO {
struct MockSVMAllocsManager : SVMAllocsManager {
    using SVMAllocsManager::memoryManager;
    using SVMAllocsManager::multiOsContextSupport;
    using SVMAllocsManager::SVMAllocs;
    using SVMAllocsManager::SVMAllocsManager;
    using SVMAllocsManager::svmMapOperations;
};
} // namespace NEO
