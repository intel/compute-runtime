/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm_memory_manager.h"

#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/deferred_deleter_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/heap_assigner.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/deferrable_deletion.h"
#include "shared/source/memory_manager/deferred_deleter.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/memory_manager/host_ptr_manager.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/windows/dxgi_wrapper.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/sys_calls_wrapper.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm_allocation.h"
#include "shared/source/os_interface/windows/wddm_residency_allocations_container.h"
#include "shared/source/os_interface/windows/wddm_residency_controller.h"
#include "shared/source/utilities/heap_allocator.h"
#include "shared/source/utilities/logger_neo_only.h"

#include <algorithm>
#include <emmintrin.h>

namespace NEO {

using AllocationStatus = MemoryManager::AllocationStatus;

template void WddmMemoryManager::adjustGpuPtrToHostAddressSpace<is32bit>(WddmAllocation &wddmAllocation, void *&requiredGpuVa);

WddmMemoryManager::~WddmMemoryManager() = default;

WddmMemoryManager::WddmMemoryManager(ExecutionEnvironment &executionEnvironment) : MemoryManager(executionEnvironment) {
    asyncDeleterEnabled = isDeferredDeleterEnabled();
    if (asyncDeleterEnabled)
        deferredDeleter = createDeferredDeleter();
    mallocRestrictions.minAddress = 0u;

    usmDeviceAllocationMode = toLocalMemAllocationMode(debugManager.flags.NEO_LOCAL_MEMORY_ALLOCATION_MODE.get());

    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < gfxPartitions.size(); ++rootDeviceIndex) {
        mallocRestrictions.minAddress = std::max(mallocRestrictions.minAddress, getWddm(rootDeviceIndex).getWddmMinAddress());
        getWddm(rootDeviceIndex).initGfxPartition(*getGfxPartition(rootDeviceIndex), rootDeviceIndex, gfxPartitions.size(), heapAssigners[rootDeviceIndex]->apiAllowExternalHeapForSshAndDsh);
        isLocalMemoryUsedForIsa(rootDeviceIndex);
        if (isLocalOnlyAllocationMode()) {
            getGmmHelper(rootDeviceIndex)->setLocalOnlyAllocationMode(true);
        }
    }

    alignmentSelector.addCandidateAlignment(MemoryConstants::pageSize64k, true, AlignmentSelector::anyWastage);
    if (debugManager.flags.AlignLocalMemoryVaTo2MB.get() != 0) {
        constexpr static float maxWastage2Mb = 0.1f;
        alignmentSelector.addCandidateAlignment(MemoryConstants::pageSize2M, false, maxWastage2Mb);
    }
    const size_t customAlignment = static_cast<size_t>(debugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.get());
    if (customAlignment > 0) {
        alignmentSelector.addCandidateAlignment(customAlignment, false, AlignmentSelector::anyWastage);
    }
    osMemory = OSMemory::create();

    initialized = true;
}

GfxMemoryAllocationMethod WddmMemoryManager::getPreferredAllocationMethod(const AllocationProperties &allocationProperties) const {
    if (debugManager.flags.ForcePreferredAllocationMethod.get() != -1) {
        return static_cast<GfxMemoryAllocationMethod>(debugManager.flags.ForcePreferredAllocationMethod.get());
    }
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[allocationProperties.rootDeviceIndex].get();
    UNRECOVERABLE_IF(!rootDeviceEnvironment);
    auto &productHelper = rootDeviceEnvironment->getHelper<ProductHelper>();
    auto preference = productHelper.getPreferredAllocationMethod(allocationProperties.allocationType);
    if (preference) {
        return *preference;
    }

    return preferredAllocationMethod;
}

bool WddmMemoryManager::unMapPhysicalDeviceMemoryFromVirtualMemory(GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize, OsContext *osContext, uint32_t rootDeviceIndex) {
    const auto wddm = static_cast<OsContextWin *>(osContext)->getWddm();
    wddm->freeGpuVirtualAddress(gpuRange, bufferSize);
    auto gfxPartition = getGfxPartition(rootDeviceIndex);
    uint64_t reservedAddress = 0u;
    auto status = wddm->reserveGpuVirtualAddress(gpuRange, gfxPartition->getHeapMinimalAddress(HeapIndex::heapStandard64KB), gfxPartition->getHeapLimit(HeapIndex::heapStandard64KB), bufferSize, &reservedAddress);
    if (status != STATUS_SUCCESS) {
        return false;
    }
    physicalAllocation->setCpuPtrAndGpuAddress(nullptr, 0u);
    physicalAllocation->setReservedAddressRange(nullptr, 0u);
    WddmAllocation *wddmAllocation = reinterpret_cast<WddmAllocation *>(physicalAllocation);
    wddmAllocation->setMappedPhysicalMemoryReservation(false);
    return true;
}

bool WddmMemoryManager::unMapPhysicalHostMemoryFromVirtualMemory(MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) {
    return true;
}

bool WddmMemoryManager::mapPhysicalDeviceMemoryToVirtualMemory(GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) {
    WddmAllocation *wddmAllocation = reinterpret_cast<WddmAllocation *>(physicalAllocation);
    auto decanonizedAddress = getGmmHelper(physicalAllocation->getRootDeviceIndex())->decanonize(gpuRange);
    wddmAllocation->setMappedPhysicalMemoryReservation(mapGpuVirtualAddress(wddmAllocation, reinterpret_cast<void *>(decanonizedAddress)));
    physicalAllocation->setCpuPtrAndGpuAddress(nullptr, gpuRange);
    physicalAllocation->setReservedAddressRange(reinterpret_cast<void *>(gpuRange), bufferSize);
    return wddmAllocation->isMappedPhysicalMemoryReservation();
}

bool WddmMemoryManager::mapPhysicalHostMemoryToVirtualMemory(RootDeviceIndicesContainer &rootDeviceIndices, MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) {
    return false;
}

GraphicsAllocation *WddmMemoryManager::allocatePhysicalDeviceMemory(const AllocationData &allocationData, AllocationStatus &status) {
    auto &productHelper = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHelper<ProductHelper>();
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper()->getHardwareInfo();
    StorageInfo systemMemoryStorageInfo = {};
    systemMemoryStorageInfo.isLockable = allocationData.storageInfo.isLockable;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::make_unique<Gmm>(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(), nullptr, allocationData.size, 0u,
                                     CacheSettingsHelper::getGmmUsageType(allocationData.type, !!allocationData.flags.uncacheable, productHelper, hwInfo), systemMemoryStorageInfo, gmmRequirements);
    auto allocation = std::make_unique<WddmAllocation>(allocationData.rootDeviceIndex,
                                                       1u, // numGmms
                                                       allocationData.type, nullptr, 0, allocationData.size, nullptr,
                                                       MemoryPool::systemCpuInaccessible, allocationData.flags.shareable, maxOsContextCount);
    allocation->setShareableWithoutNTHandle(allocationData.flags.shareableWithoutNTHandle);
    allocation->setDefaultGmm(gmm.get());
    if (!createPhysicalAllocation(allocation.get())) {
        return nullptr;
    }

    gmm.release();
    status = AllocationStatus::Success;
    return allocation.release();
}

GraphicsAllocation *WddmMemoryManager::allocateMemoryByKMD(const AllocationData &allocationData) {
    if (allocationData.size > getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod::allocateByKmd)) {
        return allocateHugeGraphicsMemory(allocationData, false);
    }

    auto &productHelper = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHelper<ProductHelper>();
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper()->getHardwareInfo();
    StorageInfo systemMemoryStorageInfo = {};
    systemMemoryStorageInfo.isLockable = allocationData.storageInfo.isLockable;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = allocationData.flags.preferCompressed;
    auto gmm = std::make_unique<Gmm>(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(), allocationData.hostPtr, allocationData.size, allocationData.alignment,
                                     CacheSettingsHelper::getGmmUsageType(allocationData.type, !!allocationData.flags.uncacheable, productHelper, hwInfo), systemMemoryStorageInfo, gmmRequirements);
    auto allocation = std::make_unique<WddmAllocation>(allocationData.rootDeviceIndex,
                                                       1u, // numGmms
                                                       allocationData.type, nullptr, 0, allocationData.size, nullptr,
                                                       MemoryPool::systemCpuInaccessible, allocationData.flags.shareable, maxOsContextCount);
    allocation->setShareableWithoutNTHandle(allocationData.flags.shareableWithoutNTHandle);
    allocation->setDefaultGmm(gmm.get());
    void *requiredGpuVa = nullptr;
    adjustGpuPtrToHostAddressSpace(*allocation.get(), requiredGpuVa);
    if (!createWddmAllocation(allocation.get(), requiredGpuVa)) {
        return nullptr;
    }

    gmm.release();

    return allocation.release();
}

