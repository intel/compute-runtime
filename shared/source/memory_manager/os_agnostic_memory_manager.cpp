/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/os_agnostic_memory_manager.h"

#include "shared/source/aub/aub_center.h"
#include "shared/source/aub/aub_helper.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/heap_assigner.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/memory_manager/host_ptr_manager.h"
#include "shared/source/memory_manager/memory_allocation.h"
#include "shared/source/memory_manager/multi_graphics_allocation.h"
#include "shared/source/memory_manager/residency.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/product_helper.h"
namespace NEO {
struct OsHandleOsAgnostic : OsHandle {
};

OsAgnosticMemoryManager::OsAgnosticMemoryManager(bool aubUsage, ExecutionEnvironment &executionEnvironment) : MemoryManager(executionEnvironment) {
    initialize(aubUsage);
}

void OsAgnosticMemoryManager::initialize(bool aubUsage) {
    // 4 x sizeof(Heap32) + 2 x sizeof(Standard/Standard64k)
    size_t reservedCpuAddressRangeSize = (4 * 4 + 2 * (aubUsage ? 32 : 4)) * MemoryConstants::gigaByte;

    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < gfxPartitions.size(); ++rootDeviceIndex) {
        auto hwInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
        auto &gfxCoreHelper = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHelper<GfxCoreHelper>();
        this->enable64kbpages[rootDeviceIndex] = is64kbPagesEnabled(hwInfo);
        this->localMemorySupported.push_back(gfxCoreHelper.getEnableLocalMemory(*hwInfo));
        auto gpuAddressSpace = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo()->capabilityTable.gpuAddressSpace;
        auto gfxTop = gpuAddressSpace + 1;
        if (!getGfxPartition(rootDeviceIndex)->init(gpuAddressSpace, reservedCpuAddressRangeSize, rootDeviceIndex, gfxPartitions.size(), heapAssigners[rootDeviceIndex]->apiAllowExternalHeapForSshAndDsh, OsAgnosticMemoryManager::getSystemSharedMemory(rootDeviceIndex), gfxTop)) {
            initialized = false;
            return;
        }
        isLocalMemoryUsedForIsa(rootDeviceIndex);
    }

    initialized = true;
}

OsAgnosticMemoryManager::~OsAgnosticMemoryManager() = default;

bool OsAgnosticMemoryManager::is64kbPagesEnabled(const HardwareInfo *hwInfo) {
    return !!debugManager.flags.Enable64kbpages.get();
}

GraphicsAllocation *OsAgnosticMemoryManager::allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) {
    auto alignment = alignUpNonZero(allocationData.alignment, MemoryConstants::pageSize);
    auto sizeAligned = alignUp(allocationData.size, alignment);
    MemoryAllocation *memoryAllocation = nullptr;

    if (fakeBigAllocations && sizeAligned > bigAllocation) {
        memoryAllocation = createMemoryAllocation(
            allocationData.type, nullptr, reinterpret_cast<void *>(dummyAddress), dummyAddress, sizeAligned, counter,
            MemoryPool::system4KBPages, allocationData.rootDeviceIndex, allocationData.flags.uncacheable, allocationData.flags.flushL3, false);
        counter++;
        return memoryAllocation;
    }

    if (allocationData.type == AllocationType::svmCpu) {
        auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex];
        auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
        alignment = alignUpNonZero<size_t>(allocationData.alignment, productHelper.getSvmCpuAlignment());
        sizeAligned = alignUp(allocationData.size, alignment);
    }
    auto cpuAllocationSize = sizeAligned;
    if (GraphicsAllocation::isDebugSurfaceAllocationType(allocationData.type)) {
        cpuAllocationSize *= allocationData.storageInfo.getNumBanks();
    }

    auto ptr = allocateSystemMemory(cpuAllocationSize, alignment);
    if (ptr != nullptr) {
        memoryAllocation = createMemoryAllocation(allocationData.type, ptr, ptr, reinterpret_cast<uint64_t>(ptr), sizeAligned,
                                                  counter, MemoryPool::system4KBPages, allocationData.rootDeviceIndex, allocationData.flags.uncacheable, allocationData.flags.flushL3, false);

        if (allocationData.type == AllocationType::svmCpu) {
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

        if (GraphicsAllocation::isDebugSurfaceAllocationType(allocationData.type)) {
            memoryAllocation->storageInfo = allocationData.storageInfo;
        }

        auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex];
        auto pHwInfo = rootDeviceEnvironment.getHardwareInfo();
        auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
        if (gfxCoreHelper.compressedBuffersSupported(*pHwInfo) &&
            allocationData.flags.preferCompressed) {
            auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
            GmmRequirements gmmRequirements{};
            gmmRequirements.allowLargePages = true;
            gmmRequirements.preferCompressed = true;
            auto gmm = std::make_unique<Gmm>(rootDeviceEnvironment.getGmmHelper(),
                                             allocationData.hostPtr,
                                             sizeAligned,
                                             alignment,
                                             CacheSettingsHelper::getGmmUsageType(memoryAllocation->getAllocationType(), !!allocationData.flags.uncacheable, productHelper, pHwInfo),
                                             allocationData.storageInfo,
                                             gmmRequirements);
            memoryAllocation->setDefaultGmm(gmm.release());
        }
    }
    counter++;
    return memoryAllocation;
}

