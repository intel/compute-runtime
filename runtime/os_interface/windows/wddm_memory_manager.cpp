/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/device/device.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/resource_info.h"
#include "runtime/helpers/surface_formats.h"
#include "runtime/memory_manager/deferred_deleter.h"
#include "runtime/memory_manager/deferrable_deletion.h"
#include "runtime/os_interface/windows/wddm_memory_manager.h"
#include "runtime/os_interface/windows/wddm_allocation.h"
#include "runtime/os_interface/windows/wddm/wddm.h"
#include <algorithm>

namespace OCLRT {

WddmMemoryManager::~WddmMemoryManager() {
    applyCommonCleanup();
    delete this->wddm;
}

WddmMemoryManager::WddmMemoryManager(bool enable64kbPages, Wddm *wddm) : MemoryManager(enable64kbPages), residencyLock(false) {
    DEBUG_BREAK_IF(wddm == nullptr);
    this->wddm = wddm;
    allocator32Bit = std::unique_ptr<Allocator32bit>(new Allocator32bit(wddm->getHeap32Base(), wddm->getHeap32Size()));
    wddm->registerTrimCallback(trimCallback, this);
    asyncDeleterEnabled = DebugManager.flags.EnableDeferredDeleter.get();
    if (asyncDeleterEnabled)
        deferredDeleter = createDeferredDeleter();
    mallocRestrictions.minAddress = wddm->getWddmMinAddress();
}

void APIENTRY WddmMemoryManager::trimCallback(_Inout_ D3DKMT_TRIMNOTIFICATION *trimNotification) {
    WddmMemoryManager *wddmMemMngr = (WddmMemoryManager *)trimNotification->Context;
    DEBUG_BREAK_IF(wddmMemMngr == nullptr);

    wddmMemMngr->trimResidency(trimNotification->Flags, trimNotification->NumBytesToTrim);
}

GraphicsAllocation *WddmMemoryManager::allocateGraphicsMemoryForImage(ImageInfo &imgInfo, Gmm *gmm) {
    if (!GmmHelper::allowTiling(*imgInfo.imgDesc) && imgInfo.mipCount == 0) {
        delete gmm;
        return allocateGraphicsMemory(imgInfo.size, MemoryConstants::preferredAlignment);
    }
    auto allocation = new WddmAllocation(nullptr, imgInfo.size, nullptr);
    allocation->gmm = gmm;

    if (!WddmMemoryManager::createWddmAllocation(allocation, AllocationOrigin::EXTERNAL_ALLOCATION)) {
        delete allocation;
        return nullptr;
    }
    return allocation;
}

GraphicsAllocation *WddmMemoryManager::allocateGraphicsMemory64kb(size_t size, size_t alignment, bool forcePin) {
    size_t sizeAligned = alignUp(size, MemoryConstants::pageSize64k);
    Gmm *gmm = nullptr;

    auto wddmAllocation = new WddmAllocation(nullptr, sizeAligned, nullptr, sizeAligned, nullptr);

    gmm = GmmHelper::create(nullptr, sizeAligned, false);
    wddmAllocation->gmm = gmm;

    if (!wddm->createAllocation64k(wddmAllocation)) {
        delete gmm;
        delete wddmAllocation;
        return nullptr;
    }

    auto cpuPtr = lockResource(wddmAllocation);
    wddmAllocation->setLocked(true);

    wddmAllocation->setAlignedCpuPtr(cpuPtr);
    // 64kb map is not needed
    auto status = wddm->mapGpuVirtualAddress(wddmAllocation, cpuPtr, sizeAligned, false, false, false);
    DEBUG_BREAK_IF(!status);
    wddmAllocation->setCpuPtrAndGpuAddress(cpuPtr, (uint64_t)wddmAllocation->gpuPtr);

    return wddmAllocation;
}

GraphicsAllocation *WddmMemoryManager::allocateGraphicsMemory(size_t size, size_t alignment, bool forcePin, bool uncacheable) {
    size_t newAlignment = alignment ? alignUp(alignment, MemoryConstants::pageSize) : MemoryConstants::pageSize;
    size_t sizeAligned = size ? alignUp(size, MemoryConstants::pageSize) : MemoryConstants::pageSize;
    void *pSysMem = allocateSystemMemory(sizeAligned, newAlignment);
    Gmm *gmm = nullptr;

    if (pSysMem == nullptr) {
        return nullptr;
    }

    auto wddmAllocation = new WddmAllocation(pSysMem, sizeAligned, pSysMem, sizeAligned, nullptr);
    wddmAllocation->cpuPtrAllocated = true;

    gmm = GmmHelper::create(pSysMem, sizeAligned, uncacheable);

    wddmAllocation->gmm = gmm;

    if (!createWddmAllocation(wddmAllocation, AllocationOrigin::EXTERNAL_ALLOCATION)) {
        delete gmm;
        delete wddmAllocation;
        freeSystemMemory(pSysMem);
        return nullptr;
    }

    return wddmAllocation;
}

GraphicsAllocation *WddmMemoryManager::allocateGraphicsMemory(size_t size, const void *ptrArg) {
    void *ptr = const_cast<void *>(ptrArg);

    if (ptr == nullptr) {
        DEBUG_BREAK_IF(true);
        return nullptr;
    }

    if (mallocRestrictions.minAddress > reinterpret_cast<uintptr_t>(ptrArg)) {
        void *reserve = nullptr;
        void *ptrAligned = alignDown(ptr, MemoryConstants::allocationAlignment);
        size_t sizeAligned = alignSizeWholePage(ptr, size);
        size_t offset = ptrDiff(ptr, ptrAligned);

        if (!wddm->reserveValidAddressRange(sizeAligned, reserve)) {
            return nullptr;
        }

        auto allocation = new WddmAllocation(ptr, size, ptrAligned, sizeAligned, reserve);
        allocation->allocationOffset = offset;

        Gmm *gmm = GmmHelper::create(ptrAligned, sizeAligned, false);
        allocation->gmm = gmm;
        if (createWddmAllocation(allocation, AllocationOrigin::EXTERNAL_ALLOCATION)) {
            return allocation;
        }
        freeGraphicsMemory(allocation);
        return nullptr;
    }

    return MemoryManager::allocateGraphicsMemory(size, ptr);
}

GraphicsAllocation *WddmMemoryManager::allocate32BitGraphicsMemory(size_t size, void *ptr, AllocationOrigin allocationOrigin) {
    Gmm *gmm = nullptr;
    const void *ptrAligned = nullptr;
    size_t sizeAligned = size;
    void *pSysMem = nullptr;
    size_t offset = 0;
    bool cpuPtrAllocated = false;

    if (ptr) {
        ptrAligned = alignDown(ptr, MemoryConstants::allocationAlignment);
        sizeAligned = alignSizeWholePage(ptr, size);
        offset = ptrDiff(ptr, ptrAligned);
    } else {
        sizeAligned = alignUp(size, MemoryConstants::allocationAlignment);
        pSysMem = allocateSystemMemory(sizeAligned, MemoryConstants::allocationAlignment);
        if (pSysMem == nullptr) {
            return nullptr;
        }
        ptrAligned = pSysMem;
        cpuPtrAllocated = true;
    }

    auto wddmAllocation = new WddmAllocation(const_cast<void *>(ptrAligned), sizeAligned, const_cast<void *>(ptrAligned), sizeAligned, nullptr);
    wddmAllocation->cpuPtrAllocated = cpuPtrAllocated;
    wddmAllocation->is32BitAllocation = true;
    wddmAllocation->allocationOffset = offset;

    gmm = GmmHelper::create(ptrAligned, sizeAligned, false);
    wddmAllocation->gmm = gmm;

    if (!createWddmAllocation(wddmAllocation, allocationOrigin)) {
        delete gmm;
        delete wddmAllocation;
        freeSystemMemory(pSysMem);
        return nullptr;
    }

    wddmAllocation->is32BitAllocation = true;
    auto baseAddress = allocationOrigin == AllocationOrigin::EXTERNAL_ALLOCATION ? allocator32Bit->getBase() : this->wddm->getGfxPartition().Heap32[1].Base;
    wddmAllocation->gpuBaseAddress = GmmHelper::canonize(baseAddress);

    return wddmAllocation;
}

GraphicsAllocation *WddmMemoryManager::createAllocationFromHandle(osHandle handle, bool requireSpecificBitness, bool ntHandle) {
    auto allocation = new WddmAllocation(nullptr, 0, handle);
    bool is32BitAllocation = false;

    bool status = ntHandle ? wddm->openNTHandle((HANDLE)((UINT_PTR)handle), allocation)
                           : wddm->openSharedHandle(handle, allocation);

    if (!status) {
        delete allocation;
        return nullptr;
    }

    // Shared objects are passed without size
    size_t size = allocation->gmm->gmmResourceInfo->getSizeAllocation();
    allocation->setSize(size);

    void *ptr = nullptr;
    if (is32bit) {
        if (!wddm->reserveValidAddressRange(size, ptr)) {
            delete allocation;
            return nullptr;
        }
        allocation->setReservedAddress(ptr);
    } else if (requireSpecificBitness && this->force32bitAllocations) {
        is32BitAllocation = true;
        allocation->is32BitAllocation = true;
        allocation->gpuBaseAddress = GmmHelper::canonize(allocator32Bit->getBase());
    }
    status = wddm->mapGpuVirtualAddress(allocation, ptr, size, is32BitAllocation, false, false);
    DEBUG_BREAK_IF(!status);
    allocation->setGpuAddress(allocation->gpuPtr);

    return allocation;
}

GraphicsAllocation *WddmMemoryManager::createGraphicsAllocationFromSharedHandle(osHandle handle, bool requireSpecificBitness, bool /*isReused*/) {
    return createAllocationFromHandle(handle, requireSpecificBitness, false);
}

GraphicsAllocation *WddmMemoryManager::createGraphicsAllocationFromNTHandle(void *handle) {
    return createAllocationFromHandle((osHandle)((UINT_PTR)handle), false, true);
}

void WddmMemoryManager::addAllocationToHostPtrManager(GraphicsAllocation *gfxAllocation) {
    WddmAllocation *wddmMemory = static_cast<WddmAllocation *>(gfxAllocation);
    FragmentStorage fragment = {};
    fragment.driverAllocation = true;
    fragment.fragmentCpuPointer = gfxAllocation->getUnderlyingBuffer();
    fragment.fragmentSize = alignUp(gfxAllocation->getUnderlyingBufferSize(), MemoryConstants::pageSize);

    fragment.osInternalStorage = new OsHandle();
    fragment.osInternalStorage->gpuPtr = gfxAllocation->getGpuAddress();
    fragment.osInternalStorage->handle = wddmMemory->handle;
    fragment.osInternalStorage->gmm = gfxAllocation->gmm;
    fragment.residency = &wddmMemory->getResidencyData();
    hostPtrManager.storeFragment(fragment);
}

void WddmMemoryManager::removeAllocationFromHostPtrManager(GraphicsAllocation *gfxAllocation) {
    auto buffer = gfxAllocation->getUnderlyingBuffer();
    auto fragment = hostPtrManager.getFragment(buffer);
    if (fragment && fragment->driverAllocation) {
        OsHandle *osStorageToRelease = fragment->osInternalStorage;
        if (hostPtrManager.releaseHostPtr(buffer)) {
            delete osStorageToRelease;
        }
    }
}

void *WddmMemoryManager::lockResource(GraphicsAllocation *graphicsAllocation) {
    return wddm->lockResource(static_cast<WddmAllocation *>(graphicsAllocation));
};
void WddmMemoryManager::unlockResource(GraphicsAllocation *graphicsAllocation) {
    wddm->unlockResource(static_cast<WddmAllocation *>(graphicsAllocation));
};

void WddmMemoryManager::freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) {
    WddmAllocation *input = static_cast<WddmAllocation *>(gfxAllocation);
    DEBUG_BREAK_IF(!validateAllocation(input));
    if (gfxAllocation == nullptr) {
        return;
    }
    acquireResidencyLock();
    if (input->getTrimCandidateListPosition() != trimListUnusedPosition) {
        removeFromTrimCandidateList(gfxAllocation, true);
    }

    releaseResidencyLock();

    UNRECOVERABLE_IF(gfxAllocation->taskCount != ObjectNotUsed && this->device && this->device->peekCommandStreamReceiver() && gfxAllocation->taskCount > *this->device->getCommandStreamReceiver().getTagAddress());

    if (input->gmm) {
        if (input->gmm->isRenderCompressed) {
            auto status = wddm->updateAuxTable(input->gpuPtr, input->gmm, false);
            DEBUG_BREAK_IF(!status);
        }
        delete input->gmm;
    }

    if (input->peekSharedHandle() == false &&
        input->cpuPtrAllocated == false &&
        input->fragmentsStorage.fragmentCount > 0) {
        cleanGraphicsMemoryCreatedFromHostPtr(gfxAllocation);
    } else {
        D3DKMT_HANDLE *allocationHandles = nullptr;
        uint32_t allocationCount = 0;
        D3DKMT_HANDLE resourceHandle = 0;
        void *cpuPtr = nullptr;
        if (input->peekSharedHandle()) {
            resourceHandle = input->resourceHandle;
        } else {
            allocationHandles = &input->handle;
            allocationCount = 1;
            if (input->cpuPtrAllocated) {
                cpuPtr = input->getAlignedCpuPtr();
            }
        }
        if (input->isLocked()) {
            unlockResource(input);
            input->setLocked(false);
        }
        auto status = tryDeferDeletions(allocationHandles, allocationCount, input->getResidencyData().lastFence, resourceHandle);
        DEBUG_BREAK_IF(!status);
        alignedFreeWrapper(cpuPtr);
    }
    wddm->releaseReservedAddress(input->getReservedAddress());
    delete gfxAllocation;
}

