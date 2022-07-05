/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm_memory_manager.h"

#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/deferred_deleter_helper.h"
#include "shared/source/helpers/heap_assigner.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/deferrable_deletion.h"
#include "shared/source/memory_manager/deferred_deleter.h"
#include "shared/source/memory_manager/host_ptr_manager.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/dxgi_wrapper.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/sys_calls_wrapper.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm_allocation.h"
#include "shared/source/os_interface/windows/wddm_residency_allocations_container.h"
#include "shared/source/os_interface/windows/wddm_residency_controller.h"

#include <algorithm>
#include <emmintrin.h>

namespace NEO {

WddmMemoryManager::~WddmMemoryManager() = default;

WddmMemoryManager::WddmMemoryManager(ExecutionEnvironment &executionEnvironment) : MemoryManager(executionEnvironment) {
    asyncDeleterEnabled = isDeferredDeleterEnabled();
    if (asyncDeleterEnabled)
        deferredDeleter = createDeferredDeleter();
    mallocRestrictions.minAddress = 0u;

    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < gfxPartitions.size(); ++rootDeviceIndex) {
        mallocRestrictions.minAddress = std::max(mallocRestrictions.minAddress, getWddm(rootDeviceIndex).getWddmMinAddress());
        getWddm(rootDeviceIndex).initGfxPartition(*getGfxPartition(rootDeviceIndex), rootDeviceIndex, gfxPartitions.size(), heapAssigner.apiAllowExternalHeapForSshAndDsh);
    }

    alignmentSelector.addCandidateAlignment(MemoryConstants::pageSize64k, true, AlignmentSelector::anyWastage);
    if (DebugManager.flags.AlignLocalMemoryVaTo2MB.get() != 0) {
        constexpr static float maxWastage2Mb = 0.1f;
        alignmentSelector.addCandidateAlignment(MemoryConstants::pageSize2Mb, false, maxWastage2Mb);
    }
    const size_t customAlignment = static_cast<size_t>(DebugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.get());
    if (customAlignment > 0) {
        alignmentSelector.addCandidateAlignment(customAlignment, false, AlignmentSelector::anyWastage);
    }

    initialized = true;
}

GraphicsAllocation *WddmMemoryManager::allocateMemoryByKMD(const AllocationData &allocationData) {
    if (allocationData.size > getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod::AllocateByKmd)) {
        return allocateHugeGraphicsMemory(allocationData, false);
    }
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHardwareInfo();

    StorageInfo systemMemoryStorageInfo = {};
    auto gmm = std::make_unique<Gmm>(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(), allocationData.hostPtr, allocationData.size, 0u,
                                     CacheSettingsHelper::getGmmUsageType(allocationData.type, !!allocationData.flags.uncacheable, *hwInfo), false, systemMemoryStorageInfo, true);
    auto allocation = std::make_unique<WddmAllocation>(allocationData.rootDeviceIndex,
                                                       1u, // numGmms
                                                       allocationData.type, nullptr, 0, allocationData.size, nullptr,
                                                       MemoryPool::SystemCpuInaccessible, allocationData.flags.shareable, maxOsContextCount);
    allocation->setDefaultGmm(gmm.get());
    if (!createWddmAllocation(allocation.get(), nullptr)) {
        return nullptr;
    }

    gmm.release();

    return allocation.release();
}

GraphicsAllocation *WddmMemoryManager::allocateGraphicsMemoryForImageImpl(const AllocationData &allocationData, std::unique_ptr<Gmm> gmm) {
    auto allocation = std::make_unique<WddmAllocation>(allocationData.rootDeviceIndex,
                                                       1u, // numGmms
                                                       allocationData.type, nullptr, 0, allocationData.imgInfo->size,
                                                       nullptr, MemoryPool::SystemCpuInaccessible,
                                                       0u, // shareable
                                                       maxOsContextCount);
    allocation->setDefaultGmm(gmm.get());
    if (!createWddmAllocation(allocation.get(), nullptr)) {
        return nullptr;
    }

    gmm.release();

    return allocation.release();
}

GraphicsAllocation *WddmMemoryManager::allocateGraphicsMemory64kb(const AllocationData &allocationData) {
    return allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(allocationData, true);
}

GraphicsAllocation *WddmMemoryManager::allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(const AllocationData &allocationData, bool allowLargePages) {
    size_t sizeAligned = alignUp(allocationData.size, allowLargePages ? MemoryConstants::pageSize64k : MemoryConstants::pageSize);
    if (sizeAligned > getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod::AllocateByKmd)) {
        return allocateHugeGraphicsMemory(allocationData, false);
    }

    auto wddmAllocation = std::make_unique<WddmAllocation>(allocationData.rootDeviceIndex,
                                                           1u, // numGmms
                                                           allocationData.type, nullptr, 0,
                                                           sizeAligned, nullptr, allowLargePages ? MemoryPool::System64KBPages : MemoryPool::System4KBPages,
                                                           0u, // shareable
                                                           maxOsContextCount);

    auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHardwareInfo();

    auto gmm = new Gmm(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(), nullptr,
                       sizeAligned, 0u,
                       CacheSettingsHelper::getGmmUsageType(wddmAllocation->getAllocationType(), !!allocationData.flags.uncacheable, *hwInfo),
                       allocationData.flags.preferCompressed,
                       allocationData.storageInfo,
                       allowLargePages);
    wddmAllocation->setDefaultGmm(gmm);
    wddmAllocation->setFlushL3Required(allocationData.flags.flushL3);
    wddmAllocation->storageInfo = allocationData.storageInfo;

    if (!getWddm(allocationData.rootDeviceIndex).createAllocation(gmm, wddmAllocation->getHandleToModify(0u))) {
        delete gmm;
        return nullptr;
    }

    auto cpuPtr = gmm->isCompressionEnabled ? nullptr : lockResource(wddmAllocation.get());

    [[maybe_unused]] auto status = true;

    if (executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHardwareInfo()->capabilityTable.gpuAddressSpace >= MemoryConstants::max64BitAppAddress || is32bit) {
        status = mapGpuVirtualAddress(wddmAllocation.get(), cpuPtr);
    } else {
        status = mapGpuVirtualAddress(wddmAllocation.get(), nullptr);
    }
    DEBUG_BREAK_IF(!status);
    wddmAllocation->setCpuAddress(cpuPtr);
    return wddmAllocation.release();
}

