/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/device/device.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/options.h"
#include "runtime/memory_manager/host_ptr_manager.h"
#include "runtime/os_interface/32bit_memory.h"
#include "runtime/os_interface/linux/drm_allocation.h"
#include "runtime/os_interface/linux/drm_buffer_object.h"
#include "runtime/os_interface/linux/drm_memory_manager.h"
#include "runtime/os_interface/linux/os_context_linux.h"
#include "runtime/helpers/surface_formats.h"
#include <cstring>
#include <iostream>

#include "drm/i915_drm.h"
#include "drm/drm.h"

#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/resource_info.h"

namespace OCLRT {

DrmMemoryManager::DrmMemoryManager(Drm *drm, gemCloseWorkerMode mode, bool forcePinAllowed, bool validateHostPtrMemory, ExecutionEnvironment &executionEnvironment) : MemoryManager(false, false, executionEnvironment),
                                                                                                                                                                      drm(drm),
                                                                                                                                                                      pinBB(nullptr),
                                                                                                                                                                      forcePinEnabled(forcePinAllowed),
                                                                                                                                                                      validateHostPtrMemory(validateHostPtrMemory) {
    MemoryManager::virtualPaddingAvailable = true;
    if (mode != gemCloseWorkerMode::gemCloseWorkerInactive) {
        gemCloseWorker.reset(new DrmGemCloseWorker(*this));
    }

    auto mem = alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize);
    DEBUG_BREAK_IF(mem == nullptr);

    if (forcePinEnabled || validateHostPtrMemory) {
        pinBB = allocUserptr(reinterpret_cast<uintptr_t>(mem), MemoryConstants::pageSize, 0, true);
    }

    if (!pinBB) {
        alignedFree(mem);
        DEBUG_BREAK_IF(true);
        UNRECOVERABLE_IF(validateHostPtrMemory);
    } else {
        pinBB->isAllocated = true;
    }

    initInternalRangeAllocator(platformDevices[0]->capabilityTable.gpuAddressSpace);
}

DrmMemoryManager::~DrmMemoryManager() {
    if (this->limitedGpuAddressRangeAllocator) {
        // internal 32bit allocator size is reserved 1 page (offset to 0x0) when the allocator created.
        uint64_t size = 4 * MemoryConstants::gigaByte - MemoryConstants::pageSize;
        this->limitedGpuAddressRangeAllocator->free(this->internal32bitAllocator->getBase(), size);
    }
    applyCommonCleanup();
    if (gemCloseWorker) {
        gemCloseWorker->close(false);
    }
    if (pinBB) {
        unreference(pinBB);
        pinBB = nullptr;
    }
}

void DrmMemoryManager::initInternalRangeAllocator(size_t gpuRange) {
    if (gpuRange < MemoryConstants::max48BitAddress) {
        // set the allocator with the whole reduced address space range
        this->limitedGpuAddressRangeAllocator.reset(new AllocatorLimitedRange(0, gpuRange));

        // reserve 4G range for internal 32bit allocator, leave 1 page (offset to 0x0) to avoid mistakenly
        // treating 0x0 as null pointer.
        uint64_t size = 4 * MemoryConstants::gigaByte;
        uint64_t internal32bitRange = size - MemoryConstants::pageSize;
        uint64_t internal32bitBase = this->limitedGpuAddressRangeAllocator->allocate(size) - internal32bitRange;
        internal32bitAllocator.reset(new Allocator32bit(internal32bitBase, internal32bitRange));
    } else {
        // when in full range space, set the internal32bitAllocator using 32bit addressing allocator.
        internal32bitAllocator.reset(new Allocator32bit);
    }
}

void DrmMemoryManager::eraseSharedBufferObject(OCLRT::BufferObject *bo) {
    auto it = std::find(sharingBufferObjects.begin(), sharingBufferObjects.end(), bo);
    //If an object isReused = true, it must be in the vector
    DEBUG_BREAK_IF(it == sharingBufferObjects.end());
    sharingBufferObjects.erase(it);
}

