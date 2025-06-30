/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_manager.h"

#include "shared/source/ail/ail_configuration.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/sub_device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/memory_properties_helpers.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/compression_selector.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_reuse_cleaner.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"

namespace NEO {

uint32_t SVMAllocsManager::UnifiedMemoryProperties::getRootDeviceIndex() const {
    if (device) {
        return device->getRootDeviceIndex();
    }
    UNRECOVERABLE_IF(rootDeviceIndices.begin() == nullptr);
    return *rootDeviceIndices.begin();
}

void SVMAllocsManager::MapBasedAllocationTracker::insert(const SvmAllocationData &allocationsPair) {
    allocations.insert(std::make_pair(reinterpret_cast<void *>(allocationsPair.gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress()), allocationsPair));
}

void SVMAllocsManager::MapBasedAllocationTracker::remove(const SvmAllocationData &allocationsPair) {
    SvmAllocationContainer::iterator iter;
    iter = allocations.find(reinterpret_cast<void *>(allocationsPair.gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress()));
    allocations.erase(iter);
}

void SVMAllocsManager::MapBasedAllocationTracker::freeAllocations(NEO::MemoryManager &memoryManager) {
    std::unique_lock<NEO::SpinLock> lock(mutex);

    for (auto &allocation : allocations) {
        for (auto &gpuAllocation : allocation.second.gpuAllocations.getGraphicsAllocations()) {
            memoryManager.freeGraphicsMemory(gpuAllocation);
        }
    }
}

SVMAllocsManager::SvmAllocationCache::SvmAllocationCache() {
    this->enablePerformanceLogging = NEO::debugManager.flags.LogUsmReuse.get();
}

bool SVMAllocsManager::SvmAllocationCache::insert(size_t size, void *ptr, SvmAllocationData *svmData, bool waitForCompletion) {
    if (false == sizeAllowed(size) ||
        svmData->isInternalAllocation ||
        svmData->isImportedAllocation) {
        return false;
    }
    if (svmData->device ? svmData->device->shouldLimitAllocationsReuse() : memoryManager->shouldLimitAllocationsReuse()) {
        return false;
    }
    if (svmData->isSavedForReuse) {
        return true;
    }
    std::lock_guard<std::mutex> lock(this->mtx);
    bool isSuccess = true;
    if (auto device = svmData->device) {
        auto lock = device->usmReuseInfo.obtainAllocationsReuseLock();
        if (size + device->usmReuseInfo.getAllocationsSavedForReuseSize() > device->usmReuseInfo.getMaxAllocationsSavedForReuseSize()) {
            isSuccess = false;
        } else {
            device->usmReuseInfo.recordAllocationSaveForReuse(size);
        }
    } else {
        auto lock = memoryManager->usmReuseInfo.obtainAllocationsReuseLock();
        if (size + memoryManager->usmReuseInfo.getAllocationsSavedForReuseSize() > memoryManager->usmReuseInfo.getMaxAllocationsSavedForReuseSize()) {
            isSuccess = false;
        } else {
            memoryManager->usmReuseInfo.recordAllocationSaveForReuse(size);
        }
    }
    if (isSuccess) {
        if (waitForCompletion) {
            svmAllocsManager->waitForEnginesCompletion(svmData);
        }
        svmData->isSavedForReuse = true;
        allocations.emplace(std::lower_bound(allocations.begin(), allocations.end(), size), size, ptr, svmData, waitForCompletion);
    }
    if (enablePerformanceLogging) {
        logCacheOperation({.allocationSize = size,
                           .timePoint = std::chrono::high_resolution_clock::now(),
                           .allocationType = svmData->memoryType,
                           .operationType = CacheOperationType::insert,
                           .isSuccess = isSuccess});
    }
    return isSuccess;
}

bool SVMAllocsManager::SvmAllocationCache::allocUtilizationAllows(size_t requestedSize, size_t reuseCandidateSize) {
    if (reuseCandidateSize >= SvmAllocationCache::minimalSizeToCheckUtilization) {
        const auto allocUtilization = static_cast<double>(requestedSize) / reuseCandidateSize;
        return allocUtilization >= SvmAllocationCache::minimalAllocUtilization;
    }
    return true;
}

bool SVMAllocsManager::SvmAllocationCache::alignmentAllows(void *ptr, size_t alignment) {
    return 0u == alignment || isAligned(castToUint64(ptr), alignment);
}

bool SVMAllocsManager::SvmAllocationCache::isInUse(SvmCacheAllocationInfo &cacheAllocInfo) {
    if (cacheAllocInfo.completed) {
        return false;
    }
    if (cacheAllocInfo.svmData->cpuAllocation && memoryManager->allocInUse(*cacheAllocInfo.svmData->cpuAllocation)) {
        return true;
    }
    for (auto &gpuAllocation : cacheAllocInfo.svmData->gpuAllocations.getGraphicsAllocations()) {
        if (gpuAllocation && memoryManager->allocInUse(*gpuAllocation)) {
            return true;
        }
    }
    return false;
}

void *SVMAllocsManager::SvmAllocationCache::get(size_t size, const UnifiedMemoryProperties &unifiedMemoryProperties) {
    if (false == sizeAllowed(size)) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(this->mtx);
    for (auto allocationIter = std::lower_bound(allocations.begin(), allocations.end(), size);
         allocationIter != allocations.end();
         ++allocationIter) {
        if (false == allocUtilizationAllows(size, allocationIter->allocationSize)) {
            break;
        }
        void *allocationPtr = allocationIter->allocation;
        DEBUG_BREAK_IF(nullptr == allocationIter->svmData);
        if (allocationIter->svmData->device == unifiedMemoryProperties.device &&
            allocationIter->svmData->allocationFlagsProperty.allFlags == unifiedMemoryProperties.allocationFlags.allFlags &&
            allocationIter->svmData->allocationFlagsProperty.allAllocFlags == unifiedMemoryProperties.allocationFlags.allAllocFlags &&
            alignmentAllows(allocationIter->allocation, unifiedMemoryProperties.alignment) &&
            false == isInUse(*allocationIter)) {
            if (allocationIter->svmData->device) {
                auto lock = allocationIter->svmData->device->usmReuseInfo.obtainAllocationsReuseLock();
                allocationIter->svmData->device->usmReuseInfo.recordAllocationGetFromReuse(allocationIter->allocationSize);
            } else {
                auto lock = memoryManager->usmReuseInfo.obtainAllocationsReuseLock();
                memoryManager->usmReuseInfo.recordAllocationGetFromReuse(allocationIter->allocationSize);
            }
            if (enablePerformanceLogging) {
                logCacheOperation({.allocationSize = allocationIter->allocationSize,
                                   .timePoint = std::chrono::high_resolution_clock::now(),
                                   .allocationType = allocationIter->svmData->memoryType,
                                   .operationType = CacheOperationType::get,
                                   .isSuccess = true});
            }
            allocationIter->svmData->size = size;
            allocationIter->svmData->isSavedForReuse = false;
            allocationIter->svmData->gpuAllocations.getDefaultGraphicsAllocation()->setAubWritable(true, std::numeric_limits<uint32_t>::max());
            allocationIter->svmData->gpuAllocations.getDefaultGraphicsAllocation()->setTbxWritable(true, std::numeric_limits<uint32_t>::max());
            allocations.erase(allocationIter);
            return allocationPtr;
        }
    }
    if (enablePerformanceLogging) {
        logCacheOperation({.allocationSize = size,
                           .timePoint = std::chrono::high_resolution_clock::now(),
                           .allocationType = unifiedMemoryProperties.memoryType,
                           .operationType = CacheOperationType::get,
                           .isSuccess = false});
    }
    return nullptr;
}

void SVMAllocsManager::SvmAllocationCache::trim() {
    std::lock_guard<std::mutex> lock(this->mtx);
    for (auto &cachedAllocationInfo : this->allocations) {
        DEBUG_BREAK_IF(nullptr == cachedAllocationInfo.svmData);
        if (cachedAllocationInfo.svmData->device) {
            auto lock = cachedAllocationInfo.svmData->device->usmReuseInfo.obtainAllocationsReuseLock();
            cachedAllocationInfo.svmData->device->usmReuseInfo.recordAllocationGetFromReuse(cachedAllocationInfo.allocationSize);
        } else {
            auto lock = memoryManager->usmReuseInfo.obtainAllocationsReuseLock();
            memoryManager->usmReuseInfo.recordAllocationGetFromReuse(cachedAllocationInfo.allocationSize);
        }
        if (enablePerformanceLogging) {
            logCacheOperation({.allocationSize = cachedAllocationInfo.allocationSize,
                               .timePoint = std::chrono::high_resolution_clock::now(),
                               .allocationType = cachedAllocationInfo.svmData->memoryType,
                               .operationType = CacheOperationType::trim,
                               .isSuccess = true});
        }
        svmAllocsManager->freeSVMAllocImpl(cachedAllocationInfo.allocation, FreePolicyType::none, cachedAllocationInfo.svmData);
    }
    this->allocations.clear();
}

void SVMAllocsManager::SvmAllocationCache::cleanup() {
    DEBUG_BREAK_IF(nullptr == this->memoryManager);
    if (auto usmReuseCleaner = this->memoryManager->peekExecutionEnvironment().unifiedMemoryReuseCleaner.get()) {
        usmReuseCleaner->unregisterSvmAllocationCache(this);
    }
    this->trim();
}

void SVMAllocsManager::SvmAllocationCache::logCacheOperation(const SvmAllocationCachePerfInfo &cachePerfEvent) const {
    std::string allocationTypeString, operationTypeString, isSuccessString;
    switch (cachePerfEvent.allocationType) {
    case InternalMemoryType::deviceUnifiedMemory:
        allocationTypeString = "device";
        break;
    case InternalMemoryType::hostUnifiedMemory:
        allocationTypeString = "host";
        break;
    default:
        allocationTypeString = "unknown";
        break;
    }

    switch (cachePerfEvent.operationType) {
    case CacheOperationType::get:
        operationTypeString = "get";
        break;
    case CacheOperationType::insert:
        operationTypeString = "insert";
        break;
    case CacheOperationType::trim:
        operationTypeString = "trim";
        break;
    case CacheOperationType::trimOld:
        operationTypeString = "trim_old";
        break;
    default:
        operationTypeString = "unknown";
        break;
    }
    isSuccessString = cachePerfEvent.isSuccess ? "TRUE" : "FALSE";
    NEO::usmReusePerfLoggerInstance().log(true, ",",
                                          cachePerfEvent.timePoint.time_since_epoch().count(), ",",
                                          allocationTypeString, ",",
                                          operationTypeString, ",",
                                          cachePerfEvent.allocationSize, ",",
                                          isSuccessString);
}

void SVMAllocsManager::SvmAllocationCache::trimOldAllocs(std::chrono::high_resolution_clock::time_point trimTimePoint, bool trimAll) {
    std::lock_guard<std::mutex> lock(this->mtx);
    auto allocCleanCandidateIndex = allocations.size();
    while (0u != allocCleanCandidateIndex) {
        auto &allocCleanCandidate = allocations[--allocCleanCandidateIndex];
        if (allocCleanCandidate.saveTime > trimTimePoint) {
            continue;
        }
        DEBUG_BREAK_IF(nullptr == allocCleanCandidate.svmData);
        if (allocCleanCandidate.svmData->device) {
            auto lock = allocCleanCandidate.svmData->device->usmReuseInfo.obtainAllocationsReuseLock();
            allocCleanCandidate.svmData->device->usmReuseInfo.recordAllocationGetFromReuse(allocCleanCandidate.allocationSize);
        } else {
            auto lock = memoryManager->usmReuseInfo.obtainAllocationsReuseLock();
            memoryManager->usmReuseInfo.recordAllocationGetFromReuse(allocCleanCandidate.allocationSize);
        }
        if (enablePerformanceLogging) {
            logCacheOperation({.allocationSize = allocCleanCandidate.allocationSize,
                               .timePoint = std::chrono::high_resolution_clock::now(),
                               .allocationType = allocCleanCandidate.svmData->memoryType,
                               .operationType = CacheOperationType::trimOld,
                               .isSuccess = true});
        }
        svmAllocsManager->freeSVMAllocImpl(allocCleanCandidate.allocation, FreePolicyType::defer, allocCleanCandidate.svmData);
        if (trimAll) {
            allocCleanCandidate.markForDelete();
        } else {
            allocations.erase(allocations.begin() + allocCleanCandidateIndex);
            break;
        }
    }
    if (trimAll) {
        std::erase_if(allocations, SvmCacheAllocationInfo::isMarkedForDelete);
    }
}

SvmAllocationData *SVMAllocsManager::MapBasedAllocationTracker::get(const void *ptr) {
    if (allocations.size() == 0) {
        return nullptr;
    }
    if (!ptr) {
        return nullptr;
    }

    SvmAllocationContainer::iterator iter;
    const SvmAllocationContainer::iterator end = allocations.end();
    SvmAllocationData *svmAllocData;
    // try faster find lookup if pointer is aligned to page
    if (isAligned<MemoryConstants::pageSize>(ptr)) {
        iter = allocations.find(ptr);
        if (iter != end) {
            return &iter->second;
        }
    }
    // do additional check with lower bound as we may deal with pointer offset
    iter = allocations.lower_bound(ptr);
    if (((iter != end) && (iter->first != ptr)) ||
        (iter == end)) {
        if (iter == allocations.begin()) {
            iter = end;
        } else {
            iter--;
        }
    }
    if (iter != end) {
        svmAllocData = &iter->second;
        char *charPtr = reinterpret_cast<char *>(svmAllocData->gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress());
        if (ptr < (charPtr + svmAllocData->size)) {
            return svmAllocData;
        }
    }
    return nullptr;
}

void SVMAllocsManager::MapOperationsTracker::insert(SvmMapOperation mapOperation) {
    operations.insert(std::make_pair(mapOperation.regionSvmPtr, mapOperation));
}

void SVMAllocsManager::MapOperationsTracker::remove(const void *regionPtr) {
    SvmMapOperationsContainer::iterator iter;
    iter = operations.find(regionPtr);
    operations.erase(iter);
}

SvmMapOperation *SVMAllocsManager::MapOperationsTracker::get(const void *regionPtr) {
    SvmMapOperationsContainer::iterator iter;
    iter = operations.find(regionPtr);
    if (iter == operations.end()) {
        return nullptr;
    }
    return &iter->second;
}

void SVMAllocsManager::addInternalAllocationsToResidencyContainer(uint32_t rootDeviceIndex,
                                                                  ResidencyContainer &residencyContainer,
                                                                  uint32_t requestedTypesMask) {
    std::shared_lock<std::shared_mutex> lock(mtx);
    for (auto &allocation : this->svmAllocs.allocations) {
        if (rootDeviceIndex >= allocation.second->gpuAllocations.getGraphicsAllocations().size()) {
            continue;
        }

        if (!(static_cast<uint32_t>(allocation.second->memoryType) & requestedTypesMask) ||
            (nullptr == allocation.second->gpuAllocations.getGraphicsAllocation(rootDeviceIndex))) {
            continue;
        }

        auto alloc = allocation.second->gpuAllocations.getGraphicsAllocation(rootDeviceIndex);
        residencyContainer.push_back(alloc);
    }
}

void SVMAllocsManager::makeInternalAllocationsResident(CommandStreamReceiver &commandStreamReceiver, uint32_t requestedTypesMask) {
    std::shared_lock<std::shared_mutex> lock(mtx);
    for (auto &allocation : this->svmAllocs.allocations) {
        if (static_cast<uint32_t>(allocation.second->memoryType) & requestedTypesMask) {
            auto gpuAllocation = allocation.second->gpuAllocations.getGraphicsAllocation(commandStreamReceiver.getRootDeviceIndex());
            if (gpuAllocation == nullptr) {
                continue;
            }
            commandStreamReceiver.makeResident(*gpuAllocation);
        }
    }
}

SVMAllocsManager::SVMAllocsManager(MemoryManager *memoryManager)
    : memoryManager(memoryManager) {
}

SVMAllocsManager::~SVMAllocsManager() = default;

void *SVMAllocsManager::createSVMAlloc(size_t size, const SvmAllocationProperties svmProperties,
                                       const RootDeviceIndicesContainer &rootDeviceIndices,
                                       const std::map<uint32_t, DeviceBitfield> &subdeviceBitfields) {
    if (size == 0)
        return nullptr;

    if (rootDeviceIndices.size() > 1) {
        return createZeroCopySvmAllocation(size, svmProperties, rootDeviceIndices, subdeviceBitfields);
    }
    if (!memoryManager->isLocalMemorySupported(*rootDeviceIndices.begin())) {
        return createZeroCopySvmAllocation(size, svmProperties, rootDeviceIndices, subdeviceBitfields);
    } else {
        UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::notSpecified, 1, rootDeviceIndices, subdeviceBitfields);
        return createUnifiedAllocationWithDeviceStorage(size, svmProperties, unifiedMemoryProperties);
    }
}