GraphicsAllocation *WddmMemoryManager::allocateGraphicsMemoryForImageImpl(const AllocationData &allocationData, std::unique_ptr<Gmm> gmm) {
    auto allocation = std::make_unique<WddmAllocation>(allocationData.rootDeviceIndex,
                                                       1u, // numGmms
                                                       allocationData.type, nullptr, 0, allocationData.imgInfo->size,
                                                       nullptr, MemoryPool::systemCpuInaccessible,
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
    AllocationData allocationData64KbAlignment = allocationData;
    allocationData64KbAlignment.alignment = MemoryConstants::pageSize64k > allocationData64KbAlignment.alignment ? MemoryConstants::pageSize64k : allocationData64KbAlignment.alignment;
    return allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(allocationData64KbAlignment, true);
}

GraphicsAllocation *WddmMemoryManager::allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(const AllocationData &allocationData, bool allowLargePages) {
    if (allocationData.alignment < MemoryConstants::pageSize64k) {
        allowLargePages = false;
    }

    size_t sizeAligned = alignUp(allocationData.size, allowLargePages ? MemoryConstants::pageSize64k : MemoryConstants::pageSize);
    if (sizeAligned > getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod::allocateByKmd)) {
        const bool isBufferHostMemory = allocationData.type == NEO::AllocationType::bufferHostMemory;
        return allocateHugeGraphicsMemory(allocationData, isBufferHostMemory);
    }

    // align gpu address of device part of usm shared allocation to 64kb for WSL2
    auto alignGpuAddressTo64KB = allocationData.allocationMethod == GfxMemoryAllocationMethod::allocateByKmd && allocationData.makeGPUVaDifferentThanCPUPtr;

    if (alignGpuAddressTo64KB) {
        sizeAligned = sizeAligned + allocationData.alignment;
    }

    auto wddmAllocation = std::make_unique<WddmAllocation>(allocationData.rootDeviceIndex,
                                                           1u, // numGmms
                                                           allocationData.type, nullptr, 0,
                                                           sizeAligned, nullptr, allowLargePages ? MemoryPool::system64KBPages : MemoryPool::system4KBPages,
                                                           0u, // shareable
                                                           maxOsContextCount);

    auto &productHelper = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHelper<ProductHelper>();
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper()->getHardwareInfo();
    auto storageInfo = allocationData.storageInfo;

    if (!allocationData.flags.preferCompressed) {
        storageInfo.isLockable = true;
    }

    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = allowLargePages;
    gmmRequirements.preferCompressed = allocationData.flags.preferCompressed;
    if (productHelper.overrideAllocationCpuCacheable(allocationData)) {
        gmmRequirements.overriderCacheable.enableOverride = true;
        gmmRequirements.overriderCacheable.value = true;
    }

    auto gmm = new Gmm(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(), nullptr,
                       sizeAligned, allocationData.alignment,
                       CacheSettingsHelper::getGmmUsageType(wddmAllocation->getAllocationType(), !!allocationData.flags.uncacheable, productHelper, hwInfo),
                       storageInfo,
                       gmmRequirements);
    wddmAllocation->setDefaultGmm(gmm);
    wddmAllocation->setFlushL3Required(allocationData.flags.flushL3);
    wddmAllocation->storageInfo = storageInfo;

    if (!getWddm(allocationData.rootDeviceIndex).createAllocation(gmm, wddmAllocation->getHandleToModify(0u))) {
        delete gmm;
        return nullptr;
    }

    auto cpuPtr = gmm->isCompressionEnabled() ? nullptr : lockResource(wddmAllocation.get());

    [[maybe_unused]] auto status = true;

    if ((!(alignGpuAddressTo64KB) && executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHardwareInfo()->capabilityTable.gpuAddressSpace >= MemoryConstants::max64BitAppAddress) || is32bit) {
        void *requiredGpuVa = cpuPtr;
        if (!cpuPtr) {
            adjustGpuPtrToHostAddressSpace(*wddmAllocation.get(), requiredGpuVa);
        }
        status = mapGpuVirtualAddress(wddmAllocation.get(), requiredGpuVa);
    } else {
        status = mapGpuVirtualAddress(wddmAllocation.get(), nullptr);

        if (alignGpuAddressTo64KB) {
            void *tempCPUPtr = cpuPtr;
            cpuPtr = alignUp(cpuPtr, std::max(allocationData.alignment, MemoryConstants::pageSize64k));
            wddmAllocation->setGpuAddress(wddmAllocation->getGpuAddress() + ptrDiff(cpuPtr, tempCPUPtr));
        }
    }
    DEBUG_BREAK_IF(!status);
    wddmAllocation->setCpuAddress(cpuPtr);
    return wddmAllocation.release();
}

NTSTATUS WddmMemoryManager::createInternalNTHandle(D3DKMT_HANDLE *resourceHandle, HANDLE *ntHandle, uint32_t rootDeviceIndex) {
    return getWddm(rootDeviceIndex).createNTHandle(resourceHandle, ntHandle);
}

GraphicsAllocation *WddmMemoryManager::allocateHugeGraphicsMemory(const AllocationData &allocationData, bool sharedVirtualAddress) {
    void *hostPtr = nullptr, *alignedPtr = nullptr;
    size_t alignedSize = 0;
    bool uncacheable = allocationData.flags.uncacheable;
    auto memoryPool = MemoryPool::system64KBPages;

    if (allocationData.hostPtr) {
        hostPtr = const_cast<void *>(allocationData.hostPtr);
        alignedSize = alignSizeWholePage(hostPtr, allocationData.size);
        alignedPtr = alignDown(hostPtr, MemoryConstants::pageSize);
        memoryPool = MemoryPool::system4KBPages;
    } else {
        alignedSize = alignUp(allocationData.size, MemoryConstants::pageSize64k);
        uncacheable = false;
        hostPtr = alignedPtr = allocateSystemMemory(alignedSize, MemoryConstants::pageSize2M);
        if (nullptr == hostPtr) {
            return nullptr;
        }
    }

    auto chunkSize = getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod::useUmdSystemPtr);
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

    auto &productHelper = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHelper<ProductHelper>();
    auto sizeRemaining = alignedSize;
    for (auto gmmId = 0u; gmmId < numGmms; ++gmmId) {
        auto size = sizeRemaining > chunkSize ? chunkSize : sizeRemaining;
        GmmRequirements gmmRequirements{};
        gmmRequirements.allowLargePages = true;
        gmmRequirements.preferCompressed = false;
        auto gmm = new Gmm(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(),
                           static_cast<char *>(alignedPtr) + gmmId * chunkSize, size, 0u,
                           CacheSettingsHelper::getGmmUsageType(wddmAllocation->getAllocationType(), !!uncacheable, productHelper, gmmHelper->getHardwareInfo()), {}, gmmRequirements);
        wddmAllocation->setGmm(gmm, gmmId);
        sizeRemaining -= size;
    }

    wddmAllocation->storageInfo.multiStorage = true;
    auto wddmAllocGpuPtr = sharedVirtualAddress ? hostPtr : nullptr;

    void *mapPtr = wddmAllocation->getAlignedCpuPtr();
    if (allocationData.type == AllocationType::svmCpu) {
        // add  padding in case mapPtr is not aligned
        size_t reserveSizeAligned = alignedSize + allocationData.alignment;
        bool ret = getWddm(wddmAllocation->getRootDeviceIndex()).reserveValidAddressRange(reserveSizeAligned, mapPtr);
        if (!ret) {
            for (auto gmmId = 0u; gmmId < wddmAllocation->getNumGmms(); ++gmmId) {
                delete wddmAllocation->getGmm(gmmId);
            }
            freeSystemMemory(wddmAllocation->getDriverAllocatedCpuPtr());
            return nullptr;
        }
        wddmAllocation->setReservedAddressRange(mapPtr, reserveSizeAligned);
        size_t newAlignment = allocationData.alignment ? alignUp(allocationData.alignment, MemoryConstants::pageSize64k) : MemoryConstants::pageSize64k;
        mapPtr = alignUp(mapPtr, newAlignment);
        wddmAllocGpuPtr = mapPtr;
    }

    if (!createWddmAllocation(wddmAllocation.get(), wddmAllocGpuPtr)) {
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
    if ((allocationData.allocationMethod == GfxMemoryAllocationMethod::useUmdSystemPtr) || (requiresNonStandardAlignment && allocationData.forceKMDAllocation == false)) {
        return allocateSystemMemoryAndCreateGraphicsAllocationFromIt(allocationData);
    } else {
        return allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(allocationData, NEO::OSInterface::osEnabled64kbPages);
    }
}