void DrmMemoryManager::pushSharedBufferObject(OCLRT::BufferObject *bo) {
    bo->isReused = true;
    sharingBufferObjects.push_back(bo);
}

uint32_t DrmMemoryManager::unreference(OCLRT::BufferObject *bo, bool synchronousDestroy) {
    if (!bo)
        return -1;

    if (synchronousDestroy) {
        while (bo->refCount > 1)
            ;
    }

    std::unique_lock<std::mutex> lock(mtx, std::defer_lock);
    if (bo->isReused) {
        lock.lock();
    }

    uint32_t r = bo->refCount.fetch_sub(1);

    if (r == 1) {
        auto unmapSize = bo->peekUnmapSize();
        auto address = bo->isAllocated || unmapSize > 0 ? bo->address : nullptr;
        auto allocatorType = bo->peekAllocationType();

        if (bo->isReused) {
            eraseSharedBufferObject(bo);
        }

        bo->close();

        if (lock) {
            lock.unlock();
        }

        delete bo;
        if (address) {
            if (unmapSize) {
                releaseGpuRange(address, unmapSize, allocatorType);
            } else {
                alignedFreeWrapper(address);
            }
        }
    }
    return r;
}

uint64_t DrmMemoryManager::acquireGpuRange(size_t &size, StorageAllocatorType &storageType, bool specificBitness) {
    if (specificBitness && this->force32bitAllocations) {
        storageType = BIT32_ALLOCATOR_EXTERNAL;
        return this->allocator32Bit->allocate(size);
    }

    if (limitedGpuAddressRangeAllocator.get()) {
        storageType = INTERNAL_ALLOCATOR_WITH_DYNAMIC_BITRANGE;
        return limitedGpuAddressRangeAllocator->allocate(size);
    }

    storageType = MMAP_ALLOCATOR;
    return reinterpret_cast<uint64_t>(mmapFunction(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0));
}

void DrmMemoryManager::releaseGpuRange(void *address, size_t unmapSize, StorageAllocatorType allocatorType) {
    if (allocatorType == MMAP_ALLOCATOR) {
        munmapFunction(address, unmapSize);
        return;
    }

    uint64_t graphicsAddress = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(address));

    if (allocatorType == BIT32_ALLOCATOR_EXTERNAL) {
        allocator32Bit->free(graphicsAddress, unmapSize);
        return;
    }

    if (allocatorType == BIT32_ALLOCATOR_INTERNAL) {
        internal32bitAllocator->free(graphicsAddress, unmapSize);
        return;
    }

    UNRECOVERABLE_IF(allocatorType != INTERNAL_ALLOCATOR_WITH_DYNAMIC_BITRANGE);
    limitedGpuAddressRangeAllocator->free(graphicsAddress, unmapSize);
}

OCLRT::BufferObject *DrmMemoryManager::allocUserptr(uintptr_t address, size_t size, uint64_t flags, bool softpin) {
    drm_i915_gem_userptr userptr = {};
    userptr.user_ptr = address;
    userptr.user_size = size;
    userptr.flags = static_cast<uint32_t>(flags);

    if (this->drm->ioctl(DRM_IOCTL_I915_GEM_USERPTR, &userptr) != 0) {
        return nullptr;
    }

    auto res = new (std::nothrow) BufferObject(this->drm, userptr.handle, false);
    if (!res) {
        DEBUG_BREAK_IF(true);
        return nullptr;
    }
    res->size = size;
    res->address = reinterpret_cast<void *>(address);
    res->softPin(address);

    return res;
}

DrmAllocation *DrmMemoryManager::createGraphicsAllocation(OsHandleStorage &handleStorage, size_t hostPtrSize, const void *hostPtr) {
    auto allocation = new DrmAllocation(nullptr, const_cast<void *>(hostPtr), hostPtrSize, MemoryPool::System4KBPages, getOsContextCount(), false);
    allocation->fragmentsStorage = handleStorage;
    return allocation;
}