bool WddmMemoryManager::tryDeferDeletions(D3DKMT_HANDLE *handles, uint32_t allocationCount, uint64_t lastFenceValue, D3DKMT_HANDLE resourceHandle) {
    bool status = true;
    if (deferredDeleter) {
        deferredDeleter->deferDeletion(DeferrableDeletion::create(wddm, handles, allocationCount, lastFenceValue, resourceHandle));
    } else {
        status = wddm->destroyAllocations(handles, allocationCount, lastFenceValue, resourceHandle);
    }
    return status;
}

bool WddmMemoryManager::validateAllocation(WddmAllocation *alloc) {
    if (alloc == nullptr)
        return false;
    auto size = alloc->getUnderlyingBufferSize();
    if (alloc->getGpuAddress() == 0u || size == 0 || (alloc->handle == 0 && alloc->fragmentsStorage.fragmentCount == 0))
        return false;
    return true;
}

MemoryManager::AllocationStatus WddmMemoryManager::populateOsHandles(OsHandleStorage &handleStorage) {
    uint32_t allocatedFragmentIndexes[max_fragments_count];
    uint32_t allocatedFragmentsCounter = 0;

    for (unsigned int i = 0; i < max_fragments_count; i++) {
        // If no fragment is present it means it already exists.
        if (!handleStorage.fragmentStorageData[i].osHandleStorage && handleStorage.fragmentStorageData[i].cpuPtr) {
            handleStorage.fragmentStorageData[i].osHandleStorage = new OsHandle();
            handleStorage.fragmentStorageData[i].residency = new ResidencyData();

            handleStorage.fragmentStorageData[i].osHandleStorage->gmm = GmmHelper::create(handleStorage.fragmentStorageData[i].cpuPtr, handleStorage.fragmentStorageData[i].fragmentSize, false);
            allocatedFragmentIndexes[allocatedFragmentsCounter] = i;
            allocatedFragmentsCounter++;
        }
    }
    NTSTATUS result = wddm->createAllocationsAndMapGpuVa(handleStorage);

    if (result == STATUS_GRAPHICS_NO_VIDEO_MEMORY) {
        return AllocationStatus::InvalidHostPointer;
    }

    for (uint32_t i = 0; i < allocatedFragmentsCounter; i++) {
        hostPtrManager.storeFragment(handleStorage.fragmentStorageData[allocatedFragmentIndexes[i]]);
    }

    return AllocationStatus::Success;
}