GraphicsAllocation *WddmMemoryManager::allocateSystemMemoryAndCreateGraphicsAllocationFromIt(const AllocationData &allocationData) {
    size_t newAlignment = allocationData.alignment ? alignUp(allocationData.alignment, MemoryConstants::pageSize) : MemoryConstants::pageSize;
    size_t sizeAligned = allocationData.size ? alignUp(allocationData.size, MemoryConstants::pageSize) : MemoryConstants::pageSize;
    if (sizeAligned > getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod::useUmdSystemPtr)) {
        return allocateHugeGraphicsMemory(allocationData, true);
    }
    void *pSysMem = allocateSystemMemory(sizeAligned, newAlignment);
    zeroCpuMemoryIfRequested(allocationData, pSysMem, sizeAligned);

    Gmm *gmm = nullptr;

    if (pSysMem == nullptr) {
        return nullptr;
    }

    auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
    auto canonizedAddress = gmmHelper->canonize(castToUint64(pSysMem));
    auto wddmAllocation = std::make_unique<WddmAllocation>(allocationData.rootDeviceIndex,
                                                           1u, // numGmms
                                                           allocationData.type, pSysMem, canonizedAddress, sizeAligned,
                                                           nullptr, MemoryPool::system4KBPages,
                                                           0u, // shareable
                                                           maxOsContextCount);
    wddmAllocation->setDriverAllocatedCpuPtr(pSysMem);
    auto &productHelper = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHelper<ProductHelper>();

    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = allocationData.flags.preferCompressed;

    gmm = new Gmm(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(), pSysMem, sizeAligned, 0u,
                  CacheSettingsHelper::getGmmUsageType(wddmAllocation->getAllocationType(), !!allocationData.flags.uncacheable, productHelper, gmmHelper->getHardwareInfo()),
                  allocationData.storageInfo, gmmRequirements);

    wddmAllocation->setDefaultGmm(gmm);
    void *mapPtr = wddmAllocation->getAlignedCpuPtr();
    if (allocationData.type == AllocationType::svmCpu) {
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
    if (alignedSize > getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod::useUmdSystemPtr)) {
        return allocateHugeGraphicsMemory(allocationData, false);
    }

    auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
    auto canonizedAddress = gmmHelper->canonize(castToUint64(const_cast<void *>(allocationData.hostPtr)));
    auto wddmAllocation = std::make_unique<WddmAllocation>(allocationData.rootDeviceIndex,
                                                           1u, // numGmms
                                                           allocationData.type, const_cast<void *>(allocationData.hostPtr), canonizedAddress,
                                                           allocationData.size, nullptr, MemoryPool::system4KBPages,
                                                           0u, // shareable
                                                           maxOsContextCount);

    auto alignedPtr = alignDown(allocationData.hostPtr, MemoryConstants::pageSize);
    auto offsetInPage = ptrDiff(allocationData.hostPtr, alignedPtr);
    wddmAllocation->setAllocationOffset(offsetInPage);
    auto &productHelper = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHelper<ProductHelper>();

    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    if (productHelper.overrideAllocationCpuCacheable(allocationData)) {
        gmmRequirements.overriderCacheable.enableOverride = true;
        gmmRequirements.overriderCacheable.value = true;
    }

    auto gmm = new Gmm(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(), alignedPtr, alignedSize, 0u,
                       CacheSettingsHelper::getGmmUsageTypeForUserPtr(allocationData.flags.flushL3, allocationData.hostPtr, allocationData.size, productHelper), {}, gmmRequirements);

    wddmAllocation->setDefaultGmm(gmm);

    if (!createWddmAllocation(wddmAllocation.get(), nullptr)) {
        delete gmm;
        return nullptr;
    }

    return wddmAllocation.release();
}

GraphicsAllocation *WddmMemoryManager::allocateGraphicsMemoryWithHostPtr(const AllocationData &allocationData) {
    if (allocationData.size > getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod::useUmdSystemPtr)) {
        bool isUSMHostAllocation = allocationData.flags.isUSMHostAllocation;
        return allocateHugeGraphicsMemory(allocationData, isUSMHostAllocation);
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
                                             reserve, MemoryPool::system4KBPages,
                                             0u, // shareable
                                             maxOsContextCount);
        allocation->setAllocationOffset(offset);

        auto &productHelper = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHelper<ProductHelper>();

        GmmRequirements gmmRequirements{};
        gmmRequirements.allowLargePages = true;
        gmmRequirements.preferCompressed = false;

        Gmm *gmm = new Gmm(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(), ptrAligned, sizeAligned, 0u,
                           CacheSettingsHelper::getGmmUsageType(allocation->getAllocationType(), !!allocationData.flags.uncacheable, productHelper, gmmHelper->getHardwareInfo()), {}, gmmRequirements);
        allocation->setDefaultGmm(gmm);
        if (createWddmAllocation(allocation, reserve)) {
            return allocation;
        }
        freeGraphicsMemory(allocation);
        return nullptr;
    }
    return MemoryManager::allocateGraphicsMemoryWithHostPtr(allocationData);
}