DrmAllocation *DrmMemoryManager::allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) {
    const size_t minAlignment = MemoryConstants::allocationAlignment;
    size_t cAlignment = alignUp(std::max(allocationData.alignment, minAlignment), minAlignment);
    // When size == 0 allocate allocationAlignment
    // It's needed to prevent overlapping pages with user pointers
    size_t cSize = std::max(alignUp(allocationData.size, minAlignment), minAlignment);

    auto res = alignedMallocWrapper(cSize, cAlignment);

    if (!res)
        return nullptr;

    BufferObject *bo = allocUserptr(reinterpret_cast<uintptr_t>(res), cSize, 0, true);

    if (!bo) {
        alignedFreeWrapper(res);
        return nullptr;
    }

    bo->isAllocated = true;
    if (forcePinEnabled && pinBB != nullptr && allocationData.flags.forcePin && allocationData.size >= this->pinThreshold) {
        pinBB->pin(&bo, 1, getDefaultCommandStreamReceiver(0)->getOsContext().get()->getDrmContextId());
    }
    return new DrmAllocation(bo, res, cSize, MemoryPool::System4KBPages, getOsContextCount(), allocationData.flags.multiOsContextCapable);
}

DrmAllocation *DrmMemoryManager::allocateGraphicsMemoryWithHostPtr(const AllocationData &allocationData) {
    auto res = static_cast<DrmAllocation *>(MemoryManager::allocateGraphicsMemoryWithHostPtr(allocationData));

    bool forcePinAllowed = res != nullptr && pinBB != nullptr && forcePinEnabled && allocationData.flags.forcePin && allocationData.size >= this->pinThreshold;
    if (!validateHostPtrMemory && forcePinAllowed) {
        BufferObject *boArray[] = {res->getBO()};
        pinBB->pin(boArray, 1, getDefaultCommandStreamReceiver(0)->getOsContext().get()->getDrmContextId());
    }
    return res;
}

DrmAllocation *DrmMemoryManager::allocateGraphicsMemoryForNonSvmHostPtr(size_t size, void *cpuPtr) {
    if ((size == 0) || !cpuPtr)
        return nullptr;

    auto alignedPtr = alignDown(reinterpret_cast<char *>(cpuPtr), MemoryConstants::pageSize);
    auto alignedSize = alignSizeWholePage(cpuPtr, size);
    auto offsetInPage = reinterpret_cast<char *>(cpuPtr) - alignedPtr;

    StorageAllocatorType allocType;
    auto gpuVirtualAddress = acquireGpuRange(alignedSize, allocType, false);
    if (!gpuVirtualAddress) {
        return nullptr;
    }

    BufferObject *bo = allocUserptr(reinterpret_cast<uintptr_t>(alignedPtr), alignedSize, 0, true);
    if (!bo) {
        releaseGpuRange(reinterpret_cast<void *>(gpuVirtualAddress), alignedSize, allocType);
        return nullptr;
    }

    bo->isAllocated = false;
    bo->setUnmapSize(alignedSize);
    bo->address = reinterpret_cast<void *>(gpuVirtualAddress);
    uintptr_t offset = (uintptr_t)bo->address;
    bo->softPin((uint64_t)offset);
    bo->setAllocationType(allocType);

    auto allocation = new DrmAllocation(bo, cpuPtr, reinterpret_cast<uint64_t>(alignedPtr), size, MemoryPool::System4KBPages, getOsContextCount(), false);
    allocation->allocationOffset = offsetInPage;
    allocation->gpuBaseAddress = limitedGpuAddressRangeAllocator->getBase();

    return allocation;
}

DrmAllocation *DrmMemoryManager::allocateGraphicsMemory64kb(AllocationData allocationData) {
    return nullptr;
}

