/*
 * Copyright (C) 2023-2025 Intel Corporation
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

    bool initialize(SVMAllocsManager *svmMemoryManager, const UnifiedMemoryProperties &memoryProperties, size_t poolSize, size_t minServicedSize, size_t maxServicedSize) override {
        if (callBaseInitialize) {
            return UsmMemAllocPool::initialize(svmMemoryManager, memoryProperties, poolSize, minServicedSize, maxServicedSize);
        }
        return initializeResult;
    }
    bool callBaseInitialize = true;
    bool initializeResult = false;

    void *createUnifiedMemoryAllocation(size_t size, const UnifiedMemoryProperties &memoryProperties) override {
        if (callBaseCreateUnifiedMemoryAllocation) {
            return UsmMemAllocPool::createUnifiedMemoryAllocation(size, memoryProperties);
        }
        return createUnifiedMemoryAllocationResult;
    }
    bool callBaseCreateUnifiedMemoryAllocation = true;
    void *createUnifiedMemoryAllocationResult = nullptr;

    void cleanup() override {
        ++cleanupCalled;
        if (callBaseCleanup) {
            UsmMemAllocPool::cleanup();
        }
    }
    uint32_t cleanupCalled = 0u;
    bool callBaseCleanup = true;

    bool freeSVMAlloc(const void *ptr, bool blocking) override {
        ++freeSVMAllocCalled;
        return UsmMemAllocPool::freeSVMAlloc(ptr, blocking);
    };
    uint32_t freeSVMAllocCalled = 0u;
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