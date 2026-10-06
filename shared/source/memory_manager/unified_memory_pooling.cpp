/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_pooling.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/memory_manager/usm_pool_params.h"
#include "shared/source/utilities/heap_allocator.h"

namespace NEO {

bool UsmMemAllocPool::initialize(SVMAllocsManager *svmMemoryManager, const UnifiedMemoryProperties &memoryProperties, size_t poolSize, size_t minServicedSize, size_t maxServicedSize) {
    void *poolAllocation = nullptr;
    if (memoryProperties.memoryType == InternalMemoryType::hostUnifiedMemory) {
        poolAllocation = svmMemoryManager->createHostUnifiedMemoryAllocation(poolSize, memoryProperties);
    } else {
        poolAllocation = svmMemoryManager->createUnifiedMemoryAllocation(poolSize, memoryProperties);
    }

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
    this->allocationData = svmData;
    this->poolEnd = ptrOffset(this->pool, svmData->size);
    size_t chunkAllocatorSizeThreshold = maxServicedSize / 2;
    if (debugManager.flags.UsmPoolChunkAllocatorSizeThreshold.get() != -1) {
        chunkAllocatorSizeThreshold = static_cast<size_t>(debugManager.flags.UsmPoolChunkAllocatorSizeThreshold.get());
    }
    this->chunkAllocator.reset(new HeapAllocator(castToUint64(this->pool),
                                                 svmData->size,
                                                 chunkAlignment,
                                                 chunkAllocatorSizeThreshold));
    this->poolMemoryType = svmData->memoryType;
    this->poolInfo.minServicedSize = minServicedSize;
    this->poolInfo.maxServicedSize = maxServicedSize;
    this->poolInfo.poolSize = svmData->size;
    this->device = svmData->device;
    if (nullptr != device) {
        allocation = svmData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    }
    return true;
}

bool UsmMemAllocPool::isInitialized() const {
    return this->pool;
}

size_t UsmMemAllocPool::getPoolSize() const {
    return this->poolInfo.poolSize;
}

void UsmMemAllocPool::cleanup() {
    if (!isInitialized()) {
        return;
    }
    bool hasDeferredChunks = false;
    {
        std::unique_lock<std::mutex> lock(mtx);
        hasDeferredChunks = false == this->deferredFreeChunks.empty();
        if (hasDeferredChunks) {
            // snapshots can bound work up to latestSentTaskCount, which is not stamped on
            // the pool allocation, so raise it first or defer free would not cover them
            this->svmMemoryManager->applyIndirectAccessTaskCountFloor(allocationData);
        }
    }

    if (this->customCleanup) {
        this->customCleanup(this->pool);
    }
    [[maybe_unused]] const auto status = hasDeferredChunks ? this->svmMemoryManager->freeSVMAllocDefer(this->pool) : this->svmMemoryManager->freeSVMAlloc(this->pool);
    DEBUG_BREAK_IF(false == status);
    this->svmMemoryManager = nullptr;
    this->pool = nullptr;
    this->poolEnd = nullptr;
    this->poolInfo.poolSize = 0u;
    this->poolMemoryType = InternalMemoryType::notSpecified;
}

bool UsmMemAllocPool::alignmentIsAllowed(size_t alignment) {
    return alignment <= poolAlignment;
}

bool UsmMemAllocPool::sizeIsAllowed(size_t size) {
    return size >= poolInfo.minServicedSize && size <= poolInfo.maxServicedSize;
}

bool UsmMemAllocPool::flagsAreAllowed(const UnifiedMemoryProperties &memoryProperties) {
    auto flagsWithoutCompression = memoryProperties.allocationFlags;
    flagsWithoutCompression.flags.compressedHint = 0u;
    flagsWithoutCompression.flags.uncompressedHint = 0u;

    return flagsWithoutCompression.allFlags == 0u &&
           memoryProperties.allocationFlags.allAllocFlags == 0u &&
           memoryProperties.allocationFlags.hostptr == 0u;
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
    if (false == isInitialized() || false == canBePooled(requestedSize, memoryProperties)) {
        return nullptr;
    }
    std::unique_lock<std::mutex> lock(mtx);
    auto actualSize = requestedSize;
    auto pooledAddress = this->chunkAllocator->allocateWithCustomAlignment(actualSize, memoryProperties.alignment);
    if (!pooledAddress && false == this->deferredFreeChunks.empty()) {
        // Chunks awaiting GPU completion are only reclaimed when their space is needed,
        // to keep the completion polling off the successful allocation path.
        this->drainDeferredFreeChunks();
        actualSize = requestedSize;
        pooledAddress = this->chunkAllocator->allocateWithCustomAlignment(actualSize, memoryProperties.alignment);
    }
    if (!pooledAddress) {
        return nullptr;
    }

    auto pooledPtr = addrToPtr(pooledAddress);
    this->allocations.insert(pooledPtr, AllocationInfo{pooledAddress, actualSize, requestedSize});
    ++this->svmMemoryManager->allocationsCounter;
    return pooledPtr;
}

bool UsmMemAllocPool::isInPoolRange(const void *ptr) const {
    return ptr >= this->pool && ptr < this->poolEnd;
}

bool UsmMemAllocPool::isEmpty() const {
    std::unique_lock<std::mutex> lock(mtx);
    return this->isEmptyImpl();
}

bool UsmMemAllocPool::isEmptyImpl() const {
    return 0u == this->allocations.getNumAllocs() && this->deferredFreeChunks.empty();
}

UsmPoolFreeResult UsmMemAllocPool::freeSVMAlloc(const void *ptr, FreePolicyType policy) {
    UsmPoolFreeResult result{};
    if (false == isInitialized() || false == isInPoolRange(ptr)) {
        return result;
    }
    std::unique_lock<std::mutex> lock(mtx);
    auto allocationInfo = allocations.extract(ptr);
    if (!allocationInfo) {
        return result;
    }
    DEBUG_BREAK_IF(allocationInfo->size == 0 || allocationInfo->address == 0);
    DEBUG_BREAK_IF(false == allocationInfo->memFreeCallbacks.empty());
    StackVec<GraphicsAllocation *, 4> peerAllocations;
    if (this->peerAllocationsFn && FreePolicyType::none != policy) {
        peerAllocations = this->peerAllocationsFn(this->pool);
    }
    if (FreePolicyType::blocking == policy) {
        svmMemoryManager->applyIndirectAccessTaskCountFloor(allocationData);
        svmMemoryManager->waitForEnginesCompletion(allocationData);
        for (auto peerAllocation : peerAllocations) {
            this->device->getMemoryManager()->waitForEnginesCompletion(*peerAllocation);
        }
    }
    if (FreePolicyType::defer == policy) {
        DeferredFreeInfo deferredFreeInfo{std::move(*allocationInfo), {}};
        svmMemoryManager->captureEngineCompletionSnapshot(allocationData, deferredFreeInfo.snapshot);
        for (auto peerAllocation : peerAllocations) {
            this->device->getMemoryManager()->captureEngineCompletionSnapshot(*peerAllocation, deferredFreeInfo.snapshot);
        }
        this->deferredFreeChunks.push_back(std::move(deferredFreeInfo));
    } else {
        this->releaseChunk(*allocationInfo);
    }
    this->drainDeferredFreeChunks();
    result.freeSucceeded = true;
    result.poolNowEmpty = this->isEmptyImpl();
    return result;
}

void UsmMemAllocPool::releaseChunk(const AllocationInfo &allocationInfo) {
    // importers keep an exported chunk mapped until they close it, which the exporter cannot observe
    if (false == allocationInfo.isExported) {
        this->chunkAllocator->free(allocationInfo.address, allocationInfo.size);
    }
    if (trackResidency) {
        OPTIONAL_UNRECOVERABLE_IF(nullptr == device || nullptr == allocation);
        for (const auto &[neoDevice, isResident] : allocationInfo.isResident) {
            if (isResident) {
                dropPoolResidency(neoDevice);
            }
        }
    }
}

void UsmMemAllocPool::markChunkExported(const void *ptr) {
    std::unique_lock<std::mutex> lock(mtx);
    if (auto allocationInfo = allocations.get(ptr)) {
        allocationInfo->isExported = true;
        this->allocationData->isExportedAllocation = true;
    }
}

void UsmMemAllocPool::reclaimDeferredFreeChunks() {
    if (false == isInitialized()) {
        return;
    }
    std::unique_lock<std::mutex> lock(mtx);
    this->drainDeferredFreeChunks();
}

void UsmMemAllocPool::drainDeferredFreeChunks() {
    std::erase_if(this->deferredFreeChunks, [this](const DeferredFreeInfo &deferredFreeInfo) {
        if (false == isEngineCompletionSnapshotReady(deferredFreeInfo.snapshot)) {
            return false;
        }
        this->releaseChunk(deferredFreeInfo.allocationInfo);
        return true;
    });
}

bool UsmMemAllocPool::freeIfOwned(UsmMemAllocPool *pool, const void *ptr, FreePolicyType policy) {
    if (nullptr == pool || false == pool->isInPoolRange(ptr)) {
        return false;
    }
    [[maybe_unused]] const auto freeResult = pool->freeSVMAlloc(ptr, policy);
    DEBUG_BREAK_IF(false == freeResult.freeSucceeded);
    return true;
}

UsmPoolLookupResult UsmMemAllocPool::lookupAlloc(const void *ptr) {
    UsmPoolLookupResult result = {};
    if (false == isInitialized() || false == isInPoolRange(ptr)) {
        return result;
    }
    result.pool = this;
    result.poolInfo = this->poolInfo;
    std::unique_lock<std::mutex> lock(mtx);
    if (auto allocationInfo = allocations.get(ptr); allocationInfo) {
        result.pooledAllocationBasePtr = addrToPtr(allocationInfo->address);
        result.pooledAllocationSize = allocationInfo->requestedSize;
    }
    return result;
}

bool UsmMemAllocPool::addMemFreeCallback(const void *ptr, MemFreeCallback callback) {
    if (false == isInitialized() || false == isInPoolRange(ptr)) {
        return false;
    }
    std::unique_lock<std::mutex> lock(mtx);
    auto allocationInfo = allocations.get(ptr);
    if (nullptr == allocationInfo) {
        return false;
    }
    allocationInfo->memFreeCallbacks.push_back(callback);
    return true;
}

std::vector<MemFreeCallback> UsmMemAllocPool::takeMemFreeCallbacks(const void *ptr) {
    if (false == isInitialized() || false == isInPoolRange(ptr)) {
        return {};
    }
    std::unique_lock<std::mutex> lock(mtx);
    auto allocationInfo = allocations.get(ptr);
    if (nullptr == allocationInfo) {
        return {};
    }
    return std::exchange(allocationInfo->memFreeCallbacks, {});
}

size_t UsmMemAllocPool::getOffsetInPool(const void *ptr) const {
    if (false == isInitialized() || false == isInPoolRange(ptr)) {
        return 0u;
    }
    return ptrDiff(ptr, this->pool);
}

uint64_t UsmMemAllocPool::getPoolAddress() const {
    return castToUint64(this->pool);
}

MemoryOperationsStatus UsmMemAllocPool::makeChunkResident(const void *ptr, Device *targetDevice, GraphicsAllocation *targetAllocation) {
    OPTIONAL_UNRECOVERABLE_IF(nullptr == targetDevice || nullptr == targetAllocation);
    std::unique_lock<std::mutex> lock(mtx);
    auto allocationInfo = allocations.get(ptr);
    if (nullptr == allocationInfo) {
        return MemoryOperationsStatus::memoryNotFound;
    }

    auto &chunkIsResident = allocationInfo->isResident[targetDevice];
    if (chunkIsResident) {
        return MemoryOperationsStatus::success;
    }

    auto &residencyCount = this->residencyCounts[targetDevice];
    if (0u == residencyCount) {
        auto status = makePoolResident(targetDevice, targetAllocation);
        if (MemoryOperationsStatus::success != status) {
            return status;
        }
        this->residentPoolAllocations[targetDevice] = targetAllocation;
    }

    chunkIsResident = true;
    ++residencyCount;
    return MemoryOperationsStatus::success;
}

MemoryOperationsStatus UsmMemAllocPool::evictChunk(const void *ptr, Device *targetDevice) {
    OPTIONAL_UNRECOVERABLE_IF(nullptr == targetDevice);
    std::unique_lock<std::mutex> lock(mtx);
    auto allocationInfo = allocations.get(ptr);
    if (nullptr == allocationInfo) {
        return MemoryOperationsStatus::memoryNotFound;
    }

    auto isResidentIt = allocationInfo->isResident.find(targetDevice);
    if (isResidentIt == allocationInfo->isResident.end() || false == isResidentIt->second) {
        return MemoryOperationsStatus::success;
    }

    isResidentIt->second = false;
    return dropPoolResidency(targetDevice);
}

MemoryOperationsStatus UsmMemAllocPool::evictPool(Device *targetDevice, GraphicsAllocation *targetAllocation) {
    OPTIONAL_UNRECOVERABLE_IF(targetAllocation->getRootDeviceIndex() != targetDevice->getRootDeviceIndex());
    auto memoryOperationsIface = targetDevice->getRootDeviceEnvironment().memoryOperationsInterface.get();
    return memoryOperationsIface->evict(targetDevice, *targetAllocation);
}

MemoryOperationsStatus UsmMemAllocPool::makePoolResident(Device *targetDevice, GraphicsAllocation *targetAllocation) {
    OPTIONAL_UNRECOVERABLE_IF(targetAllocation->getRootDeviceIndex() != targetDevice->getRootDeviceIndex());
    auto memoryOperationsIface = targetDevice->getRootDeviceEnvironment().memoryOperationsInterface.get();
    return memoryOperationsIface->makeResident(targetDevice, ArrayRef<NEO::GraphicsAllocation *>(&targetAllocation, 1), true, true);
}

MemoryOperationsStatus UsmMemAllocPool::dropPoolResidency(Device *targetDevice) {
    OPTIONAL_UNRECOVERABLE_IF(0u == this->residencyCounts[targetDevice]);
    if (1u != this->residencyCounts[targetDevice]--) {
        return MemoryOperationsStatus::success;
    }
    auto residentPoolAllocationIt = this->residentPoolAllocations.find(targetDevice);
    OPTIONAL_UNRECOVERABLE_IF(residentPoolAllocationIt == this->residentPoolAllocations.end());
    auto targetAllocation = residentPoolAllocationIt->second;
    this->residentPoolAllocations.erase(residentPoolAllocationIt);
    return evictPool(targetDevice, targetAllocation);
}

const std::array<const PoolInfo, 3> UsmMemAllocPoolsManager::getPoolInfos() {
    return (device ? PoolInfo::getPoolInfos(device->getGfxCoreHelper()) : PoolInfo::getHostPoolInfos());
}

size_t UsmMemAllocPoolsManager::getMaxPoolableSize() {
    return (device ? PoolInfo::getMaxPoolableSize(device->getGfxCoreHelper()) : PoolInfo::getHostMaxPoolableSize());
}

bool UsmMemAllocPoolsManager::initialize(SVMAllocsManager *svmMemoryManager) {
    DEBUG_BREAK_IF(poolMemoryType != InternalMemoryType::deviceUnifiedMemory &&
                   poolMemoryType != InternalMemoryType::hostUnifiedMemory);
    DEBUG_BREAK_IF(device == nullptr && poolMemoryType == InternalMemoryType::deviceUnifiedMemory);
    this->svmMemoryManager = svmMemoryManager;
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
    for (const auto &poolInfo : getPoolInfos()) {
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
            if (trackResidency) {
                pool->enableResidencyTracking();
            }
            pool->setCustomCleanup(this->customCleanup);
            pool->setPeerAllocationsFn(this->peerAllocationsFn);
            this->pools[poolInfo].push_back(std::move(pool));
        }
    }
    return poolPtr;
}
bool UsmMemAllocPoolsManager::canAddPool(PoolInfo poolInfo) {
    return true;
}