GraphicsAllocation *WddmMemoryManager::allocateHugeGraphicsMemory(const AllocationData &allocationData, bool sharedVirtualAddress) {
    void *hostPtr = nullptr, *alignedPtr = nullptr;
    size_t alignedSize = 0;
    bool uncacheable = allocationData.flags.uncacheable;
    auto memoryPool = MemoryPool::System64KBPages;

    if (allocationData.hostPtr) {
        hostPtr = const_cast<void *>(allocationData.hostPtr);
        alignedSize = alignSizeWholePage(hostPtr, allocationData.size);
        alignedPtr = alignDown(hostPtr, MemoryConstants::pageSize);
        memoryPool = MemoryPool::System4KBPages;
    } else {
        alignedSize = alignUp(allocationData.size, MemoryConstants::pageSize64k);
        uncacheable = false;
        hostPtr = alignedPtr = allocateSystemMemory(alignedSize, MemoryConstants::pageSize64k);
        if (nullptr == hostPtr) {
            return nullptr;
        }
    }

    auto chunkSize = getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod::UseUmdSystemPtr);
    auto numGmms = (alignedSize + chunkSize - 1) / chunkSize;
    auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
    auto canonizedAddress = gmmHelper->canonize(castToUint64(const_cast<void *>(hostPtr)));
    auto wddmAllocation = std::make_unique<WddmAllocation>(allocationData.rootDeviceIndex, numGmms,
                                                           allocationData.type, hostPtr, canonizedAddress, allocationData.size,
                                                           nullptr, memoryPool,
                                                           0u, // shareable
                                                           maxOsContextCount);

    if (allocationData.hostPtr) {
        wddmAllocation->setAllocationOffset(ptrDiff(hostPtr, alignedPtr));
    } else {
        wddmAllocation->setSize(alignedSize);
        wddmAllocation->setDriverAllocatedCpuPtr(hostPtr);
    }

    auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHardwareInfo();
    auto sizeRemaining = alignedSize;
    for (auto gmmId = 0u; gmmId < numGmms; ++gmmId) {
        auto size = sizeRemaining > chunkSize ? chunkSize : sizeRemaining;
        auto gmm = new Gmm(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(),
                           static_cast<char *>(alignedPtr) + gmmId * chunkSize, size, 0u,
                           CacheSettingsHelper::getGmmUsageType(wddmAllocation->getAllocationType(), !!uncacheable, *hwInfo), false, {}, true);
        wddmAllocation->setGmm(gmm, gmmId);
        sizeRemaining -= size;
    }

    wddmAllocation->storageInfo.multiStorage = true;

    if (!createWddmAllocation(wddmAllocation.get(), sharedVirtualAddress ? hostPtr : nullptr)) {
        for (auto gmmId = 0u; gmmId < wddmAllocation->getNumGmms(); ++gmmId) {
            delete wddmAllocation->getGmm(gmmId);
        }
        freeSystemMemory(wddmAllocation->getDriverAllocatedCpuPtr());
        return nullptr;
    }

    return wddmAllocation.release();
}

GraphicsAllocation *WddmMemoryManager::allocateUSMHostGraphicsMemory(const AllocationData &allocationData) {
    return allocateGraphicsMemoryWithHostPtr(allocationData);
}

GraphicsAllocation *WddmMemoryManager::allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) {
    auto pageSize = NEO::OSInterface::osEnabled64kbPages ? MemoryConstants::pageSize64k : MemoryConstants::pageSize;
    bool requiresNonStandardAlignment = allocationData.alignment > pageSize;
    if ((preferredAllocationMethod == GfxMemoryAllocationMethod::UseUmdSystemPtr) || requiresNonStandardAlignment) {
        return allocateSystemMemoryAndCreateGraphicsAllocationFromIt(allocationData);
    } else {
        return allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(allocationData, NEO::OSInterface::osEnabled64kbPages);
    }
}

GraphicsAllocation *WddmMemoryManager::allocateSystemMemoryAndCreateGraphicsAllocationFromIt(const AllocationData &allocationData) {
    size_t newAlignment = allocationData.alignment ? alignUp(allocationData.alignment, MemoryConstants::pageSize) : MemoryConstants::pageSize;
    size_t sizeAligned = allocationData.size ? alignUp(allocationData.size, MemoryConstants::pageSize) : MemoryConstants::pageSize;
    if (sizeAligned > getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod::UseUmdSystemPtr)) {
        return allocateHugeGraphicsMemory(allocationData, true);
    }
    void *pSysMem = allocateSystemMemory(sizeAligned, newAlignment);
    Gmm *gmm = nullptr;

    if (pSysMem == nullptr) {
        return nullptr;
    }

    auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
    auto canonizedAddress = gmmHelper->canonize(castToUint64(pSysMem));
    auto wddmAllocation = std::make_unique<WddmAllocation>(allocationData.rootDeviceIndex,
                                                           1u, // numGmms
                                                           allocationData.type, pSysMem, canonizedAddress, sizeAligned,
                                                           nullptr, MemoryPool::System4KBPages,
                                                           0u, // shareable
                                                           maxOsContextCount);
    wddmAllocation->setDriverAllocatedCpuPtr(pSysMem);

    auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHardwareInfo();

    gmm = new Gmm(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(), pSysMem, sizeAligned, 0u,
                  CacheSettingsHelper::getGmmUsageType(wddmAllocation->getAllocationType(), !!allocationData.flags.uncacheable, *hwInfo),
                  allocationData.flags.preferCompressed, allocationData.storageInfo, true);

    wddmAllocation->setDefaultGmm(gmm);
    void *mapPtr = wddmAllocation->getAlignedCpuPtr();
    if (allocationData.type == AllocationType::SVM_CPU) {
        // add  padding in case mapPtr is not aligned
        size_t reserveSizeAligned = sizeAligned + allocationData.alignment;
        bool ret = getWddm(wddmAllocation->getRootDeviceIndex()).reserveValidAddressRange(reserveSizeAligned, mapPtr);
        if (!ret) {
            delete gmm;
            freeSystemMemory(pSysMem);
            return nullptr;
        }
        wddmAllocation->setReservedAddressRange(mapPtr, reserveSizeAligned);
        mapPtr = alignUp(mapPtr, newAlignment);
    }

    mapPtr = isLimitedGPU(allocationData.rootDeviceIndex) ? nullptr : mapPtr;
    if (!createWddmAllocation(wddmAllocation.get(), mapPtr)) {
        delete gmm;
        freeSystemMemory(pSysMem);
        return nullptr;
    }

    return wddmAllocation.release();
}

GraphicsAllocation *WddmMemoryManager::allocateGraphicsMemoryForNonSvmHostPtr(const AllocationData &allocationData) {
    auto alignedSize = alignSizeWholePage(allocationData.hostPtr, allocationData.size);
    if (alignedSize > getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod::UseUmdSystemPtr)) {
        return allocateHugeGraphicsMemory(allocationData, false);
    }

    auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
    auto canonizedAddress = gmmHelper->canonize(castToUint64(const_cast<void *>(allocationData.hostPtr)));
    auto wddmAllocation = std::make_unique<WddmAllocation>(allocationData.rootDeviceIndex,
                                                           1u, // numGmms
                                                           allocationData.type, const_cast<void *>(allocationData.hostPtr), canonizedAddress,
                                                           allocationData.size, nullptr, MemoryPool::System4KBPages,
                                                           0u, // shareable
                                                           maxOsContextCount);

    auto alignedPtr = alignDown(allocationData.hostPtr, MemoryConstants::pageSize);
    auto offsetInPage = ptrDiff(allocationData.hostPtr, alignedPtr);
    wddmAllocation->setAllocationOffset(offsetInPage);

    auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHardwareInfo();

    auto gmm = new Gmm(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(), alignedPtr, alignedSize, 0u,
                       CacheSettingsHelper::getGmmUsageType(wddmAllocation->getAllocationType(), !!allocationData.flags.uncacheable, *hwInfo), false, {}, true);

    wddmAllocation->setDefaultGmm(gmm);

    if (!createWddmAllocation(wddmAllocation.get(), nullptr)) {
        delete gmm;
        return nullptr;
    }

    return wddmAllocation.release();
}

