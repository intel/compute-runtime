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
    this->poolSize = svmData->size;
    this->poolMemoryType = svmData->memoryType;
    this->minServicedSize = minServicedSize;
    this->maxServicedSize = maxServicedSize;
    return true;
}

bool UsmMemAllocPool::isInitialized() const {
    return this->pool;
}

size_t UsmMemAllocPool::getPoolSize() const {
    return this->poolSize;
}

void UsmMemAllocPool::cleanup() {
    if (isInitialized()) {
        [[maybe_unused]] const auto status = this->svmMemoryManager->freeSVMAlloc(this->pool, true);
        DEBUG_BREAK_IF(false == status);
        this->svmMemoryManager = nullptr;
        this->pool = nullptr;
        this->poolEnd = nullptr;
        this->poolSize = 0u;
        this->poolMemoryType = InternalMemoryType::notSpecified;
    }
}

bool UsmMemAllocPool::alignmentIsAllowed(size_t alignment) {
    return alignment % chunkAlignment == 0 && alignment <= poolAlignment;
}

bool UsmMemAllocPool::sizeIsAllowed(size_t size) {
    return size >= minServicedSize && size <= maxServicedSize;
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

bool UsmMemAllocPool::isEmpty() {
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

bool UsmMemAllocPoolsManager::PoolInfo::isPreallocated() const {
    return 0u != preallocateSize;
}

bool UsmMemAllocPoolsManager::ensureInitialized(SVMAllocsManager *svmMemoryManager) {
    DEBUG_BREAK_IF(poolMemoryType != InternalMemoryType::deviceUnifiedMemory &&
                   poolMemoryType != InternalMemoryType::hostUnifiedMemory);
    if (isInitialized()) {
        return true;
    }
    std::unique_lock<std::mutex> lock(mtx);
    if (isInitialized()) {
        return true;
    }
    bool allPoolAllocationsSucceeded = true;
    this->totalSize = 0u;
    SVMAllocsManager::UnifiedMemoryProperties poolsMemoryProperties(poolMemoryType, MemoryConstants::pageSize2M, rootDeviceIndices, deviceBitFields);
    poolsMemoryProperties.device = device;
    for (const auto &poolInfo : this->poolInfos) {
        this->pools[poolInfo] = std::vector<std::unique_ptr<UsmMemAllocPool>>();
        if (poolInfo.isPreallocated()) {
            auto pool = std::make_unique<UsmMemAllocPool>();
            allPoolAllocationsSucceeded &= pool->initialize(svmMemoryManager, poolsMemoryProperties, poolInfo.preallocateSize, poolInfo.minServicedSize, poolInfo.maxServicedSize);
            this->pools[poolInfo].push_back(std::move(pool));
            this->totalSize += poolInfo.preallocateSize;
        }
    }
    if (false == allPoolAllocationsSucceeded) {
        cleanup();
        return false;
    }
    this->svmMemoryManager = svmMemoryManager;
    return true;
}

bool UsmMemAllocPoolsManager::isInitialized() const {
    return nullptr != this->svmMemoryManager;
}

void UsmMemAllocPoolsManager::trim() {
    std::unique_lock<std::mutex> lock(mtx);
    for (const auto &poolInfo : this->poolInfos) {
        if (false == poolInfo.isPreallocated()) {
            trim(this->pools[poolInfo]);
        }
    }
}

void UsmMemAllocPoolsManager::trim(std::vector<std::unique_ptr<UsmMemAllocPool>> &poolVector) {
    auto poolIterator = poolVector.begin();
    while (poolIterator != poolVector.end()) {
        if ((*poolIterator)->isEmpty()) {
            this->totalSize -= (*poolIterator)->getPoolSize();
            (*poolIterator)->cleanup();
            poolIterator = poolVector.erase(poolIterator);
        } else {
            ++poolIterator;
        }
    }
}

void UsmMemAllocPoolsManager::cleanup() {
    for (const auto &poolInfo : this->poolInfos) {
        for (const auto &pool : this->pools[poolInfo]) {
            pool->cleanup();
        }
    }
    this->svmMemoryManager = nullptr;
}

void *UsmMemAllocPoolsManager::createUnifiedMemoryAllocation(size_t size, const UnifiedMemoryProperties &memoryProperties) {
    DEBUG_BREAK_IF(false == isInitialized());
    if (!canBePooled(size, memoryProperties)) {
        return nullptr;
    }
    std::unique_lock<std::mutex> lock(mtx);
    for (const auto &poolInfo : this->poolInfos) {
        if (size <= poolInfo.maxServicedSize) {
            for (auto &pool : this->pools[poolInfo]) {
                if (void *ptr = pool->createUnifiedMemoryAllocation(size, memoryProperties)) {
                    return ptr;
                }
            }
            break;
        }
    }
    return nullptr;
}

bool UsmMemAllocPoolsManager::freeSVMAlloc(const void *ptr, bool blocking) {
    if (UsmMemAllocPool *pool = this->getPoolContainingAlloc(ptr)) {
        return pool->freeSVMAlloc(ptr, blocking);
    }
    return false;
}

size_t UsmMemAllocPoolsManager::getPooledAllocationSize(const void *ptr) {
    if (UsmMemAllocPool *pool = this->getPoolContainingAlloc(ptr)) {
        return pool->getPooledAllocationSize(ptr);
    }
    return 0u;
}

void *UsmMemAllocPoolsManager::getPooledAllocationBasePtr(const void *ptr) {
    if (UsmMemAllocPool *pool = this->getPoolContainingAlloc(ptr)) {
        return pool->getPooledAllocationBasePtr(ptr);
    }
    return nullptr;
}

size_t UsmMemAllocPoolsManager::getOffsetInPool(const void *ptr) {
    if (UsmMemAllocPool *pool = this->getPoolContainingAlloc(ptr)) {
        return pool->getOffsetInPool(ptr);
    }
    return 0u;
}

uint64_t UsmMemAllocPoolsManager::getFreeMemory() {
    const auto isIntegrated = device->getHardwareInfo().capabilityTable.isIntegratedDevice;
    const uint64_t deviceMemory = isIntegrated ? device->getDeviceInfo().globalMemSize : device->getDeviceInfo().localMemSize;
    const uint64_t usedMemory = memoryManager->getUsedLocalMemorySize(device->getRootDeviceIndex());
    DEBUG_BREAK_IF(usedMemory > deviceMemory);
    const uint64_t freeMemory = deviceMemory - usedMemory;
    return freeMemory;
}

bool UsmMemAllocPoolsManager::recycleSVMAlloc(void *ptr, bool blocking) {
    if (false == isInitialized()) {
        return false;
    }
    auto svmData = this->svmMemoryManager->getSVMAlloc(ptr);
    DEBUG_BREAK_IF(svmData->memoryType != this->poolMemoryType);
    if (svmData->size > maxPoolableSize || belongsInPreallocatedPool(svmData->size)) {
        return false;
    }
    if (this->totalSize + svmData->size > getFreeMemory() * UsmMemAllocPool::getPercentOfFreeMemoryForRecycling(svmData->memoryType)) {
        return false;
    }
    std::unique_lock<std::mutex> lock(mtx);
    for (auto poolInfoIndex = firstNonPreallocatedIndex; poolInfoIndex < this->poolInfos.size(); ++poolInfoIndex) {
        const auto &poolInfo = this->poolInfos[poolInfoIndex];
        if (svmData->size <= poolInfo.maxServicedSize) {
            auto pool = std::make_unique<UsmMemAllocPool>();
            pool->initialize(this->svmMemoryManager, ptr, svmData, poolInfo.minServicedSize, svmData->size);
            this->pools[poolInfo].push_back(std::move(pool));
            this->totalSize += svmData->size;
            return true;
        }
    }
    DEBUG_BREAK_IF(true);
    return false;
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