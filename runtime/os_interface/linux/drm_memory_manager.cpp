/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/drm_memory_manager.h"

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/device/device.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/resource_info.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/surface_formats.h"
#include "runtime/memory_manager/host_ptr_manager.h"
#include "runtime/os_interface/linux/os_context_linux.h"
#include "runtime/os_interface/linux/os_interface.h"
#include "runtime/os_interface/linux/tiling_mode_helper.h"

#include "drm/i915_drm.h"

#include <cstring>
#include <iostream>

namespace NEO {

DrmMemoryManager::DrmMemoryManager(gemCloseWorkerMode mode,
                                   bool forcePinAllowed,
                                   bool validateHostPtrMemory,
                                   ExecutionEnvironment &executionEnvironment) : MemoryManager(executionEnvironment),
                                                                                 drm(executionEnvironment.osInterface->get()->getDrm()),
                                                                                 pinBB(nullptr),
                                                                                 forcePinEnabled(forcePinAllowed),
                                                                                 validateHostPtrMemory(validateHostPtrMemory) {
    gfxPartition.init(platformDevices[0]->capabilityTable.gpuAddressSpace);

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
}

DrmMemoryManager::~DrmMemoryManager() {
    applyCommonCleanup();
    if (gemCloseWorker) {
        gemCloseWorker->close(false);
    }
    if (pinBB) {
        unreference(pinBB);
        pinBB = nullptr;
    }
}

void DrmMemoryManager::eraseSharedBufferObject(NEO::BufferObject *bo) {
    auto it = std::find(sharingBufferObjects.begin(), sharingBufferObjects.end(), bo);
    //If an object isReused = true, it must be in the vector
    DEBUG_BREAK_IF(it == sharingBufferObjects.end());
    sharingBufferObjects.erase(it);
}

void DrmMemoryManager::pushSharedBufferObject(NEO::BufferObject *bo) {
    bo->isReused = true;
    sharingBufferObjects.push_back(bo);
}

uint32_t DrmMemoryManager::unreference(NEO::BufferObject *bo, bool synchronousDestroy) {
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
        auto address = bo->isAllocated || unmapSize > 0 ? reinterpret_cast<void *>(bo->gpuAddress) : nullptr;
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
        return gfxPartition.heapAllocate(HeapIndex::HEAP_EXTERNAL, size);
    }

    if (isLimitedRange()) {
        storageType = INTERNAL_ALLOCATOR_WITH_DYNAMIC_BITRANGE;
        return gfxPartition.heapAllocate(HeapIndex::HEAP_STANDARD, size);
    }

    storageType = MMAP_ALLOCATOR;
    return reinterpret_cast<uint64_t>(reserveCpuAddressRange(size));
}

void DrmMemoryManager::releaseGpuRange(void *address, size_t unmapSize, StorageAllocatorType allocatorType) {
    if (allocatorType == MMAP_ALLOCATOR) {
        releaseReservedCpuAddressRange(address, unmapSize);
        return;
    }

    uint64_t graphicsAddress = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(address));

    if (allocatorType == BIT32_ALLOCATOR_EXTERNAL) {
        gfxPartition.heapFree(HeapIndex::HEAP_EXTERNAL, graphicsAddress, unmapSize);
        return;
    }

    if (allocatorType == BIT32_ALLOCATOR_INTERNAL) {
        gfxPartition.heapFree(internalHeapIndex, graphicsAddress, unmapSize);
        return;
    }

    UNRECOVERABLE_IF(allocatorType != INTERNAL_ALLOCATOR_WITH_DYNAMIC_BITRANGE);
    gfxPartition.heapFree(HeapIndex::HEAP_STANDARD, graphicsAddress, unmapSize);
}

NEO::BufferObject *DrmMemoryManager::allocUserptr(uintptr_t address, size_t size, uint64_t flags, bool softpin) {
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
    res->gpuAddress = address;

    return res;
}

void DrmMemoryManager::emitPinningRequest(BufferObject *bo, const AllocationData &allocationData) const {
    if (forcePinEnabled && pinBB != nullptr && allocationData.flags.forcePin && allocationData.size >= this->pinThreshold) {
        auto &osContextLinux = static_cast<OsContextLinux &>(getDefaultCommandStreamReceiver(0)->getOsContext());
        pinBB->pin(&bo, 1, osContextLinux.getDrmContextId());
    }
}