GraphicsAllocation *WddmMemoryManager::allocateGraphicsMemoryWithHostPtr(const AllocationData &allocationData) {
    if (allocationData.size > getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod::UseUmdSystemPtr)) {
        return allocateHugeGraphicsMemory(allocationData, false);
    }

    if (mallocRestrictions.minAddress > reinterpret_cast<uintptr_t>(allocationData.hostPtr)) {
        auto inputPtr = allocationData.hostPtr;
        void *reserve = nullptr;
        auto ptrAligned = alignDown(inputPtr, MemoryConstants::allocationAlignment);
        size_t sizeAligned = alignSizeWholePage(inputPtr, allocationData.size);
        size_t offset = ptrDiff(inputPtr, ptrAligned);

        if (!getWddm(allocationData.rootDeviceIndex).reserveValidAddressRange(sizeAligned, reserve)) {
            return nullptr;
        }

        auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
        auto canonizedAddress = gmmHelper->canonize(castToUint64(const_cast<void *>(inputPtr)));
        auto allocation = new WddmAllocation(allocationData.rootDeviceIndex,
                                             1u, // numGmms
                                             allocationData.type,
                                             const_cast<void *>(inputPtr),
                                             canonizedAddress,
                                             allocationData.size,
                                             reserve, MemoryPool::System4KBPages,
                                             0u, // shareable
                                             maxOsContextCount);
        allocation->setAllocationOffset(offset);

        auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHardwareInfo();

        Gmm *gmm = new Gmm(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(), ptrAligned, sizeAligned, 0u,
                           CacheSettingsHelper::getGmmUsageType(allocation->getAllocationType(), !!allocationData.flags.uncacheable, *hwInfo), false, {}, true);
        allocation->setDefaultGmm(gmm);
        if (createWddmAllocation(allocation, reserve)) {
            return allocation;
        }
        freeGraphicsMemory(allocation);
        return nullptr;
    }
    return MemoryManager::allocateGraphicsMemoryWithHostPtr(allocationData);
}

GraphicsAllocation *WddmMemoryManager::allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData, bool useLocalMemory) {
    Gmm *gmm = nullptr;
    const void *ptrAligned = nullptr;
    size_t sizeAligned = allocationData.size;
    void *pSysMem = nullptr;
    size_t offset = 0;

    if (allocationData.hostPtr) {
        ptrAligned = alignDown(allocationData.hostPtr, MemoryConstants::allocationAlignment);
        sizeAligned = alignSizeWholePage(allocationData.hostPtr, sizeAligned);
        offset = ptrDiff(allocationData.hostPtr, ptrAligned);
    } else if (preferredAllocationMethod == GfxMemoryAllocationMethod::UseUmdSystemPtr) {
        sizeAligned = alignUp(sizeAligned, MemoryConstants::allocationAlignment);
        pSysMem = allocateSystemMemory(sizeAligned, MemoryConstants::allocationAlignment);
        if (pSysMem == nullptr) {
            return nullptr;
        }
        ptrAligned = pSysMem;
    } else {
        sizeAligned = alignUp(sizeAligned, MemoryConstants::allocationAlignment);
    }

    auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
    auto canonizedAddress = gmmHelper->canonize(castToUint64(const_cast<void *>(ptrAligned)));
    auto wddmAllocation = std::make_unique<WddmAllocation>(allocationData.rootDeviceIndex,
                                                           1u, // numGmms
                                                           allocationData.type, const_cast<void *>(ptrAligned), canonizedAddress,
                                                           sizeAligned, nullptr,
                                                           MemoryPool::System4KBPagesWith32BitGpuAddressing,
                                                           0u, // shareable
                                                           maxOsContextCount);
    wddmAllocation->setDriverAllocatedCpuPtr(pSysMem);
    wddmAllocation->set32BitAllocation(true);
    wddmAllocation->setAllocationOffset(offset);
    wddmAllocation->allocInFrontWindowPool = allocationData.flags.use32BitFrontWindow;

    auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHardwareInfo();

    gmm = new Gmm(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(), ptrAligned, sizeAligned, 0u,
                  CacheSettingsHelper::getGmmUsageType(wddmAllocation->getAllocationType(), !!allocationData.flags.uncacheable, *hwInfo), false, {}, true);
    wddmAllocation->setDefaultGmm(gmm);

    if (!createWddmAllocation(wddmAllocation.get(), nullptr)) {
        delete gmm;
        freeSystemMemory(pSysMem);
        return nullptr;
    }
    auto baseAddress = getGfxPartition(allocationData.rootDeviceIndex)->getHeapBase(heapAssigner.get32BitHeapIndex(allocationData.type, useLocalMemory, *hwInfo, allocationData.flags.use32BitFrontWindow));
    wddmAllocation->setGpuBaseAddress(gmmHelper->canonize(baseAddress));

    if (preferredAllocationMethod != GfxMemoryAllocationMethod::UseUmdSystemPtr) {
        auto lockedPtr = lockResource(wddmAllocation.get());
        wddmAllocation->setCpuAddress(lockedPtr);
    }

    return wddmAllocation.release();
}

bool WddmMemoryManager::verifyHandle(osHandle handle, uint32_t rootDeviceIndex, bool ntHandle) {
    bool status = ntHandle ? getWddm(rootDeviceIndex).verifyNTHandle(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(handle)))
                           : getWddm(rootDeviceIndex).verifySharedHandle(handle);
    return status;
}

bool WddmMemoryManager::isNTHandle(osHandle handle, uint32_t rootDeviceIndex) {
    bool status = getWddm(rootDeviceIndex).verifyNTHandle(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(handle)));
    return status;
}

GraphicsAllocation *WddmMemoryManager::createAllocationFromHandle(osHandle handle, bool requireSpecificBitness, bool ntHandle, AllocationType allocationType, uint32_t rootDeviceIndex) {
    auto allocation = std::make_unique<WddmAllocation>(rootDeviceIndex, allocationType, nullptr, 0, handle, MemoryPool::SystemCpuInaccessible, maxOsContextCount, 0llu);

    bool status = ntHandle ? getWddm(rootDeviceIndex).openNTHandle(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(handle)), allocation.get())
                           : getWddm(rootDeviceIndex).openSharedHandle(handle, allocation.get());

    if (!status) {
        return nullptr;
    }

    // Shared objects are passed without size
    size_t size = allocation->getDefaultGmm()->gmmResourceInfo->getSizeAllocation();
    allocation->setSize(size);

    if (is32bit) {
        void *ptr = nullptr;
        if (!getWddm(rootDeviceIndex).reserveValidAddressRange(size, ptr)) {
            return nullptr;
        }
        allocation->setReservedAddressRange(ptr, size);
    } else if (requireSpecificBitness && this->force32bitAllocations) {
        allocation->set32BitAllocation(true);
        auto gmmHelper = getGmmHelper(allocation->getRootDeviceIndex());
        allocation->setGpuBaseAddress(gmmHelper->canonize(getExternalHeapBaseAddress(allocation->getRootDeviceIndex(), false)));
    }
    status = mapGpuVirtualAddress(allocation.get(), allocation->getReservedAddressPtr());
    DEBUG_BREAK_IF(!status);
    if (!status) {
        freeGraphicsMemoryImpl(allocation.release());
        return nullptr;
    }

    fileLoggerInstance().logAllocation(allocation.get());
    return allocation.release();
}

GraphicsAllocation *WddmMemoryManager::createGraphicsAllocationFromMultipleSharedHandles(std::vector<osHandle> handles, AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation) {
    return nullptr;
}

GraphicsAllocation *WddmMemoryManager::createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation) {
    return createAllocationFromHandle(handle, requireSpecificBitness, false, properties.allocationType, properties.rootDeviceIndex);
}

GraphicsAllocation *WddmMemoryManager::createGraphicsAllocationFromNTHandle(void *handle, uint32_t rootDeviceIndex, AllocationType allocType) {
    return createAllocationFromHandle(toOsHandle(handle), false, true, allocType, rootDeviceIndex);
}