GraphicsAllocation *DrmMemoryManager::allocateGraphicsMemoryForImage(ImageInfo &imgInfo, const void *hostPtr) {
    auto gmm = std::make_unique<Gmm>(imgInfo);

    auto hostPtrAllocation = allocateGraphicsMemoryForImageFromHostPtr(imgInfo, hostPtr);

    if (hostPtrAllocation) {
        hostPtrAllocation->gmm = gmm.release();
        return hostPtrAllocation;
    }

    if (!GmmHelper::allowTiling(*imgInfo.imgDesc)) {
        auto alloc = MemoryManager::allocateGraphicsMemoryWithProperties({imgInfo.size, GraphicsAllocation::AllocationType::UNDECIDED});
        if (alloc) {
            alloc->gmm = gmm.release();
        }
        return alloc;
    }

    StorageAllocatorType allocatorType = UNKNOWN_ALLOCATOR;
    uint64_t gpuRange = acquireGpuRange(imgInfo.size, allocatorType, false);
    DEBUG_BREAK_IF(gpuRange == reinterpret_cast<uint64_t>(MAP_FAILED));

    drm_i915_gem_create create = {0, 0, 0};
    create.size = imgInfo.size;

    auto ret = this->drm->ioctl(DRM_IOCTL_I915_GEM_CREATE, &create);
    DEBUG_BREAK_IF(ret != 0);
    ((void)(ret));

    auto bo = new (std::nothrow) BufferObject(this->drm, create.handle, true);
    if (!bo) {
        return nullptr;
    }
    bo->size = imgInfo.size;
    bo->address = reinterpret_cast<void *>(gpuRange);
    bo->softPin(gpuRange);

    auto ret2 = bo->setTiling(I915_TILING_Y, static_cast<uint32_t>(imgInfo.rowPitch));
    DEBUG_BREAK_IF(ret2 != true);
    ((void)(ret2));

    bo->setUnmapSize(imgInfo.size);

    auto allocation = new DrmAllocation(bo, nullptr, (uint64_t)gpuRange, imgInfo.size, MemoryPool::SystemCpuInaccessible, getOsContextCount(), false);
    bo->setAllocationType(allocatorType);
    allocation->gmm = gmm.release();
    return allocation;
}

