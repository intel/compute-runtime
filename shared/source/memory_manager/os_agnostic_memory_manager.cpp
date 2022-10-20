/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/os_agnostic_memory_manager.h"

#include "shared/source/aub/aub_center.h"
#include "shared/source/aub/aub_helper.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/heap_assigner.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/host_ptr_manager.h"
#include "shared/source/memory_manager/residency.h"
#include "shared/source/os_interface/os_memory.h"

#include <cassert>

namespace NEO {
struct OsHandleOsAgnostic : OsHandle {
};

OsAgnosticMemoryManager::OsAgnosticMemoryManager(bool aubUsage, ExecutionEnvironment &executionEnvironment) : MemoryManager(executionEnvironment) {
    initialize(aubUsage);
}

void OsAgnosticMemoryManager::initialize(bool aubUsage) {
    // 4 x sizeof(Heap32) + 2 x sizeof(Standard/Standard64k)
    size_t reservedCpuAddressRangeSize = (4 * 4 + 2 * (aubUsage ? 32 : 4)) * GB;

    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < gfxPartitions.size(); ++rootDeviceIndex) {
        auto hwInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
        this->enable64kbpages[rootDeviceIndex] = is64kbPagesEnabled(hwInfo);
        auto gpuAddressSpace = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo()->capabilityTable.gpuAddressSpace;
        if (!getGfxPartition(rootDeviceIndex)->init(gpuAddressSpace, reservedCpuAddressRangeSize, rootDeviceIndex, gfxPartitions.size(), heapAssigner.apiAllowExternalHeapForSshAndDsh)) {
            initialized = false;
            return;
        }
    }

    initialized = true;
}

OsAgnosticMemoryManager::~OsAgnosticMemoryManager() = default;

bool OsAgnosticMemoryManager::is64kbPagesEnabled(const HardwareInfo *hwInfo) {
    return hwInfo->capabilityTable.ftr64KBpages && !!DebugManager.flags.Enable64kbpages.get();
}

GraphicsAllocation *OsAgnosticMemoryManager::allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) {
    auto sizeAligned = alignUp(allocationData.size, MemoryConstants::pageSize);
    MemoryAllocation *memoryAllocation = nullptr;

    if (fakeBigAllocations && allocationData.size > bigAllocation) {
        memoryAllocation = createMemoryAllocation(
            allocationData.type, nullptr, (void *)dummyAddress, static_cast<uint64_t>(dummyAddress), allocationData.size, counter,
            MemoryPool::System4KBPages, allocationData.rootDeviceIndex, allocationData.flags.uncacheable, allocationData.flags.flushL3, false);
        counter++;
        return memoryAllocation;
    }

    auto alignment = allocationData.alignment;
    if (allocationData.type == AllocationType::SVM_CPU) {
        alignment = MemoryConstants::pageSize2Mb;
        sizeAligned = alignUp(allocationData.size, MemoryConstants::pageSize2Mb);
    }

    if (allocationData.type == AllocationType::DEBUG_CONTEXT_SAVE_AREA ||
        allocationData.type == AllocationType::DEBUG_SBA_TRACKING_BUFFER) {
        sizeAligned *= allocationData.storageInfo.getNumBanks();
    }

    auto ptr = allocateSystemMemory(sizeAligned, alignment ? alignUp(alignment, MemoryConstants::pageSize) : MemoryConstants::pageSize);
    if (ptr != nullptr) {
        memoryAllocation = createMemoryAllocation(allocationData.type, ptr, ptr, reinterpret_cast<uint64_t>(ptr), allocationData.size,
                                                  counter, MemoryPool::System4KBPages, allocationData.rootDeviceIndex, allocationData.flags.uncacheable, allocationData.flags.flushL3, false);

        if (allocationData.type == AllocationType::SVM_CPU) {
            // add  padding in case mapPtr is not aligned
            size_t reserveSize = sizeAligned + alignment;
            void *gpuPtr = reserveCpuAddressRange(reserveSize, allocationData.rootDeviceIndex);
            if (!gpuPtr) {
                delete memoryAllocation;
                alignedFreeWrapper(ptr);
                return nullptr;
            }
            memoryAllocation->setReservedAddressRange(gpuPtr, reserveSize);
            gpuPtr = alignUp(gpuPtr, alignment);

            auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
            auto canonizedGpuAddress = gmmHelper->canonize(reinterpret_cast<uint64_t>(gpuPtr));
            memoryAllocation->setCpuPtrAndGpuAddress(ptr, canonizedGpuAddress);
        }

        if (allocationData.type == AllocationType::DEBUG_CONTEXT_SAVE_AREA ||
            allocationData.type == AllocationType::DEBUG_SBA_TRACKING_BUFFER) {
            memoryAllocation->storageInfo = allocationData.storageInfo;
        }

        auto pHwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHardwareInfo();
        if (HwHelper::get(pHwInfo->platform.eRenderCoreFamily).compressedBuffersSupported(*pHwInfo) &&
            allocationData.flags.preferCompressed) {
            auto gmm = std::make_unique<Gmm>(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(),
                                             allocationData.hostPtr,
                                             sizeAligned,
                                             alignment,
                                             CacheSettingsHelper::getGmmUsageType(memoryAllocation->getAllocationType(), !!allocationData.flags.uncacheable, *pHwInfo),
                                             true,
                                             allocationData.storageInfo,
                                             true);
            memoryAllocation->setDefaultGmm(gmm.release());
        }
    }
    counter++;
    return memoryAllocation;
}