void WddmMemoryManager::addAllocationToHostPtrManager(GraphicsAllocation *gfxAllocation) {
    WddmAllocation *wddmMemory = static_cast<WddmAllocation *>(gfxAllocation);
    FragmentStorage fragment = {};
    fragment.driverAllocation = true;
    fragment.fragmentCpuPointer = gfxAllocation->getUnderlyingBuffer();
    fragment.fragmentSize = alignUp(gfxAllocation->getUnderlyingBufferSize(), MemoryConstants::pageSize);

    auto osHandle = new OsHandleWin();
    osHandle->gpuPtr = gfxAllocation->getGpuAddress();
    osHandle->handle = wddmMemory->getDefaultHandle();
    osHandle->gmm = gfxAllocation->getDefaultGmm();

    fragment.osInternalStorage = osHandle;
    fragment.residency = &wddmMemory->getResidencyData();
    hostPtrManager->storeFragment(gfxAllocation->getRootDeviceIndex(), fragment);
}

void WddmMemoryManager::removeAllocationFromHostPtrManager(GraphicsAllocation *gfxAllocation) {
    auto buffer = gfxAllocation->getUnderlyingBuffer();
    auto fragment = hostPtrManager->getFragment({buffer, gfxAllocation->getRootDeviceIndex()});
    if (fragment && fragment->driverAllocation) {
        OsHandle *osStorageToRelease = fragment->osInternalStorage;
        if (hostPtrManager->releaseHostPtr(gfxAllocation->getRootDeviceIndex(), buffer)) {
            delete osStorageToRelease;
        }
    }
}

void *WddmMemoryManager::lockResourceImpl(GraphicsAllocation &graphicsAllocation) {
    auto &wddmAllocation = static_cast<WddmAllocation &>(graphicsAllocation);
    return getWddm(graphicsAllocation.getRootDeviceIndex()).lockResource(wddmAllocation.getDefaultHandle(), wddmAllocation.needsMakeResidentBeforeLock, wddmAllocation.getAlignedSize());
}
void WddmMemoryManager::unlockResourceImpl(GraphicsAllocation &graphicsAllocation) {
    auto &wddmAllocation = static_cast<WddmAllocation &>(graphicsAllocation);
    getWddm(graphicsAllocation.getRootDeviceIndex()).unlockResource(wddmAllocation.getDefaultHandle());
    if (wddmAllocation.needsMakeResidentBeforeLock) {
        [[maybe_unused]] auto evictionStatus = getWddm(graphicsAllocation.getRootDeviceIndex()).getTemporaryResourcesContainer()->evictResource(wddmAllocation.getDefaultHandle());
        DEBUG_BREAK_IF(evictionStatus == MemoryOperationsStatus::FAILED);
    }
}
void WddmMemoryManager::freeAssociatedResourceImpl(GraphicsAllocation &graphicsAllocation) {
    auto &wddmAllocation = static_cast<WddmAllocation &>(graphicsAllocation);
    if (wddmAllocation.needsMakeResidentBeforeLock) {
        for (auto i = 0u; i < wddmAllocation.getNumGmms(); i++) {
            getWddm(graphicsAllocation.getRootDeviceIndex()).getTemporaryResourcesContainer()->removeResource(wddmAllocation.getHandles()[i]);
        }
    }
}

void WddmMemoryManager::freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation, bool isImportedAllocation) {
    return freeGraphicsMemoryImpl(gfxAllocation);
}

void WddmMemoryManager::freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) {
    WddmAllocation *input = static_cast<WddmAllocation *>(gfxAllocation);
    DEBUG_BREAK_IF(!validateAllocation(input));

    for (auto &engine : this->registeredEngines) {
        auto &residencyController = static_cast<OsContextWin *>(engine.osContext)->getResidencyController();
        auto lock = residencyController.acquireLock();
        residencyController.removeFromTrimCandidateListIfUsed(input, true);
    }

    auto defaultGmm = gfxAllocation->getDefaultGmm();
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[gfxAllocation->getRootDeviceIndex()]->getHardwareInfo();
    if (gfxAllocation->isCompressionEnabled() && HwInfoConfig::get(hwInfo->platform.eProductFamily)->isPageTableManagerSupported(*hwInfo)) {
        for (auto engine : registeredEngines) {
            if (engine.commandStreamReceiver->pageTableManager.get()) {
                [[maybe_unused]] auto status = engine.commandStreamReceiver->pageTableManager->updateAuxTable(input->getGpuAddress(), defaultGmm, false);
                DEBUG_BREAK_IF(!status);
            }
        }
    }
    for (auto handleId = 0u; handleId < gfxAllocation->getNumGmms(); handleId++) {
        delete gfxAllocation->getGmm(handleId);
    }
    if (input->peekInternalHandle(nullptr) != 0u) {
        [[maybe_unused]] auto status = SysCalls::closeHandle(reinterpret_cast<void *>(reinterpret_cast<uintptr_t *>(input->peekInternalHandle(nullptr))));
        DEBUG_BREAK_IF(!status);
    }

    if (input->peekSharedHandle() == false &&
        input->getDriverAllocatedCpuPtr() == nullptr &&
        input->fragmentsStorage.fragmentCount > 0) {
        cleanGraphicsMemoryCreatedFromHostPtr(gfxAllocation);
    } else {
        if (input->peekSharedHandle() || input->peekInternalHandle(nullptr) != 0) {
            [[maybe_unused]] auto status = tryDeferDeletions(nullptr, 0, input->resourceHandle, gfxAllocation->getRootDeviceIndex());
            DEBUG_BREAK_IF(!status);
        } else {
            for (auto handle : input->getHandles()) {
                [[maybe_unused]] auto status = tryDeferDeletions(&handle, 1, 0, gfxAllocation->getRootDeviceIndex());
                DEBUG_BREAK_IF(!status);
            }
        }

        alignedFreeWrapper(input->getDriverAllocatedCpuPtr());
    }
    if (input->getReservedAddressPtr()) {
        releaseReservedCpuAddressRange(input->getReservedAddressPtr(), input->getReservedAddressSize(), gfxAllocation->getRootDeviceIndex());
    }
    if (input->reservedGpuVirtualAddress) {
        getWddm(gfxAllocation->getRootDeviceIndex()).freeGpuVirtualAddress(input->reservedGpuVirtualAddress, input->reservedSizeForGpuVirtualAddress);
    }
    delete gfxAllocation;
}

void WddmMemoryManager::handleFenceCompletion(GraphicsAllocation *allocation) {
    auto wddmAllocation = static_cast<WddmAllocation *>(allocation);
    for (auto &engine : this->registeredEngines) {
        const auto lastFenceValue = wddmAllocation->getResidencyData().getFenceValueForContextId(engine.osContext->getContextId());
        if (lastFenceValue != 0u) {
            const auto &monitoredFence = static_cast<OsContextWin *>(engine.osContext)->getResidencyController().getMonitoredFence();
            const auto wddm = static_cast<OsContextWin *>(engine.osContext)->getWddm();
            wddm->waitFromCpu(lastFenceValue, monitoredFence);
        }
    }
}

