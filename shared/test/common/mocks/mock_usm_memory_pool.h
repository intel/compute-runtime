/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/unified_memory_pooling.h"

namespace NEO {
class MockUsmMemAllocPool : public UsmMemAllocPool {
  public:
    using UsmMemAllocPool::allocations;
    using UsmMemAllocPool::maxServicedSize;
    using UsmMemAllocPool::minServicedSize;
    using UsmMemAllocPool::pool;
    using UsmMemAllocPool::poolEnd;
    using UsmMemAllocPool::poolMemoryType;
    using UsmMemAllocPool::poolSize;
};

class MockUsmMemAllocPoolsManager : public UsmMemAllocPoolsManager {
  public:
    using UsmMemAllocPoolsManager::canBePooled;
    using UsmMemAllocPoolsManager::device;
    using UsmMemAllocPoolsManager::getPoolContainingAlloc;
    using UsmMemAllocPoolsManager::memoryManager;
    using UsmMemAllocPoolsManager::pools;
    using UsmMemAllocPoolsManager::totalSize;
    using UsmMemAllocPoolsManager::UsmMemAllocPoolsManager;
    uint64_t getFreeMemory() override {
        if (callBaseGetFreeMemory) {
            return UsmMemAllocPoolsManager::getFreeMemory();
        }
        return mockFreeMemory;
    }
    uint64_t mockFreeMemory = 0u;
    bool callBaseGetFreeMemory = false;
};
} // namespace NEO