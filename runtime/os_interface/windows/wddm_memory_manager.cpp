/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/wddm_memory_manager.h"

#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/device/device.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/resource_info.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/surface_formats.h"
#include "runtime/memory_manager/deferrable_deletion.h"
#include "runtime/memory_manager/deferred_deleter.h"
#include "runtime/memory_manager/host_ptr_manager.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/os_interface/windows/os_context_win.h"
#include "runtime/os_interface/windows/os_interface.h"
#include "runtime/os_interface/windows/wddm/wddm.h"
#include "runtime/os_interface/windows/wddm_allocation.h"
#include "runtime/os_interface/windows/wddm_residency_controller.h"
#include "runtime/platform/platform.h"

#include <algorithm>

namespace NEO {

WddmMemoryManager::~WddmMemoryManager() {
    applyCommonCleanup();
}

WddmMemoryManager::WddmMemoryManager(ExecutionEnvironment &executionEnvironment) : MemoryManager(executionEnvironment),
                                                                                   wddm(executionEnvironment.osInterface->get()->getWddm()) {
    DEBUG_BREAK_IF(wddm == nullptr);

    asyncDeleterEnabled = DebugManager.flags.EnableDeferredDeleter.get();
    if (asyncDeleterEnabled)
        deferredDeleter = createDeferredDeleter();
    mallocRestrictions.minAddress = wddm->getWddmMinAddress();
    wddm->initGfxPartition(gfxPartition);
}

GraphicsAllocation *WddmMemoryManager::allocateGraphicsMemoryForImageImpl(const AllocationData &allocationData, std::unique_ptr<Gmm> gmm) {
    if (!GmmHelper::allowTiling(*allocationData.imgInfo->imgDesc) && allocationData.imgInfo->mipCount == 0) {
        return allocateGraphicsMemoryWithAlignment(allocationData);
    }

    auto allocation = std::make_unique<WddmAllocation>(allocationData.type, nullptr, allocationData.imgInfo->size, nullptr, MemoryPool::SystemCpuInaccessible, false);
    allocation->setDefaultGmm(gmm.get());
    if (!createWddmAllocation(allocation.get(), nullptr)) {
        return nullptr;
    }

    gmm.release();

    return allocation.release();
}

GraphicsAllocation *WddmMemoryManager::allocateGraphicsMemory64kb(const AllocationData &allocationData) {
    size_t sizeAligned = alignUp(allocationData.size, MemoryConstants::pageSize64k);

    auto wddmAllocation = std::make_unique<WddmAllocation>(allocationData.type, nullptr, sizeAligned, nullptr, MemoryPool::System64KBPages, !!allocationData.flags.multiOsContextCapable);

    auto gmm = new Gmm(nullptr, sizeAligned, false, allocationData.flags.preferRenderCompressed, true, {});
    wddmAllocation->setDefaultGmm(gmm);

    if (!wddm->createAllocation64k(gmm, wddmAllocation->getHandleToModify(0u))) {
        delete gmm;
        return nullptr;
    }

    auto cpuPtr = lockResource(wddmAllocation.get());

    // 64kb map is not needed
    auto status = mapGpuVirtualAddressWithRetry(wddmAllocation.get(), cpuPtr);
    DEBUG_BREAK_IF(!status);
    wddmAllocation->setCpuAddress(cpuPtr);

    return wddmAllocation.release();
}

GraphicsAllocation *WddmMemoryManager::allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) {
    size_t newAlignment = allocationData.alignment ? alignUp(allocationData.alignment, MemoryConstants::pageSize) : MemoryConstants::pageSize;
    size_t sizeAligned = allocationData.size ? alignUp(allocationData.size, MemoryConstants::pageSize) : MemoryConstants::pageSize;
    void *pSysMem = allocateSystemMemory(sizeAligned, newAlignment);
    Gmm *gmm = nullptr;

    if (pSysMem == nullptr) {
        return nullptr;
    }

    auto wddmAllocation = std::make_unique<WddmAllocation>(allocationData.type, pSysMem, sizeAligned, nullptr, MemoryPool::System4KBPages, allocationData.flags.multiOsContextCapable);
    wddmAllocation->setDriverAllocatedCpuPtr(pSysMem);

    gmm = new Gmm(pSysMem, sizeAligned, allocationData.flags.uncacheable);

    wddmAllocation->setDefaultGmm(gmm);
    void *mapPtr = wddmAllocation->getAlignedCpuPtr();
    if (allocationData.type == GraphicsAllocation::AllocationType::SVM_CPU) {
        //add 2MB padding in case mapPtr is not 2MB aligned
        size_t reserveSizeAligned = sizeAligned + allocationData.alignment;
        bool ret = wddm->reserveValidAddressRange(reserveSizeAligned, mapPtr);
        if (!ret) {
            delete gmm;
            freeSystemMemory(pSysMem);
            return nullptr;
        }
        wddmAllocation->setReservedAddressRange(mapPtr, reserveSizeAligned);
        mapPtr = alignUp(mapPtr, newAlignment);
    }

    if (!createWddmAllocation(wddmAllocation.get(), mapPtr)) {
        delete gmm;
        freeSystemMemory(pSysMem);
        return nullptr;
    }

    return wddmAllocation.release();
}

