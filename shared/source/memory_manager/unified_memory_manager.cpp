/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_manager.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/memory_properties_helpers.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

void SVMAllocsManager::MapBasedAllocationTracker::insert(SvmAllocationData allocationsPair) {
    allocations.insert(std::make_pair(reinterpret_cast<void *>(allocationsPair.gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress()), allocationsPair));
}

void SVMAllocsManager::MapBasedAllocationTracker::remove(SvmAllocationData allocationsPair) {
    SvmAllocationContainer::iterator iter;
    iter = allocations.find(reinterpret_cast<void *>(allocationsPair.gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress()));
    allocations.erase(iter);
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
        svmAllocsManager->freeSVMAllocImpl(cachedAllocationInfo.allocation, false, svmData);
    }
    this->allocations.clear();
}

SvmAllocationData *SVMAllocsManager::MapBasedAllocationTracker::get(const void *ptr) {
    SvmAllocationContainer::iterator iter, end;
    SvmAllocationData *svmAllocData;
    if ((ptr == nullptr) || (allocations.size() == 0)) {
        return nullptr;
    }
    end = allocations.end();
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
    for (auto &allocation : this->SVMAllocs.allocations) {
        if (rootDeviceIndex >= allocation.second.gpuAllocations.getGraphicsAllocations().size()) {
            continue;
        }

        if (!(allocation.second.memoryType & requestedTypesMask) ||
            (nullptr == allocation.second.gpuAllocations.getGraphicsAllocation(rootDeviceIndex))) {
            continue;
        }

        auto alloc = allocation.second.gpuAllocations.getGraphicsAllocation(rootDeviceIndex);
        residencyContainer.push_back(alloc);
    }
}

void SVMAllocsManager::makeInternalAllocationsResident(CommandStreamReceiver &commandStreamReceiver, uint32_t requestedTypesMask) {
    std::shared_lock<std::shared_mutex> lock(mtx);
    for (auto &allocation : this->SVMAllocs.allocations) {
        if (allocation.second.memoryType & requestedTypesMask) {
            auto gpuAllocation = allocation.second.gpuAllocations.getGraphicsAllocation(commandStreamReceiver.getRootDeviceIndex());
            if (gpuAllocation == nullptr) {
                continue;
            }
            commandStreamReceiver.makeResident(*gpuAllocation);
        }
    }
}

SVMAllocsManager::SVMAllocsManager(MemoryManager *memoryManager, bool multiOsContextSupport)
    : memoryManager(memoryManager), multiOsContextSupport(multiOsContextSupport) {
    if (DebugManager.flags.ExperimentalEnableDeviceAllocationCache.get()) {
        this->initUsmDeviceAllocationsCache();
        this->usmDeviceAllocationsCacheEnabled = true;
    }
}

SVMAllocsManager::~SVMAllocsManager() {
    this->trimUSMDeviceAllocCache();
}

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
        UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::NOT_SPECIFIED, rootDeviceIndices, subdeviceBitfields);
        return createUnifiedAllocationWithDeviceStorage(size, svmProperties, unifiedMemoryProperties);
    }
}

void *SVMAllocsManager::createHostUnifiedMemoryAllocation(size_t size,
                                                          const UnifiedMemoryProperties &memoryProperties) {
    size_t pageSizeForAlignment = MemoryConstants::pageSize;
    size_t alignedSize = alignUp<size_t>(size, pageSizeForAlignment);

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
    this->SVMAllocs.insert(allocData);

    return usmPtr;
}