GraphicsAllocation *OsAgnosticMemoryManager::allocateUSMHostGraphicsMemory(const AllocationData &allocationData) {
    AllocationData allocData = allocationData;
    if (allocData.type == AllocationType::svmCpu) {
        auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[allocData.rootDeviceIndex];
        auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
        allocData.alignment = alignUpNonZero<size_t>(allocationData.alignment, productHelper.getSvmCpuAlignment());
        allocData.size = alignUp(allocationData.size, allocData.alignment);
    }
    auto memoryAllocation = allocateGraphicsMemoryWithHostPtr(allocData);
    if (memoryAllocation && allocData.type == AllocationType::svmCpu) {
        void *gpuPtr = reserveCpuAddressRange(allocData.size, allocData.rootDeviceIndex);
        if (nullptr == gpuPtr) {
            cleanGraphicsMemoryCreatedFromHostPtr(memoryAllocation);
            delete memoryAllocation;
            return nullptr;
        }
        memoryAllocation->setReservedAddressRange(gpuPtr, allocData.size);
        gpuPtr = alignUp(gpuPtr, allocData.alignment);

        auto cpuPtr = const_cast<void *>(allocData.hostPtr);
        auto gmmHelper = getGmmHelper(allocData.rootDeviceIndex);
        auto canonizedGpuAddress = gmmHelper->canonize(reinterpret_cast<uint64_t>(gpuPtr));
        memoryAllocation->setCpuPtrAndGpuAddress(cpuPtr, canonizedGpuAddress);
    }
    return memoryAllocation;
}

GraphicsAllocation *OsAgnosticMemoryManager::allocateGraphicsMemoryForNonSvmHostPtr(const AllocationData &allocationData) {
    auto alignedPtr = alignDown(allocationData.hostPtr, MemoryConstants::pageSize);
    auto offsetInPage = ptrDiff(allocationData.hostPtr, alignedPtr);

    auto memoryAllocation = createMemoryAllocation(allocationData.type, nullptr, const_cast<void *>(allocationData.hostPtr),
                                                   reinterpret_cast<uint64_t>(alignedPtr), allocationData.size, counter,
                                                   MemoryPool::system4KBPages, allocationData.rootDeviceIndex, false, allocationData.flags.flushL3, false);

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
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex].get();
    auto hwInfo = rootDeviceEnvironment.getHardwareInfo();
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    allocationDataAlign.alignment = gfxCoreHelper.is1MbAlignmentSupported(*hwInfo, allocationData.flags.preferCompressed)
                                        ? MemoryConstants::megaByte
                                        : MemoryConstants::pageSize64k;
    auto memoryAllocation = allocateGraphicsMemoryWithAlignment(allocationDataAlign);
    if (memoryAllocation) {
        static_cast<MemoryAllocation *>(memoryAllocation)->overrideMemoryPool(MemoryPool::system64KBPages);
        if (memoryAllocation->getDefaultGmm() == nullptr) {
            auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
            GmmRequirements gmmRequirements{};
            gmmRequirements.allowLargePages = true;
            gmmRequirements.preferCompressed = allocationData.flags.preferCompressed;
            auto gmm = std::make_unique<Gmm>(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(),
                                             allocationData.hostPtr,
                                             allocationDataAlign.size,
                                             allocationDataAlign.alignment,
                                             CacheSettingsHelper::getGmmUsageType(memoryAllocation->getAllocationType(), !!allocationData.flags.uncacheable, productHelper, hwInfo),
                                             allocationData.storageInfo, gmmRequirements);
            memoryAllocation->setDefaultGmm(gmm.release());
        }
    }
    return memoryAllocation;
}

