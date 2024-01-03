/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_pooling.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/utilities/heap_allocator.h"

namespace NEO {

bool UsmMemAllocPool::initialize(SVMAllocsManager *svmMemoryManager, const UnifiedMemoryProperties &memoryProperties, size_t poolSize) {
    this->pool = svmMemoryManager->createUnifiedMemoryAllocation(poolSize, memoryProperties);
    if (nullptr == this->pool) {
        return false;
    }
    this->svmMemoryManager = svmMemoryManager;
    this->poolEnd = ptrOffset(this->pool, poolSize);
    this->chunkAllocator.reset(new HeapAllocator(startingOffset,
                                                 poolSize,
                                                 chunkAlignment));
    this->poolSize = poolSize;
    this->poolMemoryType = memoryProperties.memoryType;
    return true;
}

bool UsmMemAllocPool::isInitialized() {
    return this->pool;
}

void UsmMemAllocPool::cleanup() {
    if (isInitialized()) {
        this->svmMemoryManager->freeSVMAlloc(this->pool, true);
        this->svmMemoryManager = nullptr;
        this->pool = nullptr;
        this->poolEnd = nullptr;
        this->poolSize = 0u;
        this->poolMemoryType = InternalMemoryType::notSpecified;
    }
}

bool UsmMemAllocPool::alignmentIsAllowed(size_t alignment) {
    return alignment % chunkAlignment == 0;
}

bool UsmMemAllocPool::canBePooled(size_t size, const UnifiedMemoryProperties &memoryProperties) {
    return size <= allocationThreshold &&
           alignmentIsAllowed(memoryProperties.alignment) &&
           memoryProperties.memoryType == this->poolMemoryType &&
           memoryProperties.allocationFlags.allFlags == 0u &&
           memoryProperties.allocationFlags.allAllocFlags == 0u;
}

void *UsmMemAllocPool::createUnifiedMemoryAllocation(size_t requestedSize, const UnifiedMemoryProperties &memoryProperties) {
    void *pooledPtr = nullptr;
    if (isInitialized()) {
        if (false == canBePooled(requestedSize, memoryProperties)) {
            return nullptr;
        }
        std::unique_lock<std::mutex> lock(mtx);
        auto actualSize = requestedSize;
        size_t offset = static_cast<size_t>(this->chunkAllocator->allocateWithCustomAlignment(actualSize, memoryProperties.alignment));
        if (offset == 0) {
            return nullptr;
        }
        offset -= startingOffset;
        DEBUG_BREAK_IF(offset >= poolSize);
        pooledPtr = ptrOffset(this->pool, offset);
        {
            this->allocations.insert(pooledPtr, AllocationInfo{offset, actualSize, requestedSize});
        }
        ++this->svmMemoryManager->allocationsCounter;
    }
    return pooledPtr;
}

bool UsmMemAllocPool::isInPool(const void *ptr) {
    return ptr >= this->pool && ptr < this->poolEnd;
}

bool UsmMemAllocPool::freeSVMAlloc(void *ptr, bool blocking) {
    if (isInitialized() && isInPool(ptr)) {
        size_t offset = 0u, size = 0u;
        {
            std::unique_lock<std::mutex> lock(mtx);
            auto allocationInfo = allocations.extract(ptr);
            if (allocationInfo) {
                offset = allocationInfo->offset;
                size = allocationInfo->size;
            }
        }
        if (size > 0u) {
            this->chunkAllocator->free(offset + startingOffset, size);
            return true;
        }
    }
    return false;
}

size_t UsmMemAllocPool::getPooledAllocationSize(const void *ptr) {
    if (isInitialized() && isInPool(ptr)) {
        std::unique_lock<std::mutex> lock(mtx);
        auto allocationInfo = allocations.get(ptr);
        if (allocationInfo) {
            return allocationInfo->requestedSize;
        }
    }
    return 0u;
}

void *UsmMemAllocPool::getPooledAllocationBasePtr(const void *ptr) {
    if (isInitialized() && isInPool(ptr)) {
        std::unique_lock<std::mutex> lock(mtx);
        auto allocationInfo = allocations.get(ptr);
        if (allocationInfo) {
            return ptrOffset(this->pool, allocationInfo->offset);
        }
    }
    return nullptr;
}

} // namespace NEO