void *SVMAllocsManager::createHostUnifiedMemoryAllocation(size_t size,
                                                          const UnifiedMemoryProperties &memoryProperties) {
    bool isDiscrete = false;
    if (size >= MemoryConstants::pageSize2M && !debugManager.flags.NEO_CAL_ENABLED.get()) {
        for (const auto rootDeviceIndex : memoryProperties.rootDeviceIndices) {
            isDiscrete |= !this->memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo()->capabilityTable.isIntegratedDevice;
            if (isDiscrete) {
                break;
            }
        }
    }
    const size_t pageSizeForAlignment = isDiscrete ? MemoryConstants::pageSize2M : MemoryConstants::pageSize;
    const size_t alignedSize = alignUp<size_t>(size, pageSizeForAlignment);

    bool compressionEnabled = false;
    AllocationType allocationType = getGraphicsAllocationTypeAndCompressionPreference(memoryProperties, compressionEnabled);

    RootDeviceIndicesContainer rootDeviceIndicesVector(memoryProperties.rootDeviceIndices);

    uint32_t rootDeviceIndex = rootDeviceIndicesVector.at(0);
    auto &deviceBitfield = memoryProperties.subdeviceBitfields.at(rootDeviceIndex);

    AllocationProperties unifiedMemoryProperties{rootDeviceIndex,
                                                 true,
                                                 alignedSize,
                                                 allocationType,
                                                 false,
                                                 (deviceBitfield.count() > 1),
                                                 deviceBitfield};
    unifiedMemoryProperties.alignment = alignUpNonZero<size_t>(memoryProperties.alignment, pageSizeForAlignment);
    unifiedMemoryProperties.flags.preferCompressed = compressionEnabled;
    unifiedMemoryProperties.flags.shareable = memoryProperties.allocationFlags.flags.shareable;
    unifiedMemoryProperties.flags.isUSMHostAllocation = true;
    unifiedMemoryProperties.flags.isUSMDeviceAllocation = false;
    unifiedMemoryProperties.cacheRegion = MemoryPropertiesHelper::getCacheRegion(memoryProperties.allocationFlags);

    if (this->usmHostAllocationsCache) {
        void *allocationFromCache = this->usmHostAllocationsCache->get(size, memoryProperties);
        if (allocationFromCache) {
            return allocationFromCache;
        }
    }

    auto maxRootDeviceIndex = *std::max_element(rootDeviceIndicesVector.begin(), rootDeviceIndicesVector.end(), std::less<uint32_t const>());
    SvmAllocationData allocData(maxRootDeviceIndex);
    void *externalHostPointer = reinterpret_cast<void *>(memoryProperties.allocationFlags.hostptr);

    void *usmPtr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndicesVector, unifiedMemoryProperties, allocData.gpuAllocations, externalHostPointer);
    if (!usmPtr) {
        if (this->usmHostAllocationsCache) {
            this->trimUSMHostAllocCache();
            usmPtr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndicesVector, unifiedMemoryProperties, allocData.gpuAllocations, externalHostPointer);
        }
        if (!usmPtr) {
            return nullptr;
        }
    }

    allocData.cpuAllocation = nullptr;
    allocData.size = size;
    allocData.memoryType = memoryProperties.memoryType;
    allocData.allocationFlagsProperty = memoryProperties.allocationFlags;
    allocData.device = nullptr;
    allocData.pageSizeForAlignment = pageSizeForAlignment;
    allocData.setAllocId(++this->allocationsCounter);

    insertSVMAlloc(usmPtr, allocData);

    return usmPtr;
}