GraphicsAllocation *OsAgnosticMemoryManager::allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData) {
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHardwareInfo();
    auto heap = heapAssigners[allocationData.rootDeviceIndex]->get32BitHeapIndex(allocationData.type, false, *hwInfo, allocationData.flags.use32BitFrontWindow);
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
            allocationData.rootDeviceIndex, 1u /*num gmms*/, allocationData.type, nullptr, const_cast<void *>(allocationData.hostPtr),
            canonizedGpuAddress, allocationData.size,
            counter, MemoryPool::system4KBPagesWith32BitGpuAddressing, false, false, maxOsContextCount);

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
        memoryAllocation = new MemoryAllocation(allocationData.rootDeviceIndex, 1u /*num gmms*/, allocationData.type, ptrAlloc, ptrAlloc,
                                                canonizedGpuAddress,
                                                allocationData.size, counter, MemoryPool::system4KBPagesWith32BitGpuAddressing,
                                                false, allocationData.flags.flushL3, maxOsContextCount);

        memoryAllocation->set32BitAllocation(true);
        memoryAllocation->setGpuBaseAddress(gmmHelper->canonize(gfxPartition->getHeapBase(heap)));
        memoryAllocation->sizeToFree = allocationSize;
    }
    counter++;
    return memoryAllocation;
}

GraphicsAllocation *OsAgnosticMemoryManager::createGraphicsAllocationFromMultipleSharedHandles(const std::vector<osHandle> &handles, AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) {
    return nullptr;
}

GraphicsAllocation *OsAgnosticMemoryManager::createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) {
    void *gpuPtr = reinterpret_cast<void *>(1);
    if (mapPointer) {
        gpuPtr = mapPointer;
    }
    auto graphicsAllocation = createMemoryAllocation(properties.allocationType, nullptr, gpuPtr, castToUint64(gpuPtr),
                                                     4096u, static_cast<uint64_t>(osHandleData.handle), MemoryPool::systemCpuInaccessible, properties.rootDeviceIndex,
                                                     false, false, requireSpecificBitness);
    graphicsAllocation->setSharedHandle(osHandleData.handle);
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

    removeAllocationFromDownloadAllocationsInCsr(gfxAllocation);

    if (gfxAllocation->getGpuAddress() == dummyAddress) {
        delete gfxAllocation;
        return;
    }

    if (reinterpret_cast<uint64_t>(gfxAllocation->getUnderlyingBuffer()) == dummyAddress) {
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
        if (aubCenter && aubCenter->getAubManager() && debugManager.flags.EnableFreeMemory.get() && gfxAllocation->getAllocationType() != AllocationType::externalHostPtr) {
            aubCenter->getAubManager()->freeMemory(
                peekExecutionEnvironment().rootDeviceEnvironments[gfxAllocation->getRootDeviceIndex()].get()->gmmHelper.get()->decanonize(gfxAllocation->getGpuAddress()), gfxAllocation->getUnderlyingBufferSize());
        }
    }
    delete gfxAllocation;
}

uint64_t OsAgnosticMemoryManager::getSystemSharedMemory(uint32_t rootDeviceIndex) {
    return 16 * MemoryConstants::gigaByte;
}

GraphicsAllocation *OsAgnosticMemoryManager::createGraphicsAllocation(OsHandleStorage &handleStorage, const AllocationData &allocationData) {
    auto allocation = createMemoryAllocation(allocationData.type, nullptr, const_cast<void *>(allocationData.hostPtr),
                                             reinterpret_cast<uint64_t>(allocationData.hostPtr), allocationData.size, counter++,
                                             MemoryPool::system4KBPages, allocationData.rootDeviceIndex, false, allocationData.flags.flushL3, false);

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
            if (aubCenter && aubCenter->getAubManager() && debugManager.flags.EnableFreeMemory.get()) {
                aubCenter->getAubManager()->freeMemory((uint64_t)handleStorage.fragmentStorageData[i].cpuPtr, handleStorage.fragmentStorageData[i].fragmentSize);
            }
            delete handleStorage.fragmentStorageData[i].osHandleStorage;
            delete handleStorage.fragmentStorageData[i].residency;
        }
    }
}