GraphicsAllocation *WddmMemoryManager::allocateGraphicsMemoryForNonSvmHostPtr(const AllocationData &allocationData) {
    auto alignedPtr = alignDown(allocationData.hostPtr, MemoryConstants::pageSize);
    auto offsetInPage = ptrDiff(allocationData.hostPtr, alignedPtr);
    auto alignedSize = alignSizeWholePage(allocationData.hostPtr, allocationData.size);

    auto wddmAllocation = std::make_unique<WddmAllocation>(allocationData.type, const_cast<void *>(allocationData.hostPtr),
                                                           allocationData.size, nullptr, MemoryPool::System4KBPages, false);
    wddmAllocation->setAllocationOffset(offsetInPage);

    auto gmm = new Gmm(alignedPtr, alignedSize, false);

    wddmAllocation->setDefaultGmm(gmm);

    if (!createWddmAllocation(wddmAllocation.get(), wddmAllocation->getAlignedCpuPtr())) {
        delete gmm;
        return nullptr;
    }

    return wddmAllocation.release();
}

GraphicsAllocation *WddmMemoryManager::allocateGraphicsMemoryWithHostPtr(const AllocationData &allocationData) {
    if (mallocRestrictions.minAddress > reinterpret_cast<uintptr_t>(allocationData.hostPtr)) {
        auto inputPtr = allocationData.hostPtr;
        void *reserve = nullptr;
        auto ptrAligned = alignDown(inputPtr, MemoryConstants::allocationAlignment);
        size_t sizeAligned = alignSizeWholePage(inputPtr, allocationData.size);
        size_t offset = ptrDiff(inputPtr, ptrAligned);

        if (!wddm->reserveValidAddressRange(sizeAligned, reserve)) {
            return nullptr;
        }

        auto allocation = new WddmAllocation(allocationData.type, const_cast<void *>(inputPtr), allocationData.size, reserve, MemoryPool::System4KBPages, false);
        allocation->setAllocationOffset(offset);

        Gmm *gmm = new Gmm(ptrAligned, sizeAligned, false);
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
    } else {
        sizeAligned = alignUp(sizeAligned, MemoryConstants::allocationAlignment);
        pSysMem = allocateSystemMemory(sizeAligned, MemoryConstants::allocationAlignment);
        if (pSysMem == nullptr) {
            return nullptr;
        }
        ptrAligned = pSysMem;
    }

    auto wddmAllocation = std::make_unique<WddmAllocation>(allocationData.type, const_cast<void *>(ptrAligned), sizeAligned, nullptr,
                                                           MemoryPool::System4KBPagesWith32BitGpuAddressing, false);
    wddmAllocation->setDriverAllocatedCpuPtr(pSysMem);
    wddmAllocation->set32BitAllocation(true);
    wddmAllocation->setAllocationOffset(offset);

    gmm = new Gmm(ptrAligned, sizeAligned, false);
    wddmAllocation->setDefaultGmm(gmm);

    if (!createWddmAllocation(wddmAllocation.get(), nullptr)) {
        delete gmm;
        freeSystemMemory(pSysMem);
        return nullptr;
    }

    auto baseAddress = useInternal32BitAllocator(allocationData.type) ? getInternalHeapBaseAddress() : getExternalHeapBaseAddress();
    wddmAllocation->setGpuBaseAddress(GmmHelper::canonize(baseAddress));

    return wddmAllocation.release();
}