void WddmMemoryManager::cleanOsHandles(OsHandleStorage &handleStorage) {

    D3DKMT_HANDLE handles[max_fragments_count] = {0};
    auto allocationCount = 0;

    uint64_t lastFenceValue = 0;

    for (unsigned int i = 0; i < max_fragments_count; i++) {
        if (handleStorage.fragmentStorageData[i].freeTheFragment) {
            handles[allocationCount] = handleStorage.fragmentStorageData[i].osHandleStorage->handle;
            handleStorage.fragmentStorageData[i].residency->resident = false;
            allocationCount++;
            lastFenceValue = std::max(handleStorage.fragmentStorageData[i].residency->lastFence, lastFenceValue);
        }
    }

    bool success = tryDeferDeletions(handles, allocationCount, lastFenceValue, 0);

    for (unsigned int i = 0; i < max_fragments_count; i++) {
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

void WddmMemoryManager::obtainGpuAddresFromFragments(WddmAllocation *allocation, OsHandleStorage &handleStorage) {
    if (this->force32bitAllocations && (handleStorage.fragmentCount > 0)) {
        auto hostPtr = allocation->getUnderlyingBuffer();
        auto fragment = hostPtrManager.getFragment(hostPtr);
        if (fragment && fragment->driverAllocation) {
            auto gpuPtr = handleStorage.fragmentStorageData[0].osHandleStorage->gpuPtr;
            for (uint32_t i = 1; i < handleStorage.fragmentCount; i++) {
                if (handleStorage.fragmentStorageData[i].osHandleStorage->gpuPtr < gpuPtr) {
                    gpuPtr = handleStorage.fragmentStorageData[i].osHandleStorage->gpuPtr;
                }
            }
            allocation->allocationOffset = reinterpret_cast<uint64_t>(hostPtr) & MemoryConstants::pageMask;
            allocation->setGpuAddress(gpuPtr);
        }
    }
}

GraphicsAllocation *WddmMemoryManager::createGraphicsAllocation(OsHandleStorage &handleStorage, size_t hostPtrSize, const void *hostPtr) {
    auto allocation = new WddmAllocation(const_cast<void *>(hostPtr), hostPtrSize, const_cast<void *>(hostPtr), hostPtrSize, nullptr);
    allocation->fragmentsStorage = handleStorage;
    obtainGpuAddresFromFragments(allocation, handleStorage);
    return allocation;
}

uint64_t WddmMemoryManager::getSystemSharedMemory() {
    return wddm->getSystemSharedMemory();
}

uint64_t WddmMemoryManager::getMaxApplicationAddress() {
    return wddm->getMaxApplicationAddress();
}

uint64_t WddmMemoryManager::getInternalHeapBaseAddress() {
    return this->wddm->getGfxPartition().Heap32[1].Base;
}

bool WddmMemoryManager::makeResidentResidencyAllocations(ResidencyContainer *allocationsForResidency) {

    auto &residencyAllocations = allocationsForResidency ? *allocationsForResidency : this->residencyAllocations;

    size_t residencyCount = residencyAllocations.size();
    std::unique_ptr<D3DKMT_HANDLE[]> handlesForResidency(new D3DKMT_HANDLE[residencyCount * max_fragments_count]);

    uint32_t totalHandlesCount = 0;

    acquireResidencyLock();

    DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "currentFenceValue =", wddm->getMonitoredFence().currentFenceValue);

    for (uint32_t i = 0; i < residencyCount; i++) {
        WddmAllocation *allocation = reinterpret_cast<WddmAllocation *>(residencyAllocations[i]);
        bool mainResidency = false;
        bool fragmentResidency[3] = {false, false, false};

        mainResidency = allocation->getResidencyData().resident;

        DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "allocation =", allocation, mainResidency ? "resident" : "not resident");

        if (allocation->getTrimCandidateListPosition() != trimListUnusedPosition) {

            DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "allocation =", allocation, "on trimCandidateList");
            removeFromTrimCandidateList(allocation);
        } else {

            for (uint32_t allocationId = 0; allocationId < allocation->fragmentsStorage.fragmentCount; allocationId++) {
                fragmentResidency[allocationId] = allocation->fragmentsStorage.fragmentStorageData[allocationId].residency->resident;

                DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "fragment handle =",
                        allocation->fragmentsStorage.fragmentStorageData[allocationId].osHandleStorage->handle,
                        fragmentResidency[allocationId] ? "resident" : "not resident");
            }
        }

        if (allocation->fragmentsStorage.fragmentCount == 0) {
            if (!mainResidency)
                handlesForResidency[totalHandlesCount++] = allocation->handle;
        } else {
            for (uint32_t allocationId = 0; allocationId < allocation->fragmentsStorage.fragmentCount; allocationId++) {
                if (!fragmentResidency[allocationId])
                    handlesForResidency[totalHandlesCount++] = allocation->fragmentsStorage.fragmentStorageData[allocationId].osHandleStorage->handle;
            }
        }
    }

    bool result = true;
    if (totalHandlesCount) {
        uint64_t bytesToTrim = 0;
        while ((result = wddm->makeResident(handlesForResidency.get(), totalHandlesCount, false, &bytesToTrim)) == false) {
            this->memoryBudgetExhausted = true;
            bool trimmingDone = trimResidencyToBudget(bytesToTrim);
            bool cantTrimFurther = !trimmingDone;
            if (cantTrimFurther) {
                result = wddm->makeResident(handlesForResidency.get(), totalHandlesCount, true, &bytesToTrim);
                break;
            }
        }
    }

    if (result == true) {
        for (uint32_t i = 0; i < residencyCount; i++) {
            WddmAllocation *allocation = reinterpret_cast<WddmAllocation *>(residencyAllocations[i]);
            // Update fence value not to early destroy / evict allocation
            allocation->getResidencyData().lastFence = wddm->getMonitoredFence().currentFenceValue;
            allocation->getResidencyData().resident = true;

            for (uint32_t allocationId = 0; allocationId < allocation->fragmentsStorage.fragmentCount; allocationId++) {
                allocation->fragmentsStorage.fragmentStorageData[allocationId].residency->resident = true;
                // Update fence value not to remove the fragment referenced by different GA in trimming callback
                allocation->fragmentsStorage.fragmentStorageData[allocationId].residency->lastFence = wddm->getMonitoredFence().currentFenceValue;
            }
        }
    }

    releaseResidencyLock();

    return result;
}