void OsAgnosticMemoryManager::unMapPhysicalDeviceMemoryFromVirtualMemory(GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize, OsContext *osContext, uint32_t rootDeviceIndex) {
    physicalAllocation->setGpuPtr(0u);
    physicalAllocation->setReservedAddressRange(nullptr, 0u);
}

void OsAgnosticMemoryManager::unMapPhysicalHostMemoryFromVirtualMemory(MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) {
    for (uint32_t i = 0; i < static_cast<uint32_t>(multiGraphicsAllocation.getGraphicsAllocations().size()); i++) {
        delete multiGraphicsAllocation.getGraphicsAllocation(i);
        multiGraphicsAllocation.removeAllocation(i);
    }
}

bool OsAgnosticMemoryManager::mapPhysicalDeviceMemoryToVirtualMemory(GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) {
    physicalAllocation->setGpuPtr(gpuRange);
    physicalAllocation->setReservedAddressRange(reinterpret_cast<void *>(gpuRange), bufferSize);
    return true;
}

bool OsAgnosticMemoryManager::mapPhysicalHostMemoryToVirtualMemory(RootDeviceIndicesContainer &rootDeviceIndices, MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) {
    for (size_t i = 0; i < rootDeviceIndices.size(); i++) {
        auto allocation = new GraphicsAllocation(rootDeviceIndices[i], 1u, AllocationType::bufferHostMemory, addrToPtr(gpuRange), bufferSize, physicalAllocation->peekSharedHandle(), MemoryPool::systemCpuInaccessible, 1, gpuRange);
        if (i == 0) {
            allocation->setGpuPtr(gpuRange);
            allocation->setReservedAddressRange(addrToPtr(gpuRange), bufferSize);
        }
        multiGraphicsAllocation.addAllocation(allocation);
    }
    return true;
}

GraphicsAllocation *OsAgnosticMemoryManager::allocatePhysicalLocalDeviceMemory(const AllocationData &allocationData, AllocationStatus &status) {
    status = AllocationStatus::Error;
    MemoryAllocation *allocation = nullptr;
    auto numHandles = allocationData.storageInfo.getNumBanks();

    std::unique_ptr<Gmm> gmm;
    size_t sizeAligned64k = 0;
    sizeAligned64k = alignUp(allocationData.size, MemoryConstants::pageSize64k);
    auto &productHelper = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHelper<ProductHelper>();
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper()->getHardwareInfo();
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = allocationData.flags.preferCompressed;
    gmm = std::make_unique<Gmm>(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(),
                                nullptr,
                                sizeAligned64k,
                                MemoryConstants::pageSize64k,
                                CacheSettingsHelper::getGmmUsageType(allocationData.type, allocationData.flags.uncacheable, productHelper, hwInfo),
                                allocationData.storageInfo,
                                gmmRequirements);

    auto systemMemory = allocateSystemMemory(sizeAligned64k, MemoryConstants::pageSize64k);
    if (systemMemory) {
        auto sizeOfHeapChunk = sizeAligned64k;
        allocation = new MemoryAllocation(allocationData.rootDeviceIndex, numHandles, allocationData.type, systemMemory, systemMemory,
                                          0u, sizeAligned64k, counter,
                                          MemoryPool::localMemory, false, allocationData.flags.flushL3, maxOsContextCount);
        counter++;
        allocation->setDefaultGmm(gmm.release());
        allocation->sizeToFree = sizeOfHeapChunk;
    }

    if (allocation) {
        allocation->overrideMemoryPool(MemoryPool::localMemory);
        allocation->storageInfo = allocationData.storageInfo;
        status = AllocationStatus::Success;
    }

    return allocation;
}

GraphicsAllocation *OsAgnosticMemoryManager::allocatePhysicalDeviceMemory(const AllocationData &allocationData, AllocationStatus &status) {
    status = AllocationStatus::Error;

    auto &productHelper = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHelper<ProductHelper>();
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper()->getHardwareInfo();
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = allocationData.flags.preferCompressed;
    auto gmm = std::make_unique<Gmm>(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(), allocationData.hostPtr,
                                     allocationData.size, 0u, CacheSettingsHelper::getGmmUsageType(allocationData.type, allocationData.flags.uncacheable, productHelper, hwInfo),
                                     allocationData.storageInfo, gmmRequirements);

    GraphicsAllocation *alloc = nullptr;

    auto ptr = allocateSystemMemory(alignUp(allocationData.size, MemoryConstants::pageSize), MemoryConstants::pageSize);
    if (ptr != nullptr) {
        alloc = new MemoryAllocation(allocationData.rootDeviceIndex, 1u /*num gmms*/, allocationData.type, ptr, ptr, 0u, allocationData.size,
                                     counter, MemoryPool::systemCpuInaccessible, allocationData.flags.uncacheable, allocationData.flags.flushL3, maxOsContextCount);
        counter++;
    }

    if (alloc) {
        alloc->setDefaultGmm(gmm.release());
        status = AllocationStatus::Success;
    }
    return alloc;
}