DrmAllocation *DrmMemoryManager::createGraphicsAllocation(OsHandleStorage &handleStorage, const AllocationData &allocationData) {
    auto hostPtr = const_cast<void *>(allocationData.hostPtr);
    auto allocation = new DrmAllocation(allocationData.type, nullptr, hostPtr, castToUint64(hostPtr), allocationData.size, MemoryPool::System4KBPages, false);
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

    // if Limited Range Alloction is enabled, memory allocation for bo in the limited Range heap is required
    if (isLimitedRange()) {
        StorageAllocatorType allocType;
        bo->gpuAddress = GmmHelper::canonize(acquireGpuRange(cSize, allocType, false));
        if (!bo->gpuAddress) {
            bo->close();
            delete bo;
            alignedFreeWrapper(res);
            return nullptr;
        }

        bo->setUnmapSize(cSize);
        bo->setAllocationType(allocType);
    }

    emitPinningRequest(bo, allocationData);

    auto allocation = new DrmAllocation(allocationData.type, bo, res, bo->gpuAddress, cSize, MemoryPool::System4KBPages, allocationData.flags.multiOsContextCapable);
    allocation->setDriverAllocatedCpuPtr(isLimitedRange() ? res : nullptr);
    return allocation;
}

DrmAllocation *DrmMemoryManager::allocateGraphicsMemoryWithHostPtr(const AllocationData &allocationData) {
    auto res = static_cast<DrmAllocation *>(MemoryManager::allocateGraphicsMemoryWithHostPtr(allocationData));

    if (res != nullptr && !validateHostPtrMemory) {
        emitPinningRequest(res->getBO(), allocationData);
    }
    return res;
}

DrmAllocation *DrmMemoryManager::allocateGraphicsMemoryForNonSvmHostPtr(const AllocationData &allocationData) {
    if (allocationData.size == 0 || !allocationData.hostPtr)
        return nullptr;

    auto alignedPtr = alignDown(allocationData.hostPtr, MemoryConstants::pageSize);
    auto alignedSize = alignSizeWholePage(allocationData.hostPtr, allocationData.size);
    auto realAllocationSize = alignedSize;
    auto offsetInPage = ptrDiff(allocationData.hostPtr, alignedPtr);

    StorageAllocatorType allocType;
    auto gpuVirtualAddress = acquireGpuRange(alignedSize, allocType, false);
    if (!gpuVirtualAddress) {
        return nullptr;
    }

    BufferObject *bo = allocUserptr(reinterpret_cast<uintptr_t>(alignedPtr), realAllocationSize, 0, true);
    if (!bo) {
        releaseGpuRange(reinterpret_cast<void *>(gpuVirtualAddress), alignedSize, allocType);
        return nullptr;
    }

    bo->isAllocated = false;
    bo->setUnmapSize(alignedSize);
    bo->gpuAddress = GmmHelper::canonize(gpuVirtualAddress);
    bo->setAllocationType(allocType);

    auto allocation = new DrmAllocation(allocationData.type, bo, const_cast<void *>(alignedPtr), gpuVirtualAddress,
                                        allocationData.size, MemoryPool::System4KBPages, false);
    allocation->setAllocationOffset(offsetInPage);

    return allocation;
}

DrmAllocation *DrmMemoryManager::allocateGraphicsMemory64kb(const AllocationData &allocationData) {
    return nullptr;
}

GraphicsAllocation *DrmMemoryManager::allocateGraphicsMemoryForImageImpl(const AllocationData &allocationData, std::unique_ptr<Gmm> gmm) {
    if (!GmmHelper::allowTiling(*allocationData.imgInfo->imgDesc)) {
        auto alloc = allocateGraphicsMemoryWithAlignment(allocationData);
        if (alloc) {
            alloc->setDefaultGmm(gmm.release());
        }
        return alloc;
    }

    StorageAllocatorType allocatorType = UNKNOWN_ALLOCATOR;
    uint64_t gpuRange = acquireGpuRange(allocationData.imgInfo->size, allocatorType, false);
    DEBUG_BREAK_IF(gpuRange == reinterpret_cast<uint64_t>(MAP_FAILED));

    drm_i915_gem_create create = {0, 0, 0};
    create.size = allocationData.imgInfo->size;

    auto ret = this->drm->ioctl(DRM_IOCTL_I915_GEM_CREATE, &create);
    DEBUG_BREAK_IF(ret != 0);
    ((void)(ret));

    auto bo = new (std::nothrow) BufferObject(this->drm, create.handle, true);
    if (!bo) {
        return nullptr;
    }
    bo->size = allocationData.imgInfo->size;
    bo->gpuAddress = GmmHelper::canonize(gpuRange);
    auto ret2 = bo->setTiling(I915_TILING_Y, static_cast<uint32_t>(allocationData.imgInfo->rowPitch));
    DEBUG_BREAK_IF(ret2 != true);
    ((void)(ret2));

    bo->setUnmapSize(allocationData.imgInfo->size);

    auto allocation = new DrmAllocation(allocationData.type, bo, nullptr, (uint64_t)gpuRange, allocationData.imgInfo->size, MemoryPool::SystemCpuInaccessible, false);
    bo->setAllocationType(allocatorType);
    allocation->setDefaultGmm(gmm.release());
    return allocation;
}