void *SVMAllocsManager::createUnifiedMemoryAllocation(size_t size,
                                                      const UnifiedMemoryProperties &memoryProperties) {
    auto rootDeviceIndex = memoryProperties.getRootDeviceIndex();
    DeviceBitfield deviceBitfield = memoryProperties.subdeviceBitfields.at(rootDeviceIndex);
    constexpr size_t pageSizeForAlignment = MemoryConstants::pageSize64k;
    const size_t alignedSize = alignUp<size_t>(size, pageSizeForAlignment);

    auto externalPtr = reinterpret_cast<void *>(memoryProperties.allocationFlags.hostptr);
    bool useExternalHostPtrForCpu = externalPtr != nullptr;

    bool compressionEnabled = false;
    AllocationType allocationType = getGraphicsAllocationTypeAndCompressionPreference(memoryProperties, compressionEnabled);

    bool multiStorageAllocation = (deviceBitfield.count() > 1);

    AllocationProperties unifiedMemoryProperties{rootDeviceIndex,
                                                 !useExternalHostPtrForCpu, // allocateMemory
                                                 alignedSize,
                                                 allocationType,
                                                 false,
                                                 multiStorageAllocation,
                                                 deviceBitfield};
    unifiedMemoryProperties.alignment = alignUpNonZero<size_t>(memoryProperties.alignment, pageSizeForAlignment);
    unifiedMemoryProperties.flags.isUSMDeviceAllocation = false;
    unifiedMemoryProperties.flags.shareable = memoryProperties.allocationFlags.flags.shareable;
    unifiedMemoryProperties.cacheRegion = MemoryPropertiesHelper::getCacheRegion(memoryProperties.allocationFlags);
    unifiedMemoryProperties.flags.uncacheable = memoryProperties.allocationFlags.flags.locallyUncachedResource;
    unifiedMemoryProperties.flags.preferCompressed = compressionEnabled || memoryProperties.allocationFlags.flags.compressedHint;
    unifiedMemoryProperties.flags.preferCompressed &= memoryManager->isCompressionSupportedForShareable(memoryProperties.allocationFlags.flags.shareable);
    unifiedMemoryProperties.flags.resource48Bit = memoryProperties.allocationFlags.flags.resource48Bit;

    if (memoryProperties.memoryType == InternalMemoryType::deviceUnifiedMemory) {
        unifiedMemoryProperties.flags.isUSMDeviceAllocation = true;
        if (this->usmDeviceAllocationsCache &&
            false == memoryProperties.isInternalAllocation) {
            void *allocationFromCache = this->usmDeviceAllocationsCache->get(size, memoryProperties);
            if (allocationFromCache) {
                return allocationFromCache;
            }
        }
    } else if (memoryProperties.memoryType == InternalMemoryType::hostUnifiedMemory) {
        unifiedMemoryProperties.flags.isUSMHostAllocation = true;
    } else {
        unifiedMemoryProperties.flags.isUSMHostAllocation = useExternalHostPtrForCpu;
    }

    GraphicsAllocation *unifiedMemoryAllocation = memoryManager->allocateGraphicsMemoryWithProperties(unifiedMemoryProperties, externalPtr);
    if (!unifiedMemoryAllocation) {
        if (memoryProperties.memoryType == InternalMemoryType::deviceUnifiedMemory &&
            this->usmDeviceAllocationsCache) {
            this->trimUSMDeviceAllocCache();
            unifiedMemoryAllocation = memoryManager->allocateGraphicsMemoryWithProperties(unifiedMemoryProperties, externalPtr);
        }
        if (!unifiedMemoryAllocation) {
            return nullptr;
        }
    }
    setUnifiedAllocationProperties(unifiedMemoryAllocation, {});

    SvmAllocationData allocData(rootDeviceIndex);
    allocData.gpuAllocations.addAllocation(unifiedMemoryAllocation);
    allocData.cpuAllocation = nullptr;
    allocData.size = size;
    allocData.pageSizeForAlignment = pageSizeForAlignment;
    allocData.memoryType = memoryProperties.memoryType;
    allocData.allocationFlagsProperty = memoryProperties.allocationFlags;
    allocData.device = memoryProperties.device;
    allocData.setAllocId(++this->allocationsCounter);
    allocData.isInternalAllocation = memoryProperties.isInternalAllocation;

    auto retPtr = reinterpret_cast<void *>(unifiedMemoryAllocation->getGpuAddress());
    insertSVMAlloc(retPtr, allocData);
    UNRECOVERABLE_IF(useExternalHostPtrForCpu && (externalPtr != retPtr));

    return retPtr;
}