DrmAllocation *DrmMemoryManager::allocate32BitGraphicsMemory(size_t size, const void *ptr, AllocationOrigin allocationOrigin) {
    auto allocatorToUse = allocationOrigin == AllocationOrigin::EXTERNAL_ALLOCATION ? allocator32Bit.get() : internal32bitAllocator.get();
    auto allocatorType = allocationOrigin == AllocationOrigin::EXTERNAL_ALLOCATION ? BIT32_ALLOCATOR_EXTERNAL : BIT32_ALLOCATOR_INTERNAL;

    if (ptr) {
        uintptr_t inputPtr = (uintptr_t)ptr;
        auto allocationSize = alignSizeWholePage((void *)ptr, size);
        auto realAllocationSize = allocationSize;
        auto gpuVirtualAddress = allocatorToUse->allocate(realAllocationSize);
        if (!gpuVirtualAddress) {
            return nullptr;
        }
        auto alignedUserPointer = (uintptr_t)alignDown(ptr, MemoryConstants::pageSize);
        auto inputPointerOffset = inputPtr - alignedUserPointer;

        BufferObject *bo = allocUserptr(alignedUserPointer, allocationSize, 0, true);
        if (!bo) {
            allocatorToUse->free(gpuVirtualAddress, realAllocationSize);
            return nullptr;
        }

        bo->isAllocated = false;
        bo->setUnmapSize(realAllocationSize);
        bo->address = reinterpret_cast<void *>(gpuVirtualAddress);
        uintptr_t offset = (uintptr_t)bo->address;
        bo->softPin((uint64_t)offset);
        bo->setAllocationType(allocatorType);
        auto drmAllocation = new DrmAllocation(bo, (void *)ptr, (uint64_t)ptrOffset(gpuVirtualAddress, inputPointerOffset),
                                               allocationSize, MemoryPool::System4KBPagesWith32BitGpuAddressing, getOsContextCount(), false);
        drmAllocation->is32BitAllocation = true;
        drmAllocation->gpuBaseAddress = allocatorToUse->getBase();
        return drmAllocation;
    }

    size_t alignedAllocationSize = alignUp(size, MemoryConstants::pageSize);
    auto allocationSize = alignedAllocationSize;
    auto res = allocatorToUse->allocate(allocationSize);

    if (!res) {
        return nullptr;
    }

    auto limitedRangeAllocation = false;
    void *ptrAlloc = nullptr;

    if (limitedGpuAddressRangeAllocator.get() && allocatorType == BIT32_ALLOCATOR_INTERNAL) {
        limitedRangeAllocation = true;
        ptrAlloc = alignedMallocWrapper(alignedAllocationSize, MemoryConstants::allocationAlignment);

        if (!ptrAlloc) {
            allocatorToUse->free(res, allocationSize);
            return nullptr;
        }
    }

    BufferObject *bo = allocUserptr(limitedRangeAllocation ? reinterpret_cast<uintptr_t>(ptrAlloc) : res, alignedAllocationSize, 0, true);

    if (!bo) {
        if (ptrAlloc != nullptr) {
            alignedFreeWrapper(ptrAlloc);
        }
        allocatorToUse->free(res, allocationSize);
        return nullptr;
    }

    bo->isAllocated = true;
    bo->setUnmapSize(allocationSize);

    bo->setAllocationType(allocatorType);

    DrmAllocation *drmAllocation = nullptr;
    if (limitedRangeAllocation) {
        drmAllocation = new DrmAllocation(bo, ptrAlloc, res, alignedAllocationSize,
                                          MemoryPool::System4KBPagesWith32BitGpuAddressing, getOsContextCount(), false);
    } else {
        drmAllocation = new DrmAllocation(bo, reinterpret_cast<void *>(res), alignedAllocationSize,
                                          MemoryPool::System4KBPagesWith32BitGpuAddressing, getOsContextCount(), false);
    }

    drmAllocation->is32BitAllocation = true;
    drmAllocation->gpuBaseAddress = allocatorToUse->getBase();
    drmAllocation->driverAllocatedCpuPointer = ptrAlloc;
    return drmAllocation;
}

BufferObject *DrmMemoryManager::findAndReferenceSharedBufferObject(int boHandle) {
    BufferObject *bo = nullptr;
    for (const auto &i : sharingBufferObjects) {
        if (i->handle == static_cast<int>(boHandle)) {
            bo = i;
            bo->reference();
            break;
        }
    }

    return bo;
}

BufferObject *DrmMemoryManager::createSharedBufferObject(int boHandle, size_t size, bool requireSpecificBitness) {
    uint64_t gpuRange = 0llu;
    StorageAllocatorType storageType = UNKNOWN_ALLOCATOR;

    gpuRange = acquireGpuRange(size, storageType, requireSpecificBitness);
    DEBUG_BREAK_IF(gpuRange == reinterpret_cast<uint64_t>(MAP_FAILED));

    auto bo = new (std::nothrow) BufferObject(this->drm, boHandle, true);
    if (!bo) {
        return nullptr;
    }

    bo->size = size;
    bo->address = reinterpret_cast<void *>(gpuRange);
    bo->softPin(gpuRange);
    bo->setUnmapSize(size);
    bo->setAllocationType(storageType);
    return bo;
}

