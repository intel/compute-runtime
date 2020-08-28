/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_manager.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "opencl/source/mem_obj/mem_obj_helper.h"

namespace NEO {

void SVMAllocsManager::MapBasedAllocationTracker::insert(SvmAllocationData allocationsPair) {
    allocations.insert(std::make_pair(reinterpret_cast<void *>(allocationsPair.gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress()), allocationsPair));
}

void SVMAllocsManager::MapBasedAllocationTracker::remove(SvmAllocationData allocationsPair) {
    SvmAllocationContainer::iterator iter;
    iter = allocations.find(reinterpret_cast<void *>(allocationsPair.gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress()));
    allocations.erase(iter);
}

SvmAllocationData *SVMAllocsManager::MapBasedAllocationTracker::get(const void *ptr) {
    SvmAllocationContainer::iterator Iter, End;
    SvmAllocationData *svmAllocData;
    if ((ptr == nullptr) || (allocations.size() == 0)) {
        return nullptr;
    }
    End = allocations.end();
    Iter = allocations.lower_bound(ptr);
    if (((Iter != End) && (Iter->first != ptr)) ||
        (Iter == End)) {
        if (Iter == allocations.begin()) {
            Iter = End;
        } else {
            Iter--;
        }
    }
    if (Iter != End) {
        svmAllocData = &Iter->second;
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
    std::unique_lock<SpinLock> lock(mtx);
    for (auto &allocation : this->SVMAllocs.allocations) {
        if (rootDeviceIndex >= allocation.second.gpuAllocations.getGraphicsAllocations().size()) {
            continue;
        }

        if (!(allocation.second.memoryType & requestedTypesMask) ||
            (nullptr == allocation.second.gpuAllocations.getGraphicsAllocation(rootDeviceIndex))) {
            continue;
        }
        residencyContainer.push_back(allocation.second.gpuAllocations.getGraphicsAllocation(rootDeviceIndex));
    }
}

void SVMAllocsManager::makeInternalAllocationsResident(CommandStreamReceiver &commandStreamReceiver, uint32_t requestedTypesMask) {
    std::unique_lock<SpinLock> lock(mtx);
    for (auto &allocation : this->SVMAllocs.allocations) {
        if (allocation.second.memoryType & requestedTypesMask) {
            auto gpuAllocation = allocation.second.gpuAllocations.getGraphicsAllocation(commandStreamReceiver.getRootDeviceIndex());
            UNRECOVERABLE_IF(nullptr == gpuAllocation);
            commandStreamReceiver.makeResident(*gpuAllocation);
        }
    }
}

SVMAllocsManager::SVMAllocsManager(MemoryManager *memoryManager) : memoryManager(memoryManager) {
}

void *SVMAllocsManager::createSVMAlloc(uint32_t rootDeviceIndex, size_t size, const SvmAllocationProperties svmProperties, const DeviceBitfield &deviceBitfield) {
    if (size == 0)
        return nullptr;

    if (!memoryManager->isLocalMemorySupported(rootDeviceIndex)) {
        return createZeroCopySvmAllocation(rootDeviceIndex, size, svmProperties, deviceBitfield);
    } else {
        UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::NOT_SPECIFIED, deviceBitfield);
        return createUnifiedAllocationWithDeviceStorage(rootDeviceIndex, size, svmProperties, unifiedMemoryProperties);
    }
}

void *SVMAllocsManager::createHostUnifiedMemoryAllocation(uint32_t maxRootDeviceIndex,
                                                          size_t size,
                                                          const UnifiedMemoryProperties &memoryProperties) {
    size_t alignedSize = alignUp<size_t>(size, MemoryConstants::pageSize64k);

    GraphicsAllocation::AllocationType allocationType = GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY;

    std::vector<uint32_t> rootDeviceIndices;
    rootDeviceIndices.reserve(maxRootDeviceIndex + 1);
    for (auto rootDeviceIndex = 0u; rootDeviceIndex <= maxRootDeviceIndex; rootDeviceIndex++) {
        rootDeviceIndices.push_back(rootDeviceIndex);
    }

    uint32_t rootDeviceIndex = rootDeviceIndices.at(0);

    AllocationProperties unifiedMemoryProperties{rootDeviceIndex,
                                                 true,
                                                 alignedSize,
                                                 allocationType,
                                                 memoryProperties.subdeviceBitfield.count() > 1,
                                                 memoryProperties.subdeviceBitfield.count() > 1,
                                                 memoryProperties.subdeviceBitfield};
    unifiedMemoryProperties.flags.shareable = memoryProperties.allocationFlags.flags.shareable;

    SvmAllocationData allocData(maxRootDeviceIndex);

    void *usmPtr = memoryManager->createMultiGraphicsAllocation(rootDeviceIndices, unifiedMemoryProperties, allocData.gpuAllocations);
    if (!usmPtr) {
        return nullptr;
    }

    allocData.cpuAllocation = nullptr;
    allocData.size = size;
    allocData.memoryType = memoryProperties.memoryType;
    allocData.allocationFlagsProperty = memoryProperties.allocationFlags;
    allocData.device = nullptr;

    std::unique_lock<SpinLock> lock(mtx);
    this->SVMAllocs.insert(allocData);

    return usmPtr;
}

void *SVMAllocsManager::createUnifiedMemoryAllocation(uint32_t rootDeviceIndex,
                                                      size_t size,
                                                      const UnifiedMemoryProperties &memoryProperties) {

    size_t alignedSize = alignUp<size_t>(size, MemoryConstants::pageSize64k);

    GraphicsAllocation::AllocationType allocationType = GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY;
    if (memoryProperties.memoryType == InternalMemoryType::DEVICE_UNIFIED_MEMORY) {
        if (memoryProperties.allocationFlags.allocFlags.allocWriteCombined) {
            allocationType = GraphicsAllocation::AllocationType::WRITE_COMBINED;
        } else {
            allocationType = GraphicsAllocation::AllocationType::BUFFER;
        }
    }

    AllocationProperties unifiedMemoryProperties{rootDeviceIndex,
                                                 true,
                                                 alignedSize,
                                                 allocationType,
                                                 memoryProperties.subdeviceBitfield.count() > 1,
                                                 memoryProperties.subdeviceBitfield.count() > 1,
                                                 memoryProperties.subdeviceBitfield};
    unifiedMemoryProperties.flags.shareable = memoryProperties.allocationFlags.flags.shareable;
    unifiedMemoryProperties.flags.isUSMDeviceAllocation = true;

    GraphicsAllocation *unifiedMemoryAllocation = memoryManager->allocateGraphicsMemoryWithProperties(unifiedMemoryProperties);
    if (!unifiedMemoryAllocation) {
        return nullptr;
    }

    SvmAllocationData allocData(rootDeviceIndex);
    allocData.gpuAllocations.addAllocation(unifiedMemoryAllocation);
    allocData.cpuAllocation = nullptr;
    allocData.size = size;
    allocData.memoryType = memoryProperties.memoryType;
    allocData.allocationFlagsProperty = memoryProperties.allocationFlags;
    allocData.device = memoryProperties.device;

    std::unique_lock<SpinLock> lock(mtx);
    this->SVMAllocs.insert(allocData);
    return reinterpret_cast<void *>(unifiedMemoryAllocation->getGpuAddress());
}

void *SVMAllocsManager::createSharedUnifiedMemoryAllocation(uint32_t rootDeviceIndex,
                                                            size_t size,
                                                            const UnifiedMemoryProperties &memoryProperties,
                                                            void *cmdQ) {
    auto supportDualStorageSharedMemory = memoryManager->isLocalMemorySupported(rootDeviceIndex);

    if (DebugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.get() != -1) {
        supportDualStorageSharedMemory = !!DebugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.get();
    }

    if (supportDualStorageSharedMemory) {
        auto unifiedMemoryPointer = createUnifiedAllocationWithDeviceStorage(rootDeviceIndex, size, {}, memoryProperties);
        if (!unifiedMemoryPointer) {
            return nullptr;
        }
        auto unifiedMemoryAllocation = this->getSVMAlloc(unifiedMemoryPointer);
        unifiedMemoryAllocation->memoryType = memoryProperties.memoryType;
        unifiedMemoryAllocation->allocationFlagsProperty = memoryProperties.allocationFlags;

        UNRECOVERABLE_IF(cmdQ == nullptr);
        auto pageFaultManager = this->memoryManager->getPageFaultManager();
        pageFaultManager->insertAllocation(unifiedMemoryPointer, size, this, cmdQ, memoryProperties.allocationFlags);

        return unifiedMemoryPointer;
    }
    return createUnifiedMemoryAllocation(rootDeviceIndex, size, memoryProperties);
}

SvmAllocationData *SVMAllocsManager::getSVMAlloc(const void *ptr) {
    std::unique_lock<SpinLock> lock(mtx);
    return SVMAllocs.get(ptr);
}

void SVMAllocsManager::insertSVMAlloc(const SvmAllocationData &svmAllocData) {
    std::unique_lock<SpinLock> lock(mtx);
    SVMAllocs.insert(svmAllocData);
}

void SVMAllocsManager::removeSVMAlloc(const SvmAllocationData &svmAllocData) {
    std::unique_lock<SpinLock> lock(mtx);
    SVMAllocs.remove(svmAllocData);
}

bool SVMAllocsManager::freeSVMAlloc(void *ptr, bool blocking) {
    SvmAllocationData *svmData = getSVMAlloc(ptr);
    if (svmData) {
        if (blocking) {
            if (svmData->cpuAllocation) {
                this->memoryManager->waitForEnginesCompletion(*svmData->cpuAllocation);
            }
            this->memoryManager->waitForEnginesCompletion(*svmData->gpuAllocations.getDefaultGraphicsAllocation());
        }

        auto pageFaultManager = this->memoryManager->getPageFaultManager();
        if (pageFaultManager) {
            pageFaultManager->removeAllocation(ptr);
        }
        std::unique_lock<SpinLock> lock(mtx);
        if (svmData->gpuAllocations.getAllocationType() == GraphicsAllocation::AllocationType::SVM_ZERO_COPY) {
            freeZeroCopySvmAllocation(svmData);
        } else {
            freeSvmAllocationWithDeviceStorage(svmData);
        }
        return true;
    }
    return false;
}

void *SVMAllocsManager::createZeroCopySvmAllocation(uint32_t rootDeviceIndex, size_t size, const SvmAllocationProperties &svmProperties, const DeviceBitfield &deviceBitfield) {
    AllocationProperties properties{rootDeviceIndex,
                                    true, // allocateMemory
                                    size,
                                    GraphicsAllocation::AllocationType::SVM_ZERO_COPY,
                                    false, // isMultiStorageAllocation
                                    deviceBitfield};
    MemoryPropertiesHelper::fillCachePolicyInProperties(properties, false, svmProperties.readOnly, false);
    GraphicsAllocation *allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);
    if (!allocation) {
        return nullptr;
    }
    allocation->setMemObjectsAllocationWithWritableFlags(!svmProperties.readOnly && !svmProperties.hostPtrReadOnly);
    allocation->setCoherent(svmProperties.coherent);