GraphicsAllocation *WddmMemoryManager::createAllocationFromHandle(osHandle handle, bool requireSpecificBitness, bool ntHandle, GraphicsAllocation::AllocationType allocationType) {
    auto allocation = std::make_unique<WddmAllocation>(allocationType, nullptr, 0, handle, MemoryPool::SystemCpuInaccessible, false);

    bool status = ntHandle ? wddm->openNTHandle(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(handle)), allocation.get())
                           : wddm->openSharedHandle(handle, allocation.get());

    if (!status) {
        return nullptr;
    }

    // Shared objects are passed without size
    size_t size = allocation->getDefaultGmm()->gmmResourceInfo->getSizeAllocation();
    allocation->setSize(size);

    if (is32bit) {
        void *ptr = nullptr;
        if (!wddm->reserveValidAddressRange(size, ptr)) {
            return nullptr;
        }
        allocation->setReservedAddressRange(ptr, size);
    } else if (requireSpecificBitness && this->force32bitAllocations) {
        allocation->set32BitAllocation(true);
        allocation->setGpuBaseAddress(GmmHelper::canonize(getExternalHeapBaseAddress()));
    }
    status = mapGpuVirtualAddressWithRetry(allocation.get(), allocation->getReservedAddressPtr());
    DEBUG_BREAK_IF(!status);
    if (!status) {
        freeGraphicsMemoryImpl(allocation.release());
        return nullptr;
    }

    DebugManager.logAllocation(allocation.get());
    return allocation.release();
}

GraphicsAllocation *WddmMemoryManager::createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness) {
    return createAllocationFromHandle(handle, requireSpecificBitness, false, properties.allocationType);
}

GraphicsAllocation *WddmMemoryManager::createGraphicsAllocationFromNTHandle(void *handle) {
    return createAllocationFromHandle((osHandle)((UINT_PTR)handle), false, true, GraphicsAllocation::AllocationType::SHARED_IMAGE);
}

void WddmMemoryManager::addAllocationToHostPtrManager(GraphicsAllocation *gfxAllocation) {
    WddmAllocation *wddmMemory = static_cast<WddmAllocation *>(gfxAllocation);
    FragmentStorage fragment = {};
    fragment.driverAllocation = true;
    fragment.fragmentCpuPointer = gfxAllocation->getUnderlyingBuffer();
    fragment.fragmentSize = alignUp(gfxAllocation->getUnderlyingBufferSize(), MemoryConstants::pageSize);

    fragment.osInternalStorage = new OsHandle();
    fragment.osInternalStorage->gpuPtr = gfxAllocation->getGpuAddress();
    fragment.osInternalStorage->handle = wddmMemory->getDefaultHandle();
    fragment.osInternalStorage->gmm = gfxAllocation->getDefaultGmm();
    fragment.residency = &wddmMemory->getResidencyData();
    hostPtrManager->storeFragment(fragment);
}

void WddmMemoryManager::removeAllocationFromHostPtrManager(GraphicsAllocation *gfxAllocation) {
    auto buffer = gfxAllocation->getUnderlyingBuffer();
    auto fragment = hostPtrManager->getFragment(buffer);
    if (fragment && fragment->driverAllocation) {
        OsHandle *osStorageToRelease = fragment->osInternalStorage;
        if (hostPtrManager->releaseHostPtr(buffer)) {
            delete osStorageToRelease;
        }
    }
}