GraphicsAllocation *DrmMemoryManager::createGraphicsAllocationFromSharedHandle(osHandle handle, bool requireSpecificBitness) {
    std::unique_lock<std::mutex> lock(mtx);

    drm_prime_handle openFd = {0, 0, 0};
    openFd.fd = handle;

    auto ret = this->drm->ioctl(DRM_IOCTL_PRIME_FD_TO_HANDLE, &openFd);

    if (ret != 0) {
        int err = errno;
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(PRIME_FD_TO_HANDLE) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
        DEBUG_BREAK_IF(ret != 0);
        ((void)(ret));
        return nullptr;
    }

    auto boHandle = openFd.handle;
    auto bo = findAndReferenceSharedBufferObject(boHandle);

    if (bo == nullptr) {
        size_t size = lseekFunction(handle, 0, SEEK_END);
        bo = createSharedBufferObject(boHandle, size, requireSpecificBitness);

        if (!bo) {
            return nullptr;
        }

        pushSharedBufferObject(bo);
    }

    lock.unlock();

    auto drmAllocation = new DrmAllocation(bo, bo->address, bo->size, handle, MemoryPool::SystemCpuInaccessible, getOsContextCount(), false);

    if (requireSpecificBitness && this->force32bitAllocations) {
        drmAllocation->is32BitAllocation = true;
        drmAllocation->gpuBaseAddress = allocator32Bit->getBase();
    } else if (this->limitedGpuAddressRangeAllocator.get()) {
        drmAllocation->gpuBaseAddress = this->limitedGpuAddressRangeAllocator->getBase();
    }
    return drmAllocation;
}

GraphicsAllocation *DrmMemoryManager::createPaddedAllocation(GraphicsAllocation *inputGraphicsAllocation, size_t sizeWithPadding) {
    uint64_t gpuRange = 0llu;
    StorageAllocatorType storageType = UNKNOWN_ALLOCATOR;

    gpuRange = acquireGpuRange(sizeWithPadding, storageType, false);

    auto srcPtr = inputGraphicsAllocation->getUnderlyingBuffer();
    auto srcSize = inputGraphicsAllocation->getUnderlyingBufferSize();
    auto alignedSrcSize = alignUp(srcSize, MemoryConstants::pageSize);
    auto alignedPtr = (uintptr_t)alignDown(srcPtr, MemoryConstants::pageSize);
    auto offset = (uintptr_t)srcPtr - alignedPtr;

    BufferObject *bo = allocUserptr(alignedPtr, alignedSrcSize, 0, true);
    if (!bo) {
        return nullptr;
    }
    bo->setAddress(reinterpret_cast<void *>(gpuRange));
    bo->softPin(gpuRange);
    bo->setUnmapSize(sizeWithPadding);
    bo->setAllocationType(storageType);
    return new DrmAllocation(bo, (void *)srcPtr, (uint64_t)ptrOffset(gpuRange, offset), sizeWithPadding,
                             inputGraphicsAllocation->getMemoryPool(), getOsContextCount(), false);
}

void DrmMemoryManager::addAllocationToHostPtrManager(GraphicsAllocation *gfxAllocation) {
    DrmAllocation *drmMemory = static_cast<DrmAllocation *>(gfxAllocation);
    FragmentStorage fragment = {};
    fragment.driverAllocation = true;
    fragment.fragmentCpuPointer = gfxAllocation->getUnderlyingBuffer();
    fragment.fragmentSize = alignUp(gfxAllocation->getUnderlyingBufferSize(), MemoryConstants::pageSize);
    fragment.osInternalStorage = new OsHandle();
    fragment.residency = new ResidencyData();
    fragment.osInternalStorage->bo = drmMemory->getBO();
    hostPtrManager->storeFragment(fragment);
}

void DrmMemoryManager::removeAllocationFromHostPtrManager(GraphicsAllocation *gfxAllocation) {
    auto buffer = gfxAllocation->getUnderlyingBuffer();
    auto fragment = hostPtrManager->getFragment(buffer);
    if (fragment && fragment->driverAllocation) {
        OsHandle *osStorageToRelease = fragment->osInternalStorage;
        ResidencyData *residencyDataToRelease = fragment->residency;
        if (hostPtrManager->releaseHostPtr(buffer)) {
            delete osStorageToRelease;
            delete residencyDataToRelease;
        }
    }
}