void WddmMemoryManager::makeNonResidentEvictionAllocations() {

    acquireResidencyLock();

    size_t residencyCount = evictionAllocations.size();

    for (uint32_t i = 0; i < residencyCount; i++) {
        WddmAllocation *allocation = reinterpret_cast<WddmAllocation *>(evictionAllocations[i]);

        addToTrimCandidateList(allocation);
    }

    releaseResidencyLock();
}

void WddmMemoryManager::removeFromTrimCandidateList(GraphicsAllocation *allocation, bool compactList) {
    WddmAllocation *wddmAllocation = (WddmAllocation *)allocation;
    size_t position = wddmAllocation->getTrimCandidateListPosition();

    DEBUG_BREAK_IF(!(trimCandidatesCount > (trimCandidatesCount - 1)));
    DEBUG_BREAK_IF(trimCandidatesCount > trimCandidateList.size());

    trimCandidatesCount--;

    trimCandidateList[position] = nullptr;

    checkTrimCandidateCount();

    if (position == trimCandidateList.size() - 1) {
        size_t erasePosition = position;

        if (position == 0) {
            trimCandidateList.resize(0);
        } else {
            while (trimCandidateList[erasePosition] == nullptr && erasePosition > 0) {
                erasePosition--;
            }

            size_t sizeRemaining = erasePosition + 1;
            if (erasePosition == 0 && trimCandidateList[erasePosition] == nullptr) {
                sizeRemaining = 0;
            }

            trimCandidateList.resize(sizeRemaining);
        }
    }
    wddmAllocation->setTrimCandidateListPosition(trimListUnusedPosition);

    if (compactList && checkTrimCandidateListCompaction()) {
        compactTrimCandidateList();
    }

    checkTrimCandidateCount();
}