void *SVMAllocsManager::createUnifiedMemoryAllocation(size_t size,
                                                      const UnifiedMemoryProperties &memoryProperties) {
    auto rootDeviceIndex = memoryProperties.device
                               ? memoryProperties.device->getRootDeviceIndex()
                               : *memoryProperties.rootDeviceIndices.begin();
    DeviceBitfield deviceBitfield = memoryProperties.subdeviceBitfields.at(rootDeviceIndex);
    size_t pageSizeForAlignment = MemoryConstants::pageSize64k;
    size_t alignedSize = alignUp<size_t>(size, pageSizeForAlignment);

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
                                                 true,
                                                 alignedSize,
                                                 allocationType,
                                                 false,
                                                 multiStorageAllocation,
                                                 deviceBitfield};
    unifiedMemoryProperties.flags.isUSMDeviceAllocation = false;
    unifiedMemoryProperties.flags.shareable = memoryProperties.allocationFlags.flags.shareable;
    unifiedMemoryProperties.cacheRegion = MemoryPropertiesHelper::getCacheRegion(memoryProperties.allocationFlags);
    unifiedMemoryProperties.flags.uncacheable = memoryProperties.allocationFlags.flags.locallyUncachedResource;
    unifiedMemoryProperties.flags.preferCompressed = compressionEnabled || memoryProperties.allocationFlags.flags.compressedHint;

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
    }

    GraphicsAllocation *unifiedMemoryAllocation = memoryManager->allocateGraphicsMemoryWithProperties(unifiedMemoryProperties);
    if (!unifiedMemoryAllocation) {
        if (memoryProperties.memoryType == InternalMemoryType::DEVICE_UNIFIED_MEMORY &&
            this->usmDeviceAllocationsCacheEnabled) {
            this->trimUSMDeviceAllocCache();
            unifiedMemoryAllocation = memoryManager->allocateGraphicsMemoryWithProperties(unifiedMemoryProperties);
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
    this->SVMAllocs.insert(allocData);
    return reinterpret_cast<void *>(unifiedMemoryAllocation->getGpuAddress());
}

void *SVMAllocsManager::createSharedUnifiedMemoryAllocation(size_t size,
                                                            const UnifiedMemoryProperties &memoryProperties,
                                                            void *cmdQ) {
    if (memoryProperties.rootDeviceIndices.size() > 1 && memoryProperties.device == nullptr) {
        return createHostUnifiedMemoryAllocation(size, memoryProperties);
    }

    auto supportDualStorageSharedMemory = memoryManager->isLocalMemorySupported(*memoryProperties.rootDeviceIndices.begin());

    if (DebugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.get() != -1) {
        supportDualStorageSharedMemory = !!DebugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.get();
    }

    if (supportDualStorageSharedMemory) {
        bool useKmdMigration = memoryManager->isKmdMigrationAvailable(*memoryProperties.rootDeviceIndices.begin());
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

    auto rootDeviceIndex = unifiedMemoryProperties.device
                               ? unifiedMemoryProperties.device->getRootDeviceIndex()
                               : *unifiedMemoryProperties.rootDeviceIndices.begin();
    auto &deviceBitfield = unifiedMemoryProperties.subdeviceBitfields.at(rootDeviceIndex);
    size_t pageSizeForAlignment = 2 * MemoryConstants::megaByte;
    size_t alignedSize = alignUp<size_t>(size, pageSizeForAlignment);
    AllocationProperties gpuProperties{rootDeviceIndex,
                                       true,
                                       alignedSize,
                                       AllocationType::UNIFIED_SHARED_MEMORY,
                                       false,
                                       false,
                                       deviceBitfield};

    gpuProperties.alignment = pageSizeForAlignment;
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
    this->SVMAllocs.insert(allocData);
    return allocationGpu->getUnderlyingBuffer();
}

void SVMAllocsManager::setUnifiedAllocationProperties(GraphicsAllocation *allocation, const SvmAllocationProperties &svmProperties) {
    allocation->setMemObjectsAllocationWithWritableFlags(!svmProperties.readOnly && !svmProperties.hostPtrReadOnly);
    allocation->setCoherent(svmProperties.coherent);
}

SvmAllocationData *SVMAllocsManager::getSVMAlloc(const void *ptr) {
    std::shared_lock<std::shared_mutex> lock(mtx);
    return SVMAllocs.get(ptr);
}

void SVMAllocsManager::insertSVMAlloc(const SvmAllocationData &svmAllocData) {
    std::unique_lock<std::shared_mutex> lock(mtx);
    SVMAllocs.insert(svmAllocData);
}

void SVMAllocsManager::removeSVMAlloc(const SvmAllocationData &svmAllocData) {
    std::unique_lock<std::shared_mutex> lock(mtx);
    SVMAllocs.remove(svmAllocData);
}

bool SVMAllocsManager::freeSVMAlloc(void *ptr, bool blocking) {
    SvmAllocationData *svmData = getSVMAlloc(ptr);
    if (svmData) {
        if (InternalMemoryType::DEVICE_UNIFIED_MEMORY == svmData->memoryType &&
            this->usmDeviceAllocationsCacheEnabled) {
            size_t alignedSize = alignUp<size_t>(svmData->size, svmData->pageSizeForAlignment);
            this->usmDeviceAllocationsCache.insert(alignedSize, ptr);
            return true;
        }
        this->freeSVMAllocImpl(ptr, blocking, svmData);
        return true;
    }
    return false;
}

void SVMAllocsManager::freeSVMAllocImpl(void *ptr, bool blocking, SvmAllocationData *svmData) {
    this->prepareIndirectAllocationForDestruction(svmData);

    if (blocking) {
        if (svmData->cpuAllocation) {
            this->memoryManager->waitForEnginesCompletion(*svmData->cpuAllocation);
        }

        for (auto &gpuAllocation : svmData->gpuAllocations.getGraphicsAllocations()) {
            if (gpuAllocation) {
                this->memoryManager->waitForEnginesCompletion(*gpuAllocation);
            }
        }
    }

    auto pageFaultManager = this->memoryManager->getPageFaultManager();
    if (pageFaultManager) {
        pageFaultManager->removeAllocation(ptr);
    }
    std::unique_lock<std::shared_mutex> lock(mtx);
    if (svmData->gpuAllocations.getAllocationType() == AllocationType::SVM_ZERO_COPY) {
        freeZeroCopySvmAllocation(svmData);
    } else {
        freeSvmAllocationWithDeviceStorage(svmData);
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
    this->SVMAllocs.insert(allocData);
    return usmPtr;
}

void *SVMAllocsManager::createUnifiedAllocationWithDeviceStorage(size_t size, const SvmAllocationProperties &svmProperties, const UnifiedMemoryProperties &unifiedMemoryProperties) {
    auto rootDeviceIndex = unifiedMemoryProperties.device
                               ? unifiedMemoryProperties.device->getRootDeviceIndex()
                               : *unifiedMemoryProperties.rootDeviceIndices.begin();
    size_t alignedSizeCpu = alignUp<size_t>(size, MemoryConstants::pageSize2Mb);
    size_t pageSizeForAlignment = MemoryConstants::pageSize64k;
    size_t alignedSizeGpu = alignUp<size_t>(size, pageSizeForAlignment);
    DeviceBitfield subDevices = unifiedMemoryProperties.subdeviceBitfields.at(rootDeviceIndex);
    AllocationProperties cpuProperties{rootDeviceIndex,
                                       true, // allocateMemory
                                       alignedSizeCpu, AllocationType::SVM_CPU,
                                       false, // isMultiStorageAllocation
                                       subDevices};
    cpuProperties.alignment = MemoryConstants::pageSize2Mb;
    auto cacheRegion = MemoryPropertiesHelper::getCacheRegion(unifiedMemoryProperties.allocationFlags);
    MemoryPropertiesHelper::fillCachePolicyInProperties(cpuProperties, false, svmProperties.readOnly, false, cacheRegion);
    GraphicsAllocation *allocationCpu = memoryManager->allocateGraphicsMemoryWithProperties(cpuProperties);
    if (!allocationCpu) {
        return nullptr;
    }
    setUnifiedAllocationProperties(allocationCpu, svmProperties);
    void *svmPtr = allocationCpu->getUnderlyingBuffer();

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
                                       alignedSizeGpu,
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
    allocData.pageSizeForAlignment = pageSizeForAlignment;
    allocData.size = size;
    allocData.setAllocId(this->allocationsCounter++);

    std::unique_lock<std::shared_mutex> lock(mtx);
    this->SVMAllocs.insert(allocData);
    return svmPtr;
}

void SVMAllocsManager::freeZeroCopySvmAllocation(SvmAllocationData *svmData) {
    auto gpuAllocations = svmData->gpuAllocations;
    SVMAllocs.remove(*svmData);
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
    SVMAllocs.remove(*svmData);

    for (auto gpuAllocation : graphicsAllocations) {
        memoryManager->freeGraphicsMemory(gpuAllocation, isImportedAllocation);
    }
    memoryManager->freeGraphicsMemory(cpuAllocation, isImportedAllocation);
}

bool SVMAllocsManager::hasHostAllocations() {
    std::shared_lock<std::shared_mutex> lock(mtx);
    for (auto &allocation : this->SVMAllocs.allocations) {
        if (allocation.second.memoryType == InternalMemoryType::HOST_UNIFIED_MEMORY) {
            return true;
        }
    }
    return false;
}

void SVMAllocsManager::makeIndirectAllocationsResident(CommandStreamReceiver &commandStreamReceiver, uint32_t taskCount) {
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
        for (auto &allocation : this->SVMAllocs.allocations) {
            auto gpuAllocation = allocation.second.gpuAllocations.getGraphicsAllocation(commandStreamReceiver.getRootDeviceIndex());
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
            const auto &hwInfoConfig = *HwInfoConfig::get(unifiedMemoryProperties.device->getHardwareInfo().platform.eProductFamily);
            if (hwInfoConfig.allowStatelessCompression(unifiedMemoryProperties.device->getHardwareInfo())) {
                compressionEnabled = true;
            }
            allocationType = AllocationType::BUFFER;
        }
    }
    return allocationType;
}

void SVMAllocsManager::prefetchMemory(Device &device, SvmAllocationData &svmData) {
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

    if (memoryManager->isKmdMigrationAvailable(device.getRootDeviceIndex()) &&
        (svmData.memoryType == InternalMemoryType::SHARED_UNIFIED_MEMORY)) {
        auto gfxAllocation = svmData.gpuAllocations.getGraphicsAllocation(device.getRootDeviceIndex());
        auto subDeviceId = getSubDeviceId(device);
        memoryManager->setMemPrefetch(gfxAllocation, subDeviceId, device.getRootDeviceIndex());
    }
}

} // namespace NEO