void DrmMemoryManager::freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) {
    DrmAllocation *input;
    input = static_cast<DrmAllocation *>(gfxAllocation);
    if (input == nullptr)
        return;
    if (input->gmm)
        delete input->gmm;

    alignedFreeWrapper(gfxAllocation->driverAllocatedCpuPointer);

    if (gfxAllocation->fragmentsStorage.fragmentCount) {
        cleanGraphicsMemoryCreatedFromHostPtr(gfxAllocation);
        delete gfxAllocation;
        return;
    }

    BufferObject *search = input->getBO();

    if (gfxAllocation->peekSharedHandle() != Sharing::nonSharedResource) {
        closeFunction(gfxAllocation->peekSharedHandle());
    }

    delete gfxAllocation;

    search->wait(-1);
    unreference(search);
}

uint64_t DrmMemoryManager::getSystemSharedMemory() {
    uint64_t hostMemorySize = MemoryConstants::pageSize * (uint64_t)(sysconf(_SC_PHYS_PAGES));

    drm_i915_gem_context_param getContextParam = {};
    getContextParam.param = I915_CONTEXT_PARAM_GTT_SIZE;
    auto ret = drm->ioctl(DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM, &getContextParam);
    DEBUG_BREAK_IF(ret != 0);
    ((void)(ret));
    uint64_t gpuMemorySize = getContextParam.value;

    return std::min(hostMemorySize, gpuMemorySize);
}

uint64_t DrmMemoryManager::getMaxApplicationAddress() {
    return MemoryConstants::max32BitAppAddress + (uint64_t)is64bit * (MemoryConstants::max64BitAppAddress - MemoryConstants::max32BitAppAddress);
}

uint64_t DrmMemoryManager::getInternalHeapBaseAddress() {
    return this->internal32bitAllocator->getBase();
}

MemoryManager::AllocationStatus DrmMemoryManager::populateOsHandles(OsHandleStorage &handleStorage) {
    BufferObject *allocatedBos[maxFragmentsCount];
    uint32_t numberOfBosAllocated = 0;
    uint32_t indexesOfAllocatedBos[maxFragmentsCount];

    for (unsigned int i = 0; i < maxFragmentsCount; i++) {
        // If there is no fragment it means it already exists.
        if (!handleStorage.fragmentStorageData[i].osHandleStorage && handleStorage.fragmentStorageData[i].fragmentSize) {
            handleStorage.fragmentStorageData[i].osHandleStorage = new OsHandle();
            handleStorage.fragmentStorageData[i].residency = new ResidencyData();

            handleStorage.fragmentStorageData[i].osHandleStorage->bo = allocUserptr((uintptr_t)handleStorage.fragmentStorageData[i].cpuPtr,
                                                                                    handleStorage.fragmentStorageData[i].fragmentSize,
                                                                                    0,
                                                                                    true);
            if (!handleStorage.fragmentStorageData[i].osHandleStorage->bo) {
                handleStorage.fragmentStorageData[i].freeTheFragment = true;
                return AllocationStatus::Error;
            }

            allocatedBos[numberOfBosAllocated] = handleStorage.fragmentStorageData[i].osHandleStorage->bo;
            indexesOfAllocatedBos[numberOfBosAllocated] = i;
            numberOfBosAllocated++;
        }
    }

    if (validateHostPtrMemory) {
        int result = pinBB->pin(allocatedBos, numberOfBosAllocated, getDefaultCommandStreamReceiver(0)->getOsContext().get()->getDrmContextId());

        if (result == EFAULT) {
            for (uint32_t i = 0; i < numberOfBosAllocated; i++) {
                handleStorage.fragmentStorageData[indexesOfAllocatedBos[i]].freeTheFragment = true;
            }
            return AllocationStatus::InvalidHostPointer;
        } else if (result != 0) {
            return AllocationStatus::Error;
        }
    }

    for (uint32_t i = 0; i < numberOfBosAllocated; i++) {
        hostPtrManager->storeFragment(handleStorage.fragmentStorageData[indexesOfAllocatedBos[i]]);
    }
    return AllocationStatus::Success;
}