void WddmMemoryManager::addToTrimCandidateList(GraphicsAllocation *allocation) {
    WddmAllocation *wddmAllocation = (WddmAllocation *)allocation;
    size_t position = trimCandidateList.size();

    DEBUG_BREAK_IF(trimCandidatesCount > trimCandidateList.size());

    if (wddmAllocation->getTrimCandidateListPosition() == trimListUnusedPosition) {
        trimCandidatesCount++;
        trimCandidateList.push_back(allocation);
        wddmAllocation->setTrimCandidateListPosition(position);
    }

    checkTrimCandidateCount();
}

void WddmMemoryManager::compactTrimCandidateList() {

    size_t size = trimCandidateList.size();
    size_t freePosition = 0;

    if (size == 0 || size == trimCandidatesCount) {
        return;
    }

    DEBUG_BREAK_IF(!(trimCandidateList[size - 1] != nullptr));

    uint32_t previousCount = trimCandidatesCount;
    DEBUG_BREAK_IF(trimCandidatesCount > trimCandidateList.size());

    while (freePosition < trimCandidatesCount && trimCandidateList[freePosition] != nullptr)
        freePosition++;

    for (uint32_t i = 1; i < size; i++) {

        if (trimCandidateList[i] != nullptr && freePosition < i) {
            trimCandidateList[freePosition] = trimCandidateList[i];
            trimCandidateList[i] = nullptr;
            ((WddmAllocation *)trimCandidateList[freePosition])->setTrimCandidateListPosition(freePosition);
            freePosition++;

            // Last element was moved, erase elements from freePosition
            if (i == size - 1) {
                trimCandidateList.resize(freePosition);
            }
        }
    }
    DEBUG_BREAK_IF(trimCandidatesCount > trimCandidateList.size());
    DEBUG_BREAK_IF(trimCandidatesCount != previousCount);

    checkTrimCandidateCount();
}