bool UsmMemAllocPoolsManager::canBePooled(size_t size, const UnifiedMemoryProperties &memoryProperties) {
    return size <= getMaxPoolableSize() &&
           UsmMemAllocPool::alignmentIsAllowed(memoryProperties.alignment) &&
           UsmMemAllocPool::flagsAreAllowed(memoryProperties);
}

void UsmMemAllocPoolsManager::trimEmptyPools(PoolInfo poolInfo) {
    std::vector<std::unique_ptr<UsmMemAllocPool>> poolsToCleanup;
    {
        std::lock_guard lock(mtx);
        auto &bucket = pools[poolInfo];
        // A pool holding chunks awaiting GPU completion never reports empty. Reclaim what
        // retired since those chunks were freed, otherwise one stale chunk would keep the pool
        // alive for good - allocations that fit do not drain, so nothing else revisits it.
        for (auto &pool : bucket) {
            pool->reclaimDeferredFreeChunks();
        }
        auto firstEmptyPoolIt = std::partition(bucket.begin(), bucket.end(), [](std::unique_ptr<UsmMemAllocPool> &pool) {
            return !pool->isEmpty();
        });
        const auto emptyPoolsCount = static_cast<size_t>(std::distance(firstEmptyPoolIt, bucket.end()));
        if (emptyPoolsCount > maxEmptyPoolsPerBucket) {
            std::advance(firstEmptyPoolIt, maxEmptyPoolsPerBucket);
            poolsToCleanup.reserve(std::distance(firstEmptyPoolIt, bucket.end()));
            for (auto it = firstEmptyPoolIt; it != bucket.end(); ++it) {
                poolsToCleanup.push_back(std::move(*it));
            }
            bucket.erase(firstEmptyPoolIt, bucket.end());
        }
    }
    // customCleanup reaches into the API layer and takes its locks, so cleanup must run unlocked
    for (auto &pool : poolsToCleanup) {
        pool->cleanup();
    }
}

