/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/memory_manager/svm_memory_manager.h"
namespace OCLRT {
struct MockSVMAllocsManager : SVMAllocsManager {

    using SVMAllocsManager::memoryManager;
    using SVMAllocsManager::SVMAllocs;
    using SVMAllocsManager::SVMAllocsManager;
};
} // namespace OCLRT