void *SVMAllocsManager::createSharedUnifiedMemoryAllocation(size_t size,
                                                            const UnifiedMemoryProperties &memoryProperties,
                                                            void *cmdQ) {
    if (memoryProperties.rootDeviceIndices.size() > 1 && memoryProperties.device == nullptr) {
        return createHostUnifiedMemoryAllocation(size, memoryProperties);
    }

    auto rootDeviceIndex = memoryProperties.getRootDeviceIndex();

    auto supportDualStorageSharedMemory = memoryManager->isLocalMemorySupported(rootDeviceIndex);

    if (debugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.get() != -1) {
        supportDualStorageSharedMemory = !!debugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.get();
    }

    if (supportDualStorageSharedMemory) {
        bool useKmdMigration = memoryManager->isKmdMigrationAvailable(rootDeviceIndex);
        void *unifiedMemoryPointer = nullptr;

        if (useKmdMigration) {
            unifiedMemoryPointer = createUnifiedKmdMigratedAllocation(size, {}, memoryProperties);
            if (!unifiedMemoryPointer) {
                return nullptr;
            }
        } else {
            unifiedMemoryPointer = createUnifiedAllocationWithDeviceStorage(size, {}, memoryProperties);
            if (!unifiedMemoryPointer) {
                return nullptr;
            }

            UNRECOVERABLE_IF(cmdQ == nullptr);
            auto pageFaultManager = this->memoryManager->getPageFaultManager();
            pageFaultManager->insertAllocation(unifiedMemoryPointer, size, this, cmdQ, memoryProperties.allocationFlags);
        }

        auto unifiedMemoryAllocation = this->getSVMAlloc(unifiedMemoryPointer);
        unifiedMemoryAllocation->memoryType = memoryProperties.memoryType;
        unifiedMemoryAllocation->allocationFlagsProperty = memoryProperties.allocationFlags;

        return unifiedMemoryPointer;
    }
    return createUnifiedMemoryAllocation(size, memoryProperties);
}

void *SVMAllocsManager::createUnifiedKmdMigratedAllocation(size_t size, const SvmAllocationProperties &svmProperties, const UnifiedMemoryProperties &unifiedMemoryProperties) {

    auto rootDeviceIndex = unifiedMemoryProperties.getRootDeviceIndex();
    auto &deviceBitfield = unifiedMemoryProperties.subdeviceBitfields.at(rootDeviceIndex);
    constexpr size_t pageSizeForAlignment = MemoryConstants::pageSize2M;
    const size_t alignedSize = alignUp<size_t>(size, pageSizeForAlignment);
    AllocationProperties gpuProperties{rootDeviceIndex,
                                       true,
                                       alignedSize,
                                       AllocationType::unifiedSharedMemory,
                                       false,
                                       false,
                                       deviceBitfield};

    gpuProperties.alignment = alignUpNonZero<size_t>(unifiedMemoryProperties.alignment, pageSizeForAlignment);
    gpuProperties.flags.resource48Bit = unifiedMemoryProperties.allocationFlags.flags.resource48Bit;
    auto cacheRegion = MemoryPropertiesHelper::getCacheRegion(unifiedMemoryProperties.allocationFlags);
    MemoryPropertiesHelper::fillCachePolicyInProperties(gpuProperties, false, svmProperties.readOnly, false, cacheRegion);
    auto initialPlacement = MemoryPropertiesHelper::getUSMInitialPlacement(unifiedMemoryProperties.allocationFlags);
    MemoryPropertiesHelper::setUSMInitialPlacement(gpuProperties, initialPlacement);
    GraphicsAllocation *allocationGpu = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);
    if (!allocationGpu) {
        return nullptr;
    }
    setUnifiedAllocationProperties(allocationGpu, svmProperties);

    SvmAllocationData allocData(rootDeviceIndex);
    allocData.gpuAllocations.addAllocation(allocationGpu);
    allocData.cpuAllocation = nullptr;
    allocData.device = unifiedMemoryProperties.device;
    allocData.size = size;
    allocData.pageSizeForAlignment = pageSizeForAlignment;
    allocData.setAllocId(++this->allocationsCounter);

    auto retPtr = allocationGpu->getUnderlyingBuffer();
    insertSVMAlloc(retPtr, allocData);
    return retPtr;
}