GraphicsAllocation *OsAgnosticMemoryManager::allocateUSMHostGraphicsMemory(const AllocationData &allocationData) {
    return allocateGraphicsMemoryWithHostPtr(allocationData);
}

GraphicsAllocation *OsAgnosticMemoryManager::allocateGraphicsMemoryForNonSvmHostPtr(const AllocationData &allocationData) {
    auto alignedPtr = alignDown(allocationData.hostPtr, MemoryConstants::pageSize);
    auto offsetInPage = ptrDiff(allocationData.hostPtr, alignedPtr);

    auto memoryAllocation = createMemoryAllocation(allocationData.type, nullptr, const_cast<void *>(allocationData.hostPtr),
                                                   reinterpret_cast<uint64_t>(alignedPtr), allocationData.size, counter,
                                                   MemoryPool::System4KBPages, allocationData.rootDeviceIndex, false, allocationData.flags.flushL3, false);

    memoryAllocation->setAllocationOffset(offsetInPage);

    counter++;
    return memoryAllocation;
}

GraphicsAllocation *OsAgnosticMemoryManager::allocateGraphicsMemoryWithGpuVa(const AllocationData &allocationData) {
    auto memoryAllocation = static_cast<MemoryAllocation *>(allocateGraphicsMemoryWithAlignment(allocationData));
    auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
    auto canonizedGpuAddress = gmmHelper->canonize(allocationData.gpuAddress);

    memoryAllocation->setCpuPtrAndGpuAddress(memoryAllocation->getUnderlyingBuffer(), canonizedGpuAddress);
    return memoryAllocation;
}

GraphicsAllocation *OsAgnosticMemoryManager::allocateGraphicsMemory64kb(const AllocationData &allocationData) {
    AllocationData allocationDataAlign = allocationData;
    allocationDataAlign.size = alignUp(allocationData.size, MemoryConstants::pageSize64k);
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo->platform.eRenderCoreFamily);
    allocationDataAlign.alignment = hwHelper.is1MbAlignmentSupported(*hwInfo, allocationData.flags.preferCompressed)
                                        ? MemoryConstants::megaByte
                                        : MemoryConstants::pageSize64k;
    auto memoryAllocation = allocateGraphicsMemoryWithAlignment(allocationDataAlign);
    if (memoryAllocation) {
        static_cast<MemoryAllocation *>(memoryAllocation)->overrideMemoryPool(MemoryPool::System64KBPages);
        if (memoryAllocation->getDefaultGmm() == nullptr) {
            auto gmm = std::make_unique<Gmm>(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(),
                                             allocationData.hostPtr,
                                             allocationDataAlign.size,
                                             allocationDataAlign.alignment,
                                             CacheSettingsHelper::getGmmUsageType(memoryAllocation->getAllocationType(), !!allocationData.flags.uncacheable, *hwInfo),
                                             allocationData.flags.preferCompressed,
                                             allocationData.storageInfo, true);
            memoryAllocation->setDefaultGmm(gmm.release());
        }
    }
    return memoryAllocation;
}