bool UsmMemAllocPoolsManager::freeSVMAlloc(const void *ptr, FreePolicyType policy) {
    const auto lookupResult = this->getPoolContainingAlloc(ptr);
    if (false == lookupResult.isAllocatedInPool()) {
        return false;
    }
    const auto freeResult = lookupResult.pool->freeSVMAlloc(ptr, policy);
    if (freeResult.poolNowEmpty) {
        trimEmptyPools(lookupResult.poolInfo);
    }
    return freeResult.freeSucceeded;
}

UsmPoolLookupResult UsmMemAllocPoolsManager::getPoolContainingAlloc(const void *ptr) {
    std::lock_guard lock(mtx);
    for (const auto &poolInfo : getPoolInfos()) {
        for (auto &pool : this->pools[poolInfo]) {
            if (pool->isInPoolRange(ptr)) {
                return pool->lookupAlloc(ptr);
            }
        }
    }
    return {};
}

bool UsmMemAllocPoolsFacade::poolingEnabled(InternalMemoryType memoryType, bool enabledByDefault) {
    int32_t poolFlag = -1;
    switch (memoryType) {
    case InternalMemoryType::deviceUnifiedMemory:
        poolFlag = NEO::debugManager.flags.EnableDeviceUsmAllocationPool.get();
        break;
    case InternalMemoryType::hostUnifiedMemory:
        poolFlag = NEO::debugManager.flags.EnableHostUsmAllocationPool.get();
        break;
    default:
        DEBUG_BREAK_IF(true);
        return false;
    }
    if (poolFlag != -1) {
        return poolFlag > 0;
    }
    return enabledByDefault;
}