void SVMAllocsManager::setUnifiedAllocationProperties(GraphicsAllocation *allocation, const SvmAllocationProperties &svmProperties) {
    allocation->setMemObjectsAllocationWithWritableFlags(!svmProperties.readOnly && !svmProperties.hostPtrReadOnly);
    allocation->setCoherent(svmProperties.coherent);
}

void SVMAllocsManager::insertSVMAlloc(const SvmAllocationData &svmAllocData) {
    insertSVMAlloc(reinterpret_cast<void *>(svmAllocData.gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress()), svmAllocData);
}

void SVMAllocsManager::removeSVMAlloc(const SvmAllocationData &svmAllocData) {
    std::unique_lock<std::shared_mutex> lock(mtx);
    internalAllocationsMap.erase(svmAllocData.getAllocId());
    svmAllocs.remove(reinterpret_cast<void *>(svmAllocData.gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress()));
}

bool SVMAllocsManager::freeSVMAlloc(void *ptr, bool blocking) {
    if (svmDeferFreeAllocs.allocations.size() > 0) {
        this->freeSVMAllocDeferImpl();
    }
    SvmAllocationData *svmData = getSVMAlloc(ptr);
    if (svmData) {
        if (InternalMemoryType::deviceUnifiedMemory == svmData->memoryType &&
            this->usmDeviceAllocationsCache) {
            if (this->usmDeviceAllocationsCache->insert(svmData->gpuAllocations.getDefaultGraphicsAllocation()->getUnderlyingBufferSize(), ptr, svmData, blocking)) {
                return true;
            }
        }
        if (InternalMemoryType::hostUnifiedMemory == svmData->memoryType &&
            this->usmHostAllocationsCache) {
            if (this->usmHostAllocationsCache->insert(svmData->gpuAllocations.getDefaultGraphicsAllocation()->getUnderlyingBufferSize(), ptr, svmData, blocking)) {
                return true;
            }
        }
        if (blocking) {
            this->freeSVMAllocImpl(ptr, FreePolicyType::blocking, svmData);
        } else {
            this->freeSVMAllocImpl(ptr, FreePolicyType::none, svmData);
        }
        return true;
    }
    return false;
}

bool SVMAllocsManager::freeSVMAllocDefer(void *ptr) {

    if (svmDeferFreeAllocs.allocations.size() > 0) {
        this->freeSVMAllocDeferImpl();
    }

    SvmAllocationData *svmData = getSVMAlloc(ptr);
    if (svmData) {
        constexpr bool waitForCompletion = false;
        if (InternalMemoryType::deviceUnifiedMemory == svmData->memoryType &&
            this->usmDeviceAllocationsCache) {
            if (this->usmDeviceAllocationsCache->insert(svmData->gpuAllocations.getDefaultGraphicsAllocation()->getUnderlyingBufferSize(), ptr, svmData, waitForCompletion)) {
                return true;
            }
        }
        if (InternalMemoryType::hostUnifiedMemory == svmData->memoryType &&
            this->usmHostAllocationsCache) {
            if (this->usmHostAllocationsCache->insert(svmData->gpuAllocations.getDefaultGraphicsAllocation()->getUnderlyingBufferSize(), ptr, svmData, waitForCompletion)) {
                return true;
            }
        }
        this->freeSVMAllocImpl(ptr, FreePolicyType::defer, svmData);
        return true;
    }
    return false;
}

void SVMAllocsManager::waitForEnginesCompletion(SvmAllocationData *allocationData) {
    if (allocationData->cpuAllocation) {
        this->memoryManager->waitForEnginesCompletion(*allocationData->cpuAllocation);
    }

    for (auto &gpuAllocation : allocationData->gpuAllocations.getGraphicsAllocations()) {
        if (gpuAllocation) {
            this->memoryManager->waitForEnginesCompletion(*gpuAllocation);
        }
    }
}

void SVMAllocsManager::freeSVMAllocImpl(void *ptr, FreePolicyType policy, SvmAllocationData *svmData) {
    auto allowNonBlockingFree = policy == FreePolicyType::none;
    this->prepareIndirectAllocationForDestruction(svmData, allowNonBlockingFree);

    if (policy == FreePolicyType::blocking) {
        this->waitForEnginesCompletion(svmData);
    } else if (policy == FreePolicyType::defer) {
        if (svmData->cpuAllocation) {
            if (this->memoryManager->allocInUse(*svmData->cpuAllocation)) {
                std::lock_guard<std::shared_mutex> lock(mtx);
                if (svmDeferFreeAllocs.get(ptr) == nullptr) {
                    this->svmDeferFreeAllocs.insert(*svmData);
                }
                return;
            }
        }
        for (auto &gpuAllocation : svmData->gpuAllocations.getGraphicsAllocations()) {
            if (gpuAllocation) {
                if (this->memoryManager->allocInUse(*gpuAllocation)) {
                    std::lock_guard<std::shared_mutex> lock(mtx);
                    if (svmDeferFreeAllocs.get(ptr) == nullptr) {
                        this->svmDeferFreeAllocs.insert(*svmData);
                    }
                    return;
                }
            }
        }
    }
    auto pageFaultManager = this->memoryManager->getPageFaultManager();
    if (svmData->cpuAllocation && pageFaultManager) {
        pageFaultManager->removeAllocation(svmData->cpuAllocation->getUnderlyingBuffer());
    }
    if (svmData->gpuAllocations.getAllocationType() == AllocationType::svmZeroCopy) {
        freeZeroCopySvmAllocation(svmData);
    } else {
        freeSvmAllocationWithDeviceStorage(svmData);
    }
}

void SVMAllocsManager::freeSVMAllocDeferImpl() {
    std::vector<void *> freedPtr;
    for (auto iter = svmDeferFreeAllocs.allocations.begin(); iter != svmDeferFreeAllocs.allocations.end(); ++iter) {
        void *ptr = reinterpret_cast<void *>(iter->second.gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress());
        this->freeSVMAllocImpl(ptr, FreePolicyType::defer, this->getSVMAlloc(ptr));

        if (this->getSVMAlloc(ptr) == nullptr) {
            freedPtr.push_back(ptr);
        }
    }
    for (uint32_t i = 0; i < freedPtr.size(); ++i) {
        svmDeferFreeAllocs.allocations.erase(freedPtr[i]);
    }
}

void SVMAllocsManager::cleanupUSMAllocCaches() {
    if (this->usmDeviceAllocationsCache) {
        this->usmDeviceAllocationsCache->cleanup();
        this->usmDeviceAllocationsCache.reset(nullptr);
    }
    if (this->usmHostAllocationsCache) {
        this->usmHostAllocationsCache->cleanup();
        this->usmHostAllocationsCache.reset(nullptr);
    }
}

void SVMAllocsManager::trimUSMDeviceAllocCache() {
    this->usmDeviceAllocationsCache->trim();
}

void SVMAllocsManager::trimUSMHostAllocCache() {
    this->usmHostAllocationsCache->trim();
}