bool WddmMemoryManager::tryDeferDeletions(const D3DKMT_HANDLE *handles, uint32_t allocationCount, D3DKMT_HANDLE resourceHandle, uint32_t rootDeviceIndex) {
    bool status = true;
    if (deferredDeleter) {
        deferredDeleter->deferDeletion(DeferrableDeletion::create(&getWddm(rootDeviceIndex), handles, allocationCount, resourceHandle));
    } else {
        status = getWddm(rootDeviceIndex).destroyAllocations(handles, allocationCount, resourceHandle);
    }
    return status;
}

bool WddmMemoryManager::isMemoryBudgetExhausted() const {
    for (auto &engine : this->registeredEngines) {
        if (static_cast<OsContextWin *>(engine.osContext)->getResidencyController().isMemoryBudgetExhausted()) {
            return true;
        }
    }
    return false;
}

bool WddmMemoryManager::validateAllocation(WddmAllocation *alloc) {
    if (alloc == nullptr)
        return false;
    auto size = alloc->getUnderlyingBufferSize();
    if (alloc->getGpuAddress() == 0u || size == 0 || (alloc->getDefaultHandle() == 0 && alloc->fragmentsStorage.fragmentCount == 0))
        return false;
    return true;
}

MemoryManager::AllocationStatus WddmMemoryManager::populateOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) {
    uint32_t allocatedFragmentIndexes[maxFragmentsCount];
    uint32_t allocatedFragmentsCounter = 0;

    auto hwInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();

    for (unsigned int i = 0; i < maxFragmentsCount; i++) {
        // If no fragment is present it means it already exists.
        if (!handleStorage.fragmentStorageData[i].osHandleStorage && handleStorage.fragmentStorageData[i].cpuPtr) {
            auto osHandle = new OsHandleWin();

            handleStorage.fragmentStorageData[i].osHandleStorage = osHandle;
            handleStorage.fragmentStorageData[i].residency = new ResidencyData(maxOsContextCount);

            osHandle->gmm = new Gmm(executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getGmmHelper(), handleStorage.fragmentStorageData[i].cpuPtr,
                                    handleStorage.fragmentStorageData[i].fragmentSize, 0u,
                                    CacheSettingsHelper::getGmmUsageType(AllocationType::EXTERNAL_HOST_PTR, false, *hwInfo), false, {}, true);
            allocatedFragmentIndexes[allocatedFragmentsCounter] = i;
            allocatedFragmentsCounter++;
        }
    }
    auto result = getWddm(rootDeviceIndex).createAllocationsAndMapGpuVa(handleStorage);

    if (result == STATUS_GRAPHICS_NO_VIDEO_MEMORY) {
        return AllocationStatus::InvalidHostPointer;
    }

    UNRECOVERABLE_IF(result != STATUS_SUCCESS);

    for (uint32_t i = 0; i < allocatedFragmentsCounter; i++) {
        hostPtrManager->storeFragment(rootDeviceIndex, handleStorage.fragmentStorageData[allocatedFragmentIndexes[i]]);
    }

    return AllocationStatus::Success;
}

void WddmMemoryManager::cleanOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) {

    D3DKMT_HANDLE handles[maxFragmentsCount] = {0};
    auto allocationCount = 0;

    for (unsigned int i = 0; i < maxFragmentsCount; i++) {
        if (handleStorage.fragmentStorageData[i].freeTheFragment) {
            handles[allocationCount++] = static_cast<OsHandleWin *>(handleStorage.fragmentStorageData[i].osHandleStorage)->handle;
            std::fill(handleStorage.fragmentStorageData[i].residency->resident.begin(), handleStorage.fragmentStorageData[i].residency->resident.end(), false);
        }
    }

    bool success = tryDeferDeletions(handles, allocationCount, 0, rootDeviceIndex);

    for (unsigned int i = 0; i < maxFragmentsCount; i++) {
        if (handleStorage.fragmentStorageData[i].freeTheFragment) {
            auto osHandle = static_cast<OsHandleWin *>(handleStorage.fragmentStorageData[i].osHandleStorage);
            if (success) {
                osHandle->handle = 0;
            }
            delete osHandle->gmm;
            delete osHandle;
            delete handleStorage.fragmentStorageData[i].residency;
        }
    }
}

void WddmMemoryManager::obtainGpuAddressFromFragments(WddmAllocation *allocation, OsHandleStorage &handleStorage) {
    if (this->force32bitAllocations && (handleStorage.fragmentCount > 0)) {
        auto hostPtr = allocation->getUnderlyingBuffer();
        auto fragment = hostPtrManager->getFragment({hostPtr, allocation->getRootDeviceIndex()});
        if (fragment && fragment->driverAllocation) {
            auto osHandle0 = static_cast<OsHandleWin *>(handleStorage.fragmentStorageData[0].osHandleStorage);

            auto gpuPtr = osHandle0->gpuPtr;
            for (uint32_t i = 1; i < handleStorage.fragmentCount; i++) {
                auto osHandle = static_cast<OsHandleWin *>(handleStorage.fragmentStorageData[i].osHandleStorage);
                if (osHandle->gpuPtr < gpuPtr) {
                    gpuPtr = osHandle->gpuPtr;
                }
            }
            allocation->setAllocationOffset(reinterpret_cast<uint64_t>(hostPtr) & MemoryConstants::pageMask);
            allocation->setGpuAddress(gpuPtr);
        }
    }
}

GraphicsAllocation *WddmMemoryManager::createGraphicsAllocation(OsHandleStorage &handleStorage, const AllocationData &allocationData) {
    auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
    auto canonizedAddress = gmmHelper->canonize(castToUint64(const_cast<void *>(allocationData.hostPtr)));
    auto allocation = new WddmAllocation(allocationData.rootDeviceIndex,
                                         1u, // numGmms
                                         allocationData.type,
                                         const_cast<void *>(allocationData.hostPtr),
                                         canonizedAddress,
                                         allocationData.size, nullptr, MemoryPool::System4KBPages,
                                         0u, // shareable
                                         maxOsContextCount);
    allocation->fragmentsStorage = handleStorage;
    obtainGpuAddressFromFragments(allocation, handleStorage);
    return allocation;
}

uint64_t WddmMemoryManager::getSystemSharedMemory(uint32_t rootDeviceIndex) {
    return getWddm(rootDeviceIndex).getSystemSharedMemory();
}

double WddmMemoryManager::getPercentOfGlobalMemoryAvailable(uint32_t rootDeviceIndex) {
    return 0.8;
}

AlignedMallocRestrictions *WddmMemoryManager::getAlignedMallocRestrictions() {
    return &mallocRestrictions;
}

bool WddmMemoryManager::createWddmAllocation(WddmAllocation *allocation, void *requiredGpuPtr) {
    auto status = createGpuAllocationsWithRetry(allocation);
    if (!status) {
        return false;
    }
    return mapGpuVirtualAddress(allocation, requiredGpuPtr);
}