bool UsmMemAllocPoolsFacade::initialize(InternalMemoryType memoryType, const RootDeviceIndicesContainer &rootDeviceIndices, const std::map<uint32_t, DeviceBitfield> &subdeviceBitfields, Device *device, SVMAllocsManager *svmMemoryManager, const InitParams &initParams) {
    const bool poolManagerEnabled = isPoolManagerSupported(memoryType, device);

    if (poolManagerEnabled) {
        auto managerDevice = memoryType == InternalMemoryType::deviceUnifiedMemory ? device : nullptr;
        this->poolManager = std::make_unique<UsmMemAllocPoolsManager>(memoryType, rootDeviceIndices, subdeviceBitfields, managerDevice);
        if (initParams.customCleanup) {
            this->poolManager->setCustomCleanup(initParams.customCleanup);
        }
        if (initParams.peerAllocations) {
            this->poolManager->setPeerAllocationsFn(initParams.peerAllocations);
        }
        if (initParams.trackResidency) {
            this->poolManager->enableResidencyTracking();
        }
        return this->poolManager->initialize(svmMemoryManager);
    } else {
        this->pool = std::make_unique<UsmMemAllocPool>();
        UnifiedMemoryProperties memoryProperties(memoryType, MemoryConstants::pageSize2M,
                                                 rootDeviceIndices, subdeviceBitfields);
        auto usmPoolParams = UsmPoolParams::getUsmPoolParams(device->getGfxCoreHelper());
        if (memoryType == InternalMemoryType::deviceUnifiedMemory && debugManager.flags.EnableDeviceUsmAllocationPool.get() != -1) {
            usmPoolParams.poolSize = debugManager.flags.EnableDeviceUsmAllocationPool.get() * MemoryConstants::megaByte;
        } else if (memoryType == InternalMemoryType::hostUnifiedMemory && debugManager.flags.EnableHostUsmAllocationPool.get() != -1) {
            usmPoolParams.poolSize = debugManager.flags.EnableHostUsmAllocationPool.get() * MemoryConstants::megaByte;
        }
        if (memoryType == InternalMemoryType::deviceUnifiedMemory) {
            memoryProperties.device = device;
            memoryProperties.allocationFlags.flags.compressedHint = initParams.compressedHint;
        }
        if (initParams.customCleanup) {
            this->pool->setCustomCleanup(initParams.customCleanup);
        }
        if (initParams.peerAllocations) {
            this->pool->setPeerAllocationsFn(initParams.peerAllocations);
        }
        if (initParams.trackResidency) {
            this->pool->enableResidencyTracking();
        }
        return this->pool->initialize(svmMemoryManager, memoryProperties, usmPoolParams.poolSize, usmPoolParams.minServicedSize, usmPoolParams.maxServicedSize);
    }
}