GraphicsAllocation *WddmMemoryManager::allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData) {
    Gmm *gmm = nullptr;
    const void *ptrAligned = nullptr;
    size_t sizeAligned = allocationData.size;
    void *pSysMem = nullptr;
    size_t offset = 0;

    if (allocationData.hostPtr) {
        ptrAligned = alignDown(allocationData.hostPtr, MemoryConstants::allocationAlignment);
        sizeAligned = alignSizeWholePage(allocationData.hostPtr, sizeAligned);
        offset = ptrDiff(allocationData.hostPtr, ptrAligned);
    } else if (allocationData.allocationMethod == GfxMemoryAllocationMethod::useUmdSystemPtr) {
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
                                                           MemoryPool::system4KBPagesWith32BitGpuAddressing,
                                                           0u, // shareable
                                                           maxOsContextCount);
    wddmAllocation->setDriverAllocatedCpuPtr(pSysMem);
    wddmAllocation->set32BitAllocation(true);
    wddmAllocation->setAllocationOffset(offset);
    wddmAllocation->setAllocInFrontWindowPool(allocationData.flags.use32BitFrontWindow);
    auto &productHelper = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHelper<ProductHelper>();

    auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHardwareInfo();

    StorageInfo storageInfo{};
    storageInfo.isLockable = allocationData.allocationMethod != GfxMemoryAllocationMethod::useUmdSystemPtr;

    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;

    gmm = new Gmm(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(), ptrAligned, sizeAligned, 0u,
                  CacheSettingsHelper::getGmmUsageType(wddmAllocation->getAllocationType(), !!allocationData.flags.uncacheable, productHelper, hwInfo), storageInfo, gmmRequirements);
    wddmAllocation->setDefaultGmm(gmm);

    if (!createWddmAllocation(wddmAllocation.get(), nullptr)) {
        delete gmm;
        freeSystemMemory(pSysMem);
        return nullptr;
    }
    [[maybe_unused]] auto baseAddress = getGfxPartition(allocationData.rootDeviceIndex)->getHeapBase(heapAssigners[allocationData.rootDeviceIndex]->get32BitHeapIndex(allocationData.type, false, *hwInfo, allocationData.flags.use32BitFrontWindow));
    DEBUG_BREAK_IF(gmmHelper->canonize(baseAddress) != wddmAllocation->getGpuBaseAddress());

    if (storageInfo.isLockable) {
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

GraphicsAllocation *WddmMemoryManager::createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) {
    auto allocation = std::make_unique<WddmAllocation>(properties.rootDeviceIndex, 1u /*num gmms*/, properties.allocationType, nullptr, 0, osHandleData.handle, MemoryPool::systemCpuInaccessible, maxOsContextCount, 0llu);
    bool status;
    if (verifyHandle(osHandleData.handle, properties.rootDeviceIndex, false))
        status = getWddm(properties.rootDeviceIndex).openSharedHandle(osHandleData, allocation.get());
    else
        status = getWddm(properties.rootDeviceIndex).openNTHandle(osHandleData, allocation.get());

    if (!status) {
        return nullptr;
    }

    // Shared objects are passed without size
    size_t size = allocation->getDefaultGmm()->gmmResourceInfo->getSizeAllocation();
    allocation->setSize(size);

    if (mapPointer == nullptr) {
        if (is32bit) {
            void *ptr = nullptr;
            if (!getWddm(properties.rootDeviceIndex).reserveValidAddressRange(size, ptr)) {
                return nullptr;
            }
            allocation->setReservedAddressRange(ptr, size);
        } else if (requireSpecificBitness && this->force32bitAllocations) {
            allocation->set32BitAllocation(true);
            auto gmmHelper = getGmmHelper(allocation->getRootDeviceIndex());
            allocation->setGpuBaseAddress(gmmHelper->canonize(getExternalHeapBaseAddress(allocation->getRootDeviceIndex(), false)));
        }
        status = mapGpuVirtualAddress(allocation.get(), allocation->getReservedAddressPtr());
    } else {
        status = mapPhysicalDeviceMemoryToVirtualMemory(allocation.get(), reinterpret_cast<uint64_t>(mapPointer), size);
    }
    this->registerSysMemAlloc(allocation.get());
    DEBUG_BREAK_IF(!status);
    if (!status) {
        freeGraphicsMemoryImpl(allocation.release());
        return nullptr;
    }

    logAllocation(fileLoggerInstance(), allocation.get(), this);
    return allocation.release();
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
    auto makeResidentPriorToLockRequired = this->peekExecutionEnvironment().rootDeviceEnvironments[wddmAllocation.getRootDeviceIndex()]->getHelper<GfxCoreHelper>().makeResidentBeforeLockNeeded(wddmAllocation.needsMakeResidentBeforeLock());
    if (makeResidentPriorToLockRequired) {
        wddmAllocation.setMakeResidentBeforeLockRequired(true);
    }

    return getWddm(graphicsAllocation.getRootDeviceIndex()).lockResource(wddmAllocation.getDefaultHandle(), makeResidentPriorToLockRequired, wddmAllocation.getAlignedSize());
}
void WddmMemoryManager::unlockResourceImpl(GraphicsAllocation &graphicsAllocation) {
    auto &wddmAllocation = static_cast<WddmAllocation &>(graphicsAllocation);
    getWddm(graphicsAllocation.getRootDeviceIndex()).unlockResource(wddmAllocation.getDefaultHandle(), wddmAllocation.needsMakeResidentBeforeLock());
}
void WddmMemoryManager::freeAssociatedResourceImpl(GraphicsAllocation &graphicsAllocation) {
    auto &wddmAllocation = static_cast<WddmAllocation &>(graphicsAllocation);
    if (wddmAllocation.needsMakeResidentBeforeLock()) {
        for (auto i = 0u; i < wddmAllocation.getNumGmms(); i++) {
            getWddm(graphicsAllocation.getRootDeviceIndex()).getTemporaryResourcesContainer()->removeResource(wddmAllocation.getHandles()[i]);
        }
    }
}

void WddmMemoryManager::freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation, bool isImportedAllocation) {
    return freeGraphicsMemoryImpl(gfxAllocation);
}

void WddmMemoryManager::closeInternalHandle(uint64_t &handle, uint32_t handleId, GraphicsAllocation *graphicsAllocation) {
    if (graphicsAllocation) {
        WddmAllocation *wddmAllocation = static_cast<WddmAllocation *>(graphicsAllocation);
        wddmAllocation->clearInternalHandle(handleId);
    }
    [[maybe_unused]] auto status = SysCalls::closeHandle(reinterpret_cast<void *>(reinterpret_cast<uintptr_t *>(handle)));
    DEBUG_BREAK_IF(!status);
}

void WddmMemoryManager::freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) {
    WddmAllocation *input = static_cast<WddmAllocation *>(gfxAllocation);
    DEBUG_BREAK_IF(!validateAllocation(input));

    if (gfxAllocation->isExplicitlyMadeResident()) {
        freeAssociatedResourceImpl(*gfxAllocation);
    }

    auto &registeredEngines = getRegisteredEngines(gfxAllocation->getRootDeviceIndex());
    for (auto &engine : registeredEngines) {
        auto &residencyController = static_cast<OsContextWin *>(engine.osContext)->getResidencyController();
        auto &evictContainer = engine.commandStreamReceiver->getEvictionAllocations();
        residencyController.removeAllocation(evictContainer, gfxAllocation);
    }

    auto defaultGmm = gfxAllocation->getDefaultGmm();
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[gfxAllocation->getRootDeviceIndex()]->getHardwareInfo();
    auto &productHelper = executionEnvironment.rootDeviceEnvironments[gfxAllocation->getRootDeviceIndex()]->getHelper<ProductHelper>();
    if (gfxAllocation->isCompressionEnabled() && productHelper.isPageTableManagerSupported(*hwInfo)) {
        for (auto &engine : registeredEngines) {
            if (engine.commandStreamReceiver->pageTableManager.get()) {
                std::unique_lock<CommandStreamReceiver::MutexType> lock;
                if (engine.commandStreamReceiver->isAnyDirectSubmissionEnabled()) {
                    engine.commandStreamReceiver->stopDirectSubmission(true, true);
                }
                [[maybe_unused]] auto status = engine.commandStreamReceiver->pageTableManager->updateAuxTable(input->getGpuAddress(), defaultGmm, false);
                DEBUG_BREAK_IF(!status);
            }
        }
    }
    for (auto handleId = 0u; handleId < gfxAllocation->getNumGmms(); handleId++) {
        auto gmm = gfxAllocation->getGmm(handleId);
        if (gmm) {
            auto gpuAddress = input->getGpuAddress();
            getWddm(gfxAllocation->getRootDeviceIndex()).freeGmmGpuVirtualAddress(gfxAllocation->getGmm(handleId), gpuAddress, input->getAlignedSize());
        }
        delete gmm;
    }
    uint64_t handle = 0;
    int ret = input->peekInternalHandle(nullptr, handle);
    if (ret == 0) {
        [[maybe_unused]] auto status = SysCalls::closeHandle(reinterpret_cast<void *>(reinterpret_cast<uintptr_t *>(handle)));
        DEBUG_BREAK_IF(!status);
    }

    if (input->peekSharedHandle() == false &&
        input->getDriverAllocatedCpuPtr() == nullptr &&
        input->fragmentsStorage.fragmentCount > 0) {
        cleanGraphicsMemoryCreatedFromHostPtr(gfxAllocation);
    } else {
        if (input->getResourceHandle() != 0) {
            [[maybe_unused]] auto status = tryDeferDeletions(nullptr, 0, input->getResourceHandle(), gfxAllocation->getRootDeviceIndex(), gfxAllocation->getAllocationType());
            DEBUG_BREAK_IF(!status);
        } else {
            [[maybe_unused]] auto status = tryDeferDeletions(input->getHandles().data(), static_cast<uint32_t>(input->getHandles().size()), 0, gfxAllocation->getRootDeviceIndex(), gfxAllocation->getAllocationType());
            DEBUG_BREAK_IF(!status);
        }

        alignedFreeWrapper(input->getDriverAllocatedCpuPtr());
    }
    if (input->getReservedAddressPtr()) {
        releaseReservedCpuAddressRange(input->getReservedAddressPtr(), input->getReservedAddressSize(), gfxAllocation->getRootDeviceIndex());
    }
    if (input->getReservedGpuVirtualAddress()) {
        getWddm(gfxAllocation->getRootDeviceIndex()).freeGpuVirtualAddress(input->getReservedGpuVirtualAddressToModify(), input->getReservedSizeForGpuVirtualAddress());
    }

    if (input->isAllocInFrontWindowPool()) {
        auto heapIndex = selectHeap(input, false, is32bit || executionEnvironment.rootDeviceEnvironments[input->getRootDeviceIndex()]->isFullRangeSvm(), input->isAllocInFrontWindowPool());
        auto alignedSize = input->getAlignedSize();
        auto gfxPartition = getGfxPartition(input->getRootDeviceIndex());
        gfxPartition->heapFree(heapIndex, input->getGpuAddress(), alignedSize);
    }

    if (gfxAllocation->isAllocatedInLocalMemoryPool()) {
        DEBUG_BREAK_IF(gfxAllocation->getUnderlyingBufferSize() > localMemAllocsSize[gfxAllocation->getRootDeviceIndex()]);
        localMemAllocsSize[gfxAllocation->getRootDeviceIndex()] -= gfxAllocation->getUnderlyingBufferSize();
    } else if (MemoryPool::memoryNull != gfxAllocation->getMemoryPool()) {
        DEBUG_BREAK_IF(gfxAllocation->getUnderlyingBufferSize() > sysMemAllocsSize);
        sysMemAllocsSize -= gfxAllocation->getUnderlyingBufferSize();
    }

    delete gfxAllocation;
}

