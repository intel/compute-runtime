/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/memory_manager/svm_memory_manager.h"
namespace NEO {
struct MockSVMAllocsManager : SVMAllocsManager {

    using SVMAllocsManager::memoryManager;
    using SVMAllocsManager::SVMAllocs;
    using SVMAllocsManager::SVMAllocsManager;
    using SVMAllocsManager::svmMapOperations;
};
} // namespace NEO