void *SVMAllocsManager::createZeroCopySvmAllocation(size_t size, const SvmAllocationProperties &svmProperties,
                                                    const RootDeviceIndicesContainer &rootDeviceIndices,
                                                    const std::map<uint32_t, DeviceBitfield> &subdeviceBitfields) {

    auto rootDeviceIndex = *rootDeviceIndices.begin();
    auto &deviceBitfield = subdeviceBitfields.at(rootDeviceIndex);
    AllocationProperties properties{rootDeviceIndex,
                                    true, // allocateMemory
                                    size,
                                    AllocationType::svmZeroCopy,
                                    false, // isMultiStorageAllocation
                                    deviceBitfield};
    MemoryPropertiesHelper::fillCachePolicyInProperties(properties, false, svmProperties.readOnly, false, properties.cacheRegion);

    RootDeviceIndicesContainer rootDeviceIndicesVector(rootDeviceIndices);

    auto maxRootDeviceIndex = *std::max_element(rootDeviceIndices.begin(), rootDeviceIndices.end(), std::less<uint32_t const>());
    SvmAllocationData allocData(maxRootDeviceIndex);

    void *usmPtr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndicesVector, properties, allocData.gpuAllocations);
    if (!usmPtr) {
        return nullptr;
    }
    for (const auto &rootDeviceIndex : rootDeviceIndices) {
        auto allocation = allocData.gpuAllocations.getGraphicsAllocation(rootDeviceIndex);
        allocation->setMemObjectsAllocationWithWritableFlags(!svmProperties.readOnly && !svmProperties.hostPtrReadOnly);
        allocation->setCoherent(svmProperties.coherent);
    }
    allocData.size = size;
    allocData.setAllocId(++this->allocationsCounter);

    insertSVMAlloc(usmPtr, allocData);
    return usmPtr;
}

void *SVMAllocsManager::createUnifiedAllocationWithDeviceStorage(size_t size, const SvmAllocationProperties &svmProperties, const UnifiedMemoryProperties &unifiedMemoryProperties) {
    auto rootDeviceIndex = unifiedMemoryProperties.getRootDeviceIndex();
    auto externalPtr = reinterpret_cast<void *>(unifiedMemoryProperties.allocationFlags.hostptr);
    bool useExternalHostPtrForCpu = externalPtr != nullptr;
    const size_t svmCpuAlignment = memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[rootDeviceIndex]->getProductHelper().getSvmCpuAlignment();

    auto minCpuAlignment = (debugManager.flags.AlignLocalMemoryVaTo2MB.get() == 1) ? MemoryConstants::pageSize2M : MemoryConstants::pageSize64k;

    const size_t effectiveSvmCpuAlignment = std::max(minCpuAlignment, svmCpuAlignment);
    const size_t alignment = alignUpNonZero<size_t>(unifiedMemoryProperties.alignment, effectiveSvmCpuAlignment);
    const size_t alignedCpuSize = alignUp<size_t>(size, alignment);
    DeviceBitfield subDevices = unifiedMemoryProperties.subdeviceBitfields.at(rootDeviceIndex);
    AllocationProperties cpuProperties{rootDeviceIndex,
                                       !useExternalHostPtrForCpu, // allocateMemory
                                       alignedCpuSize, AllocationType::svmCpu,
                                       false, // isMultiStorageAllocation
                                       subDevices};
    cpuProperties.alignment = alignment;
    cpuProperties.flags.isUSMHostAllocation = useExternalHostPtrForCpu;
    cpuProperties.forceKMDAllocation = true;
    cpuProperties.makeGPUVaDifferentThanCPUPtr = true;
    auto cacheRegion = MemoryPropertiesHelper::getCacheRegion(unifiedMemoryProperties.allocationFlags);
    MemoryPropertiesHelper::fillCachePolicyInProperties(cpuProperties, false, svmProperties.readOnly, false, cacheRegion);
    GraphicsAllocation *allocationCpu = memoryManager->allocateGraphicsMemoryWithProperties(cpuProperties, externalPtr);
    if (!allocationCpu) {
        return nullptr;
    }
    setUnifiedAllocationProperties(allocationCpu, svmProperties);
    void *svmPtr = allocationCpu->getUnderlyingBuffer();
    UNRECOVERABLE_IF(useExternalHostPtrForCpu && (externalPtr != svmPtr));

    const size_t alignedGpuSize = alignUp<size_t>(size, MemoryConstants::pageSize64k);
    AllocationProperties gpuProperties{rootDeviceIndex,
                                       false,
                                       alignedGpuSize,
                                       AllocationType::svmGpu,
                                       false,
                                       subDevices.count() > 1,
                                       subDevices};

    gpuProperties.alignment = alignment;
    auto compressionSupported = false;
    if (unifiedMemoryProperties.device) {
        auto &gfxCoreHelper = unifiedMemoryProperties.device->getGfxCoreHelper();
        auto &hwInfo = unifiedMemoryProperties.device->getHardwareInfo();
        compressionSupported = gfxCoreHelper.usmCompressionSupported(hwInfo);
        compressionSupported &= memoryManager->isCompressionSupportedForShareable(unifiedMemoryProperties.allocationFlags.flags.shareable);
    }
    gpuProperties.flags.preferCompressed = compressionSupported;

    MemoryPropertiesHelper::fillCachePolicyInProperties(gpuProperties, false, svmProperties.readOnly, false, cacheRegion);
    GraphicsAllocation *allocationGpu = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties, svmPtr);
    if (!allocationGpu) {
        memoryManager->freeGraphicsMemory(allocationCpu);
        return nullptr;
    }
    setUnifiedAllocationProperties(allocationGpu, svmProperties);

    SvmAllocationData allocData(rootDeviceIndex);
    allocData.gpuAllocations.addAllocation(allocationGpu);
    allocData.cpuAllocation = allocationCpu;
    allocData.device = unifiedMemoryProperties.device;
    allocData.pageSizeForAlignment = effectiveSvmCpuAlignment;
    allocData.size = size;
    allocData.setAllocId(++this->allocationsCounter);

    insertSVMAlloc(svmPtr, allocData);
    return svmPtr;
}

void SVMAllocsManager::freeSVMData(SvmAllocationData *svmData) {
    std::unique_lock<std::mutex> lockForIndirect(mtxForIndirectAccess);
    std::unique_lock<std::shared_mutex> lock(mtx);
    internalAllocationsMap.erase(svmData->getAllocId());
    svmAllocs.remove(reinterpret_cast<void *>(svmData->gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress()));
}

void SVMAllocsManager::freeZeroCopySvmAllocation(SvmAllocationData *svmData) {
    auto gpuAllocations = svmData->gpuAllocations;
    freeSVMData(svmData);
    for (const auto &graphicsAllocation : gpuAllocations.getGraphicsAllocations()) {
        memoryManager->freeGraphicsMemory(graphicsAllocation);
    }
}

void SVMAllocsManager::initUsmDeviceAllocationsCache(Device &device) {
    this->usmDeviceAllocationsCache.reset(new SvmAllocationCache);
    this->usmDeviceAllocationsCache->allocations.reserve(128u);
    this->usmDeviceAllocationsCache->svmAllocsManager = this;
    this->usmDeviceAllocationsCache->memoryManager = memoryManager;
    if (auto usmReuseCleaner = device.getExecutionEnvironment()->unifiedMemoryReuseCleaner.get()) {
        usmReuseCleaner->registerSvmAllocationCache(this->usmDeviceAllocationsCache.get());
    }
}

