/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_manager.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/sub_device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/basic_math.h"
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

void SVMAllocsManager::SortedVectorBasedAllocationTracker::insert(const SvmAllocationData &allocationsPair) {
    allocations.push_back(std::make_pair(reinterpret_cast<void *>(allocationsPair.gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress()), std::make_unique<SvmAllocationData>(allocationsPair)));
    for (size_t i = allocations.size() - 1; i > 0; --i) {
        if (allocations[i].first < allocations[i - 1].first) {
            std::iter_swap(allocations.begin() + i, allocations.begin() + i - 1);
        } else {
            break;
        }
    }
}

void SVMAllocsManager::SortedVectorBasedAllocationTracker::remove(const SvmAllocationData &allocationsPair) {
    auto gpuAddress = reinterpret_cast<void *>(allocationsPair.gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress());
    auto removeIt = std::remove_if(allocations.begin(), allocations.end(), [&gpuAddress](const auto &other) {
        return gpuAddress == other.first;
    });
    allocations.erase(removeIt);
}

void SVMAllocsManager::SvmAllocationCache::insert(size_t size, void *ptr) {
    std::lock_guard<std::mutex> lock(this->mtx);
    allocations.emplace(std::lower_bound(allocations.begin(), allocations.end(), size), size, ptr);
}

void *SVMAllocsManager::SvmAllocationCache::get(size_t size, const UnifiedMemoryProperties &unifiedMemoryProperties, SVMAllocsManager *svmAllocsManager) {
    std::lock_guard<std::mutex> lock(this->mtx);
    for (auto allocationIter = std::lower_bound(allocations.begin(), allocations.end(), size);
         allocationIter != allocations.end();
         ++allocationIter) {
        void *allocationPtr = allocationIter->allocation;
        SvmAllocationData *svmAllocData = svmAllocsManager->getSVMAlloc(allocationPtr);
        UNRECOVERABLE_IF(!svmAllocData);
        if (svmAllocData->device == unifiedMemoryProperties.device &&
            svmAllocData->allocationFlagsProperty.allFlags == unifiedMemoryProperties.allocationFlags.allFlags &&
            svmAllocData->allocationFlagsProperty.allAllocFlags == unifiedMemoryProperties.allocationFlags.allAllocFlags) {
            allocations.erase(allocationIter);
            return allocationPtr;
        }
    }
    return nullptr;
}