void WddmMemoryManager::handleFenceCompletion(GraphicsAllocation *allocation) {
    auto wddmAllocation = static_cast<WddmAllocation *>(allocation);
    for (auto &engine : getRegisteredEngines(allocation->getRootDeviceIndex())) {
        const auto lastFenceValue = wddmAllocation->getResidencyData().getFenceValueForContextId(engine.osContext->getContextId());
        if (lastFenceValue != 0u) {
            const auto &monitoredFence = static_cast<OsContextWin *>(engine.osContext)->getResidencyController().getMonitoredFence();
            const auto wddm = static_cast<OsContextWin *>(engine.osContext)->getWddm();
            wddm->waitFromCpu(lastFenceValue, monitoredFence, engine.commandStreamReceiver->isAnyDirectSubmissionEnabled());
        }
    }
}

bool WddmMemoryManager::tryDeferDeletions(const D3DKMT_HANDLE *handles, uint32_t allocationCount, D3DKMT_HANDLE resourceHandle, uint32_t rootDeviceIndex, AllocationType type) {
    bool status = true;
    if (deferredDeleter) {
        deferredDeleter->deferDeletion(DeferrableDeletion::create(&getWddm(rootDeviceIndex), handles, allocationCount, resourceHandle, type));
    } else {
        status = getWddm(rootDeviceIndex).destroyAllocations(handles, allocationCount, resourceHandle);
    }
    return status;
}

bool WddmMemoryManager::isMemoryBudgetExhausted() const {
    for (auto &engineContainer : allRegisteredEngines) {
        for (auto &engine : engineContainer) {
            if (static_cast<OsContextWin *>(engine.osContext)->getResidencyController().isMemoryBudgetExhausted()) {
                return true;
            }
        }
    }
    return false;
}

bool WddmMemoryManager::validateAllocation(WddmAllocation *alloc) {
    if (alloc == nullptr)
        return false;
    if (alloc->isPhysicalMemoryReservation() && !alloc->isMappedPhysicalMemoryReservation()) {
        return true;
    }
    if (alloc->getGpuAddress() == 0u || (alloc->getDefaultHandle() == 0 && alloc->fragmentsStorage.fragmentCount == 0))
        return false;
    return true;
}