void SVMAllocsManager::initUsmHostAllocationsCache() {
    this->usmHostAllocationsCache.reset(new SvmAllocationCache);
    this->usmHostAllocationsCache->allocations.reserve(128u);
    this->usmHostAllocationsCache->svmAllocsManager = this;
    this->usmHostAllocationsCache->memoryManager = memoryManager;
    if (auto usmReuseCleaner = this->memoryManager->peekExecutionEnvironment().unifiedMemoryReuseCleaner.get()) {
        usmReuseCleaner->registerSvmAllocationCache(this->usmHostAllocationsCache.get());
    }
}

void SVMAllocsManager::initUsmAllocationsCaches(Device &device) {
    bool usmDeviceAllocationsCacheEnabled = NEO::ApiSpecificConfig::isDeviceAllocationCacheEnabled() && device.getProductHelper().isDeviceUsmAllocationReuseSupported();
    if (debugManager.flags.ExperimentalEnableDeviceAllocationCache.get() != -1) {
        usmDeviceAllocationsCacheEnabled = !!debugManager.flags.ExperimentalEnableDeviceAllocationCache.get();
    }
    if (usmDeviceAllocationsCacheEnabled && device.usmReuseInfo.getMaxAllocationsSavedForReuseSize() > 0u) {
        device.getExecutionEnvironment()->initializeUnifiedMemoryReuseCleaner(device.isAnyDirectSubmissionLightEnabled());
        this->initUsmDeviceAllocationsCache(device);
    }

    bool usmHostAllocationsCacheEnabled = NEO::ApiSpecificConfig::isHostAllocationCacheEnabled() && device.getProductHelper().isHostUsmAllocationReuseSupported();
    if (debugManager.flags.ExperimentalEnableHostAllocationCache.get() != -1) {
        usmHostAllocationsCacheEnabled = !!debugManager.flags.ExperimentalEnableHostAllocationCache.get();
    }
    if (usmHostAllocationsCacheEnabled && this->memoryManager->usmReuseInfo.getMaxAllocationsSavedForReuseSize() > 0u) {
        device.getExecutionEnvironment()->initializeUnifiedMemoryReuseCleaner(device.isAnyDirectSubmissionLightEnabled());
        this->initUsmHostAllocationsCache();
    }
}

void SVMAllocsManager::freeSvmAllocationWithDeviceStorage(SvmAllocationData *svmData) {
    auto graphicsAllocations = svmData->gpuAllocations.getGraphicsAllocations();
    GraphicsAllocation *cpuAllocation = svmData->cpuAllocation;
    bool isImportedAllocation = svmData->isImportedAllocation;
    freeSVMData(svmData);
    for (auto gpuAllocation : graphicsAllocations) {
        memoryManager->freeGraphicsMemory(gpuAllocation, isImportedAllocation);
    }
    memoryManager->freeGraphicsMemory(cpuAllocation, isImportedAllocation);
}

bool SVMAllocsManager::hasHostAllocations() {
    std::shared_lock<std::shared_mutex> lock(mtx);
    for (auto &allocation : this->svmAllocs.allocations) {
        if (allocation.second->memoryType == InternalMemoryType::hostUnifiedMemory) {
            return true;
        }
    }
    return false;
}

void SVMAllocsManager::makeIndirectAllocationsResident(CommandStreamReceiver &commandStreamReceiver, TaskCountType taskCount) {
    std::unique_lock<std::shared_mutex> lock(mtx);
    bool parseAllAllocations = false;
    auto entry = indirectAllocationsResidency.find(&commandStreamReceiver);
    TaskCountType previousCounter = 0;
    if (entry == indirectAllocationsResidency.end()) {
        parseAllAllocations = true;

        InternalAllocationsTracker tracker = {};
        tracker.latestResidentObjectId = this->allocationsCounter;
        tracker.latestSentTaskCount = taskCount;

        this->indirectAllocationsResidency.insert(std::make_pair(&commandStreamReceiver, tracker));
    } else {
        if (this->allocationsCounter > entry->second.latestResidentObjectId) {
            parseAllAllocations = true;
            previousCounter = entry->second.latestResidentObjectId;
            entry->second.latestResidentObjectId = this->allocationsCounter;
        }
        entry->second.latestSentTaskCount = taskCount;
    }
    if (parseAllAllocations) {
        auto currentCounter = this->allocationsCounter.load();
        for (auto allocationId = static_cast<uint32_t>(previousCounter + 1); allocationId <= currentCounter; allocationId++) {
            makeResidentForAllocationsWithId(allocationId, commandStreamReceiver);
        }
    }
}

void SVMAllocsManager::prepareIndirectAllocationForDestruction(SvmAllocationData *allocationData, bool isNonBlockingFree) {
    std::unique_lock<std::shared_mutex> lock(mtx);
    if (this->indirectAllocationsResidency.size() > 0u) {
        for (auto &internalAllocationsHandling : this->indirectAllocationsResidency) {
            auto commandStreamReceiver = internalAllocationsHandling.first;
            auto gpuAllocation = allocationData->gpuAllocations.getGraphicsAllocation(commandStreamReceiver->getRootDeviceIndex());
            if (gpuAllocation == nullptr) {
                continue;
            }

            // If this is non blocking free, we will wait for latest known usage of this allocation.
            // However, if this is blocking free, we must select "safest" task count to wait for.
            TaskCountType desiredTaskCount = std::max(internalAllocationsHandling.second.latestSentTaskCount, gpuAllocation->getTaskCount(commandStreamReceiver->getOsContext().getContextId()));
            if (isNonBlockingFree) {
                desiredTaskCount = gpuAllocation->getTaskCount(commandStreamReceiver->getOsContext().getContextId());
            }
            if (gpuAllocation->isAlwaysResident(commandStreamReceiver->getOsContext().getContextId())) {
                gpuAllocation->updateResidencyTaskCount(GraphicsAllocation::objectNotResident, commandStreamReceiver->getOsContext().getContextId());
                gpuAllocation->updateResidencyTaskCount(desiredTaskCount, commandStreamReceiver->getOsContext().getContextId());
                gpuAllocation->updateTaskCount(desiredTaskCount, commandStreamReceiver->getOsContext().getContextId());
            }
        }
    }
}

SvmMapOperation *SVMAllocsManager::getSvmMapOperation(const void *ptr) {
    std::shared_lock<std::shared_mutex> lock(mtx);
    return svmMapOperations.get(ptr);
}

void SVMAllocsManager::insertSvmMapOperation(void *regionSvmPtr, size_t regionSize, void *baseSvmPtr, size_t offset, bool readOnlyMap) {
    SvmMapOperation svmMapOperation;
    svmMapOperation.regionSvmPtr = regionSvmPtr;
    svmMapOperation.baseSvmPtr = baseSvmPtr;
    svmMapOperation.offset = offset;
    svmMapOperation.regionSize = regionSize;
    svmMapOperation.readOnlyMap = readOnlyMap;
    std::unique_lock<std::shared_mutex> lock(mtx);
    svmMapOperations.insert(svmMapOperation);
}

void SVMAllocsManager::removeSvmMapOperation(const void *regionSvmPtr) {
    std::unique_lock<std::shared_mutex> lock(mtx);
    svmMapOperations.remove(regionSvmPtr);
}