    SvmAllocationData allocData(rootDeviceIndex);
    allocData.gpuAllocations.addAllocation(allocation);
    allocData.size = size;

    std::unique_lock<SpinLock> lock(mtx);
    this->SVMAllocs.insert(allocData);
    return allocation->getUnderlyingBuffer();
}

void *SVMAllocsManager::createUnifiedAllocationWithDeviceStorage(uint32_t rootDeviceIndex, size_t size, const SvmAllocationProperties &svmProperties, const UnifiedMemoryProperties &unifiedMemoryProperties) {
    size_t alignedSize = alignUp<size_t>(size, 2 * MemoryConstants::megaByte);
    AllocationProperties cpuProperties{rootDeviceIndex,
                                       true, // allocateMemory
                                       alignedSize, GraphicsAllocation::AllocationType::SVM_CPU,
                                       false, // isMultiStorageAllocation
                                       unifiedMemoryProperties.subdeviceBitfield};
    cpuProperties.alignment = 2 * MemoryConstants::megaByte;
    MemoryPropertiesHelper::fillCachePolicyInProperties(cpuProperties, false, svmProperties.readOnly, false);
    GraphicsAllocation *allocationCpu = memoryManager->allocateGraphicsMemoryWithProperties(cpuProperties);
    if (!allocationCpu) {
        return nullptr;
    }
    allocationCpu->setMemObjectsAllocationWithWritableFlags(!svmProperties.readOnly && !svmProperties.hostPtrReadOnly);
    allocationCpu->setCoherent(svmProperties.coherent);
    void *svmPtr = allocationCpu->getUnderlyingBuffer();

    AllocationProperties gpuProperties{rootDeviceIndex,
                                       false,
                                       alignedSize,
                                       GraphicsAllocation::AllocationType::SVM_GPU,
                                       unifiedMemoryProperties.subdeviceBitfield.count() > 1,
                                       false,
                                       unifiedMemoryProperties.subdeviceBitfield};

    gpuProperties.alignment = 2 * MemoryConstants::megaByte;
    MemoryPropertiesHelper::fillCachePolicyInProperties(gpuProperties, false, svmProperties.readOnly, false);
    GraphicsAllocation *allocationGpu = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties, svmPtr);
    if (!allocationGpu) {
        memoryManager->freeGraphicsMemory(allocationCpu);
        return nullptr;
    }
    allocationGpu->setMemObjectsAllocationWithWritableFlags(!svmProperties.readOnly && !svmProperties.hostPtrReadOnly);
    allocationGpu->setCoherent(svmProperties.coherent);

    SvmAllocationData allocData(rootDeviceIndex);
    allocData.gpuAllocations.addAllocation(allocationGpu);
    allocData.cpuAllocation = allocationCpu;
    allocData.device = unifiedMemoryProperties.device;
    allocData.size = size;

    std::unique_lock<SpinLock> lock(mtx);
    this->SVMAllocs.insert(allocData);
    return svmPtr;
}

