/*
 * Copyright (C) 2019-2024 Intel Corporation
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
#include "shared/source/helpers/memory_properties_helpers.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/compression_selector.h"
#include "shared/source/memory_manager/memory_manager.h"
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

bool SVMAllocsManager::SvmAllocationCache::insert(size_t size, void *ptr, SvmAllocationData *svmData) {
    if (false == sizeAllowed(size)) {
        return false;
    }
    std::lock_guard<std::mutex> lock(this->mtx);
    if (auto device = svmData->device) {
        auto lock = device->obtainAllocationsReuseLock();
        const auto usedSize = device->getAllocationsSavedForReuseSize();
        if (size + usedSize > this->maxSize) {
            return false;
        }
        device->recordAllocationSaveForReuse(size);
    } else {
        auto lock = memoryManager->obtainHostAllocationsReuseLock();
        const auto usedSize = memoryManager->getHostAllocationsSavedForReuseSize();
        if (size + usedSize > this->maxSize) {
            return false;
        }
        memoryManager->recordHostAllocationSaveForReuse(size);
    }
    allocations.emplace(std::lower_bound(allocations.begin(), allocations.end(), size), size, ptr);
    return true;
}

bool SVMAllocsManager::SvmAllocationCache::allocUtilizationAllows(size_t requestedSize, size_t reuseCandidateSize) {
    if (reuseCandidateSize >= SvmAllocationCache::minimalSizeToCheckUtilization) {
        const auto allocUtilization = static_cast<double>(requestedSize) / reuseCandidateSize;
        return allocUtilization >= SvmAllocationCache::minimalAllocUtilization;
    }
    return true;
}

bool SVMAllocsManager::SvmAllocationCache::isInUse(SvmAllocationData *svmData) {
    if (svmData->cpuAllocation && memoryManager->allocInUse(*svmData->cpuAllocation)) {
        return true;
    }
    for (auto &gpuAllocation : svmData->gpuAllocations.getGraphicsAllocations()) {
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
        SvmAllocationData *svmAllocData = svmAllocsManager->getSVMAlloc(allocationPtr);
        UNRECOVERABLE_IF(!svmAllocData);
        if (svmAllocData->device == unifiedMemoryProperties.device &&
            svmAllocData->allocationFlagsProperty.allFlags == unifiedMemoryProperties.allocationFlags.allFlags &&
            svmAllocData->allocationFlagsProperty.allAllocFlags == unifiedMemoryProperties.allocationFlags.allAllocFlags &&
            false == isInUse(svmAllocData)) {
            if (svmAllocData->device) {
                auto lock = svmAllocData->device->obtainAllocationsReuseLock();
                svmAllocData->device->recordAllocationGetFromReuse(allocationIter->allocationSize);
            } else {
                auto lock = memoryManager->obtainHostAllocationsReuseLock();
                memoryManager->recordHostAllocationGetFromReuse(allocationIter->allocationSize);
            }
            allocations.erase(allocationIter);
            return allocationPtr;
        }
    }
    return nullptr;
}

void SVMAllocsManager::SvmAllocationCache::trim() {
    std::lock_guard<std::mutex> lock(this->mtx);
    for (auto &cachedAllocationInfo : this->allocations) {
        SvmAllocationData *svmData = svmAllocsManager->getSVMAlloc(cachedAllocationInfo.allocation);
        DEBUG_BREAK_IF(nullptr == svmData);
        if (svmData->device) {
            auto lock = svmData->device->obtainAllocationsReuseLock();
            svmData->device->recordAllocationGetFromReuse(cachedAllocationInfo.allocationSize);
        } else {
            auto lock = memoryManager->obtainHostAllocationsReuseLock();
            memoryManager->recordHostAllocationGetFromReuse(cachedAllocationInfo.allocationSize);
        }
        svmAllocsManager->freeSVMAllocImpl(cachedAllocationInfo.allocation, FreePolicyType::none, svmData);
    }
    this->allocations.clear();
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

SVMAllocsManager::SVMAllocsManager(MemoryManager *memoryManager, bool multiOsContextSupport)
    : memoryManager(memoryManager), multiOsContextSupport(multiOsContextSupport) {
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
    constexpr size_t pageSizeForAlignment = MemoryConstants::pageSize;
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
                                                 (deviceBitfield.count() > 1) && multiOsContextSupport,
                                                 deviceBitfield};
    unifiedMemoryProperties.alignment = alignUpNonZero<size_t>(memoryProperties.alignment, pageSizeForAlignment);
    unifiedMemoryProperties.flags.preferCompressed = compressionEnabled;
    unifiedMemoryProperties.flags.shareable = memoryProperties.allocationFlags.flags.shareable;
    unifiedMemoryProperties.flags.isUSMHostAllocation = true;
    unifiedMemoryProperties.flags.isUSMDeviceAllocation = false;
    unifiedMemoryProperties.cacheRegion = MemoryPropertiesHelper::getCacheRegion(memoryProperties.allocationFlags);

    if (this->usmHostAllocationsCacheEnabled) {
        void *allocationFromCache = this->usmHostAllocationsCache.get(size, memoryProperties);
        if (allocationFromCache) {
            return allocationFromCache;
        }
    }

    auto maxRootDeviceIndex = *std::max_element(rootDeviceIndicesVector.begin(), rootDeviceIndicesVector.end(), std::less<uint32_t const>());
    SvmAllocationData allocData(maxRootDeviceIndex);
    void *externalHostPointer = reinterpret_cast<void *>(memoryProperties.allocationFlags.hostptr);

    void *usmPtr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndicesVector, unifiedMemoryProperties, allocData.gpuAllocations, externalHostPointer);
    if (!usmPtr) {
        if (this->usmHostAllocationsCacheEnabled) {
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

    bool multiStorageAllocation = (deviceBitfield.count() > 1) && multiOsContextSupport;
    if ((deviceBitfield.count() > 1) && !multiOsContextSupport) {
        for (uint32_t i = 0;; i++) {
            if (deviceBitfield.test(i)) {
                deviceBitfield.reset();
                deviceBitfield.set(i);
                break;
            }
        }
    }

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
        if (this->usmDeviceAllocationsCacheEnabled &&
            false == memoryProperties.isInternalAllocation) {
            void *allocationFromCache = this->usmDeviceAllocationsCache.get(size, memoryProperties);
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
            this->usmDeviceAllocationsCacheEnabled) {
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
            false == svmData->isInternalAllocation &&
            this->usmDeviceAllocationsCacheEnabled) {
            if (this->usmDeviceAllocationsCache.insert(svmData->gpuAllocations.getDefaultGraphicsAllocation()->getUnderlyingBufferSize(), ptr, svmData)) {
                return true;
            }
        }
        if (InternalMemoryType::hostUnifiedMemory == svmData->memoryType &&
            this->usmHostAllocationsCacheEnabled) {
            if (this->usmHostAllocationsCache.insert(svmData->size, ptr, svmData)) {
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
        if (InternalMemoryType::deviceUnifiedMemory == svmData->memoryType &&
            this->usmDeviceAllocationsCacheEnabled) {
            if (this->usmDeviceAllocationsCache.insert(svmData->size, ptr, svmData)) {
                return true;
            }
        }
        if (InternalMemoryType::hostUnifiedMemory == svmData->memoryType &&
            this->usmHostAllocationsCacheEnabled) {
            if (this->usmHostAllocationsCache.insert(svmData->size, ptr, svmData)) {
                return true;
            }
        }
        this->freeSVMAllocImpl(ptr, FreePolicyType::defer, svmData);
        return true;
    }
    return false;
}

void SVMAllocsManager::freeSVMAllocImpl(void *ptr, FreePolicyType policy, SvmAllocationData *svmData) {
    auto allowNonBlockingFree = policy == FreePolicyType::none;
    this->prepareIndirectAllocationForDestruction(svmData, allowNonBlockingFree);

    if (policy == FreePolicyType::blocking) {
        if (svmData->cpuAllocation) {
            this->memoryManager->waitForEnginesCompletion(*svmData->cpuAllocation);
        }

        for (auto &gpuAllocation : svmData->gpuAllocations.getGraphicsAllocations()) {
            if (gpuAllocation) {
                this->memoryManager->waitForEnginesCompletion(*gpuAllocation);
            }
        }
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

void SVMAllocsManager::trimUSMDeviceAllocCache() {
    this->usmDeviceAllocationsCache.trim();
}

void SVMAllocsManager::trimUSMHostAllocCache() {
    this->usmHostAllocationsCache.trim();
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

    bool multiStorageAllocation = (subDevices.count() > 1) && multiOsContextSupport;
    if ((subDevices.count() > 1) && !multiOsContextSupport) {
        for (uint32_t i = 0;; i++) {
            if (subDevices.test(i)) {
                subDevices.reset();
                subDevices.set(i);
                break;
            }
        }
    }

    const size_t alignedGpuSize = alignUp<size_t>(size, MemoryConstants::pageSize64k);
    AllocationProperties gpuProperties{rootDeviceIndex,
                                       false,
                                       alignedGpuSize,
                                       AllocationType::svmGpu,
                                       false,
                                       multiStorageAllocation,
                                       subDevices};

    gpuProperties.alignment = alignment;
    auto compressionSupported = false;
    if (unifiedMemoryProperties.device) {
        compressionSupported = memoryManager->usmCompressionSupported(unifiedMemoryProperties.device);
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
    const auto totalDeviceMemory = device.getGlobalMemorySize(static_cast<uint32_t>(device.getDeviceBitfield().to_ulong()));
    auto ailConfiguration = device.getAilConfigurationHelper();
    const bool limitDeviceMemoryForReuse = ailConfiguration && ailConfiguration->limitAmountOfDeviceMemoryForRecycling();
    auto fractionOfTotalMemoryForRecycling = limitDeviceMemoryForReuse ? 0 : 0.08;
    if (debugManager.flags.ExperimentalEnableDeviceAllocationCache.get() != -1) {
        fractionOfTotalMemoryForRecycling = 0.01 * std::min(100, debugManager.flags.ExperimentalEnableDeviceAllocationCache.get());
    }
    this->usmDeviceAllocationsCache.maxSize = static_cast<size_t>(fractionOfTotalMemoryForRecycling * totalDeviceMemory);
    if (this->usmDeviceAllocationsCache.maxSize > 0u) {
        this->usmDeviceAllocationsCache.allocations.reserve(128u);
    }
    this->usmDeviceAllocationsCache.svmAllocsManager = this;
    this->usmDeviceAllocationsCache.memoryManager = memoryManager;
}

void SVMAllocsManager::initUsmHostAllocationsCache() {
    const auto totalSystemMemory = this->memoryManager->getSystemSharedMemory(0u);
    auto fractionOfTotalMemoryForRecycling = 0.02;
    if (debugManager.flags.ExperimentalEnableHostAllocationCache.get() != -1) {
        fractionOfTotalMemoryForRecycling = 0.01 * std::min(100, debugManager.flags.ExperimentalEnableHostAllocationCache.get());
    }
    this->usmHostAllocationsCache.maxSize = static_cast<size_t>(fractionOfTotalMemoryForRecycling * totalSystemMemory);
    if (this->usmHostAllocationsCache.maxSize > 0u) {
        this->usmHostAllocationsCache.allocations.reserve(128u);
    }
    this->usmHostAllocationsCache.svmAllocsManager = this;
    this->usmHostAllocationsCache.memoryManager = memoryManager;
}

void SVMAllocsManager::initUsmAllocationsCaches(Device &device) {
    this->usmDeviceAllocationsCacheEnabled = NEO::ApiSpecificConfig::isDeviceAllocationCacheEnabled() && device.getProductHelper().isDeviceUsmAllocationReuseSupported();
    if (debugManager.flags.ExperimentalEnableDeviceAllocationCache.get() != -1) {
        this->usmDeviceAllocationsCacheEnabled = !!debugManager.flags.ExperimentalEnableDeviceAllocationCache.get();
    }
    if (this->usmDeviceAllocationsCacheEnabled) {
        this->initUsmDeviceAllocationsCache(device);
    }

    this->usmHostAllocationsCacheEnabled = NEO::ApiSpecificConfig::isHostAllocationCacheEnabled() && device.getProductHelper().isHostUsmAllocationReuseSupported();
    if (debugManager.flags.ExperimentalEnableHostAllocationCache.get() != -1) {
        this->usmHostAllocationsCacheEnabled = !!debugManager.flags.ExperimentalEnableHostAllocationCache.get();
    }
    if (this->usmHostAllocationsCacheEnabled) {
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
            if (CompressionSelector::allowStatelessCompression() || memoryManager->usmCompressionSupported(unifiedMemoryProperties.device)) {
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

void SVMAllocsManager::prefetchMemory(Device &device, CommandStreamReceiver &commandStreamReceiver, SvmAllocationData &svmData) {
    auto getSubDeviceId = [](Device &device) {
        if (!device.isSubDevice()) {
            uint32_t deviceBitField = static_cast<uint32_t>(device.getDeviceBitfield().to_ulong());
            if (device.getDeviceBitfield().count() > 1) {
                deviceBitField &= ~deviceBitField + 1;
            }
            return Math::log2(deviceBitField);
        }
        return static_cast<NEO::SubDevice *>(&device)->getSubDeviceIndex();
    };

    auto getSubDeviceIds = [](CommandStreamReceiver &csr) {
        SubDeviceIdsVec subDeviceIds;
        for (auto subDeviceId = 0u; subDeviceId < csr.getOsContext().getDeviceBitfield().size(); subDeviceId++) {
            if (csr.getOsContext().getDeviceBitfield().test(subDeviceId)) {
                subDeviceIds.push_back(subDeviceId);
            }
        }
        return subDeviceIds;
    };

    // Perform prefetch for chunks if EnableBOChunkingPrefetch is 1
    // and if KMD migration is set, as current target is to use
    // chunking only with KMD migration
    bool isChunkingNeededForDeviceAllocations = false;
    if (NEO::debugManager.flags.EnableBOChunkingDevMemPrefetch.get() &&
        memoryManager->isKmdMigrationAvailable(device.getRootDeviceIndex()) &&
        (svmData.memoryType == InternalMemoryType::deviceUnifiedMemory)) {
        isChunkingNeededForDeviceAllocations = true;
    }

    if ((memoryManager->isKmdMigrationAvailable(device.getRootDeviceIndex()) &&
         (svmData.memoryType == InternalMemoryType::sharedUnifiedMemory)) ||
        isChunkingNeededForDeviceAllocations) {
        auto gfxAllocation = svmData.gpuAllocations.getGraphicsAllocation(device.getRootDeviceIndex());
        auto subDeviceIds = commandStreamReceiver.getActivePartitions() > 1 ? getSubDeviceIds(commandStreamReceiver) : SubDeviceIdsVec{getSubDeviceId(device)};
        memoryManager->setMemPrefetch(gfxAllocation, subDeviceIds, device.getRootDeviceIndex());
    }
}

void SVMAllocsManager::prefetchSVMAllocs(Device &device, CommandStreamReceiver &commandStreamReceiver) {
    std::shared_lock<std::shared_mutex> lock(mtx);
    for (auto &allocation : this->svmAllocs.allocations) {
        NEO::SvmAllocationData allocData = *allocation.second;
        this->prefetchMemory(device, commandStreamReceiver, allocData);
    }
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