GraphicsAllocation *OsAgnosticMemoryManager::allocatePhysicalHostMemory(const AllocationData &allocationData, AllocationStatus &status) {
    status = AllocationStatus::Error;

    auto &productHelper = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHelper<ProductHelper>();
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper()->getHardwareInfo();
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::make_unique<Gmm>(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(), nullptr,
                                     allocationData.size, 0u, CacheSettingsHelper::getGmmUsageType(allocationData.type, allocationData.flags.uncacheable, productHelper, hwInfo),
                                     allocationData.storageInfo, gmmRequirements);

    GraphicsAllocation *alloc = nullptr;

    auto ptr = allocateSystemMemory(alignUp(allocationData.size, MemoryConstants::pageSize), MemoryConstants::pageSize2M);
    if (ptr != nullptr) {
        alloc = new MemoryAllocation(allocationData.rootDeviceIndex, 1u /*num gmms*/, allocationData.type, ptr, ptr, 0u, allocationData.size,
                                     counter, MemoryPool::system4KBPages, allocationData.flags.uncacheable, allocationData.flags.flushL3, maxOsContextCount);
        counter++;
    }

    if (alloc) {
        alloc->setDefaultGmm(gmm.release());
        status = AllocationStatus::Success;
    }

    return alloc;
}

GraphicsAllocation *OsAgnosticMemoryManager::allocateMemoryByKMD(const AllocationData &allocationData) {

    auto &productHelper = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHelper<ProductHelper>();
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper()->getHardwareInfo();
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = allocationData.flags.preferCompressed;
    auto gmm = std::make_unique<Gmm>(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(), allocationData.hostPtr,
                                     allocationData.size, 0u, CacheSettingsHelper::getGmmUsageType(allocationData.type, allocationData.flags.uncacheable, productHelper, hwInfo),
                                     allocationData.storageInfo, gmmRequirements);

    GraphicsAllocation *alloc = nullptr;

    const size_t alignment = std::max(allocationData.alignment, MemoryConstants::pageSize);
    auto ptr = allocateSystemMemory(alignUp(allocationData.size, alignment), alignment);
    if (ptr != nullptr) {
        alloc = createMemoryAllocation(allocationData.type, ptr, ptr, reinterpret_cast<uint64_t>(ptr), allocationData.size,
                                       counter, MemoryPool::systemCpuInaccessible, allocationData.rootDeviceIndex, allocationData.flags.uncacheable, allocationData.flags.flushL3, false);
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
                                       counter, MemoryPool::systemCpuInaccessible, allocationData.rootDeviceIndex, allocationData.flags.uncacheable, allocationData.flags.flushL3, false);
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

AddressRange OsAgnosticMemoryManager::reserveCpuAddress(const uint64_t requiredStartAddress, size_t size) {
    return {castToUint64(alignedMallocWrapper(size, MemoryConstants::pageSize)), size};
}

void OsAgnosticMemoryManager::freeCpuAddress(AddressRange addressRange) {
    alignedFreeWrapper(addrToPtr(addressRange.address));
}

MemoryAllocation *OsAgnosticMemoryManager::createMemoryAllocation(AllocationType allocationType, void *driverAllocatedCpuPointer,
                                                                  void *pMem, uint64_t gpuAddress, size_t memSize, uint64_t count,
                                                                  MemoryPool pool, uint32_t rootDeviceIndex, bool uncacheable,
                                                                  bool flushL3Required, bool requireSpecificBitness) {
    auto gmmHelper = getGmmHelper(rootDeviceIndex);
    if (!isLimitedRange(rootDeviceIndex)) {
        auto canonizedGpuAddress = gmmHelper->canonize(gpuAddress);
        return new MemoryAllocation(rootDeviceIndex, 1u /*num gmms*/, allocationType, driverAllocatedCpuPointer, pMem, canonizedGpuAddress, memSize,
                                    count, pool, uncacheable, flushL3Required, maxOsContextCount);
    }

    size_t alignedSize = alignSizeWholePage(pMem, memSize);

    auto heap = (force32bitAllocations || requireSpecificBitness) ? HeapIndex::heapExternal : HeapIndex::heapStandard;

    auto gfxPartition = getGfxPartition(rootDeviceIndex);
    uint64_t limitedGpuAddress = gfxPartition->heapAllocate(heap, alignedSize);
    auto canonizedGpuAddress = gmmHelper->canonize(limitedGpuAddress);
    auto memoryAllocation = new MemoryAllocation(rootDeviceIndex, 1u /*num gmms*/, allocationType, driverAllocatedCpuPointer, pMem, canonizedGpuAddress, memSize,
                                                 count, pool, uncacheable, flushL3Required, maxOsContextCount);

    if (heap == HeapIndex::heapExternal) {
        memoryAllocation->setGpuBaseAddress(gmmHelper->canonize(gfxPartition->getHeapBase(heap)));
    }
    memoryAllocation->sizeToFree = alignedSize;

    return memoryAllocation;
}

size_t OsAgnosticMemoryManager::selectAlignmentAndHeap(size_t size, HeapIndex *heap) {
    *heap = HeapIndex::heapStandard;
    return MemoryConstants::pageSize64k;
}

AddressRange OsAgnosticMemoryManager::reserveGpuAddress(const uint64_t requiredStartAddress, size_t size, const RootDeviceIndicesContainer &rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex) {
    return reserveGpuAddressOnHeap(requiredStartAddress, size, rootDeviceIndices, reservedOnRootDeviceIndex, HeapIndex::heapStandard, MemoryConstants::pageSize64k);
}

AddressRange OsAgnosticMemoryManager::reserveGpuAddressOnHeap(const uint64_t requiredStartAddress, size_t size, const RootDeviceIndicesContainer &rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex, HeapIndex heap, size_t alignment) {
    uint64_t gpuVa = 0u;
    *reservedOnRootDeviceIndex = 0;
    for (auto rootDeviceIndex : rootDeviceIndices) {
        auto gfxPartition = getGfxPartition(rootDeviceIndex);
        auto gmmHelper = getGmmHelper(rootDeviceIndex);
        gpuVa = gmmHelper->canonize(gfxPartition->heapAllocate(heap, size));
        if (gpuVa != 0u) {
            *reservedOnRootDeviceIndex = rootDeviceIndex;
            break;
        }
    }
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
    bool use32Allocator = heapAssigners[allocationData.rootDeviceIndex]->use32BitHeap(allocationData.type);
    if (allocationData.type == AllocationType::svmGpu) {
        auto storage = allocateSystemMemory(allocationData.size, MemoryConstants::pageSize2M);
        auto canonizedGpuAddress = gmmHelper->canonize(reinterpret_cast<uint64_t>(allocationData.hostPtr));
        allocation = new MemoryAllocation(allocationData.rootDeviceIndex, numHandles, allocationData.type, storage, storage, canonizedGpuAddress,
                                          allocationData.size, counter, MemoryPool::localMemory, false, allocationData.flags.flushL3, maxOsContextCount);
        counter++;
        if (allocationData.flags.preferCompressed) {
            auto &productHelper = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHelper<ProductHelper>();
            GmmRequirements gmmRequirements{};
            gmmRequirements.allowLargePages = true;
            gmmRequirements.preferCompressed = true;

            auto gmm = std::make_unique<Gmm>(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(),
                                             allocationData.hostPtr,
                                             allocationData.size,
                                             MemoryConstants::pageSize2M,
                                             CacheSettingsHelper::getGmmUsageType(allocationData.type, allocationData.flags.uncacheable, productHelper, gmmHelper->getHardwareInfo()),
                                             allocationData.storageInfo,
                                             gmmRequirements);
            allocation->setDefaultGmm(gmm.release());
        }
    } else {
        std::unique_ptr<Gmm> gmm;
        size_t sizeAligned64k = 0;
        if (allocationData.type == AllocationType::image ||
            allocationData.type == AllocationType::sharedResourceCopy) {
            allocationData.imgInfo->useLocalMemory = true;
            gmm = std::make_unique<Gmm>(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(), *allocationData.imgInfo,
                                        allocationData.storageInfo, allocationData.flags.preferCompressed);
            sizeAligned64k = alignUp(allocationData.imgInfo->size, MemoryConstants::pageSize64k);
        } else {
            sizeAligned64k = alignUp(allocationData.size, MemoryConstants::pageSize64k);
            if (debugManager.flags.RenderCompressedBuffersEnabled.get() &&
                allocationData.flags.preferCompressed) {
                auto &productHelper = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHelper<ProductHelper>();
                GmmRequirements gmmRequirements{};
                gmmRequirements.allowLargePages = true;
                gmmRequirements.preferCompressed = true;

                gmm = std::make_unique<Gmm>(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(),
                                            allocationData.hostPtr,
                                            sizeAligned64k,
                                            MemoryConstants::pageSize64k,
                                            CacheSettingsHelper::getGmmUsageType(allocationData.type, allocationData.flags.uncacheable, productHelper, gmmHelper->getHardwareInfo()),
                                            allocationData.storageInfo,
                                            gmmRequirements);
            }
        }

        auto gfxPartition = getGfxPartition(allocationData.rootDeviceIndex);
        auto heapIndex = HeapIndex::heapStandard64KB;

        if (use32Allocator) {
            auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHardwareInfo();
            heapIndex = heapAssigners[allocationData.rootDeviceIndex]->get32BitHeapIndex(allocationData.type, true, *hwInfo, allocationData.flags.use32BitFrontWindow);
        } else if ((gfxPartition->getHeapLimit(HeapIndex::heapExtended) > 0) && !allocationData.flags.resource48Bit) {
            heapIndex = HeapIndex::heapExtended;
        }

        auto systemMemory = allocateSystemMemory(sizeAligned64k, MemoryConstants::pageSize64k);
        if (allocationData.type == AllocationType::preemption) {
            memset(systemMemory, 0, sizeAligned64k);
        }
        auto sizeOfHeapChunk = sizeAligned64k;
        auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
        auto canonizedGpuAddress = gmmHelper->canonize(gfxPartition->heapAllocate(heapIndex, sizeOfHeapChunk));
        if (heapIndex == HeapIndex::heapExtended) {
            canonizedGpuAddress = MemoryManager::adjustToggleBitFlagForGpuVa(allocationData.type, canonizedGpuAddress);
        }
        allocation = new MemoryAllocation(allocationData.rootDeviceIndex, numHandles, allocationData.type, systemMemory, systemMemory,
                                          canonizedGpuAddress, sizeAligned64k, counter,
                                          MemoryPool::localMemory, false, allocationData.flags.flushL3, maxOsContextCount);
        counter++;
        allocation->setDefaultGmm(gmm.release());
        allocation->sizeToFree = sizeOfHeapChunk;
        if (use32Allocator) {
            allocation->setGpuBaseAddress(gmmHelper->canonize(gfxPartition->getHeapBase(heapIndex)));
        }
    }

    if (allocation) {
        allocation->overrideMemoryPool(MemoryPool::localMemory);
        allocation->storageInfo = allocationData.storageInfo;
        status = AllocationStatus::Success;
    } else {
        status = AllocationStatus::Error;
    }

    return allocation;
}

uint64_t OsAgnosticMemoryManager::getLocalMemorySize(uint32_t rootDeviceIndex, uint32_t deviceBitfield) {
    DeviceBitfield bitfield = deviceBitfield;
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
    auto releaseHelper = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getReleaseHelper();
    return (AubHelper::getPerTileLocalMemorySize(hwInfo, releaseHelper) * bitfield.count());
}

double OsAgnosticMemoryManager::getPercentOfGlobalMemoryAvailable(uint32_t rootDeviceIndex) {
    return 0.8;
}

void OsAgnosticMemoryManager::handleFenceCompletion(GraphicsAllocation *allocation) {

    for (auto &engine : getRegisteredEngines(allocation->getRootDeviceIndex())) {
        const auto usedByContext = allocation->isUsedByOsContext(engine.osContext->getContextId());
        if (usedByContext) {
            engine.commandStreamReceiver->pollForCompletion();
        }
    }
}

void OsAgnosticMemoryManager::removeAllocationFromDownloadAllocationsInCsr(GraphicsAllocation *alloc) {
    for (const auto &engineControl : getRegisteredEngines(alloc->getRootDeviceIndex())) {
        engineControl.commandStreamReceiver->removeDownloadAllocation(alloc);
    }
}

} // namespace NEO