GraphicsAllocation *OsAgnosticMemoryManager::allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData, bool useLocalMemory) {
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHardwareInfo();
    auto heap = heapAssigner.get32BitHeapIndex(allocationData.type, useLocalMemory, *hwInfo, allocationData.flags.use32BitFrontWindow);
    auto gfxPartition = getGfxPartition(allocationData.rootDeviceIndex);
    auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);

    if (allocationData.hostPtr) {
        auto allocationSize = alignSizeWholePage(allocationData.hostPtr, allocationData.size);
        auto gpuVirtualAddress = gfxPartition->heapAllocate(heap, allocationSize);
        if (!gpuVirtualAddress) {
            return nullptr;
        }
        uint64_t offset = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(allocationData.hostPtr) & MemoryConstants::pageMask);

        auto canonizedGpuAddress = gmmHelper->canonize(gpuVirtualAddress + offset);
        MemoryAllocation *memAlloc = new MemoryAllocation(
            allocationData.rootDeviceIndex, allocationData.type, nullptr, const_cast<void *>(allocationData.hostPtr),
            canonizedGpuAddress, allocationData.size,
            counter, MemoryPool::System4KBPagesWith32BitGpuAddressing, false, false, maxOsContextCount);

        memAlloc->set32BitAllocation(true);
        memAlloc->setGpuBaseAddress(gmmHelper->canonize(gfxPartition->getHeapBase(heap)));
        memAlloc->sizeToFree = allocationSize;

        counter++;
        return memAlloc;
    }

    auto allocationSize = alignUp(allocationData.size, MemoryConstants::pageSize);
    void *ptrAlloc = nullptr;
    auto gpuAddress = gfxPartition->heapAllocate(heap, allocationSize);

    if (allocationData.size < 0xfffff000) {
        if (fakeBigAllocations) {
            ptrAlloc = reinterpret_cast<void *>(dummyAddress);
        } else {
            ptrAlloc = alignedMallocWrapper(allocationSize, MemoryConstants::allocationAlignment);
        }
    }

    MemoryAllocation *memoryAllocation = nullptr;
    if (ptrAlloc != nullptr) {
        auto canonizedGpuAddress = gmmHelper->canonize(gpuAddress);
        memoryAllocation = new MemoryAllocation(allocationData.rootDeviceIndex, allocationData.type, ptrAlloc, ptrAlloc,
                                                canonizedGpuAddress,
                                                allocationData.size, counter, MemoryPool::System4KBPagesWith32BitGpuAddressing,
                                                false, allocationData.flags.flushL3, maxOsContextCount);

        memoryAllocation->set32BitAllocation(true);
        memoryAllocation->setGpuBaseAddress(gmmHelper->canonize(gfxPartition->getHeapBase(heap)));
        memoryAllocation->sizeToFree = allocationSize;
    }
    counter++;
    return memoryAllocation;
}

GraphicsAllocation *OsAgnosticMemoryManager::createGraphicsAllocationFromMultipleSharedHandles(const std::vector<osHandle> &handles, AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation) {
    return nullptr;
}

GraphicsAllocation *OsAgnosticMemoryManager::createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation) {
    auto graphicsAllocation = createMemoryAllocation(properties.allocationType, nullptr, reinterpret_cast<void *>(1), 1,
                                                     4096u, static_cast<uint64_t>(handle), MemoryPool::SystemCpuInaccessible, properties.rootDeviceIndex,
                                                     false, false, requireSpecificBitness);
    graphicsAllocation->setSharedHandle(handle);
    graphicsAllocation->set32BitAllocation(requireSpecificBitness);

    if (properties.imgInfo) {
        Gmm *gmm = new Gmm(executionEnvironment.rootDeviceEnvironments[properties.rootDeviceIndex]->getGmmHelper(), *properties.imgInfo, createStorageInfoFromProperties(properties), false);
        graphicsAllocation->setDefaultGmm(gmm);
    }

    return graphicsAllocation;
}