MemoryManager::AllocationStatus WddmMemoryManager::populateOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) {
    uint32_t allocatedFragmentIndexes[maxFragmentsCount];
    uint32_t allocatedFragmentsCounter = 0;

    auto &productHelper = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHelper<ProductHelper>();
    auto hwInfo = getGmmHelper(rootDeviceIndex)->getHardwareInfo();

    for (unsigned int i = 0; i < maxFragmentsCount; i++) {
        // If no fragment is present it means it already exists.
        if (!handleStorage.fragmentStorageData[i].osHandleStorage && handleStorage.fragmentStorageData[i].cpuPtr) {
            auto osHandle = new OsHandleWin();

            handleStorage.fragmentStorageData[i].osHandleStorage = osHandle;
            handleStorage.fragmentStorageData[i].residency = new ResidencyData(maxOsContextCount);

            GmmRequirements gmmRequirements{};
            gmmRequirements.allowLargePages = true;
            gmmRequirements.preferCompressed = false;

            osHandle->gmm = new Gmm(executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getGmmHelper(), handleStorage.fragmentStorageData[i].cpuPtr,
                                    handleStorage.fragmentStorageData[i].fragmentSize, 0u,
                                    CacheSettingsHelper::getGmmUsageType(AllocationType::externalHostPtr, false, productHelper, hwInfo), {}, gmmRequirements);
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

    bool success = tryDeferDeletions(handles, allocationCount, 0, rootDeviceIndex, AllocationType::unknown);

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
                                         allocationData.size, nullptr, MemoryPool::system4KBPages,
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
    if (isLocalMemorySupported(rootDeviceIndex)) {
        return 0.98;
    }
    return 0.94;
}

AlignedMallocRestrictions *WddmMemoryManager::getAlignedMallocRestrictions() {
    return &mallocRestrictions;
}

bool WddmMemoryManager::createPhysicalAllocation(WddmAllocation *allocation) {
    auto status = createGpuAllocationsWithRetry(allocation);
    if (!status) {
        return false;
    }
    allocation->setPhysicalMemoryReservation(true);
    return true;
}

bool WddmMemoryManager::createWddmAllocation(WddmAllocation *allocation, void *requiredGpuPtr) {
    auto status = createGpuAllocationsWithRetry(allocation);
    if (!status) {
        return false;
    }
    return mapGpuVirtualAddress(allocation, requiredGpuPtr);
}

size_t WddmMemoryManager::selectAlignmentAndHeap(size_t size, HeapIndex *heap) {
    AlignmentSelector::CandidateAlignment alignment = alignmentSelector.selectAlignment(size);
    *heap = HeapIndex::heapStandard64KB;
    return alignment.alignment;
}

AddressRange WddmMemoryManager::reserveGpuAddress(const uint64_t requiredStartAddress, size_t size, const RootDeviceIndicesContainer &rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex) {
    return reserveGpuAddressOnHeap(requiredStartAddress, size, rootDeviceIndices, reservedOnRootDeviceIndex, HeapIndex::heapStandard64KB, MemoryConstants::pageSize64k);
}

AddressRange WddmMemoryManager::reserveGpuAddressOnHeap(const uint64_t requiredStartAddress, size_t size, const RootDeviceIndicesContainer &rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex, HeapIndex heap, size_t alignment) {
    uint64_t gpuVa = 0u;
    *reservedOnRootDeviceIndex = 0;
    size_t reservedSize = 0;
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    auto gmmHelper = executionEnvironment.rootDeviceEnvironments[*reservedOnRootDeviceIndex]->getGmmHelper();
    for (auto rootDeviceIndex : rootDeviceIndices) {
        auto gfxPartition = getGfxPartition(rootDeviceIndex);
        status = getWddm(rootDeviceIndex).reserveGpuVirtualAddress(gmmHelper->decanonize(requiredStartAddress), gfxPartition->getHeapMinimalAddress(heap), gfxPartition->getHeapLimit(heap), size, &gpuVa);
        if (requiredStartAddress != 0ull && status != STATUS_SUCCESS) {
            status = getWddm(rootDeviceIndex).reserveGpuVirtualAddress(0ull, gfxPartition->getHeapMinimalAddress(heap), gfxPartition->getHeapLimit(heap), size, &gpuVa);
        }
        if (status == STATUS_SUCCESS) {
            *reservedOnRootDeviceIndex = rootDeviceIndex;
            reservedSize = size;
            break;
        }
    }
    if (status != STATUS_SUCCESS) {
        return AddressRange{0u, 0};
    }
    gpuVa = gmmHelper->canonize(gpuVa);
    return AddressRange{gpuVa, reservedSize};
}

void WddmMemoryManager::freeGpuAddress(AddressRange addressRange, uint32_t rootDeviceIndex) {
    getWddm(rootDeviceIndex).freeGpuVirtualAddress(addressRange.address, addressRange.size);
}

AddressRange WddmMemoryManager::reserveCpuAddress(const uint64_t requiredStartAddress, size_t size) {
    void *ptr = osMemory->osReserveCpuAddressRange(addrToPtr(requiredStartAddress), size, false);
    return {castToUint64(ptr), size};
}

void WddmMemoryManager::freeCpuAddress(AddressRange addressRange) {
    osMemory->osReleaseCpuAddressRange(addrToPtr(addressRange.address), addressRange.size);
}

bool WddmMemoryManager::mapGpuVaForOneHandleAllocation(WddmAllocation *allocation, const void *preferredGpuVirtualAddress) {
    D3DGPU_VIRTUAL_ADDRESS addressToMap = castToUint64(preferredGpuVirtualAddress);
    auto heapIndex = selectHeap(allocation, preferredGpuVirtualAddress != nullptr, is32bit || executionEnvironment.rootDeviceEnvironments[allocation->getRootDeviceIndex()]->isFullRangeSvm(), allocation->isAllocInFrontWindowPool());
    if (!executionEnvironment.rootDeviceEnvironments[allocation->getRootDeviceIndex()]->isFullRangeSvm() && !is32bit) {
        addressToMap = 0u;
    }
    if (allocation->getReservedGpuVirtualAddress()) {
        addressToMap = allocation->getReservedGpuVirtualAddress();
    }

    auto customHeapAllocatorCfg = getCustomHeapAllocatorConfig(allocation->getAllocationType(), allocation->isAllocInFrontWindowPool());

    auto gfxPartition = getGfxPartition(allocation->getRootDeviceIndex());
    auto alignedSize = allocation->getAlignedSize();

    if (customHeapAllocatorCfg.has_value()) {
        auto &customRange = customHeapAllocatorCfg.value().get();
        addressToMap = customRange.allocator->allocateWithCustomAlignment(alignedSize, MemoryConstants::pageSize64k);
    } else if (allocation->isAllocInFrontWindowPool()) {
        addressToMap = gfxPartition->heapAllocate(heapIndex, alignedSize);
    }

    D3DGPU_VIRTUAL_ADDRESS minimumAddress = gfxPartition->getHeapMinimalAddress(heapIndex);
    D3DGPU_VIRTUAL_ADDRESS maximumAddress = gfxPartition->getHeapLimit(heapIndex);
    auto status = getWddm(allocation->getRootDeviceIndex()).mapGpuVirtualAddress(allocation->getDefaultGmm(), allocation->getDefaultHandle(), minimumAddress, maximumAddress, addressToMap, allocation->getGpuAddressToModify(), allocation->getAllocationType());

    if (!status && deferredDeleter) {
        deferredDeleter->drain(true, false);
        status = getWddm(allocation->getRootDeviceIndex()).mapGpuVirtualAddress(allocation->getDefaultGmm(), allocation->getDefaultHandle(), minimumAddress, maximumAddress, addressToMap, allocation->getGpuAddressToModify(), allocation->getAllocationType());
    }
    if (!status) {
        if (allocation->getReservedGpuVirtualAddress()) {
            getWddm(allocation->getRootDeviceIndex()).freeGpuVirtualAddress(allocation->getReservedGpuVirtualAddressToModify(), allocation->getReservedSizeForGpuVirtualAddress());
        }
        getWddm(allocation->getRootDeviceIndex()).destroyAllocations(&allocation->getHandles()[0], allocation->getNumGmms(), allocation->getResourceHandle());
        return false;
    }

    if (auto config = customHeapAllocatorCfg; config.has_value() && config->get().gpuVaBase != std::numeric_limits<uint64_t>::max()) {
        auto gmmHelper = getGmmHelper(allocation->getRootDeviceIndex());
        allocation->setGpuBaseAddress(gmmHelper->canonize(config->get().gpuVaBase));
    } else if (GfxPartition::isAnyHeap32(heapIndex)) {
        auto gmmHelper = getGmmHelper(allocation->getRootDeviceIndex());
        allocation->setGpuBaseAddress(gmmHelper->canonize(gfxPartition->getHeapBase(heapIndex)));
    }

    return true;
}

bool WddmMemoryManager::mapMultiHandleAllocationWithRetry(WddmAllocation *allocation, const void *preferredGpuVirtualAddress) {
    Wddm &wddm = getWddm(allocation->getRootDeviceIndex());
    auto gfxPartition = getGfxPartition(allocation->getRootDeviceIndex());

    auto alignedSize = allocation->getAlignedSize();
    uint64_t addressToMap = 0;
    HeapIndex heapIndex = preferredGpuVirtualAddress ? HeapIndex::heapSvm : HeapIndex::heapStandard64KB;

    if (preferredGpuVirtualAddress) {
        addressToMap = castToUint64(preferredGpuVirtualAddress);
        allocation->getGpuAddressToModify() = addressToMap;
    } else {
        allocation->setReservedSizeForGpuVirtualAddress(alignUp(alignedSize, MemoryConstants::pageSize64k));
        auto status = wddm.reserveGpuVirtualAddress(0ull, gfxPartition->getHeapMinimalAddress(heapIndex), gfxPartition->getHeapLimit(heapIndex),
                                                    allocation->getReservedSizeForGpuVirtualAddress(), &allocation->getReservedGpuVirtualAddressToModify());
        UNRECOVERABLE_IF(status != STATUS_SUCCESS);
        auto gmmHelper = getGmmHelper(allocation->getRootDeviceIndex());
        allocation->getGpuAddressToModify() = gmmHelper->canonize(allocation->getReservedGpuVirtualAddress());
        addressToMap = allocation->getReservedGpuVirtualAddress();
    }

    for (auto currentHandle = 0u; currentHandle < allocation->getNumGmms(); currentHandle++) {
        uint64_t gpuAddress = 0;
        auto status = wddm.mapGpuVirtualAddress(allocation->getGmm(currentHandle), allocation->getHandles()[currentHandle],
                                                gfxPartition->getHeapMinimalAddress(heapIndex), gfxPartition->getHeapLimit(heapIndex), addressToMap, gpuAddress, allocation->getAllocationType());

        if (!status && deferredDeleter) {
            deferredDeleter->drain(true, false);
            status = wddm.mapGpuVirtualAddress(allocation->getGmm(currentHandle), allocation->getHandles()[currentHandle],
                                               gfxPartition->getHeapMinimalAddress(heapIndex), gfxPartition->getHeapLimit(heapIndex), addressToMap, gpuAddress, allocation->getAllocationType());
        }
        if (!status) {
            if (allocation->getReservedGpuVirtualAddress()) {
                wddm.freeGpuVirtualAddress(allocation->getReservedGpuVirtualAddressToModify(), allocation->getReservedSizeForGpuVirtualAddress());
            }
            wddm.destroyAllocations(&allocation->getHandles()[0], allocation->getNumGmms(), allocation->getResourceHandle());
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
        bool createNTHandle = allocation->isShareable() && !allocation->isShareableWithoutNTHandle();
        auto status = getWddm(allocation->getRootDeviceIndex()).createAllocation(allocation->getUnderlyingBuffer(), gmm, allocation->getHandleToModify(handleId), allocation->getResourceHandleToModify(), allocation->getSharedHandleToModify(), createNTHandle);
        if (status == STATUS_GRAPHICS_NO_VIDEO_MEMORY && deferredDeleter) {
            deferredDeleter->drain(true, false);
            status = getWddm(allocation->getRootDeviceIndex()).createAllocation(allocation->getUnderlyingBuffer(), gmm, allocation->getHandleToModify(handleId), allocation->getResourceHandleToModify(), allocation->getSharedHandleToModify(), createNTHandle);
        }
        if (status != STATUS_SUCCESS) {
            getWddm(allocation->getRootDeviceIndex()).destroyAllocations(&allocation->getHandles()[0], handleId, allocation->getResourceHandle());
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
    auto ret = getWddm(rootDeviceIndex).reserveValidAddressRange(size, reservePtr);
    UNRECOVERABLE_IF(!ret && reservePtr);
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
    constexpr auto slownessFactor = 50u;
    static int64_t meassurmentOverhead = std::numeric_limits<int64_t>::max();
    static int64_t fastestLocalRead = std::numeric_limits<int64_t>::max();

    // local variable that we will read for comparison
    int cacheable = 1;
    volatile int *localVariablePointer = &cacheable;
    volatile const int *volatileInputPtr = static_cast<volatile const int *>(ptr);

    int64_t timestamp0, timestamp1, localVariableReadDelta, inputPointerReadDelta;

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
    cacheable = *localVariablePointer;

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
    cacheable = *volatileInputPtr;

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
    return inputPointerReadDelta > slownessFactor * fastestLocalRead;
}

bool WddmMemoryManager::copyMemoryToAllocation(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy) {
    if (graphicsAllocation->getUnderlyingBuffer() && (graphicsAllocation->storageInfo.getNumBanks() == 1 || GraphicsAllocation::isDebugSurfaceAllocationType(graphicsAllocation->getAllocationType()))) {
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
        auto ptr = wddm.lockResource(wddmAllocation->getHandles()[handleId], wddmAllocation->needsMakeResidentBeforeLock(), wddmAllocation->getAlignedSize());
        if (!ptr) {
            return false;
        }
        memcpy_s(ptrOffset(ptr, destinationOffset), graphicsAllocation->getUnderlyingBufferSize() - destinationOffset, memoryToCopy, sizeToCopy);
        wddm.unlockResource(wddmAllocation->getHandles()[handleId], wddmAllocation->needsMakeResidentBeforeLock());
    }
    wddmAllocation->setExplicitlyMadeResident(wddmAllocation->needsMakeResidentBeforeLock());
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
    auto &productHelper = gmmHelper->getRootDeviceEnvironment().getHelper<ProductHelper>();

    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = compression;

    for (auto handleId = 0u; handleId < handles; handleId++) {
        auto currentSize = alignUp(remainingSize / (handles - handleId), MemoryConstants::pageSize64k);
        remainingSize -= currentSize;
        StorageInfo limitedStorageInfo = storageInfo;
        limitedStorageInfo.memoryBanks &= static_cast<uint32_t>(1u << handleId);
        auto gmm = new Gmm(gmmHelper,
                           nullptr,
                           currentSize,
                           0u,
                           CacheSettingsHelper::getGmmUsageType(allocation.getAllocationType(), false, productHelper, gmmHelper->getHardwareInfo()),
                           limitedStorageInfo, gmmRequirements);
        allocation.setGmm(gmm, handleId);
    }
}

void fillGmmsInAllocation(GmmHelper *gmmHelper, WddmAllocation *allocation, const StorageInfo &storageInfo) {
    auto &productHelper = gmmHelper->getRootDeviceEnvironment().getHelper<ProductHelper>();

    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;

    for (auto handleId = 0u; handleId < storageInfo.getNumBanks(); handleId++) {
        StorageInfo limitedStorageInfo = storageInfo;
        limitedStorageInfo.memoryBanks &= static_cast<uint32_t>(1u << handleId);
        limitedStorageInfo.pageTablesVisibility &= static_cast<uint32_t>(1u << handleId);
        auto gmm = new Gmm(gmmHelper, nullptr, allocation->getAlignedSize(), 0u,
                           CacheSettingsHelper::getGmmUsageType(allocation->getAllocationType(), false, productHelper, gmmHelper->getHardwareInfo()), limitedStorageInfo, gmmRequirements);
        allocation->setGmm(gmm, handleId);
    }
}

void splitGmmsInAllocation(GmmHelper *gmmHelper, WddmAllocation *wddmAllocation, size_t alignment, size_t chunkSize, StorageInfo &storageInfo) {
    auto sizeRemaining = wddmAllocation->getAlignedSize();
    auto &productHelper = gmmHelper->getRootDeviceEnvironment().getHelper<ProductHelper>();

    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;

    for (auto gmmId = 0u; gmmId < wddmAllocation->getNumGmms(); ++gmmId) {
        auto size = sizeRemaining > chunkSize ? chunkSize : sizeRemaining;
        auto gmm = new Gmm(gmmHelper, nullptr, size, alignment,
                           CacheSettingsHelper::getGmmUsageType(wddmAllocation->getAllocationType(), false, productHelper, gmmHelper->getHardwareInfo()), storageInfo, gmmRequirements);
        wddmAllocation->setGmm(gmm, gmmId);
        sizeRemaining -= size;
    }
    storageInfo.multiStorage = true;
}

uint32_t getPriorityForAllocation(AllocationType allocationType) {
    if (GraphicsAllocation::isIsaAllocationType(allocationType) ||
        allocationType == AllocationType::commandBuffer ||
        allocationType == AllocationType::internalHeap ||
        allocationType == AllocationType::linearStream) {
        return DXGI_RESOURCE_PRIORITY_HIGH;
    }
    return DXGI_RESOURCE_PRIORITY_NORMAL;
}

GraphicsAllocation *WddmMemoryManager::allocatePhysicalLocalDeviceMemory(const AllocationData &allocationData, AllocationStatus &status) {
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex];
    auto gmmHelper = rootDeviceEnvironment.getGmmHelper();

    std::unique_ptr<Gmm> gmm;
    size_t sizeAligned = 0;
    size_t alignment = 0;
    auto numBanks = allocationData.storageInfo.getNumBanks();
    bool singleBankAllocation = numBanks == 1;
    alignment = alignmentSelector.selectAlignment(allocationData.size).alignment;
    sizeAligned = alignUp(allocationData.size, alignment);

    if (singleBankAllocation) {
        auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();

        GmmRequirements gmmRequirements{};
        gmmRequirements.allowLargePages = true;
        gmmRequirements.preferCompressed = allocationData.flags.preferCompressed;

        gmm = std::make_unique<Gmm>(gmmHelper,
                                    nullptr,
                                    sizeAligned,
                                    alignment,
                                    CacheSettingsHelper::getGmmUsageType(allocationData.type, !!allocationData.flags.uncacheable, productHelper, gmmHelper->getHardwareInfo()),
                                    allocationData.storageInfo,
                                    gmmRequirements);
    }

    const auto chunkSize = alignDown(getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod::allocateByKmd), alignment);
    const size_t numGmms = static_cast<size_t>((static_cast<uint64_t>(sizeAligned) + chunkSize - 1) / chunkSize);

    auto wddmAllocation = std::make_unique<WddmAllocation>(allocationData.rootDeviceIndex, singleBankAllocation ? numGmms : numBanks,
                                                           allocationData.type, nullptr, 0, sizeAligned, nullptr, MemoryPool::localMemory, allocationData.flags.shareable, maxOsContextCount);
    wddmAllocation->setShareableWithoutNTHandle(allocationData.flags.shareableWithoutNTHandle);
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
    wddmAllocation->setMakeResidentBeforeLockRequired(true);

    auto &wddm = getWddm(allocationData.rootDeviceIndex);

    if (!createPhysicalAllocation(wddmAllocation.get())) {
        for (auto handleId = 0u; handleId < allocationData.storageInfo.getNumBanks(); handleId++) {
            delete wddmAllocation->getGmm(handleId);
        }
        status = AllocationStatus::Error;
        return nullptr;
    }

    auto handles = wddmAllocation->getHandles();

    if (!wddm.setAllocationPriority(handles.data(), static_cast<UINT>(handles.size()), getPriorityForAllocation(allocationData.type))) {
        freeGraphicsMemory(wddmAllocation.release());
        status = AllocationStatus::Error;
        return nullptr;
    }

    status = AllocationStatus::Success;
    return wddmAllocation.release();
}

GraphicsAllocation *WddmMemoryManager::allocatePhysicalHostMemory(const AllocationData &allocationData, AllocationStatus &status) {
    return nullptr;
}

GraphicsAllocation *WddmMemoryManager::allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) {
    status = AllocationStatus::RetryInNonDevicePool;

    if (!this->localMemorySupported[allocationData.rootDeviceIndex] ||
        allocationData.flags.useSystemMemory ||
        (allocationData.flags.allow32Bit && this->force32bitAllocations)) {
        return nullptr;
    }

    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex];
    auto gmmHelper = rootDeviceEnvironment.getGmmHelper();

    std::unique_ptr<Gmm> gmm;
    size_t sizeAligned = 0;
    size_t alignment = 0;
    auto numBanks = allocationData.storageInfo.getNumBanks();
    bool singleBankAllocation = numBanks == 1;
    if (allocationData.type == AllocationType::image ||
        allocationData.type == AllocationType::sharedResourceCopy) {
        allocationData.imgInfo->useLocalMemory = true;
        gmm = std::make_unique<Gmm>(gmmHelper, *allocationData.imgInfo, allocationData.storageInfo, allocationData.flags.preferCompressed);
        alignment = MemoryConstants::pageSize64k;
        sizeAligned = allocationData.imgInfo->size;
    } else {
        alignment = alignmentSelector.selectAlignment(allocationData.size, allocationData.type == AllocationType::svmGpu ? allocationData.alignment : std::numeric_limits<size_t>::max()).alignment;
        sizeAligned = alignUp(allocationData.size, alignment);

        if (debugManager.flags.ExperimentalAlignLocalMemorySizeTo2MB.get()) {
            alignment = alignUp(alignment, MemoryConstants::pageSize2M);
            sizeAligned = alignUp(sizeAligned, MemoryConstants::pageSize2M);
        }

        if (singleBankAllocation) {
            auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();

            GmmRequirements gmmRequirements{};
            gmmRequirements.allowLargePages = true;
            gmmRequirements.preferCompressed = allocationData.flags.preferCompressed;

            gmm = std::make_unique<Gmm>(gmmHelper,
                                        nullptr,
                                        sizeAligned,
                                        alignment,
                                        CacheSettingsHelper::getGmmUsageType(allocationData.type, !!allocationData.flags.uncacheable, productHelper, gmmHelper->getHardwareInfo()),
                                        allocationData.storageInfo,
                                        gmmRequirements);
        }
    }

    const auto chunkSize = alignDown(getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod::allocateByKmd), alignment);
    const size_t numGmms = static_cast<size_t>((static_cast<uint64_t>(sizeAligned) + chunkSize - 1) / chunkSize);

    auto wddmAllocation = std::make_unique<WddmAllocation>(allocationData.rootDeviceIndex, singleBankAllocation ? numGmms : numBanks,
                                                           allocationData.type, nullptr, 0, sizeAligned, nullptr, MemoryPool::localMemory, allocationData.flags.shareable, maxOsContextCount);
    wddmAllocation->setShareableWithoutNTHandle(allocationData.flags.shareableWithoutNTHandle);
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
    wddmAllocation->setMakeResidentBeforeLockRequired(true);
    if (heapAssigners[allocationData.rootDeviceIndex]->use32BitHeap(allocationData.type)) {
        wddmAllocation->setAllocInFrontWindowPool(allocationData.flags.use32BitFrontWindow);
    }

    void *requiredGpuVa = nullptr;
    if (allocationData.type == AllocationType::svmGpu) {
        requiredGpuVa = const_cast<void *>(allocationData.hostPtr);
    }

    auto &wddm = getWddm(allocationData.rootDeviceIndex);

    if (!heapAssigners[allocationData.rootDeviceIndex]->use32BitHeap(allocationData.type)) {
        adjustGpuPtrToHostAddressSpace(*wddmAllocation.get(), requiredGpuVa);
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
        freeGraphicsMemory(wddmAllocation.release());
        status = AllocationStatus::Error;
        return nullptr;
    }
    if (allocationData.flags.requiresCpuAccess) {
        wddmAllocation->setCpuAddress(lockResource(wddmAllocation.get()));
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
                                              selectHeap(allocation, requiredPtr != nullptr, executionEnvironment.rootDeviceEnvironments[allocation->getRootDeviceIndex()]->isFullRangeSvm(), allocation->isAllocInFrontWindowPool()),
                                              *getGfxPartition(allocation->getRootDeviceIndex()));
        } else if (allocation->storageInfo.multiStorage) {
            return mapMultiHandleAllocationWithRetry(allocation, requiredPtr);
        }
    } else if (allocation->getAllocationType() == AllocationType::writeCombined) {
        requiredPtr = lockResource(allocation);
        allocation->setCpuAddress(const_cast<void *>(requiredPtr));
    }
    return mapGpuVaForOneHandleAllocation(allocation, requiredPtr);
}