DrmAllocation *DrmMemoryManager::allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData) {
    auto internal = useInternal32BitAllocator(allocationData.type);
    auto allocatorToUse = internal ? internalHeapIndex : HeapIndex::HEAP_EXTERNAL;
    auto allocatorType = internal ? BIT32_ALLOCATOR_INTERNAL : BIT32_ALLOCATOR_EXTERNAL;

    if (allocationData.hostPtr) {
        uintptr_t inputPtr = reinterpret_cast<uintptr_t>(allocationData.hostPtr);
        auto allocationSize = alignSizeWholePage(allocationData.hostPtr, allocationData.size);
        auto realAllocationSize = allocationSize;
        auto gpuVirtualAddress = gfxPartition.heapAllocate(allocatorToUse, realAllocationSize);
        if (!gpuVirtualAddress) {
            return nullptr;
        }
        auto alignedUserPointer = reinterpret_cast<uintptr_t>(alignDown(allocationData.hostPtr, MemoryConstants::pageSize));
        auto inputPointerOffset = inputPtr - alignedUserPointer;

        BufferObject *bo = allocUserptr(alignedUserPointer, allocationSize, 0, true);
        if (!bo) {
            gfxPartition.heapFree(allocatorToUse, gpuVirtualAddress, realAllocationSize);
            return nullptr;
        }

        bo->isAllocated = false;
        bo->setUnmapSize(realAllocationSize);
        bo->gpuAddress = GmmHelper::canonize(gpuVirtualAddress);
        bo->setAllocationType(allocatorType);
        auto allocation = new DrmAllocation(allocationData.type, bo, const_cast<void *>(allocationData.hostPtr), ptrOffset(gpuVirtualAddress, inputPointerOffset),
                                            allocationSize, MemoryPool::System4KBPagesWith32BitGpuAddressing, false);
        allocation->set32BitAllocation(true);
        allocation->setGpuBaseAddress(gfxPartition.getHeapBase(allocatorToUse));
        return allocation;
    }

    size_t alignedAllocationSize = alignUp(allocationData.size, MemoryConstants::pageSize);
    auto allocationSize = alignedAllocationSize;
    auto res = gfxPartition.heapAllocate(allocatorToUse, allocationSize);

    if (!res) {
        return nullptr;
    }

    auto ptrAlloc = alignedMallocWrapper(alignedAllocationSize, MemoryConstants::allocationAlignment);

    if (!ptrAlloc) {
        gfxPartition.heapFree(allocatorToUse, res, allocationSize);
        return nullptr;
    }

    BufferObject *bo = allocUserptr(reinterpret_cast<uintptr_t>(ptrAlloc), alignedAllocationSize, 0, true);

    if (!bo) {
        alignedFreeWrapper(ptrAlloc);
        gfxPartition.heapFree(allocatorToUse, res, allocationSize);
        return nullptr;
    }

    bo->isAllocated = true;
    bo->setUnmapSize(allocationSize);

    bo->setAllocationType(allocatorType);
    bo->gpuAddress = GmmHelper::canonize(res);

    // softpin to the GPU address, res if it uses Limited Range Allocation
    auto allocation = new DrmAllocation(allocationData.type, bo, ptrAlloc, res, alignedAllocationSize,
                                        MemoryPool::System4KBPagesWith32BitGpuAddressing, false);

    allocation->set32BitAllocation(true);
    allocation->setGpuBaseAddress(gfxPartition.getHeapBase(allocatorToUse));
    allocation->setDriverAllocatedCpuPtr(ptrAlloc);
    return allocation;
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
    bo->gpuAddress = GmmHelper::canonize(gpuRange);
    bo->setUnmapSize(size);
    bo->setAllocationType(storageType);
    return bo;
}