void OsAgnosticMemoryManager::addAllocationToHostPtrManager(GraphicsAllocation *gfxAllocation) {
    FragmentStorage fragment = {};
    fragment.driverAllocation = true;
    fragment.fragmentCpuPointer = gfxAllocation->getUnderlyingBuffer();
    fragment.fragmentSize = alignUp(gfxAllocation->getUnderlyingBufferSize(), MemoryConstants::pageSize);
    fragment.osInternalStorage = new OsHandleOsAgnostic();
    fragment.residency = new ResidencyData(maxOsContextCount);
    hostPtrManager->storeFragment(gfxAllocation->getRootDeviceIndex(), fragment);
}

void OsAgnosticMemoryManager::removeAllocationFromHostPtrManager(GraphicsAllocation *gfxAllocation) {
    auto buffer = gfxAllocation->getUnderlyingBuffer();
    auto rootDeviceIndex = gfxAllocation->getRootDeviceIndex();
    auto fragment = hostPtrManager->getFragment({buffer, rootDeviceIndex});
    if (fragment && fragment->driverAllocation) {
        OsHandle *osStorageToRelease = fragment->osInternalStorage;
        ResidencyData *residencyDataToRelease = fragment->residency;
        if (hostPtrManager->releaseHostPtr(rootDeviceIndex, buffer)) {
            delete osStorageToRelease;
            delete residencyDataToRelease;
        }
    }
}

void OsAgnosticMemoryManager::freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation, bool isImportedAllocation) {
    return freeGraphicsMemoryImpl(gfxAllocation);
}

void OsAgnosticMemoryManager::freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) {
    for (auto handleId = 0u; handleId < gfxAllocation->getNumGmms(); handleId++) {
        delete gfxAllocation->getGmm(handleId);
    }

    if ((uintptr_t)gfxAllocation->getUnderlyingBuffer() == dummyAddress) {
        delete gfxAllocation;
        return;
    }

    if (gfxAllocation->fragmentsStorage.fragmentCount) {
        cleanGraphicsMemoryCreatedFromHostPtr(gfxAllocation);
        delete gfxAllocation;
        return;
    }
    auto memoryAllocation = static_cast<MemoryAllocation *>(gfxAllocation);
    auto sizeToFree = memoryAllocation->sizeToFree;
    auto rootDeviceIndex = gfxAllocation->getRootDeviceIndex();

    alignedFreeWrapper(gfxAllocation->getDriverAllocatedCpuPtr());
    if (gfxAllocation->getReservedAddressPtr()) {
        releaseReservedCpuAddressRange(gfxAllocation->getReservedAddressPtr(), gfxAllocation->getReservedAddressSize(), gfxAllocation->getRootDeviceIndex());
    }

    if (executionEnvironment.rootDeviceEnvironments.size() > rootDeviceIndex) {
        if (sizeToFree) {
            auto gmmHelper = getGmmHelper(memoryAllocation->getRootDeviceIndex());
            auto gpuAddressToFree = gmmHelper->decanonize(memoryAllocation->getGpuAddress()) & ~MemoryConstants::pageMask;
            auto gfxPartition = getGfxPartition(memoryAllocation->getRootDeviceIndex());
            gfxPartition->freeGpuAddressRange(gpuAddressToFree, sizeToFree);
        }

        auto aubCenter = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->aubCenter.get();
        if (aubCenter && aubCenter->getAubManager() && DebugManager.flags.EnableFreeMemory.get()) {
            aubCenter->getAubManager()->freeMemory(gfxAllocation->getGpuAddress(), gfxAllocation->getUnderlyingBufferSize());
        }
    }
    delete gfxAllocation;
}

uint64_t OsAgnosticMemoryManager::getSystemSharedMemory(uint32_t rootDeviceIndex) {
    return 16 * GB;
}

GraphicsAllocation *OsAgnosticMemoryManager::createGraphicsAllocation(OsHandleStorage &handleStorage, const AllocationData &allocationData) {
    auto allocation = createMemoryAllocation(allocationData.type, nullptr, const_cast<void *>(allocationData.hostPtr),
                                             reinterpret_cast<uint64_t>(allocationData.hostPtr), allocationData.size, counter++,
                                             MemoryPool::System4KBPages, allocationData.rootDeviceIndex, false, allocationData.flags.flushL3, false);

    allocation->fragmentsStorage = handleStorage;
    return allocation;
}