void WddmMemoryManager::trimResidency(D3DDDI_TRIMRESIDENCYSET_FLAGS flags, uint64_t bytes) {
    if (flags.PeriodicTrim) {
        bool periodicTrimDone = false;
        D3DKMT_HANDLE fragmentEvictHandles[3] = {0};
        uint64_t sizeToTrim = 0;

        acquireResidencyLock();

        WddmAllocation *wddmAllocation = nullptr;
        while ((wddmAllocation = getTrimCandidateHead()) != nullptr) {

            DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "lastPeriodicTrimFenceValue = ", lastPeriodicTrimFenceValue);

            // allocation was not used from last periodic trim
            if ((wddmAllocation)->getResidencyData().lastFence <= lastPeriodicTrimFenceValue) {

                DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "allocation: handle =", wddmAllocation->handle, "lastFence =", (wddmAllocation)->getResidencyData().lastFence);

                uint32_t fragmentsToEvict = 0;

                if (wddmAllocation->fragmentsStorage.fragmentCount == 0) {
                    DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "Evict allocation: handle =", wddmAllocation->handle, "lastFence =", (wddmAllocation)->getResidencyData().lastFence);
                    wddm->evict(&wddmAllocation->handle, 1, sizeToTrim);
                }

                for (uint32_t allocationId = 0; allocationId < wddmAllocation->fragmentsStorage.fragmentCount; allocationId++) {
                    if (wddmAllocation->fragmentsStorage.fragmentStorageData[allocationId].residency->lastFence <= lastPeriodicTrimFenceValue) {

                        DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "Evict fragment: handle =", wddmAllocation->fragmentsStorage.fragmentStorageData[allocationId].osHandleStorage->handle, "lastFence =", wddmAllocation->fragmentsStorage.fragmentStorageData[allocationId].residency->lastFence);

                        fragmentEvictHandles[fragmentsToEvict++] = wddmAllocation->fragmentsStorage.fragmentStorageData[allocationId].osHandleStorage->handle;
                        wddmAllocation->fragmentsStorage.fragmentStorageData[allocationId].residency->resident = false;
                    }
                }

                if (fragmentsToEvict != 0) {
                    wddm->evict((D3DKMT_HANDLE *)fragmentEvictHandles, fragmentsToEvict, sizeToTrim);
                }

                wddmAllocation->getResidencyData().resident = false;
                removeFromTrimCandidateList(wddmAllocation);
            } else {
                periodicTrimDone = true;
                break;
            }
        }

        if (checkTrimCandidateListCompaction()) {
            compactTrimCandidateList();
        }

        releaseResidencyLock();
    }

    if (flags.TrimToBudget) {

        acquireResidencyLock();

        trimResidencyToBudget(bytes);

        releaseResidencyLock();
    }

    if (flags.PeriodicTrim || flags.RestartPeriodicTrim) {
        lastPeriodicTrimFenceValue = *wddm->getMonitoredFence().cpuAddress;
        DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "updated lastPeriodicTrimFenceValue =", lastPeriodicTrimFenceValue);
    }
}