uint64_t WddmMemoryManager::getLocalMemorySize(uint32_t rootDeviceIndex, uint32_t deviceBitfield) {
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
    uint64_t subDevicesCount = GfxCoreHelper::getSubDevicesCount(hwInfo);

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

bool WddmMemoryManager::isStatelessAccessRequired(AllocationType type) {
    if (type == AllocationType::buffer ||
        type == AllocationType::sharedBuffer ||
        type == AllocationType::scratchSurface ||
        type == AllocationType::linearStream ||
        type == AllocationType::privateSurface ||
        type == AllocationType::constantSurface ||
        type == AllocationType::globalSurface ||
        type == AllocationType::printfSurface) {
        return true;
    }
    return false;
}

template <bool is32Bit>
void WddmMemoryManager::adjustGpuPtrToHostAddressSpace(WddmAllocation &wddmAllocation, void *&requiredGpuVa) {
    if constexpr (is32Bit) {
        auto rootDeviceIndex = wddmAllocation.getRootDeviceIndex();
        if (executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->isFullRangeSvm()) {
            if (isStatelessAccessRequired(wddmAllocation.getAllocationType())) {
                size_t reserveSizeAligned = wddmAllocation.getUnderlyingBufferSize();
                auto isLocalMemory = wddmAllocation.getMemoryPool() == MemoryPool::localMemory;
                if (isLocalMemory) {
                    // add 2MB padding to make sure there are no overlaps between system and local memory
                    reserveSizeAligned += 2 * MemoryConstants::megaByte;
                }
                auto &wddm = getWddm(rootDeviceIndex);
                wddm.reserveValidAddressRange(reserveSizeAligned, requiredGpuVa);
                wddmAllocation.setReservedAddressRange(requiredGpuVa, reserveSizeAligned);
                requiredGpuVa = isLocalMemory ? alignUp(requiredGpuVa, 2 * MemoryConstants::megaByte) : requiredGpuVa;
            }
        }
    }
}

} // namespace NEO
