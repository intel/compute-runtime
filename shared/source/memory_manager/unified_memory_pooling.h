/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/utilities/heap_allocator.h"
#include "shared/source/utilities/sorted_vector.h"

namespace NEO {
class UsmMemAllocPool {
  public:
    using UnifiedMemoryProperties = SVMAllocsManager::UnifiedMemoryProperties;
    struct AllocationInfo {
        size_t offset;
        size_t size;
        size_t requestedSize;
    };
    using AllocationsInfoStorage = BaseSortedPointerWithValueVector<AllocationInfo>;

    UsmMemAllocPool() = default;
    bool initialize(SVMAllocsManager *svmMemoryManager, const UnifiedMemoryProperties &memoryProperties, size_t poolSize);
    bool isInitialized();
    void cleanup();
    bool alignmentIsAllowed(size_t alignment);
    bool canBePooled(size_t size, const UnifiedMemoryProperties &memoryProperties);
    void *createUnifiedMemoryAllocation(size_t size, const UnifiedMemoryProperties &memoryProperties);
    bool isInPool(const void *ptr);
    bool freeSVMAlloc(void *ptr, bool blocking);
    size_t getPooledAllocationSize(const void *ptr);
    void *getPooledAllocationBasePtr(const void *ptr);

    static constexpr auto allocationThreshold = 1 * MemoryConstants::megaByte;
    static constexpr auto chunkAlignment = 512u;
    static constexpr auto startingOffset = 2 * allocationThreshold;

  protected:
    size_t poolSize{};
    std::unique_ptr<HeapAllocator> chunkAllocator;
    void *pool{};
    void *poolEnd{};
    SVMAllocsManager *svmMemoryManager{};
    AllocationsInfoStorage allocations;
    std::mutex mtx;
    InternalMemoryType poolMemoryType;
};

} // namespace NEO