void OsAgnosticMemoryManager::turnOnFakingBigAllocations() {
    this->fakeBigAllocations = true;
}

MemoryManager::AllocationStatus OsAgnosticMemoryManager::populateOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) {
    for (unsigned int i = 0; i < maxFragmentsCount; i++) {
        if (!handleStorage.fragmentStorageData[i].osHandleStorage && handleStorage.fragmentStorageData[i].cpuPtr) {
            handleStorage.fragmentStorageData[i].osHandleStorage = new OsHandleOsAgnostic();
            handleStorage.fragmentStorageData[i].residency = new ResidencyData(maxOsContextCount);

            FragmentStorage newFragment = {};
            newFragment.fragmentCpuPointer = const_cast<void *>(handleStorage.fragmentStorageData[i].cpuPtr);
            newFragment.fragmentSize = handleStorage.fragmentStorageData[i].fragmentSize;
            newFragment.osInternalStorage = handleStorage.fragmentStorageData[i].osHandleStorage;
            newFragment.residency = handleStorage.fragmentStorageData[i].residency;
            hostPtrManager->storeFragment(rootDeviceIndex, newFragment);
        }
    }
    return AllocationStatus::Success;
}
void OsAgnosticMemoryManager::cleanOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) {
    for (unsigned int i = 0; i < maxFragmentsCount; i++) {
        if (handleStorage.fragmentStorageData[i].freeTheFragment) {
            auto aubCenter = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->aubCenter.get();
            if (aubCenter && aubCenter->getAubManager() && DebugManager.flags.EnableFreeMemory.get()) {
                aubCenter->getAubManager()->freeMemory((uint64_t)handleStorage.fragmentStorageData[i].cpuPtr, handleStorage.fragmentStorageData[i].fragmentSize);
            }
            delete handleStorage.fragmentStorageData[i].osHandleStorage;
            delete handleStorage.fragmentStorageData[i].residency;
        }
    }
}

GraphicsAllocation *OsAgnosticMemoryManager::allocateMemoryByKMD(const AllocationData &allocationData) {
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHardwareInfo();

    auto gmm = std::make_unique<Gmm>(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(), allocationData.hostPtr,
                                     allocationData.size, 0u, CacheSettingsHelper::getGmmUsageType(allocationData.type, allocationData.flags.uncacheable, *hwInfo),
                                     allocationData.flags.preferCompressed, allocationData.storageInfo, true);

    GraphicsAllocation *alloc = nullptr;

    auto ptr = allocateSystemMemory(alignUp(allocationData.size, MemoryConstants::pageSize), MemoryConstants::pageSize);
    if (ptr != nullptr) {
        alloc = createMemoryAllocation(allocationData.type, ptr, ptr, reinterpret_cast<uint64_t>(ptr), allocationData.size,
                                       counter, MemoryPool::SystemCpuInaccessible, allocationData.rootDeviceIndex, allocationData.flags.uncacheable, allocationData.flags.flushL3, false);
        counter++;
    }

    if (alloc) {
        alloc->setDefaultGmm(gmm.release());
    }

    return alloc;
}

GraphicsAllocation *OsAgnosticMemoryManager::allocateGraphicsMemoryForImageImpl(const AllocationData &allocationData, std::unique_ptr<Gmm> gmm) {
    GraphicsAllocation *alloc = nullptr;

    if (allocationData.imgInfo->linearStorage && allocationData.imgInfo->mipCount == 0) {
        alloc = allocateGraphicsMemoryWithAlignment(allocationData);
        if (alloc) {
            alloc->setDefaultGmm(gmm.release());
        }
        return alloc;
    }

    auto ptr = allocateSystemMemory(alignUp(allocationData.imgInfo->size, MemoryConstants::pageSize), MemoryConstants::pageSize);
    if (ptr != nullptr) {
        alloc = createMemoryAllocation(allocationData.type, ptr, ptr, reinterpret_cast<uint64_t>(ptr), allocationData.imgInfo->size,
                                       counter, MemoryPool::SystemCpuInaccessible, allocationData.rootDeviceIndex, allocationData.flags.uncacheable, allocationData.flags.flushL3, false);
        counter++;
    }

    if (alloc) {
        alloc->setDefaultGmm(gmm.release());
    }

    return alloc;
}