void DrmMemoryManager::cleanOsHandles(OsHandleStorage &handleStorage) {
    for (unsigned int i = 0; i < maxFragmentsCount; i++) {
        if (handleStorage.fragmentStorageData[i].freeTheFragment) {
            if (handleStorage.fragmentStorageData[i].osHandleStorage->bo) {
                BufferObject *search = handleStorage.fragmentStorageData[i].osHandleStorage->bo;
                search->wait(-1);
                auto refCount = unreference(search, true);
                DEBUG_BREAK_IF(refCount != 1u);
                ((void)(refCount));
            }
            delete handleStorage.fragmentStorageData[i].osHandleStorage;
            handleStorage.fragmentStorageData[i].osHandleStorage = nullptr;
            delete handleStorage.fragmentStorageData[i].residency;
            handleStorage.fragmentStorageData[i].residency = nullptr;
        }
    }
}

BufferObject *DrmMemoryManager::getPinBB() const {
    return pinBB;
}

bool DrmMemoryManager::setDomainCpu(GraphicsAllocation &graphicsAllocation, bool writeEnable) {
    DEBUG_BREAK_IF(writeEnable); //unsupported path (for CPU writes call SW_FINISH ioctl in unlockResource)

    auto bo = static_cast<DrmAllocation *>(&graphicsAllocation)->getBO();
    if (bo == nullptr)
        return false;

    // move a buffer object to the CPU read, and possibly write domain, including waiting on flushes to occur
    drm_i915_gem_set_domain set_domain = {};
    set_domain.handle = bo->peekHandle();
    set_domain.read_domains = I915_GEM_DOMAIN_CPU;
    set_domain.write_domain = writeEnable ? I915_GEM_DOMAIN_CPU : 0;

    return drm->ioctl(DRM_IOCTL_I915_GEM_SET_DOMAIN, &set_domain) == 0;
}

void *DrmMemoryManager::lockResource(GraphicsAllocation *graphicsAllocation) {
    if (graphicsAllocation == nullptr)
        return nullptr;

    auto cpuPtr = graphicsAllocation->getUnderlyingBuffer();
    if (cpuPtr != nullptr) {
        auto success = setDomainCpu(*graphicsAllocation, false);
        DEBUG_BREAK_IF(!success);
        (void)success;
        return cpuPtr;
    }

    auto bo = static_cast<DrmAllocation *>(graphicsAllocation)->getBO();
    if (bo == nullptr)
        return nullptr;

    drm_i915_gem_mmap mmap_arg = {};
    mmap_arg.handle = bo->peekHandle();
    mmap_arg.size = bo->peekSize();
    if (drm->ioctl(DRM_IOCTL_I915_GEM_MMAP, &mmap_arg) != 0) {
        return nullptr;
    }

    bo->setLockedAddress(reinterpret_cast<void *>(mmap_arg.addr_ptr));

    auto success = setDomainCpu(*graphicsAllocation, false);
    DEBUG_BREAK_IF(!success);
    (void)success;

    return bo->peekLockedAddress();
}

void DrmMemoryManager::unlockResource(GraphicsAllocation *graphicsAllocation) {
    if (graphicsAllocation == nullptr)
        return;

    auto cpuPtr = graphicsAllocation->getUnderlyingBuffer();
    if (cpuPtr != nullptr) {
        return;
    }

    auto bo = static_cast<DrmAllocation *>(graphicsAllocation)->getBO();
    if (bo == nullptr)
        return;

    munmapFunction(bo->peekLockedAddress(), bo->peekSize());

    bo->setLockedAddress(nullptr);
}
} // namespace OCLRT