GraphicsAllocation *DrmMemoryManager::createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness) {
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

    auto drmAllocation = new DrmAllocation(properties.allocationType, bo, reinterpret_cast<void *>(bo->gpuAddress), bo->size,
                                           handle, MemoryPool::SystemCpuInaccessible, false);

    if (requireSpecificBitness && this->force32bitAllocations) {
        drmAllocation->set32BitAllocation(true);
        drmAllocation->setGpuBaseAddress(gfxPartition.getHeapBase(HeapIndex::HEAP_EXTERNAL));
    } else if (isLimitedRange()) {
        drmAllocation->setGpuBaseAddress(gfxPartition.getHeapBase(HeapIndex::HEAP_STANDARD));
    }

    if (properties.imgInfo) {
        drm_i915_gem_get_tiling getTiling = {0};
        getTiling.handle = boHandle;
        ret = this->drm->ioctl(DRM_IOCTL_I915_GEM_GET_TILING, &getTiling);

        DEBUG_BREAK_IF(ret != 0);
        ((void)(ret));

        properties.imgInfo->tilingMode = TilingModeHelper::convert(getTiling.tiling_mode);
        Gmm *gmm = new Gmm(*properties.imgInfo);
        drmAllocation->setDefaultGmm(gmm);
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
    bo->gpuAddress = GmmHelper::canonize(gpuRange);
    bo->setUnmapSize(sizeWithPadding);
    bo->setAllocationType(storageType);
    return new DrmAllocation(inputGraphicsAllocation->getAllocationType(), bo, srcPtr, ptrOffset(gpuRange, offset), sizeWithPadding,
                             inputGraphicsAllocation->getMemoryPool(), false);
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
    for (auto handleId = 0u; handleId < maxHandleCount; handleId++) {
        if (gfxAllocation->getGmm(handleId)) {
            delete gfxAllocation->getGmm(handleId);
        }
    }

    alignedFreeWrapper(gfxAllocation->getDriverAllocatedCpuPtr());

    if (gfxAllocation->fragmentsStorage.fragmentCount) {
        cleanGraphicsMemoryCreatedFromHostPtr(gfxAllocation);
        delete gfxAllocation;
        return;
    }

    BufferObject *search = input->getBO();

    if (gfxAllocation->peekSharedHandle() != Sharing::nonSharedResource) {
        closeFunction(gfxAllocation->peekSharedHandle());
    }
    void *reserveAddress = gfxAllocation->getReservedAddressPtr();
    if (reserveAddress) {
        releaseReservedCpuAddressRange(reserveAddress, gfxAllocation->getReservedAddressSize());
    }
    delete gfxAllocation;

    unreference(search);
}

void DrmMemoryManager::handleFenceCompletion(GraphicsAllocation *allocation) {
    static_cast<DrmAllocation *>(allocation)->getBO()->wait(-1);
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
    return is64bit ? MemoryConstants::max64BitAppAddress : MemoryConstants::max32BitAppAddress;
}

uint64_t DrmMemoryManager::getInternalHeapBaseAddress() {
    return gfxPartition.getHeapBase(internalHeapIndex);
}

uint64_t DrmMemoryManager::getExternalHeapBaseAddress() {
    return gfxPartition.getHeapBase(HeapIndex::HEAP_EXTERNAL);
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
        auto &osContextLinux = static_cast<OsContextLinux &>(getDefaultCommandStreamReceiver(0)->getOsContext());
        int result = pinBB->pin(allocatedBos, numberOfBosAllocated, osContextLinux.getDrmContextId());

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

void *DrmMemoryManager::lockResourceImpl(GraphicsAllocation &graphicsAllocation) {
    auto cpuPtr = graphicsAllocation.getUnderlyingBuffer();
    if (cpuPtr != nullptr) {
        auto success = setDomainCpu(graphicsAllocation, false);
        DEBUG_BREAK_IF(!success);
        (void)success;
        return cpuPtr;
    }

    auto bo = static_cast<DrmAllocation &>(graphicsAllocation).getBO();
    if (bo == nullptr)
        return nullptr;

    drm_i915_gem_mmap mmap_arg = {};
    mmap_arg.handle = bo->peekHandle();
    mmap_arg.size = bo->peekSize();
    if (drm->ioctl(DRM_IOCTL_I915_GEM_MMAP, &mmap_arg) != 0) {
        return nullptr;
    }

    bo->setLockedAddress(reinterpret_cast<void *>(mmap_arg.addr_ptr));

    auto success = setDomainCpu(graphicsAllocation, false);
    DEBUG_BREAK_IF(!success);
    (void)success;

    return bo->peekLockedAddress();
}

void DrmMemoryManager::unlockResourceImpl(GraphicsAllocation &graphicsAllocation) {
    auto cpuPtr = graphicsAllocation.getUnderlyingBuffer();
    if (cpuPtr != nullptr) {
        return;
    }

    auto bo = static_cast<DrmAllocation &>(graphicsAllocation).getBO();
    if (bo == nullptr)
        return;

    releaseReservedCpuAddressRange(bo->peekLockedAddress(), bo->peekSize());

    bo->setLockedAddress(nullptr);
}
void *DrmMemoryManager::reserveCpuAddressRange(size_t size) {
    void *reservePtr = mmapFunction(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    return reservePtr;
}

void DrmMemoryManager::releaseReservedCpuAddressRange(void *reserved, size_t size) {
    munmapFunction(reserved, size);
}
} // namespace NEO