AllocationType SVMAllocsManager::getGraphicsAllocationTypeAndCompressionPreference(const UnifiedMemoryProperties &unifiedMemoryProperties, bool &compressionEnabled) const {
    compressionEnabled = false;

    AllocationType allocationType = AllocationType::bufferHostMemory;
    if (unifiedMemoryProperties.memoryType == InternalMemoryType::deviceUnifiedMemory) {
        if (unifiedMemoryProperties.allocationFlags.allocFlags.allocWriteCombined) {
            allocationType = AllocationType::writeCombined;
        } else {
            UNRECOVERABLE_IF(nullptr == unifiedMemoryProperties.device);
            auto &gfxCoreHelper = unifiedMemoryProperties.device->getGfxCoreHelper();
            auto &hwInfo = unifiedMemoryProperties.device->getHardwareInfo();
            if (CompressionSelector::allowStatelessCompression() || gfxCoreHelper.usmCompressionSupported(hwInfo)) {
                compressionEnabled = true;
            }
            if (unifiedMemoryProperties.requestedAllocationType != AllocationType::unknown) {
                allocationType = unifiedMemoryProperties.requestedAllocationType;
            } else {
                allocationType = AllocationType::buffer;
            }
        }
    }
    return allocationType;
}

static uint32_t getSubDeviceId(Device &device) {
    if (!device.isSubDevice()) {
        uint32_t deviceBitField = static_cast<uint32_t>(device.getDeviceBitfield().to_ulong());
        if (device.getDeviceBitfield().count() > 1) {
            deviceBitField &= ~deviceBitField + 1;
        }
        return Math::log2(deviceBitField);
    }
    return static_cast<NEO::SubDevice *>(&device)->getSubDeviceIndex();
};

static NEO::SubDeviceIdsVec getSubDeviceIds(CommandStreamReceiver &csr) {
    SubDeviceIdsVec subDeviceIds;
    for (auto subDeviceId = 0u; subDeviceId < csr.getOsContext().getDeviceBitfield().size(); subDeviceId++) {
        if (csr.getOsContext().getDeviceBitfield().test(subDeviceId)) {
            subDeviceIds.push_back(subDeviceId);
        }
    }
    return subDeviceIds;
};

void SVMAllocsManager::sharedSystemMemAdvise(Device &device, MemAdvise memAdviseOp, const void *ptr, const size_t size) {

    // All vm_ids on a single device for shared system USM allocation
    auto subDeviceIds = NEO::SubDevice::getSubDeviceIdsFromDevice(device);

    memoryManager->setSharedSystemMemAdvise(ptr, size, memAdviseOp, subDeviceIds, device.getRootDeviceIndex());
}

void SVMAllocsManager::prefetchMemory(Device &device, CommandStreamReceiver &commandStreamReceiver, const void *ptr, const size_t size) {

    auto svmData = getSVMAlloc(ptr);

    if (!svmData) {
        if (device.areSharedSystemAllocationsAllowed()) {
            // Single vm_id for shared system USM allocation
            auto subDeviceIds = SubDeviceIdsVec{getSubDeviceId(device)};
            memoryManager->prefetchSharedSystemAlloc(ptr, size, subDeviceIds, device.getRootDeviceIndex());
        }
        return;
    }

    if ((memoryManager->isKmdMigrationAvailable(device.getRootDeviceIndex()) &&
         (svmData->memoryType == InternalMemoryType::sharedUnifiedMemory))) {
        auto gfxAllocation = svmData->gpuAllocations.getGraphicsAllocation(device.getRootDeviceIndex());
        auto subDeviceIds = commandStreamReceiver.getActivePartitions() > 1 ? getSubDeviceIds(commandStreamReceiver) : SubDeviceIdsVec{getSubDeviceId(device)};
        memoryManager->setMemPrefetch(gfxAllocation, subDeviceIds, device.getRootDeviceIndex());
    }
}

void SVMAllocsManager::prefetchSVMAllocs(Device &device, CommandStreamReceiver &commandStreamReceiver) {
    std::shared_lock<std::shared_mutex> lock(mtx);

    auto subDeviceIds = commandStreamReceiver.getActivePartitions() > 1 ? getSubDeviceIds(commandStreamReceiver) : SubDeviceIdsVec{getSubDeviceId(device)};
    if (memoryManager->isKmdMigrationAvailable(device.getRootDeviceIndex())) {
        for (auto &allocation : this->svmAllocs.allocations) {
            NEO::SvmAllocationData svmData = *allocation.second;

            if (svmData.memoryType == InternalMemoryType::sharedUnifiedMemory) {
                auto gfxAllocation = svmData.gpuAllocations.getGraphicsAllocation(device.getRootDeviceIndex());
                memoryManager->setMemPrefetch(gfxAllocation, subDeviceIds, device.getRootDeviceIndex());
            }
        }
    }
}

void SVMAllocsManager::sharedSystemAtomicAccess(Device &device, AtomicAccessMode mode, const void *ptr, const size_t size) {

    // All vm_ids on a single device for shared system USM allocation
    auto subDeviceIds = NEO::SubDevice::getSubDeviceIdsFromDevice(device);

    memoryManager->setSharedSystemAtomicAccess(ptr, size, mode, subDeviceIds, device.getRootDeviceIndex());
}

std::unique_lock<std::mutex> SVMAllocsManager::obtainOwnership() {
    return std::unique_lock<std::mutex>(mtxForIndirectAccess);
}

void SVMAllocsManager::insertSVMAlloc(void *svmPtr, const SvmAllocationData &allocData) {
    std::unique_lock<std::shared_mutex> lock(mtx);
    this->svmAllocs.insert(svmPtr, allocData);
    UNRECOVERABLE_IF(internalAllocationsMap.count(allocData.getAllocId()) > 0);
    for (auto alloc : allocData.gpuAllocations.getGraphicsAllocations()) {
        if (alloc != nullptr) {
            internalAllocationsMap.insert({allocData.getAllocId(), alloc});
        }
    }
}

/**
 * @brief This method calls makeResident for allocation with specific allocId.
 * Since single allocation id might be shared for different allocations in multi gpu scenario,
 * this method iterates over all of these allocations and selects correct one based on device index
 *
 * @param[in] allocationId id of the allocation which should be resident
 * @param[in] csr command stream receiver which will make allocation resident
 */
void SVMAllocsManager::makeResidentForAllocationsWithId(uint32_t allocationId, CommandStreamReceiver &csr) {
    for (auto [iter, rangeEnd] = internalAllocationsMap.equal_range(allocationId); iter != rangeEnd; ++iter) {
        auto gpuAllocation = iter->second;
        if (gpuAllocation->getRootDeviceIndex() != csr.getRootDeviceIndex()) {
            continue;
        }
        csr.makeResident(*gpuAllocation);
        gpuAllocation->updateResidencyTaskCount(GraphicsAllocation::objectAlwaysResident, csr.getOsContext().getContextId());
        gpuAllocation->setEvictable(false);
    }
}

bool SVMAllocsManager::submitIndirectAllocationsAsPack(CommandStreamReceiver &csr) {
    auto submitAsPack = memoryManager->allowIndirectAllocationsAsPack(csr.getRootDeviceIndex());
    if (debugManager.flags.MakeIndirectAllocationsResidentAsPack.get() != -1) {
        submitAsPack = !!NEO::debugManager.flags.MakeIndirectAllocationsResidentAsPack.get();
    }

    if (submitAsPack) {
        makeIndirectAllocationsResident(csr, csr.peekTaskCount() + 1u);
    }
    return submitAsPack;
}
} // namespace NEO