void WddmMemoryManager::checkTrimCandidateCount() {
    if (DebugManager.flags.ResidencyDebugEnable.get()) {
        uint32_t sum = 0;
        for (size_t i = 0; i < trimCandidateList.size(); i++) {
            if (trimCandidateList[i] != nullptr) {
                sum++;
            }
        }
        DEBUG_BREAK_IF(sum != trimCandidatesCount);
    }
}

bool WddmMemoryManager::checkTrimCandidateListCompaction() {
    if (2 * trimCandidatesCount <= trimCandidateList.size()) {
        return true;
    }
    return false;
}

bool WddmMemoryManager::trimResidencyToBudget(uint64_t bytes) {
    bool trimToBudgetDone = false;
    D3DKMT_HANDLE fragmentEvictHandles[3] = {0};
    uint64_t numberOfBytesToTrim = bytes;
    WddmAllocation *wddmAllocation = nullptr;

    trimToBudgetDone = (numberOfBytesToTrim == 0);

    while (!trimToBudgetDone) {
        uint64_t lastFence = 0;
        wddmAllocation = getTrimCandidateHead();

        if (wddmAllocation == nullptr) {
            break;
        }

        lastFence = wddmAllocation->getResidencyData().lastFence;

        if (lastFence <= wddm->getMonitoredFence().lastSubmittedFence) {
            uint32_t fragmentsToEvict = 0;
            uint64_t sizeEvicted = 0;
            uint64_t sizeToTrim = 0;

            if (lastFence > *wddm->getMonitoredFence().cpuAddress) {
                wddm->waitFromCpu(lastFence);
            }

            if (wddmAllocation->fragmentsStorage.fragmentCount == 0) {
                wddm->evict(&wddmAllocation->handle, 1, sizeToTrim);

                sizeEvicted = wddmAllocation->getAlignedSize();
            } else {
                for (uint32_t allocationId = 0; allocationId < wddmAllocation->fragmentsStorage.fragmentCount; allocationId++) {
                    if (wddmAllocation->fragmentsStorage.fragmentStorageData[allocationId].residency->lastFence <= wddm->getMonitoredFence().lastSubmittedFence) {
                        fragmentEvictHandles[fragmentsToEvict++] = wddmAllocation->fragmentsStorage.fragmentStorageData[allocationId].osHandleStorage->handle;
                    }
                }

                if (fragmentsToEvict != 0) {
                    wddm->evict((D3DKMT_HANDLE *)fragmentEvictHandles, fragmentsToEvict, sizeToTrim);

                    for (uint32_t allocationId = 0; allocationId < wddmAllocation->fragmentsStorage.fragmentCount; allocationId++) {
                        if (wddmAllocation->fragmentsStorage.fragmentStorageData[allocationId].residency->lastFence <= wddm->getMonitoredFence().lastSubmittedFence) {
                            wddmAllocation->fragmentsStorage.fragmentStorageData[allocationId].residency->resident = false;
                            sizeEvicted += wddmAllocation->fragmentsStorage.fragmentStorageData[allocationId].fragmentSize;
                        }
                    }
                }
            }

            if (sizeEvicted >= numberOfBytesToTrim) {
                numberOfBytesToTrim = 0;
            } else {
                numberOfBytesToTrim -= sizeEvicted;
            }

            wddmAllocation->getResidencyData().resident = false;
            removeFromTrimCandidateList(wddmAllocation);
            trimToBudgetDone = (numberOfBytesToTrim == 0);
        } else {
            trimToBudgetDone = true;
        }
    }

    if (bytes > numberOfBytesToTrim && checkTrimCandidateListCompaction()) {
        compactTrimCandidateList();
    }

    return numberOfBytesToTrim == 0;
}