bool WddmMemoryManager::mapGpuVaForOneHandleAllocation(WddmAllocation *allocation, const void *preferredGpuVirtualAddress) {
    D3DGPU_VIRTUAL_ADDRESS addressToMap = castToUint64(preferredGpuVirtualAddress);
    auto heapIndex = selectHeap(allocation, preferredGpuVirtualAddress != nullptr, is32bit || executionEnvironment.rootDeviceEnvironments[allocation->getRootDeviceIndex()]->isFullRangeSvm(), allocation->allocInFrontWindowPool);
    if (!executionEnvironment.rootDeviceEnvironments[allocation->getRootDeviceIndex()]->isFullRangeSvm() && !is32bit) {
        addressToMap = 0u;
    }
    if (allocation->reservedGpuVirtualAddress) {
        addressToMap = allocation->reservedGpuVirtualAddress;
    }
    auto gfxPartition = getGfxPartition(allocation->getRootDeviceIndex());
    if (allocation->allocInFrontWindowPool) {
        auto alignedSize = allocation->getAlignedSize();
        addressToMap = gfxPartition->heapAllocate(heapIndex, alignedSize);
    }

    D3DGPU_VIRTUAL_ADDRESS minimumAddress = gfxPartition->getHeapMinimalAddress(heapIndex);
    D3DGPU_VIRTUAL_ADDRESS maximumAddress = gfxPartition->getHeapLimit(heapIndex);
    auto status = getWddm(allocation->getRootDeviceIndex()).mapGpuVirtualAddress(allocation->getDefaultGmm(), allocation->getDefaultHandle(), minimumAddress, maximumAddress, addressToMap, allocation->getGpuAddressToModify());

    if (!status && deferredDeleter) {
        deferredDeleter->drain(true);
        status = getWddm(allocation->getRootDeviceIndex()).mapGpuVirtualAddress(allocation->getDefaultGmm(), allocation->getDefaultHandle(), minimumAddress, maximumAddress, addressToMap, allocation->getGpuAddressToModify());
    }
    if (!status) {
        if (allocation->reservedGpuVirtualAddress) {
            getWddm(allocation->getRootDeviceIndex()).freeGpuVirtualAddress(allocation->reservedGpuVirtualAddress, allocation->reservedSizeForGpuVirtualAddress);
        }
        getWddm(allocation->getRootDeviceIndex()).destroyAllocations(&allocation->getHandles()[0], allocation->getNumGmms(), allocation->resourceHandle);
        return false;
    }
    return true;
}

bool WddmMemoryManager::mapMultiHandleAllocationWithRetry(WddmAllocation *allocation, const void *preferredGpuVirtualAddress) {
    Wddm &wddm = getWddm(allocation->getRootDeviceIndex());
    auto gfxPartition = getGfxPartition(allocation->getRootDeviceIndex());

    auto alignedSize = allocation->getAlignedSize();
    uint64_t addressToMap = 0;
    HeapIndex heapIndex = preferredGpuVirtualAddress ? HeapIndex::HEAP_SVM : HeapIndex::HEAP_STANDARD64KB;

    if (preferredGpuVirtualAddress) {
        addressToMap = castToUint64(preferredGpuVirtualAddress);
        allocation->getGpuAddressToModify() = addressToMap;
    } else {
        allocation->reservedSizeForGpuVirtualAddress = alignUp(alignedSize, MemoryConstants::pageSize64k);
        allocation->reservedGpuVirtualAddress = wddm.reserveGpuVirtualAddress(gfxPartition->getHeapMinimalAddress(heapIndex), gfxPartition->getHeapLimit(heapIndex),
                                                                              allocation->reservedSizeForGpuVirtualAddress);
        auto gmmHelper = getGmmHelper(allocation->getRootDeviceIndex());
        allocation->getGpuAddressToModify() = gmmHelper->canonize(allocation->reservedGpuVirtualAddress);
        addressToMap = allocation->reservedGpuVirtualAddress;
    }

    for (auto currentHandle = 0u; currentHandle < allocation->getNumGmms(); currentHandle++) {
        uint64_t gpuAddress = 0;
        auto status = wddm.mapGpuVirtualAddress(allocation->getGmm(currentHandle), allocation->getHandles()[currentHandle],
                                                gfxPartition->getHeapMinimalAddress(heapIndex), gfxPartition->getHeapLimit(heapIndex), addressToMap, gpuAddress);

        if (!status && deferredDeleter) {
            deferredDeleter->drain(true);
            status = wddm.mapGpuVirtualAddress(allocation->getGmm(currentHandle), allocation->getHandles()[currentHandle],
                                               gfxPartition->getHeapMinimalAddress(heapIndex), gfxPartition->getHeapLimit(heapIndex), addressToMap, gpuAddress);
        }
        if (!status) {
            if (allocation->reservedGpuVirtualAddress) {
                wddm.freeGpuVirtualAddress(allocation->reservedGpuVirtualAddress, allocation->reservedSizeForGpuVirtualAddress);
            }
            wddm.destroyAllocations(&allocation->getHandles()[0], allocation->getNumGmms(), allocation->resourceHandle);
            return false;
        }
        gpuAddress = getGmmHelper(allocation->getRootDeviceIndex())->decanonize(gpuAddress);
        UNRECOVERABLE_IF(addressToMap != gpuAddress);
        addressToMap += allocation->getGmm(currentHandle)->gmmResourceInfo->getSizeAllocation();
    }
    return true;
}

bool WddmMemoryManager::createGpuAllocationsWithRetry(WddmAllocation *allocation) {
    for (auto handleId = 0u; handleId < allocation->getNumGmms(); handleId++) {
        auto gmm = allocation->getGmm(handleId);
        auto status = getWddm(allocation->getRootDeviceIndex()).createAllocation(gmm->gmmResourceInfo->getSystemMemPointer(), gmm, allocation->getHandleToModify(handleId), allocation->resourceHandle, allocation->getSharedHandleToModify());
        if (status == STATUS_GRAPHICS_NO_VIDEO_MEMORY && deferredDeleter) {
            deferredDeleter->drain(true);
            status = getWddm(allocation->getRootDeviceIndex()).createAllocation(gmm->gmmResourceInfo->getSystemMemPointer(), gmm, allocation->getHandleToModify(handleId), allocation->resourceHandle, allocation->getSharedHandleToModify());
        }
        if (status != STATUS_SUCCESS) {
            getWddm(allocation->getRootDeviceIndex()).destroyAllocations(&allocation->getHandles()[0], handleId, allocation->resourceHandle);
            return false;
        }
    }
    return true;
}

Wddm &WddmMemoryManager::getWddm(uint32_t rootDeviceIndex) const {
    return *this->executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->as<Wddm>();
}

void *WddmMemoryManager::reserveCpuAddressRange(size_t size, uint32_t rootDeviceIndex) {
    void *reservePtr = nullptr;
    getWddm(rootDeviceIndex).reserveValidAddressRange(size, reservePtr);
    return reservePtr;
}

void WddmMemoryManager::releaseReservedCpuAddressRange(void *reserved, size_t size, uint32_t rootDeviceIndex) {
    getWddm(rootDeviceIndex).releaseReservedAddress(reserved);
}