void *OsAgnosticMemoryManager::reserveCpuAddressRange(size_t size, uint32_t rootDeviceIndex) {
    void *reservePtr = allocateSystemMemory(size, MemoryConstants::preferredAlignment);
    return reservePtr;
}

void OsAgnosticMemoryManager::releaseReservedCpuAddressRange(void *reserved, size_t size, uint32_t rootDeviceIndex) {
    alignedFreeWrapper(reserved);
}

MemoryAllocation *OsAgnosticMemoryManager::createMemoryAllocation(AllocationType allocationType, void *driverAllocatedCpuPointer,
                                                                  void *pMem, uint64_t gpuAddress, size_t memSize, uint64_t count,
                                                                  MemoryPool pool, uint32_t rootDeviceIndex, bool uncacheable,
                                                                  bool flushL3Required, bool requireSpecificBitness) {
    auto gmmHelper = getGmmHelper(rootDeviceIndex);
    if (!isLimitedRange(rootDeviceIndex)) {
        auto canonizedGpuAddress = gmmHelper->canonize(gpuAddress);
        return new MemoryAllocation(rootDeviceIndex, allocationType, driverAllocatedCpuPointer, pMem, canonizedGpuAddress, memSize,
                                    count, pool, uncacheable, flushL3Required, maxOsContextCount);
    }

    size_t alignedSize = alignSizeWholePage(pMem, memSize);

    auto heap = (force32bitAllocations || requireSpecificBitness) ? HeapIndex::HEAP_EXTERNAL : HeapIndex::HEAP_STANDARD;

    auto gfxPartition = getGfxPartition(rootDeviceIndex);
    uint64_t limitedGpuAddress = gfxPartition->heapAllocate(heap, alignedSize);
    auto canonizedGpuAddress = gmmHelper->canonize(limitedGpuAddress);
    auto memoryAllocation = new MemoryAllocation(rootDeviceIndex, allocationType, driverAllocatedCpuPointer, pMem, canonizedGpuAddress, memSize,
                                                 count, pool, uncacheable, flushL3Required, maxOsContextCount);

    if (heap == HeapIndex::HEAP_EXTERNAL) {
        memoryAllocation->setGpuBaseAddress(gmmHelper->canonize(gfxPartition->getHeapBase(heap)));
    }
    memoryAllocation->sizeToFree = alignedSize;

    return memoryAllocation;
}

AddressRange OsAgnosticMemoryManager::reserveGpuAddress(size_t size, uint32_t rootDeviceIndex) {
    auto gfxPartition = getGfxPartition(rootDeviceIndex);
    auto gmmHelper = getGmmHelper(rootDeviceIndex);
    auto gpuVa = gmmHelper->canonize(gfxPartition->heapAllocate(HeapIndex::HEAP_STANDARD, size));
    return AddressRange{gpuVa, size};
}

void OsAgnosticMemoryManager::freeGpuAddress(AddressRange addressRange, uint32_t rootDeviceIndex) {
    uint64_t graphicsAddress = addressRange.address;
    auto gmmHelper = getGmmHelper(rootDeviceIndex);
    graphicsAddress = gmmHelper->decanonize(graphicsAddress);
    auto gfxPartition = getGfxPartition(rootDeviceIndex);
    gfxPartition->freeGpuAddressRange(graphicsAddress, addressRange.size);
}