void *WddmMemoryManager::lockResourceImpl(GraphicsAllocation &graphicsAllocation) {
    auto &wddmAllocation = static_cast<WddmAllocation &>(graphicsAllocation);
    return wddm->lockResource(wddmAllocation.getDefaultHandle(), wddmAllocation.needsMakeResidentBeforeLock);
}
void WddmMemoryManager::unlockResourceImpl(GraphicsAllocation &graphicsAllocation) {
    auto &wddmAllocation = static_cast<WddmAllocation &>(graphicsAllocation);
    wddm->unlockResource(wddmAllocation.getDefaultHandle());
    if (wddmAllocation.needsMakeResidentBeforeLock) {
        auto evictionStatus = wddm->evictTemporaryResource(wddmAllocation.getDefaultHandle());
        DEBUG_BREAK_IF(evictionStatus == EvictionStatus::FAILED);
    }
}
void WddmMemoryManager::freeAssociatedResourceImpl(GraphicsAllocation &graphicsAllocation) {
    auto &wddmAllocation = static_cast<WddmAllocation &>(graphicsAllocation);
    if (wddmAllocation.needsMakeResidentBeforeLock) {
        for (auto i = 0u; i < wddmAllocation.getNumHandles(); i++) {
            wddm->removeTemporaryResource(wddmAllocation.getHandles()[i]);
        }
    }
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
    if (defaultGmm) {
        if (defaultGmm->isRenderCompressed && wddm->getPageTableManager()) {
            auto status = wddm->updateAuxTable(input->getGpuAddress(), defaultGmm, false);
            DEBUG_BREAK_IF(!status);
        }
    }
    for (auto handleId = 0u; handleId < maxHandleCount; handleId++) {
        delete gfxAllocation->getGmm(handleId);
    }

    if (input->peekSharedHandle() == false &&
        input->getDriverAllocatedCpuPtr() == nullptr &&
        input->fragmentsStorage.fragmentCount > 0) {
        cleanGraphicsMemoryCreatedFromHostPtr(gfxAllocation);
    } else {
        const D3DKMT_HANDLE *allocationHandles = nullptr;
        uint32_t allocationCount = 0;
        D3DKMT_HANDLE resourceHandle = 0;
        if (input->peekSharedHandle()) {
            resourceHandle = input->resourceHandle;
        } else {
            allocationHandles = input->getHandles().data();
            allocationCount = input->getNumHandles();
        }
        auto status = tryDeferDeletions(allocationHandles, allocationCount, resourceHandle);
        DEBUG_BREAK_IF(!status);
        alignedFreeWrapper(input->getDriverAllocatedCpuPtr());
    }
    if (input->getReservedAddressPtr()) {
        releaseReservedCpuAddressRange(input->getReservedAddressPtr(), input->getReservedAddressSize());
    }
    if (input->preferredGpuAddress) {
        wddm->freeGpuVirtualAddress(input->preferredGpuAddress, input->getAlignedSize());
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

bool WddmMemoryManager::tryDeferDeletions(const D3DKMT_HANDLE *handles, uint32_t allocationCount, D3DKMT_HANDLE resourceHandle) {
    bool status = true;
    if (deferredDeleter) {
        deferredDeleter->deferDeletion(DeferrableDeletion::create(wddm, handles, allocationCount, resourceHandle));
    } else {
        status = wddm->destroyAllocations(handles, allocationCount, resourceHandle);
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

MemoryManager::AllocationStatus WddmMemoryManager::populateOsHandles(OsHandleStorage &handleStorage) {
    uint32_t allocatedFragmentIndexes[maxFragmentsCount];
    uint32_t allocatedFragmentsCounter = 0;

    for (unsigned int i = 0; i < maxFragmentsCount; i++) {
        // If no fragment is present it means it already exists.
        if (!handleStorage.fragmentStorageData[i].osHandleStorage && handleStorage.fragmentStorageData[i].cpuPtr) {
            handleStorage.fragmentStorageData[i].osHandleStorage = new OsHandle();
            handleStorage.fragmentStorageData[i].residency = new ResidencyData();

            handleStorage.fragmentStorageData[i].osHandleStorage->gmm = new Gmm(handleStorage.fragmentStorageData[i].cpuPtr, handleStorage.fragmentStorageData[i].fragmentSize, false);
            allocatedFragmentIndexes[allocatedFragmentsCounter] = i;
            allocatedFragmentsCounter++;
        }
    }
    auto result = wddm->createAllocationsAndMapGpuVa(handleStorage);

    if (result == STATUS_GRAPHICS_NO_VIDEO_MEMORY) {
        return AllocationStatus::InvalidHostPointer;
    }

    UNRECOVERABLE_IF(result != STATUS_SUCCESS);

    for (uint32_t i = 0; i < allocatedFragmentsCounter; i++) {
        hostPtrManager->storeFragment(handleStorage.fragmentStorageData[allocatedFragmentIndexes[i]]);
    }

    return AllocationStatus::Success;
}

void WddmMemoryManager::cleanOsHandles(OsHandleStorage &handleStorage) {

    D3DKMT_HANDLE handles[maxFragmentsCount] = {0};
    auto allocationCount = 0;

    for (unsigned int i = 0; i < maxFragmentsCount; i++) {
        if (handleStorage.fragmentStorageData[i].freeTheFragment) {
            handles[allocationCount++] = handleStorage.fragmentStorageData[i].osHandleStorage->handle;
            std::fill_n(handleStorage.fragmentStorageData[i].residency->resident, maxOsContextCount, false);
        }
    }

    bool success = tryDeferDeletions(handles, allocationCount, 0);

    for (unsigned int i = 0; i < maxFragmentsCount; i++) {
        if (handleStorage.fragmentStorageData[i].freeTheFragment) {
            if (success) {
                handleStorage.fragmentStorageData[i].osHandleStorage->handle = 0;
            }
            delete handleStorage.fragmentStorageData[i].osHandleStorage->gmm;
            delete handleStorage.fragmentStorageData[i].osHandleStorage;
            delete handleStorage.fragmentStorageData[i].residency;
        }
    }
}

void WddmMemoryManager::obtainGpuAddressFromFragments(WddmAllocation *allocation, OsHandleStorage &handleStorage) {
    if (this->force32bitAllocations && (handleStorage.fragmentCount > 0)) {
        auto hostPtr = allocation->getUnderlyingBuffer();
        auto fragment = hostPtrManager->getFragment(hostPtr);
        if (fragment && fragment->driverAllocation) {
            auto gpuPtr = handleStorage.fragmentStorageData[0].osHandleStorage->gpuPtr;
            for (uint32_t i = 1; i < handleStorage.fragmentCount; i++) {
                if (handleStorage.fragmentStorageData[i].osHandleStorage->gpuPtr < gpuPtr) {
                    gpuPtr = handleStorage.fragmentStorageData[i].osHandleStorage->gpuPtr;
                }
            }
            allocation->setAllocationOffset(reinterpret_cast<uint64_t>(hostPtr) & MemoryConstants::pageMask);
            allocation->setGpuAddress(gpuPtr);
        }
    }
}

GraphicsAllocation *WddmMemoryManager::createGraphicsAllocation(OsHandleStorage &handleStorage, const AllocationData &allocationData) {
    auto allocation = new WddmAllocation(allocationData.type, const_cast<void *>(allocationData.hostPtr), allocationData.size, nullptr, MemoryPool::System4KBPages, false);
    allocation->fragmentsStorage = handleStorage;
    obtainGpuAddressFromFragments(allocation, handleStorage);
    return allocation;
}

uint64_t WddmMemoryManager::getSystemSharedMemory() {
    return wddm->getSystemSharedMemory();
}

uint64_t WddmMemoryManager::getMaxApplicationAddress() {
    return wddm->getMaxApplicationAddress();
}

uint64_t WddmMemoryManager::getInternalHeapBaseAddress() {
    return gfxPartition.getHeapBase(internalHeapIndex);
}

uint64_t WddmMemoryManager::getExternalHeapBaseAddress() {
    return gfxPartition.getHeapBase(HeapIndex::HEAP_EXTERNAL);
}

void WddmMemoryManager::setForce32BitAllocations(bool newValue) {
    force32bitAllocations = newValue;
}

bool WddmMemoryManager::mapAuxGpuVA(GraphicsAllocation *graphicsAllocation) {
    return wddm->updateAuxTable(graphicsAllocation->getGpuAddress(), graphicsAllocation->getDefaultGmm(), true);
}

AlignedMallocRestrictions *WddmMemoryManager::getAlignedMallocRestrictions() {
    return &mallocRestrictions;
}

bool WddmMemoryManager::createWddmAllocation(WddmAllocation *allocation, void *requiredGpuPtr) {
    auto status = createGpuAllocationsWithRetry(allocation);
    if (!status) {
        return false;
    }
    obtainGpuAddressIfNeeded(allocation);
    bool mapSuccess = mapGpuVirtualAddressWithRetry(allocation, requiredGpuPtr);
    if (!mapSuccess) {
        if (allocation->preferredGpuAddress) {
            wddm->freeGpuVirtualAddress(allocation->preferredGpuAddress, allocation->getAlignedSize());
        }
        wddm->destroyAllocations(allocation->getHandles().data(), allocation->getNumHandles(), allocation->resourceHandle);
        return false;
    }
    return true;
}

bool WddmMemoryManager::createGpuAllocationsWithRetry(WddmAllocation *allocation) {
    for (auto handleId = 0u; handleId < allocation->getNumHandles(); handleId++) {
        auto status = wddm->createAllocation(allocation->getAlignedCpuPtr(), allocation->getGmm(handleId), allocation->getHandleToModify(handleId));
        if (status == STATUS_GRAPHICS_NO_VIDEO_MEMORY && deferredDeleter) {
            deferredDeleter->drain(true);
            status = wddm->createAllocation(allocation->getAlignedCpuPtr(), allocation->getGmm(handleId), allocation->getHandleToModify(handleId));
        }
        if (status != STATUS_SUCCESS) {
            wddm->destroyAllocations(allocation->getHandles().data(), handleId, allocation->resourceHandle);
            return false;
        }
    }
    return true;
}

bool WddmMemoryManager::mapGpuVirtualAddressWithRetry(WddmAllocation *graphicsAllocation, const void *preferredGpuVirtualAddress) {
    uint32_t numMappedAllocations = mapGpuVirtualAddress(graphicsAllocation, preferredGpuVirtualAddress, 0u);
    if (numMappedAllocations < graphicsAllocation->getNumHandles() && deferredDeleter) {
        deferredDeleter->drain(true);
        numMappedAllocations += mapGpuVirtualAddress(graphicsAllocation, preferredGpuVirtualAddress, numMappedAllocations);
    }
    return numMappedAllocations == graphicsAllocation->getNumHandles();
}

uint32_t WddmMemoryManager::mapGpuVirtualAddress(WddmAllocation *graphicsAllocation, const void *preferredGpuVirtualAddress, uint32_t startingIndex) {
    auto numMappedAllocations = 0u;
    D3DGPU_VIRTUAL_ADDRESS addressToMap = reinterpret_cast<D3DGPU_VIRTUAL_ADDRESS>(preferredGpuVirtualAddress);
    auto heapIndex = selectHeap(graphicsAllocation, preferredGpuVirtualAddress != nullptr, executionEnvironment.isFullRangeSvm());
    if (!executionEnvironment.isFullRangeSvm()) {
        addressToMap = 0u;
    }
    if (graphicsAllocation->preferredGpuAddress) {
        addressToMap = graphicsAllocation->preferredGpuAddress;
    }
    for (auto handleId = startingIndex; handleId < graphicsAllocation->getNumHandles(); handleId++) {

        if (!wddm->mapGpuVirtualAddress(graphicsAllocation->getGmm(handleId), graphicsAllocation->getHandles()[handleId],
                                        gfxPartition.getHeapMinimalAddress(heapIndex), gfxPartition.getHeapLimit(heapIndex),
                                        addressToMap, graphicsAllocation->getGpuAddressToModify())) {
            return numMappedAllocations;
        }
        numMappedAllocations++;
    }
    return numMappedAllocations;
}

void WddmMemoryManager::obtainGpuAddressIfNeeded(WddmAllocation *allocation) {
    if (allocation->getNumHandles() > 1u) {
        auto heapIndex = selectHeap(allocation, false, executionEnvironment.isFullRangeSvm());
        allocation->preferredGpuAddress = wddm->reserveGpuVirtualAddress(gfxPartition.getHeapMinimalAddress(heapIndex),
                                                                         gfxPartition.getHeapLimit(heapIndex), allocation->getAlignedSize());
    }
}

void *WddmMemoryManager::reserveCpuAddressRange(size_t size) {
    void *reservePtr = nullptr;
    wddm->reserveValidAddressRange(size, reservePtr);
    return reservePtr;
}

void WddmMemoryManager::releaseReservedCpuAddressRange(void *reserved, size_t size) {
    wddm->releaseReservedAddress(reserved);
}

} // namespace NEO