void SVMAllocsManager::SvmAllocationCache::trim(SVMAllocsManager *svmAllocsManager) {
    std::lock_guard<std::mutex> lock(this->mtx);
    for (auto &cachedAllocationInfo : this->allocations) {
        SvmAllocationData *svmData = svmAllocsManager->getSVMAlloc(cachedAllocationInfo.allocation);
        DEBUG_BREAK_IF(nullptr == svmData);
        svmAllocsManager->freeSVMAllocImpl(cachedAllocationInfo.allocation, FreePolicyType::POLICY_NONE, svmData);
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

SvmAllocationData *SVMAllocsManager::SortedVectorBasedAllocationTracker::get(const void *ptr) {
    if (allocations.size() == 0) {
        return nullptr;
    }
    if (!ptr) {
        return nullptr;
    }

    int begin = 0;
    int end = static_cast<int>(allocations.size() - 1);
    while (end >= begin) {
        int currentPos = (begin + end) / 2;
        const auto &allocation = allocations[currentPos];
        if (allocation.first == ptr || (allocation.first < ptr &&
                                        (reinterpret_cast<uintptr_t>(ptr) < (reinterpret_cast<uintptr_t>(allocation.first) + allocation.second->size)))) {
            return allocation.second.get();
        } else if (ptr < allocation.first) {
            end = currentPos - 1;
            continue;
        } else {
            begin = currentPos + 1;
            continue;
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

        if (!(allocation.second->memoryType & requestedTypesMask) ||
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
        if (allocation.second->memoryType & requestedTypesMask) {
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
    this->usmDeviceAllocationsCacheEnabled = NEO::ApiSpecificConfig::isDeviceAllocationCacheEnabled();
    if (debugManager.flags.ExperimentalEnableDeviceAllocationCache.get() != -1) {
        this->usmDeviceAllocationsCacheEnabled = !!debugManager.flags.ExperimentalEnableDeviceAllocationCache.get();
    }
    if (this->usmDeviceAllocationsCacheEnabled) {
        this->initUsmDeviceAllocationsCache();
    }
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
        UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::NOT_SPECIFIED, 1, rootDeviceIndices, subdeviceBitfields);
        return createUnifiedAllocationWithDeviceStorage(size, svmProperties, unifiedMemoryProperties);
    }
}

void *SVMAllocsManager::createHostUnifiedMemoryAllocation(size_t size,
                                                          const UnifiedMemoryProperties &memoryProperties) {
    size_t pageSizeForAlignment = alignUp<size_t>(memoryProperties.alignment, MemoryConstants::pageSize);
    size_t alignedSize = alignUp<size_t>(size, MemoryConstants::pageSize);

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
    unifiedMemoryProperties.alignment = pageSizeForAlignment;
    unifiedMemoryProperties.flags.preferCompressed = compressionEnabled;
    unifiedMemoryProperties.flags.shareable = memoryProperties.allocationFlags.flags.shareable;
    unifiedMemoryProperties.flags.isUSMHostAllocation = true;
    unifiedMemoryProperties.flags.isUSMDeviceAllocation = false;
    unifiedMemoryProperties.cacheRegion = MemoryPropertiesHelper::getCacheRegion(memoryProperties.allocationFlags);

    auto maxRootDeviceIndex = *std::max_element(rootDeviceIndicesVector.begin(), rootDeviceIndicesVector.end(), std::less<uint32_t const>());
    SvmAllocationData allocData(maxRootDeviceIndex);
    void *externalHostPointer = reinterpret_cast<void *>(memoryProperties.allocationFlags.hostptr);

    void *usmPtr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndicesVector, unifiedMemoryProperties, allocData.gpuAllocations, externalHostPointer);
    if (!usmPtr) {
        return nullptr;
    }

    allocData.cpuAllocation = nullptr;
    allocData.size = size;
    allocData.memoryType = memoryProperties.memoryType;
    allocData.allocationFlagsProperty = memoryProperties.allocationFlags;
    allocData.device = nullptr;
    allocData.pageSizeForAlignment = pageSizeForAlignment;
    allocData.setAllocId(this->allocationsCounter++);

    std::unique_lock<std::shared_mutex> lock(mtx);
    this->svmAllocs.insert(allocData);

    return usmPtr;
}

void *SVMAllocsManager::createUnifiedMemoryAllocation(size_t size,
                                                      const UnifiedMemoryProperties &memoryProperties) {
    auto rootDeviceIndex = memoryProperties.getRootDeviceIndex();
    DeviceBitfield deviceBitfield = memoryProperties.subdeviceBitfields.at(rootDeviceIndex);
    size_t pageSizeForAlignment = alignUp<size_t>(memoryProperties.alignment, MemoryConstants::pageSize64k);
    size_t alignedSize = alignUp<size_t>(size, MemoryConstants::pageSize64k);

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
    unifiedMemoryProperties.alignment = pageSizeForAlignment;
    unifiedMemoryProperties.flags.isUSMDeviceAllocation = false;
    unifiedMemoryProperties.flags.shareable = memoryProperties.allocationFlags.flags.shareable;
    unifiedMemoryProperties.cacheRegion = MemoryPropertiesHelper::getCacheRegion(memoryProperties.allocationFlags);
    unifiedMemoryProperties.flags.uncacheable = memoryProperties.allocationFlags.flags.locallyUncachedResource;
    unifiedMemoryProperties.flags.preferCompressed = compressionEnabled || memoryProperties.allocationFlags.flags.compressedHint;
    unifiedMemoryProperties.flags.resource48Bit = memoryProperties.allocationFlags.flags.resource48Bit;

    if (memoryProperties.memoryType == InternalMemoryType::DEVICE_UNIFIED_MEMORY) {
        unifiedMemoryProperties.flags.isUSMDeviceAllocation = true;
        if (this->usmDeviceAllocationsCacheEnabled) {
            void *allocationFromCache = this->usmDeviceAllocationsCache.get(size, memoryProperties, this);
            if (allocationFromCache) {
                return allocationFromCache;
            }
        }
    } else if (memoryProperties.memoryType == InternalMemoryType::HOST_UNIFIED_MEMORY) {
        unifiedMemoryProperties.flags.isUSMHostAllocation = true;
    } else {
        unifiedMemoryProperties.flags.isUSMHostAllocation = useExternalHostPtrForCpu;
    }

    GraphicsAllocation *unifiedMemoryAllocation = memoryManager->allocateGraphicsMemoryWithProperties(unifiedMemoryProperties, externalPtr);
    if (!unifiedMemoryAllocation) {
        if (memoryProperties.memoryType == InternalMemoryType::DEVICE_UNIFIED_MEMORY &&
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
    allocData.setAllocId(this->allocationsCounter++);

    std::unique_lock<std::shared_mutex> lock(mtx);
    this->svmAllocs.insert(allocData);

    auto retPtr = reinterpret_cast<void *>(unifiedMemoryAllocation->getGpuAddress());
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
    size_t pageSizeForAlignment = std::max(alignUp<size_t>(unifiedMemoryProperties.alignment, MemoryConstants::pageSize2M), MemoryConstants::pageSize2M);
    size_t alignedSize = alignUp<size_t>(size, MemoryConstants::pageSize2M);
    AllocationProperties gpuProperties{rootDeviceIndex,
                                       true,
                                       alignedSize,
                                       AllocationType::UNIFIED_SHARED_MEMORY,
                                       false,
                                       false,
                                       deviceBitfield};

    gpuProperties.alignment = pageSizeForAlignment;
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
    allocData.setAllocId(this->allocationsCounter++);

    std::unique_lock<std::shared_mutex> lock(mtx);
    this->svmAllocs.insert(allocData);
    return allocationGpu->getUnderlyingBuffer();
}

void SVMAllocsManager::setUnifiedAllocationProperties(GraphicsAllocation *allocation, const SvmAllocationProperties &svmProperties) {
    allocation->setMemObjectsAllocationWithWritableFlags(!svmProperties.readOnly && !svmProperties.hostPtrReadOnly);
    allocation->setCoherent(svmProperties.coherent);
}

void SVMAllocsManager::insertSVMAlloc(const SvmAllocationData &svmAllocData) {
    std::unique_lock<std::shared_mutex> lock(mtx);
    svmAllocs.insert(svmAllocData);
}

void SVMAllocsManager::removeSVMAlloc(const SvmAllocationData &svmAllocData) {
    std::unique_lock<std::shared_mutex> lock(mtx);
    svmAllocs.remove(svmAllocData);
}

bool SVMAllocsManager::freeSVMAlloc(void *ptr, bool blocking) {
    if (svmDeferFreeAllocs.allocations.size() > 0) {
        this->freeSVMAllocDeferImpl();
    }
    SvmAllocationData *svmData = getSVMAlloc(ptr);
    if (svmData) {
        if (InternalMemoryType::DEVICE_UNIFIED_MEMORY == svmData->memoryType &&
            this->usmDeviceAllocationsCacheEnabled) {
            this->usmDeviceAllocationsCache.insert(svmData->size, ptr);
            return true;
        }
        if (blocking) {
            this->freeSVMAllocImpl(ptr, FreePolicyType::POLICY_BLOCKING, svmData);
        } else {
            this->freeSVMAllocImpl(ptr, FreePolicyType::POLICY_NONE, svmData);
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
        if (InternalMemoryType::DEVICE_UNIFIED_MEMORY == svmData->memoryType &&
            this->usmDeviceAllocationsCacheEnabled) {
            this->usmDeviceAllocationsCache.insert(svmData->size, ptr);
            return true;
        }
        this->freeSVMAllocImpl(ptr, FreePolicyType::POLICY_DEFER, svmData);
        return true;
    }
    return false;
}

void SVMAllocsManager::freeSVMAllocImpl(void *ptr, FreePolicyType policy, SvmAllocationData *svmData) {
    this->prepareIndirectAllocationForDestruction(svmData);

    if (policy == FreePolicyType::POLICY_BLOCKING) {
        if (svmData->cpuAllocation) {
            this->memoryManager->waitForEnginesCompletion(*svmData->cpuAllocation);
        }

        for (auto &gpuAllocation : svmData->gpuAllocations.getGraphicsAllocations()) {
            if (gpuAllocation) {
                this->memoryManager->waitForEnginesCompletion(*gpuAllocation);
            }
        }
    } else if (policy == FreePolicyType::POLICY_DEFER) {
        if (svmData->cpuAllocation) {
            if (this->memoryManager->allocInUse(*svmData->cpuAllocation)) {
                if (getSVMDeferFreeAlloc(ptr) == nullptr) {
                    this->svmDeferFreeAllocs.insert(*svmData);
                }
                return;
            }
        }
        for (auto &gpuAllocation : svmData->gpuAllocations.getGraphicsAllocations()) {
            if (gpuAllocation) {
                if (this->memoryManager->allocInUse(*gpuAllocation)) {
                    if (getSVMDeferFreeAlloc(ptr) == nullptr) {
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
    if (svmData->gpuAllocations.getAllocationType() == AllocationType::SVM_ZERO_COPY) {
        freeZeroCopySvmAllocation(svmData);
    } else {
        freeSvmAllocationWithDeviceStorage(svmData);
    }
}

void SVMAllocsManager::freeSVMAllocDeferImpl() {
    std::vector<void *> freedPtr;
    for (auto iter = svmDeferFreeAllocs.allocations.begin(); iter != svmDeferFreeAllocs.allocations.end(); ++iter) {
        void *ptr = reinterpret_cast<void *>(iter->second.gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress());
        this->freeSVMAllocImpl(ptr, FreePolicyType::POLICY_DEFER, this->getSVMAlloc(ptr));

        if (this->getSVMAlloc(ptr) == nullptr) {
            freedPtr.push_back(ptr);
        }
    }
    for (uint32_t i = 0; i < freedPtr.size(); ++i) {
        svmDeferFreeAllocs.allocations.erase(freedPtr[i]);
    }
}

void SVMAllocsManager::trimUSMDeviceAllocCache() {
    this->usmDeviceAllocationsCache.trim(this);
}

void *SVMAllocsManager::createZeroCopySvmAllocation(size_t size, const SvmAllocationProperties &svmProperties,
                                                    const RootDeviceIndicesContainer &rootDeviceIndices,
                                                    const std::map<uint32_t, DeviceBitfield> &subdeviceBitfields) {

    auto rootDeviceIndex = *rootDeviceIndices.begin();
    auto &deviceBitfield = subdeviceBitfields.at(rootDeviceIndex);
    AllocationProperties properties{rootDeviceIndex,
                                    true, // allocateMemory
                                    size,
                                    AllocationType::SVM_ZERO_COPY,
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

    std::unique_lock<std::shared_mutex> lock(mtx);
    this->svmAllocs.insert(allocData);
    return usmPtr;
}

void *SVMAllocsManager::createUnifiedAllocationWithDeviceStorage(size_t size, const SvmAllocationProperties &svmProperties, const UnifiedMemoryProperties &unifiedMemoryProperties) {
    auto rootDeviceIndex = unifiedMemoryProperties.getRootDeviceIndex();
    auto externalPtr = reinterpret_cast<void *>(unifiedMemoryProperties.allocationFlags.hostptr);
    bool useExternalHostPtrForCpu = externalPtr != nullptr;
    const auto pageSizeForAlignment = alignUp<size_t>(unifiedMemoryProperties.alignment, MemoryConstants::pageSize64k);
    size_t alignedSize = alignUp<size_t>(size, MemoryConstants::pageSize64k);
    DeviceBitfield subDevices = unifiedMemoryProperties.subdeviceBitfields.at(rootDeviceIndex);
    auto cpuAlignment = std::max(pageSizeForAlignment, memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[rootDeviceIndex]->getProductHelper().getSvmCpuAlignment());
    AllocationProperties cpuProperties{rootDeviceIndex,
                                       !useExternalHostPtrForCpu, // allocateMemory
                                       alignUp(alignedSize, cpuAlignment), AllocationType::SVM_CPU,
                                       false, // isMultiStorageAllocation
                                       subDevices};
    cpuProperties.alignment = cpuAlignment;
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

    AllocationProperties gpuProperties{rootDeviceIndex,
                                       false,
                                       alignedSize,
                                       AllocationType::SVM_GPU,
                                       false,
                                       multiStorageAllocation,
                                       subDevices};

    gpuProperties.alignment = pageSizeForAlignment;
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
    allocData.pageSizeForAlignment = cpuAlignment;
    allocData.size = size;
    allocData.setAllocId(this->allocationsCounter++);

    std::unique_lock<std::shared_mutex> lock(mtx);
    this->svmAllocs.insert(allocData);
    return svmPtr;
}

void SVMAllocsManager::freeSVMData(SvmAllocationData *svmData) {
    std::unique_lock<std::mutex> lockForIndirect(mtxForIndirectAccess);
    std::unique_lock<std::shared_mutex> lock(mtx);
    svmAllocs.remove(*svmData);
}

void SVMAllocsManager::freeZeroCopySvmAllocation(SvmAllocationData *svmData) {
    auto gpuAllocations = svmData->gpuAllocations;
    freeSVMData(svmData);
    for (const auto &graphicsAllocation : gpuAllocations.getGraphicsAllocations()) {
        memoryManager->freeGraphicsMemory(graphicsAllocation);
    }
}

void SVMAllocsManager::initUsmDeviceAllocationsCache() {
    this->usmDeviceAllocationsCache.allocations.reserve(128u);
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
        if (allocation.second->memoryType == InternalMemoryType::HOST_UNIFIED_MEMORY) {
            return true;
        }
    }
    return false;
}

void SVMAllocsManager::makeIndirectAllocationsResident(CommandStreamReceiver &commandStreamReceiver, TaskCountType taskCount) {
    std::unique_lock<std::shared_mutex> lock(mtx);
    bool parseAllAllocations = false;
    auto entry = indirectAllocationsResidency.find(&commandStreamReceiver);

    if (entry == indirectAllocationsResidency.end()) {
        parseAllAllocations = true;

        InternalAllocationsTracker tracker = {};
        tracker.latestResidentObjectId = this->allocationsCounter;
        tracker.latestSentTaskCount = taskCount;

        this->indirectAllocationsResidency.insert(std::make_pair(&commandStreamReceiver, tracker));
    } else {
        if (this->allocationsCounter > entry->second.latestResidentObjectId) {
            parseAllAllocations = true;

            entry->second.latestResidentObjectId = this->allocationsCounter;
        }
        entry->second.latestSentTaskCount = taskCount;
    }
    if (parseAllAllocations) {
        for (auto &allocation : this->svmAllocs.allocations) {
            auto gpuAllocation = allocation.second->gpuAllocations.getGraphicsAllocation(commandStreamReceiver.getRootDeviceIndex());
            if (gpuAllocation == nullptr) {
                continue;
            }
            commandStreamReceiver.makeResident(*gpuAllocation);
            gpuAllocation->updateResidencyTaskCount(GraphicsAllocation::objectAlwaysResident, commandStreamReceiver.getOsContext().getContextId());
            gpuAllocation->setEvictable(false);
        }
    }
}

void SVMAllocsManager::prepareIndirectAllocationForDestruction(SvmAllocationData *allocationData) {
    std::unique_lock<std::shared_mutex> lock(mtx);
    if (this->indirectAllocationsResidency.size() > 0u) {
        for (auto &internalAllocationsHandling : this->indirectAllocationsResidency) {
            auto commandStreamReceiver = internalAllocationsHandling.first;
            auto gpuAllocation = allocationData->gpuAllocations.getGraphicsAllocation(commandStreamReceiver->getRootDeviceIndex());
            if (gpuAllocation == nullptr) {
                continue;
            }
            auto desiredTaskCount = std::max(internalAllocationsHandling.second.latestSentTaskCount, gpuAllocation->getTaskCount(commandStreamReceiver->getOsContext().getContextId()));
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

    AllocationType allocationType = AllocationType::BUFFER_HOST_MEMORY;
    if (unifiedMemoryProperties.memoryType == InternalMemoryType::DEVICE_UNIFIED_MEMORY) {
        if (unifiedMemoryProperties.allocationFlags.allocFlags.allocWriteCombined) {
            allocationType = AllocationType::WRITE_COMBINED;
        } else {
            UNRECOVERABLE_IF(nullptr == unifiedMemoryProperties.device);
            if (CompressionSelector::allowStatelessCompression()) {
                compressionEnabled = true;
            }
            if (unifiedMemoryProperties.requestedAllocationType != AllocationType::UNKNOWN) {
                allocationType = unifiedMemoryProperties.requestedAllocationType;
            } else {
                allocationType = AllocationType::BUFFER;
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
        (svmData.memoryType == InternalMemoryType::DEVICE_UNIFIED_MEMORY)) {
        isChunkingNeededForDeviceAllocations = true;
    }

    if ((memoryManager->isKmdMigrationAvailable(device.getRootDeviceIndex()) &&
         (svmData.memoryType == InternalMemoryType::SHARED_UNIFIED_MEMORY)) ||
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
} // namespace NEO