void SVMAllocsManager::freeZeroCopySvmAllocation(SvmAllocationData *svmData) {
    GraphicsAllocation *gpuAllocation = svmData->gpuAllocations.getDefaultGraphicsAllocation();
    SVMAllocs.remove(*svmData);

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

void SVMAllocsManager::freeSvmAllocationWithDeviceStorage(SvmAllocationData *svmData) {
    auto graphicsAllocations = svmData->gpuAllocations.getGraphicsAllocations();
    GraphicsAllocation *cpuAllocation = svmData->cpuAllocation;
    SVMAllocs.remove(*svmData);

    for (auto gpuAllocation : graphicsAllocations) {
        memoryManager->freeGraphicsMemory(gpuAllocation);
    }
    memoryManager->freeGraphicsMemory(cpuAllocation);
}

SvmMapOperation *SVMAllocsManager::getSvmMapOperation(const void *ptr) {
    std::unique_lock<SpinLock> lock(mtx);
    return svmMapOperations.get(ptr);
}

void SVMAllocsManager::insertSvmMapOperation(void *regionSvmPtr, size_t regionSize, void *baseSvmPtr, size_t offset, bool readOnlyMap) {
    SvmMapOperation svmMapOperation;
    svmMapOperation.regionSvmPtr = regionSvmPtr;
    svmMapOperation.baseSvmPtr = baseSvmPtr;
    svmMapOperation.offset = offset;
    svmMapOperation.regionSize = regionSize;
    svmMapOperation.readOnlyMap = readOnlyMap;
    std::unique_lock<SpinLock> lock(mtx);
    svmMapOperations.insert(svmMapOperation);
}

void SVMAllocsManager::removeSvmMapOperation(const void *regionSvmPtr) {
    std::unique_lock<SpinLock> lock(mtx);
    svmMapOperations.remove(regionSvmPtr);
}

} // namespace NEO