bool WddmMemoryManager::mapAuxGpuVA(GraphicsAllocation *graphicsAllocation) {
    return wddm->updateAuxTable(graphicsAllocation->getGpuAddress(), graphicsAllocation->gmm, true);
}

AlignedMallocRestrictions *WddmMemoryManager::getAlignedMallocRestrictions() {
    return &mallocRestrictions;
}

bool WddmMemoryManager::createWddmAllocation(WddmAllocation *allocation, AllocationOrigin allocationOrigin) {
    bool useHeap1 = (allocationOrigin == AllocationOrigin::INTERNAL_ALLOCATION);
    auto wddmSuccess = wddm->createAllocation(allocation);
    if (wddmSuccess == STATUS_GRAPHICS_NO_VIDEO_MEMORY && deferredDeleter) {
        deferredDeleter->drain(true);
        wddmSuccess = wddm->createAllocation(allocation);
    }
    if (wddmSuccess == STATUS_SUCCESS) {
        bool mapSuccess = wddm->mapGpuVirtualAddress(allocation, allocation->getAlignedCpuPtr(), allocation->getAlignedSize(), allocation->is32BitAllocation, false, useHeap1);
        if (!mapSuccess && deferredDeleter) {
            deferredDeleter->drain(true);
            mapSuccess = wddm->mapGpuVirtualAddress(allocation, allocation->getAlignedCpuPtr(), allocation->getAlignedSize(), allocation->is32BitAllocation, false, useHeap1);
        }
        if (!mapSuccess) {
            wddm->destroyAllocations(&allocation->handle, 1, 0, allocation->resourceHandle);
            wddmSuccess = STATUS_UNSUCCESSFUL;
        }
        allocation->setGpuAddress(allocation->gpuPtr);
    }
    return (wddmSuccess == STATUS_SUCCESS);
}

} // namespace OCLRT