bool WddmMemoryManager::isCpuCopyRequired(const void *ptr) {
    // check if any device support local memory
    if (std::all_of(this->localMemorySupported.begin(), this->localMemorySupported.end(), [](bool value) { return !value; })) {
        return false;
    }

    // function checks what is the delta between reading from cachead memory
    // compare to reading from provided pointer
    // if value is above threshold, it means that pointer is uncached.
    constexpr auto slownessFactor = 5u;
    static volatile int64_t meassurmentOverhead = std::numeric_limits<int64_t>::max();
    static volatile int64_t fastestLocalRead = std::numeric_limits<int64_t>::max();
    static volatile int64_t max = std::numeric_limits<int64_t>::min();
    static volatile int64_t min = std::numeric_limits<int64_t>::max();

    // local variable that we will read for comparison
    volatile int cacheable = 1;
    // auto hostPtr = std::make_unique<volatile int[]>(0x13000 / sizeof(int));
    volatile int *localVariablePointer = &cacheable;
    volatile int *volatileInputPtr = (volatile int *)(ptr);

    volatile int64_t timestamp0, timestamp1, localVariableReadDelta, inputPointerReadDelta;

    // compute timing overhead
    _mm_lfence();
    timestamp0 = __rdtsc();
    _mm_lfence();
    timestamp1 = __rdtsc();
    _mm_lfence();

    if (timestamp1 - timestamp0 < meassurmentOverhead) {
        meassurmentOverhead = timestamp1 - timestamp0;
    }

    // dummy read
    // cacheable = *localVariablePointer;

    _mm_lfence();
    timestamp0 = __rdtsc();
    _mm_lfence();
    // do read
    cacheable = *localVariablePointer;
    _mm_lfence();
    timestamp1 = __rdtsc();
    _mm_lfence();
    localVariableReadDelta = timestamp1 - timestamp0 - meassurmentOverhead;
    if (localVariableReadDelta <= 0) {
        localVariableReadDelta = 1;
    }
    if (localVariableReadDelta < fastestLocalRead) {
        fastestLocalRead = localVariableReadDelta;
    }
    // dummy read
    // cacheable = *volatileInputPtr;

    _mm_lfence();
    timestamp0 = __rdtsc();
    _mm_lfence();
    cacheable = *volatileInputPtr;
    _mm_lfence();
    timestamp1 = __rdtsc();
    _mm_lfence();
    inputPointerReadDelta = timestamp1 - timestamp0 - meassurmentOverhead;
    if (inputPointerReadDelta <= 0) {
        inputPointerReadDelta = 1;
    }
    max = std::max(max, localVariableReadDelta);
    min = std::min(min, inputPointerReadDelta);
    // UNRECOVERABLE_IF(max > min);
    return inputPointerReadDelta > slownessFactor * fastestLocalRead;
}

bool WddmMemoryManager::copyMemoryToAllocation(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy) {
    if (graphicsAllocation->getUnderlyingBuffer()) {
        return MemoryManager::copyMemoryToAllocation(graphicsAllocation, destinationOffset, memoryToCopy, sizeToCopy);
    }
    return copyMemoryToAllocationBanks(graphicsAllocation, destinationOffset, memoryToCopy, sizeToCopy, maxNBitValue(graphicsAllocation->storageInfo.getNumBanks()));
}

bool WddmMemoryManager::copyMemoryToAllocationBanks(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy, DeviceBitfield handleMask) {
    if (MemoryPoolHelper::isSystemMemoryPool(graphicsAllocation->getMemoryPool())) {
        return false;
    }
    auto &wddm = getWddm(graphicsAllocation->getRootDeviceIndex());
    auto wddmAllocation = static_cast<WddmAllocation *>(graphicsAllocation);
    for (auto handleId = 0u; handleId < graphicsAllocation->storageInfo.getNumBanks(); handleId++) {
        if (!handleMask.test(handleId)) {
            continue;
        }
        auto ptr = wddm.lockResource(wddmAllocation->getHandles()[handleId], wddmAllocation->needsMakeResidentBeforeLock, wddmAllocation->getAlignedSize());
        if (!ptr) {
            return false;
        }
        memcpy_s(ptrOffset(ptr, destinationOffset), graphicsAllocation->getUnderlyingBufferSize() - destinationOffset, memoryToCopy, sizeToCopy);
        wddm.unlockResource(wddmAllocation->getHandles()[handleId]);
    }
    return true;
}

void createColouredGmms(GmmHelper *gmmHelper, WddmAllocation &allocation, const StorageInfo &storageInfo, bool compression) {
    auto remainingSize = allocation.getAlignedSize();
    auto handles = storageInfo.getNumBanks();
    /* This logic is to colour resource as equally as possible.
    Divide size by number of devices and align result up to 64kb page, then subtract it from whole size and allocate it on the first tile. First tile has it's chunk.
    In the following iteration divide rest of a size by remaining devices and again subtract it.
    Notice that if allocation size (in pages) is not divisible by 4 then remainder can be equal to 1,2,3 and by using this algorithm it can be spread efficiently.

    For example: 18 pages allocation and 4 devices. Page size is 64kb.
    Divide by 4 and align up to page size and result is 5 pages. After subtract, remaining size is 13 pages.
    Now divide 13 by 3 and align up - result is 5 pages. After subtract, remaining size is 8 pages.
    Divide 8 by 2 - result is 4 pages.
    In last iteration remaining 4 pages go to last tile.
    18 pages is coloured to (5, 5, 4, 4).

    It was tested and doesn't require any debug*/
    for (auto handleId = 0u; handleId < handles; handleId++) {
        auto currentSize = alignUp(remainingSize / (handles - handleId), MemoryConstants::pageSize64k);
        remainingSize -= currentSize;
        StorageInfo limitedStorageInfo = storageInfo;
        limitedStorageInfo.memoryBanks &= static_cast<uint32_t>(1u << handleId);
        auto gmm = new Gmm(gmmHelper,
                           nullptr,
                           currentSize,
                           0u,
                           CacheSettingsHelper::getGmmUsageType(allocation.getAllocationType(), false, *gmmHelper->getHardwareInfo()),
                           compression,
                           limitedStorageInfo, true);
        allocation.setGmm(gmm, handleId);
    }
}

void fillGmmsInAllocation(GmmHelper *gmmHelper, WddmAllocation *allocation, const StorageInfo &storageInfo) {
    for (auto handleId = 0u; handleId < storageInfo.getNumBanks(); handleId++) {
        StorageInfo limitedStorageInfo = storageInfo;
        limitedStorageInfo.memoryBanks &= static_cast<uint32_t>(1u << handleId);
        limitedStorageInfo.pageTablesVisibility &= static_cast<uint32_t>(1u << handleId);
        auto gmm = new Gmm(gmmHelper, nullptr, allocation->getAlignedSize(), 0u,
                           CacheSettingsHelper::getGmmUsageType(allocation->getAllocationType(), false, *gmmHelper->getHardwareInfo()), false, limitedStorageInfo, true);
        allocation->setGmm(gmm, handleId);
    }
}

void splitGmmsInAllocation(GmmHelper *gmmHelper, WddmAllocation *wddmAllocation, size_t alignment, size_t chunkSize, StorageInfo &storageInfo) {
    auto sizeRemaining = wddmAllocation->getAlignedSize();
    for (auto gmmId = 0u; gmmId < wddmAllocation->getNumGmms(); ++gmmId) {
        auto size = sizeRemaining > chunkSize ? chunkSize : sizeRemaining;
        auto gmm = new Gmm(gmmHelper, nullptr, size, alignment,
                           CacheSettingsHelper::getGmmUsageType(wddmAllocation->getAllocationType(), false, *gmmHelper->getHardwareInfo()), false, storageInfo, true);
        wddmAllocation->setGmm(gmm, gmmId);
        sizeRemaining -= size;
    }
    storageInfo.multiStorage = true;
}

uint32_t getPriorityForAllocation(AllocationType allocationType) {
    if (GraphicsAllocation::isIsaAllocationType(allocationType) ||
        allocationType == AllocationType::COMMAND_BUFFER ||
        allocationType == AllocationType::INTERNAL_HEAP ||
        allocationType == AllocationType::LINEAR_STREAM) {
        return DXGI_RESOURCE_PRIORITY_HIGH;
    }
    return DXGI_RESOURCE_PRIORITY_NORMAL;
}