bool UsmMemAllocPoolsFacade::isInitialized() const {
    if (this->poolManager) {
        return this->poolManager->isInitialized();
    } else if (this->pool) {
        return this->pool->isInitialized();
    }
    return false;
}

void UsmMemAllocPoolsFacade::cleanup() {
    if (this->poolManager) {
        this->poolManager->cleanup();
        this->poolManager.reset();
    } else if (this->pool) {
        this->pool->cleanup();
        this->pool.reset();
    }
}

void *UsmMemAllocPoolsFacade::createUnifiedMemoryAllocation(size_t size, const UnifiedMemoryProperties &memoryProperties) {
    if (this->poolManager) {
        return this->poolManager->createUnifiedMemoryAllocation(size, memoryProperties);
    } else if (this->pool) {
        return this->pool->createUnifiedMemoryAllocation(size, memoryProperties);
    }
    return nullptr;
}

bool UsmMemAllocPoolsFacade::freeSVMAlloc(const void *ptr, FreePolicyType policy) {
    if (this->poolManager) {
        return this->poolManager->freeSVMAlloc(ptr, policy);
    } else if (this->pool) {
        return this->pool->freeSVMAlloc(ptr, policy).freeSucceeded;
    }
    return false;
}

size_t UsmMemAllocPoolsFacade::getPooledAllocationSize(const void *ptr) {
    return this->getPoolContainingAlloc(ptr).pooledAllocationSize;
}

void *UsmMemAllocPoolsFacade::getPooledAllocationBasePtr(const void *ptr) {
    return this->getPoolContainingAlloc(ptr).pooledAllocationBasePtr;
}

UsmPoolLookupResult UsmMemAllocPoolsFacade::getPoolContainingAlloc(const void *ptr) {
    if (this->poolManager) {
        return this->poolManager->getPoolContainingAlloc(ptr);
    } else if (this->pool) {
        return this->pool->lookupAlloc(ptr);
    }
    return {};
}

} // namespace NEO