GraphicsAllocation *OsAgnosticMemoryManager::allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) {
    MemoryAllocation *allocation = nullptr;
    status = AllocationStatus::RetryInNonDevicePool;
    auto numHandles = allocationData.storageInfo.getNumBanks();
    auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);

    if (!this->localMemorySupported[allocationData.rootDeviceIndex]) {
        return nullptr;
    }

    if (allocationData.flags.useSystemMemory || (allocationData.flags.allow32Bit && this->force32bitAllocations)) {
        return nullptr;
    }
    bool use32Allocator = heapAssigner.use32BitHeap(allocationData.type);
    if (use32Allocator) {
        auto adjustedAllocationData(allocationData);
        adjustedAllocationData.size = alignUp(allocationData.size, MemoryConstants::pageSize64k);
        adjustedAllocationData.alignment = MemoryConstants::pageSize64k;
        allocation = static_cast<MemoryAllocation *>(allocate32BitGraphicsMemoryImpl(adjustedAllocationData, true));
    } else if (allocationData.type == AllocationType::SVM_GPU) {
        auto storage = allocateSystemMemory(allocationData.size, MemoryConstants::pageSize2Mb);
        auto canonizedGpuAddress = gmmHelper->canonize(reinterpret_cast<uint64_t>(allocationData.hostPtr));
        allocation = new MemoryAllocation(allocationData.rootDeviceIndex, numHandles, allocationData.type, storage, storage, canonizedGpuAddress,
                                          allocationData.size, counter, MemoryPool::LocalMemory, false, allocationData.flags.flushL3, maxOsContextCount);
        counter++;
    } else {
        std::unique_ptr<Gmm> gmm;
        size_t sizeAligned64k = 0;
        if (allocationData.type == AllocationType::IMAGE ||
            allocationData.type == AllocationType::SHARED_RESOURCE_COPY) {
            allocationData.imgInfo->useLocalMemory = true;
            gmm = std::make_unique<Gmm>(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(), *allocationData.imgInfo,
                                        allocationData.storageInfo, allocationData.flags.preferCompressed);
            sizeAligned64k = alignUp(allocationData.imgInfo->size, MemoryConstants::pageSize64k);
        } else {
            sizeAligned64k = alignUp(allocationData.size, MemoryConstants::pageSize64k);
            if (DebugManager.flags.RenderCompressedBuffersEnabled.get() &&
                allocationData.flags.preferCompressed) {
                auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHardwareInfo();
                gmm = std::make_unique<Gmm>(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(),
                                            allocationData.hostPtr,
                                            sizeAligned64k,
                                            MemoryConstants::pageSize64k,
                                            CacheSettingsHelper::getGmmUsageType(allocationData.type, allocationData.flags.uncacheable, *hwInfo),
                                            true,
                                            allocationData.storageInfo,
                                            true);
            }
        }

        auto gfxPartition = getGfxPartition(allocationData.rootDeviceIndex);
        auto heapIndex = HeapIndex::HEAP_STANDARD64KB;

        if ((gfxPartition->getHeapLimit(HeapIndex::HEAP_EXTENDED) > 0) && !allocationData.flags.resource48Bit) {
            heapIndex = HeapIndex::HEAP_EXTENDED;
        }

        auto systemMemory = allocateSystemMemory(sizeAligned64k, MemoryConstants::pageSize64k);
        if (allocationData.type == AllocationType::PREEMPTION) {
            memset(systemMemory, 0, sizeAligned64k);
        }
        auto sizeOfHeapChunk = sizeAligned64k;
        auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
        auto canonizedGpuAddress = gmmHelper->canonize(gfxPartition->heapAllocate(heapIndex, sizeOfHeapChunk));
        allocation = new MemoryAllocation(allocationData.rootDeviceIndex, numHandles, allocationData.type, systemMemory, systemMemory,
                                          canonizedGpuAddress, sizeAligned64k, counter,
                                          MemoryPool::LocalMemory, false, allocationData.flags.flushL3, maxOsContextCount);
        counter++;
        allocation->setDefaultGmm(gmm.release());
        allocation->sizeToFree = sizeOfHeapChunk;
    }

    if (allocation) {
        allocation->overrideMemoryPool(MemoryPool::LocalMemory);
        allocation->storageInfo = allocationData.storageInfo;
        status = AllocationStatus::Success;
    } else {
        status = AllocationStatus::Error;
    }

    return allocation;
}

uint64_t OsAgnosticMemoryManager::getLocalMemorySize(uint32_t rootDeviceIndex, uint32_t deviceBitfield) {
    DeviceBitfield bitfield = deviceBitfield;
    return (AubHelper::getPerTileLocalMemorySize(executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo()) * bitfield.count());
}

double OsAgnosticMemoryManager::getPercentOfGlobalMemoryAvailable(uint32_t rootDeviceIndex) {
    return 0.8;
}

void MemoryAllocation::overrideMemoryPool(MemoryPool pool) {
    if (DebugManager.flags.AUBDumpForceAllToLocalMemory.get()) {
        this->memoryPool = MemoryPool::LocalMemory;
        return;
    }
    this->memoryPool = pool;
}

} // namespace NEO