GraphicsAllocation *WddmMemoryManager::allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) {
    status = AllocationStatus::RetryInNonDevicePool;

    if (!this->localMemorySupported[allocationData.rootDeviceIndex] ||
        allocationData.flags.useSystemMemory ||
        (allocationData.flags.allow32Bit && this->force32bitAllocations)) {
        return nullptr;
    }

    auto gmmClientContext = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmClientContext();
    auto gmmHelper = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper();

    std::unique_ptr<Gmm> gmm;
    size_t sizeAligned = 0;
    size_t alignment = 0;
    auto numBanks = allocationData.storageInfo.getNumBanks();
    bool singleBankAllocation = numBanks == 1;
    if (allocationData.type == AllocationType::IMAGE ||
        allocationData.type == AllocationType::SHARED_RESOURCE_COPY) {
        allocationData.imgInfo->useLocalMemory = true;
        gmm = std::make_unique<Gmm>(gmmHelper, *allocationData.imgInfo, allocationData.storageInfo, allocationData.flags.preferCompressed);
        alignment = MemoryConstants::pageSize64k;
        sizeAligned = allocationData.imgInfo->size;
    } else {
        alignment = alignmentSelector.selectAlignment(allocationData.size).alignment;
        sizeAligned = alignUp(allocationData.size, alignment);

        if (singleBankAllocation) {
            gmm = std::make_unique<Gmm>(gmmHelper,
                                        nullptr,
                                        sizeAligned,
                                        alignment,
                                        CacheSettingsHelper::getGmmUsageType(allocationData.type, !!allocationData.flags.uncacheable, *gmmClientContext->getHardwareInfo()),
                                        allocationData.flags.preferCompressed,
                                        allocationData.storageInfo,
                                        true);
        }
    }

    const auto chunkSize = alignDown(getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod::AllocateByKmd), alignment);
    const size_t numGmms = (static_cast<uint64_t>(sizeAligned) + chunkSize - 1) / chunkSize;

    auto wddmAllocation = std::make_unique<WddmAllocation>(allocationData.rootDeviceIndex, singleBankAllocation ? numGmms : numBanks,
                                                           allocationData.type, nullptr, 0, sizeAligned, nullptr, MemoryPool::LocalMemory, allocationData.flags.shareable, maxOsContextCount);
    if (singleBankAllocation) {
        if (numGmms > 1) {
            splitGmmsInAllocation(gmmHelper, wddmAllocation.get(), alignment, chunkSize, const_cast<StorageInfo &>(allocationData.storageInfo));
        } else {
            wddmAllocation->setDefaultGmm(gmm.release());
        }
    } else if (allocationData.storageInfo.multiStorage) {
        createColouredGmms(gmmHelper, *wddmAllocation, allocationData.storageInfo, allocationData.flags.preferCompressed);
    } else {
        fillGmmsInAllocation(gmmHelper, wddmAllocation.get(), allocationData.storageInfo);
    }
    wddmAllocation->storageInfo = allocationData.storageInfo;
    wddmAllocation->setFlushL3Required(allocationData.flags.flushL3);
    wddmAllocation->needsMakeResidentBeforeLock = true;

    void *requiredGpuVa = nullptr;
    if (allocationData.type == AllocationType::SVM_GPU) {
        requiredGpuVa = const_cast<void *>(allocationData.hostPtr);
    }

    auto &wddm = getWddm(allocationData.rootDeviceIndex);

    if (is32bit && executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->isFullRangeSvm()) {
        if (allocationData.type == AllocationType::BUFFER ||
            allocationData.type == AllocationType::SHARED_BUFFER ||
            allocationData.type == AllocationType::SCRATCH_SURFACE ||
            allocationData.type == AllocationType::LINEAR_STREAM ||
            allocationData.type == AllocationType::PRIVATE_SURFACE) {
            // add 2MB padding to make sure there are no overlaps between system and local memory
            size_t reserveSizeAligned = sizeAligned + 2 * MemoryConstants::megaByte;
            wddm.reserveValidAddressRange(reserveSizeAligned, requiredGpuVa);
            wddmAllocation->setReservedAddressRange(requiredGpuVa, reserveSizeAligned);
            requiredGpuVa = alignUp(requiredGpuVa, 2 * MemoryConstants::megaByte);
        }
    }

    if (!createWddmAllocation(wddmAllocation.get(), requiredGpuVa)) {
        for (auto handleId = 0u; handleId < allocationData.storageInfo.getNumBanks(); handleId++) {
            delete wddmAllocation->getGmm(handleId);
        }
        status = AllocationStatus::Error;
        return nullptr;
    }

    auto handles = wddmAllocation->getHandles();

    if (!wddm.setAllocationPriority(handles.data(), static_cast<UINT>(handles.size()), getPriorityForAllocation(allocationData.type))) {
        for (auto handleId = 0u; handleId < allocationData.storageInfo.getNumBanks(); handleId++) {
            delete wddmAllocation->getGmm(handleId);
        }
        status = AllocationStatus::Error;
        return nullptr;
    }
    if (allocationData.flags.requiresCpuAccess) {
        wddmAllocation->setCpuAddress(lockResource(wddmAllocation.get()));
    }
    if (heapAssigner.useInternal32BitHeap(allocationData.type)) {
        auto gmmHelper = getGmmHelper(wddmAllocation->getRootDeviceIndex());
        wddmAllocation->setGpuBaseAddress(gmmHelper->canonize(getInternalHeapBaseAddress(wddmAllocation->getRootDeviceIndex(), true)));
    }

    status = AllocationStatus::Success;
    return wddmAllocation.release();
}

bool mapTileInstancedAllocation(WddmAllocation *allocation, const void *requiredPtr, Wddm *wddm, HeapIndex heapIndex, GfxPartition &gfxPartition);

bool WddmMemoryManager::mapGpuVirtualAddress(WddmAllocation *allocation, const void *requiredPtr) {
    if (allocation->getNumGmms() > 1) {
        if (allocation->storageInfo.tileInstanced) {
            return mapTileInstancedAllocation(allocation,
                                              requiredPtr,
                                              &getWddm(allocation->getRootDeviceIndex()),
                                              selectHeap(allocation, requiredPtr != nullptr, executionEnvironment.rootDeviceEnvironments[allocation->getRootDeviceIndex()]->isFullRangeSvm(), allocation->allocInFrontWindowPool),
                                              *getGfxPartition(allocation->getRootDeviceIndex()));
        } else if (allocation->storageInfo.multiStorage) {
            return mapMultiHandleAllocationWithRetry(allocation, requiredPtr);
        }
    } else if (allocation->getAllocationType() == AllocationType::WRITE_COMBINED) {
        requiredPtr = lockResource(allocation);
        allocation->setCpuAddress(const_cast<void *>(requiredPtr));
    }
    return mapGpuVaForOneHandleAllocation(allocation, requiredPtr);
}

uint64_t WddmMemoryManager::getLocalMemorySize(uint32_t rootDeviceIndex, uint32_t deviceBitfield) {
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
    uint64_t subDevicesCount = HwHelper::getSubDevicesCount(hwInfo);

    auto singleRegionSize = getWddm(rootDeviceIndex).getDedicatedVideoMemory() / subDevicesCount;

    return singleRegionSize * DeviceBitfield(deviceBitfield).count();
}

void WddmMemoryManager::registerAllocationInOs(GraphicsAllocation *allocation) {
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[allocation->getRootDeviceIndex()].get();
    auto debuggerL0 = static_cast<DebuggerL0 *>(rootDeviceEnvironment->debugger.get());

    if (debuggerL0) {
        debuggerL0->registerAllocationType(allocation);
    }
}

} // namespace NEO
