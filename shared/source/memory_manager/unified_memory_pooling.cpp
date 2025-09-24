/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_pooling.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/utilities/heap_allocator.h"

namespace NEO {

bool UsmMemAllocPool::initialize(SVMAllocsManager *svmMemoryManager, const UnifiedMemoryProperties &memoryProperties, size_t poolSize, size_t minServicedSize, size_t maxServicedSize) {
    auto poolAllocation = svmMemoryManager->createUnifiedMemoryAllocation(poolSize, memoryProperties);
    if (nullptr == poolAllocation) {
        return false;
    }
    auto svmData = svmMemoryManager->getSVMAlloc(poolAllocation);
    return initialize(svmMemoryManager, poolAllocation, svmData, minServicedSize, maxServicedSize);
}

bool UsmMemAllocPool::initialize(SVMAllocsManager *svmMemoryManager, void *ptr, SvmAllocationData *svmData, size_t minServicedSize, size_t maxServicedSize) {
    DEBUG_BREAK_IF(nullptr == ptr);
    this->pool = ptr;
    this->svmMemoryManager = svmMemoryManager;
    this->poolEnd = ptrOffset(this->pool, svmData->size);
    this->chunkAllocator.reset(new HeapAllocator(castToUint64(this->pool),
                                                 svmData->size,
                                                 chunkAlignment,
                                                 maxServicedSize / 2));
    this->poolMemoryType = svmData->memoryType;
    this->poolInfo.minServicedSize = minServicedSize;
    this->poolInfo.maxServicedSize = maxServicedSize;
    this->poolInfo.poolSize = svmData->size;
    return true;
}

bool UsmMemAllocPool::isInitialized() const {
    return this->pool;
}

size_t UsmMemAllocPool::getPoolSize() const {
    return this->poolInfo.poolSize;
}

void UsmMemAllocPool::cleanup() {
    if (isInitialized()) {
        [[maybe_unused]] const auto status = this->svmMemoryManager->freeSVMAlloc(this->pool, true);
        DEBUG_BREAK_IF(false == status);
        this->svmMemoryManager = nullptr;
        this->pool = nullptr;
        this->poolEnd = nullptr;
        this->poolInfo.poolSize = 0u;
        this->poolMemoryType = InternalMemoryType::notSpecified;
    }
}

bool UsmMemAllocPool::alignmentIsAllowed(size_t alignment) {
    return alignment % chunkAlignment == 0 && alignment <= poolAlignment;
}

bool UsmMemAllocPool::sizeIsAllowed(size_t size) {
    return size >= poolInfo.minServicedSize && size <= poolInfo.maxServicedSize;
}

bool UsmMemAllocPool::flagsAreAllowed(const UnifiedMemoryProperties &memoryProperties) {
    auto flagsWithoutCompression = memoryProperties.allocationFlags;
    flagsWithoutCompression.flags.compressedHint = 0u;
    flagsWithoutCompression.flags.uncompressedHint = 0u;

    return flagsWithoutCompression.allFlags == 0u &&
           memoryProperties.allocationFlags.allAllocFlags == 0u;
}

double UsmMemAllocPool::getPercentOfFreeMemoryForRecycling(InternalMemoryType memoryType) {
    if (InternalMemoryType::deviceUnifiedMemory == memoryType) {
        return 0.08;
    }
    if (InternalMemoryType::hostUnifiedMemory == memoryType) {
        return 0.02;
    }
    return 0.0;
}

bool UsmMemAllocPool::canBePooled(size_t size, const UnifiedMemoryProperties &memoryProperties) {
    return sizeIsAllowed(size) &&
           alignmentIsAllowed(memoryProperties.alignment) &&
           flagsAreAllowed(memoryProperties) &&
           memoryProperties.memoryType == this->poolMemoryType;
}

void *UsmMemAllocPool::createUnifiedMemoryAllocation(size_t requestedSize, const UnifiedMemoryProperties &memoryProperties) {
    void *pooledPtr = nullptr;
    if (isInitialized()) {
        if (false == canBePooled(requestedSize, memoryProperties)) {
            return nullptr;
        }
        std::unique_lock<std::mutex> lock(mtx);
        auto actualSize = requestedSize;
        auto pooledAddress = this->chunkAllocator->allocateWithCustomAlignment(actualSize, memoryProperties.alignment);
        if (!pooledAddress) {
            return nullptr;
        }

        pooledPtr = addrToPtr(pooledAddress);
        this->allocations.insert(pooledPtr, AllocationInfo{pooledAddress, actualSize, requestedSize});

        ++this->svmMemoryManager->allocationsCounter;
    }
    return pooledPtr;
}

bool UsmMemAllocPool::isInPool(const void *ptr) const {
    return ptr >= this->pool && ptr < this->poolEnd;
}

bool UsmMemAllocPool::isEmpty() const {
    return 0u == this->allocations.getNumAllocs();
}

bool UsmMemAllocPool::freeSVMAlloc(const void *ptr, bool blocking) {
    if (isInitialized() && isInPool(ptr)) {
        std::unique_lock<std::mutex> lock(mtx);
        auto allocationInfo = allocations.extract(ptr);
        if (allocationInfo) {
            DEBUG_BREAK_IF(allocationInfo->size == 0 || allocationInfo->address == 0);
            this->chunkAllocator->free(allocationInfo->address, allocationInfo->size);
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
            return addrToPtr(allocationInfo->address);
        }
    }
    return nullptr;
}

size_t UsmMemAllocPool::getOffsetInPool(const void *ptr) const {
    if (isInitialized() && isInPool(ptr)) {
        return ptrDiff(ptr, this->pool);
    }
    return 0u;
}

uint64_t UsmMemAllocPool::getPoolAddress() const {
    return castToUint64(this->pool);
}

UsmMemAllocPool::PoolInfo UsmMemAllocPool::getPoolInfo() const {
    return poolInfo;
}

bool UsmMemAllocPoolsManager::initialize(SVMAllocsManager *svmMemoryManager) {
    DEBUG_BREAK_IF(poolMemoryType != InternalMemoryType::deviceUnifiedMemory &&
                   poolMemoryType != InternalMemoryType::hostUnifiedMemory);
    DEBUG_BREAK_IF(device == nullptr && poolMemoryType == InternalMemoryType::deviceUnifiedMemory);
    this->svmMemoryManager = svmMemoryManager;
    for (const auto &poolInfo : this->poolInfos) {
        this->pools[poolInfo] = std::vector<std::unique_ptr<UsmMemAllocPool>>();
        auto pool = tryAddPool(poolInfo);
        if (nullptr == pool) {
            cleanup();
            return false;
        }
    }
    return true;
}

bool UsmMemAllocPoolsManager::isInitialized() const {
    return nullptr != this->svmMemoryManager;
}

void UsmMemAllocPoolsManager::cleanup() {
    for (auto &[_, bucket] : this->pools) {
        for (const auto &pool : bucket) {
            pool->cleanup();
        }
    }
    this->pools.clear();
    this->svmMemoryManager = nullptr;
}

void *UsmMemAllocPoolsManager::createUnifiedMemoryAllocation(size_t size, const UnifiedMemoryProperties &memoryProperties) {
    DEBUG_BREAK_IF(false == isInitialized());
    if (!canBePooled(size, memoryProperties)) {
        return nullptr;
    }
    std::unique_lock<std::mutex> lock(mtx);
    void *ptr = nullptr;
    for (const auto &poolInfo : this->poolInfos) {
        if (size <= poolInfo.maxServicedSize) {
            for (auto &pool : this->pools[poolInfo]) {
                if (nullptr != (ptr = pool->createUnifiedMemoryAllocation(size, memoryProperties))) {
                    break;
                }
            }
            if (nullptr == ptr) {
                if (auto pool = tryAddPool(poolInfo)) {
                    ptr = pool->createUnifiedMemoryAllocation(size, memoryProperties);
                    DEBUG_BREAK_IF(nullptr == ptr);
                }
            }
            break;
        }
    }
    return ptr;
}

UsmMemAllocPool *UsmMemAllocPoolsManager::tryAddPool(PoolInfo poolInfo) {
    UsmMemAllocPool *poolPtr = nullptr;
    if (canAddPool(poolInfo)) {
        auto pool = std::make_unique<UsmMemAllocPool>();
        if (pool->initialize(svmMemoryManager, poolMemoryProperties, poolInfo.poolSize, poolInfo.minServicedSize, poolInfo.maxServicedSize)) {
            poolPtr = pool.get();
            this->totalSize += pool->getPoolSize();
            this->pools[poolInfo].push_back(std::move(pool));
        }
    }
    return poolPtr;
}
bool UsmMemAllocPoolsManager::canAddPool(PoolInfo poolInfo) {
    return true;
}

void UsmMemAllocPoolsManager::trimEmptyPools(PoolInfo poolInfo) {
    std::lock_guard lock(mtx);
    auto &bucket = pools[poolInfo];
    auto firstEmptyPoolIt = std::partition(bucket.begin(), bucket.end(), [](std::unique_ptr<UsmMemAllocPool> &pool) {
        return !pool->isEmpty();
    });
    const auto emptyPoolsCount = static_cast<size_t>(std::distance(firstEmptyPoolIt, bucket.end()));
    if (emptyPoolsCount > maxEmptyPoolsPerBucket) {
        std::advance(firstEmptyPoolIt, maxEmptyPoolsPerBucket);
        for (auto it = firstEmptyPoolIt; it != bucket.end(); ++it) {
            (*it)->cleanup();
        }
        bucket.erase(firstEmptyPoolIt, bucket.end());
    }
}

bool UsmMemAllocPoolsManager::freeSVMAlloc(const void *ptr, bool blocking) {
    bool allocFreed = false;
    if (auto pool = this->getPoolContainingAlloc(ptr); pool) {
        allocFreed = pool->freeSVMAlloc(ptr, blocking);
        if (allocFreed && pool->isEmpty()) {
            trimEmptyPools(pool->getPoolInfo());
        }
    }
    return allocFreed;
}

size_t UsmMemAllocPoolsManager::getPooledAllocationSize(const void *ptr) {
    if (auto pool = this->getPoolContainingAlloc(ptr); pool) {
        return pool->getPooledAllocationSize(ptr);
    }
    return 0u;
}

void *UsmMemAllocPoolsManager::getPooledAllocationBasePtr(const void *ptr) {
    if (auto pool = this->getPoolContainingAlloc(ptr); pool) {
        return pool->getPooledAllocationBasePtr(ptr);
    }
    return nullptr;
}

size_t UsmMemAllocPoolsManager::getOffsetInPool(const void *ptr) {
    if (auto pool = this->getPoolContainingAlloc(ptr); pool) {
        return pool->getOffsetInPool(ptr);
    }
    return 0u;
}

UsmMemAllocPool *UsmMemAllocPoolsManager::getPoolContainingAlloc(const void *ptr) {
    std::unique_lock<std::mutex> lock(mtx);
    for (const auto &poolInfo : this->poolInfos) {
        for (auto &pool : this->pools[poolInfo]) {
            if (pool->isInPool(ptr)) {
                return pool.get();
            }
        }
    }
    return nullptr;
}

} // namespace NEO