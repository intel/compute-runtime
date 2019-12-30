/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/drm_memory_manager.h"

#include "core/gmm_helper/gmm_helper.h"
#include "core/gmm_helper/resource_info.h"
#include "core/helpers/hw_info.h"
#include "core/helpers/options.h"
#include "core/helpers/ptr_math.h"
#include "core/memory_manager/host_ptr_manager.h"
#include "core/memory_manager/residency.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/device/device.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/helpers/surface_formats.h"
#include "runtime/os_interface/linux/allocator_helper.h"
#include "runtime/os_interface/linux/os_context_linux.h"
#include "runtime/os_interface/linux/os_interface.h"

#include "drm/i915_drm.h"

#include <cstring>
#include <iostream>

namespace NEO {

DrmMemoryManager::DrmMemoryManager(gemCloseWorkerMode mode,
                                   bool forcePinAllowed,
                                   bool validateHostPtrMemory,
                                   ExecutionEnvironment &executionEnvironment) : MemoryManager(executionEnvironment),
                                                                                 drm(executionEnvironment.osInterface->get()->getDrm()),
                                                                                 forcePinEnabled(forcePinAllowed),
                                                                                 validateHostPtrMemory(validateHostPtrMemory) {
    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < gfxPartitions.size(); ++rootDeviceIndex) {
        getGfxPartition(rootDeviceIndex)->init(platformDevices[0]->capabilityTable.gpuAddressSpace, getSizeToReserve(), rootDeviceIndex);
    }
    MemoryManager::virtualPaddingAvailable = true;
    if (mode != gemCloseWorkerMode::gemCloseWorkerInactive) {
        gemCloseWorker.reset(new DrmGemCloseWorker(*this));
    }

    memoryForPinBB = alignedMallocWrapper(MemoryConstants::pageSize, MemoryConstants::pageSize);
    DEBUG_BREAK_IF(memoryForPinBB == nullptr);

    if (forcePinEnabled || validateHostPtrMemory) {
        pinBB = allocUserptr(reinterpret_cast<uintptr_t>(memoryForPinBB), MemoryConstants::pageSize, 0, 0);
    }

    if (!pinBB) {
        alignedFreeWrapper(memoryForPinBB);
        memoryForPinBB = nullptr;
        DEBUG_BREAK_IF(true);
        UNRECOVERABLE_IF(validateHostPtrMemory);
    }
}

DrmMemoryManager::~DrmMemoryManager() {
    if (gemCloseWorker) {
        gemCloseWorker->close(false);
    }
    if (pinBB) {
        DrmMemoryManager::unreference(pinBB, true);
        pinBB = nullptr;
    }
    if (memoryForPinBB) {
        MemoryManager::alignedFreeWrapper(memoryForPinBB);
    }
}

void DrmMemoryManager::eraseSharedBufferObject(NEO::BufferObject *bo) {
    auto it = std::find(sharingBufferObjects.begin(), sharingBufferObjects.end(), bo);
    DEBUG_BREAK_IF(it == sharingBufferObjects.end());
    releaseGpuRange(reinterpret_cast<void *>((*it)->gpuAddress), (*it)->peekUnmapSize(), (*it)->peekRootDeviceIndex());
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
        if (bo->isReused) {
            eraseSharedBufferObject(bo);
        }

        bo->close();

        if (lock) {
            lock.unlock();
        }

        delete bo;
    }
    return r;
}

uint64_t DrmMemoryManager::acquireGpuRange(size_t &size, bool specificBitness, uint32_t rootDeviceIndex) {
    auto gfxPartition = getGfxPartition(rootDeviceIndex);
    if (specificBitness && this->force32bitAllocations) {
        return GmmHelper::canonize(gfxPartition->heapAllocate(HeapIndex::HEAP_EXTERNAL, size));
    } else {
        return GmmHelper::canonize(gfxPartition->heapAllocate(HeapIndex::HEAP_STANDARD, size));
    }
}

void DrmMemoryManager::releaseGpuRange(void *address, size_t unmapSize, uint32_t rootDeviceIndex) {
    uint64_t graphicsAddress = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(address));
    graphicsAddress = GmmHelper::decanonize(graphicsAddress);
    auto gfxPartition = getGfxPartition(rootDeviceIndex);
    gfxPartition->freeGpuAddressRange(graphicsAddress, unmapSize);
}

NEO::BufferObject *DrmMemoryManager::allocUserptr(uintptr_t address, size_t size, uint64_t flags, uint32_t rootDeviceIndex) {
    drm_i915_gem_userptr userptr = {};
    userptr.user_ptr = address;
    userptr.user_size = size;
    userptr.flags = static_cast<uint32_t>(flags);

    if (this->drm->ioctl(DRM_IOCTL_I915_GEM_USERPTR, &userptr) != 0) {
        return nullptr;
    }

    auto res = new (std::nothrow) BufferObject(this->drm, userptr.handle, rootDeviceIndex);
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
        pinBB->pin(&bo, 1, getDefaultDrmContextId());
    }
}

DrmAllocation *DrmMemoryManager::createGraphicsAllocation(OsHandleStorage &handleStorage, const AllocationData &allocationData) {
    auto hostPtr = const_cast<void *>(allocationData.hostPtr);
    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, allocationData.type, nullptr, hostPtr, castToUint64(hostPtr), allocationData.size, MemoryPool::System4KBPages);
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

    BufferObject *bo = allocUserptr(reinterpret_cast<uintptr_t>(res), cSize, 0, allocationData.rootDeviceIndex);

    if (!bo) {
        alignedFreeWrapper(res);
        return nullptr;
    }

    // if limitedRangeAlloction is enabled, memory allocation for bo in the limited Range heap is required
    uint64_t gpuAddress = 0;
    size_t alignedSize = cSize;
    auto svmCpuAllocation = allocationData.type == GraphicsAllocation::AllocationType::SVM_CPU;
    if (svmCpuAllocation) {
        //add 2MB padding in case reserved addr is not 2MB aligned
        alignedSize = alignUp(cSize, cAlignment) + cAlignment;
    }

    if (isLimitedRange(allocationData.rootDeviceIndex) || svmCpuAllocation) {
        gpuAddress = acquireGpuRange(alignedSize, false, allocationData.rootDeviceIndex);
        if (!gpuAddress) {
            bo->close();
            delete bo;
            alignedFreeWrapper(res);
            return nullptr;
        }

        if (svmCpuAllocation) {
            bo->gpuAddress = alignUp(gpuAddress, cAlignment);
        } else {
            bo->gpuAddress = gpuAddress;
        }
    }

    emitPinningRequest(bo, allocationData);

    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, allocationData.type, bo, res, bo->gpuAddress, cSize, MemoryPool::System4KBPages);
    allocation->setDriverAllocatedCpuPtr(res);

    allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuAddress), alignedSize);

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

    auto gpuVirtualAddress = acquireGpuRange(alignedSize, false, allocationData.rootDeviceIndex);
    if (!gpuVirtualAddress) {
        return nullptr;
    }

    BufferObject *bo = allocUserptr(reinterpret_cast<uintptr_t>(alignedPtr), realAllocationSize, 0, allocationData.rootDeviceIndex);
    if (!bo) {
        releaseGpuRange(reinterpret_cast<void *>(gpuVirtualAddress), alignedSize, allocationData.rootDeviceIndex);
        return nullptr;
    }

    if (validateHostPtrMemory) {
        int result = pinBB->pin(&bo, 1, getDefaultDrmContextId());
        if (result != SUCCESS) {
            unreference(bo, true);
            return nullptr;
        }
    }

    bo->gpuAddress = gpuVirtualAddress;

    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, allocationData.type, bo, const_cast<void *>(allocationData.hostPtr),
                                        gpuVirtualAddress, allocationData.size, MemoryPool::System4KBPages);
    allocation->setAllocationOffset(offsetInPage);

    allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuVirtualAddress), alignedSize);

    return allocation;
}

DrmAllocation *DrmMemoryManager::allocateGraphicsMemory64kb(const AllocationData &allocationData) {
    return nullptr;
}

GraphicsAllocation *DrmMemoryManager::allocateShareableMemory(const AllocationData &allocationData) {
    auto gmm = std::make_unique<Gmm>(executionEnvironment.getGmmClientContext(), allocationData.hostPtr, allocationData.size, false);
    size_t bufferSize = allocationData.size;
    uint64_t gpuRange = acquireGpuRange(bufferSize, false, allocationData.rootDeviceIndex);

    drm_i915_gem_create create = {0, 0, 0};
    create.size = bufferSize;

    auto ret = this->drm->ioctl(DRM_IOCTL_I915_GEM_CREATE, &create);
    DEBUG_BREAK_IF(ret != 0);
    ((void)(ret));

    auto bo = new BufferObject(this->drm, create.handle, allocationData.rootDeviceIndex);
    bo->size = bufferSize;
    bo->gpuAddress = gpuRange;

    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, allocationData.type, bo, nullptr, gpuRange, bufferSize, MemoryPool::SystemCpuInaccessible);
    allocation->setDefaultGmm(gmm.release());

    allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuRange), bufferSize);

    return allocation;
}

GraphicsAllocation *DrmMemoryManager::allocateGraphicsMemoryForImageImpl(const AllocationData &allocationData, std::unique_ptr<Gmm> gmm) {
    if (allocationData.imgInfo->linearStorage) {
        auto alloc = allocateGraphicsMemoryWithAlignment(allocationData);
        if (alloc) {
            alloc->setDefaultGmm(gmm.release());
        }
        return alloc;
    }

    uint64_t gpuRange = acquireGpuRange(allocationData.imgInfo->size, false, allocationData.rootDeviceIndex);

    drm_i915_gem_create create = {0, 0, 0};
    create.size = allocationData.imgInfo->size;

    auto ret = this->drm->ioctl(DRM_IOCTL_I915_GEM_CREATE, &create);
    DEBUG_BREAK_IF(ret != 0);
    UNUSED_VARIABLE(ret);

    auto bo = new (std::nothrow) BufferObject(this->drm, create.handle, allocationData.rootDeviceIndex);
    if (!bo) {
        return nullptr;
    }
    bo->size = allocationData.imgInfo->size;
    bo->gpuAddress = gpuRange;

    auto ret2 = bo->setTiling(I915_TILING_Y, static_cast<uint32_t>(allocationData.imgInfo->rowPitch));
    DEBUG_BREAK_IF(ret2 != true);
    UNUSED_VARIABLE(ret2);

    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, allocationData.type, bo, nullptr, gpuRange, allocationData.imgInfo->size, MemoryPool::SystemCpuInaccessible);
    allocation->setDefaultGmm(gmm.release());

    allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuRange), allocationData.imgInfo->size);

    return allocation;
}

DrmAllocation *DrmMemoryManager::allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData) {
    auto internal = useInternal32BitAllocator(allocationData.type);
    auto allocatorToUse = internal ? HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY : HeapIndex::HEAP_EXTERNAL;

    if (allocationData.hostPtr) {
        uintptr_t inputPtr = reinterpret_cast<uintptr_t>(allocationData.hostPtr);
        auto allocationSize = alignSizeWholePage(allocationData.hostPtr, allocationData.size);
        auto realAllocationSize = allocationSize;
        auto gfxPartition = getGfxPartition(allocationData.rootDeviceIndex);
        auto gpuVirtualAddress = gfxPartition->heapAllocate(allocatorToUse, realAllocationSize);
        if (!gpuVirtualAddress) {
            return nullptr;
        }
        auto alignedUserPointer = reinterpret_cast<uintptr_t>(alignDown(allocationData.hostPtr, MemoryConstants::pageSize));
        auto inputPointerOffset = inputPtr - alignedUserPointer;

        BufferObject *bo = allocUserptr(alignedUserPointer, allocationSize, 0, allocationData.rootDeviceIndex);
        if (!bo) {
            gfxPartition->heapFree(allocatorToUse, gpuVirtualAddress, realAllocationSize);
            return nullptr;
        }

        bo->gpuAddress = GmmHelper::canonize(gpuVirtualAddress);
        auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, allocationData.type, bo, const_cast<void *>(allocationData.hostPtr), GmmHelper::canonize(ptrOffset(gpuVirtualAddress, inputPointerOffset)),
                                            allocationSize, MemoryPool::System4KBPagesWith32BitGpuAddressing);
        allocation->set32BitAllocation(true);
        allocation->setGpuBaseAddress(GmmHelper::canonize(gfxPartition->getHeapBase(allocatorToUse)));
        allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuVirtualAddress), realAllocationSize);
        return allocation;
    }

    size_t alignedAllocationSize = alignUp(allocationData.size, MemoryConstants::pageSize);
    auto allocationSize = alignedAllocationSize;
    auto gfxPartition = getGfxPartition(allocationData.rootDeviceIndex);
    auto res = gfxPartition->heapAllocate(allocatorToUse, allocationSize);

    if (!res) {
        return nullptr;
    }

    auto ptrAlloc = alignedMallocWrapper(alignedAllocationSize, MemoryConstants::allocationAlignment);

    if (!ptrAlloc) {
        gfxPartition->heapFree(allocatorToUse, res, allocationSize);
        return nullptr;
    }

    BufferObject *bo = allocUserptr(reinterpret_cast<uintptr_t>(ptrAlloc), alignedAllocationSize, 0, allocationData.rootDeviceIndex);

    if (!bo) {
        alignedFreeWrapper(ptrAlloc);
        gfxPartition->heapFree(allocatorToUse, res, allocationSize);
        return nullptr;
    }

    bo->gpuAddress = GmmHelper::canonize(res);

    // softpin to the GPU address, res if it uses limitedRange Allocation
    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, allocationData.type, bo, ptrAlloc, GmmHelper::canonize(res), alignedAllocationSize,
                                        MemoryPool::System4KBPagesWith32BitGpuAddressing);

    allocation->set32BitAllocation(true);
    allocation->setGpuBaseAddress(GmmHelper::canonize(gfxPartition->getHeapBase(allocatorToUse)));
    allocation->setDriverAllocatedCpuPtr(ptrAlloc);
    allocation->setReservedAddressRange(reinterpret_cast<void *>(res), allocationSize);
    return allocation;
}

BufferObject *DrmMemoryManager::findAndReferenceSharedBufferObject(int boHandle) {
    BufferObject *bo = nullptr;
    for (const auto &i : sharingBufferObjects) {
        if (i->handle == boHandle) {
            bo = i;
            bo->reference();
            break;
        }
    }

    return bo;
}

BufferObject *DrmMemoryManager::createSharedBufferObject(int boHandle, size_t size, bool requireSpecificBitness, uint32_t rootDeviceIndex) {
    uint64_t gpuRange = 0llu;

    gpuRange = acquireGpuRange(size, requireSpecificBitness, rootDeviceIndex);

    auto bo = new (std::nothrow) BufferObject(this->drm, boHandle, rootDeviceIndex);
    if (!bo) {
        return nullptr;
    }

    bo->size = size;
    bo->gpuAddress = gpuRange;
    bo->setUnmapSize(size);
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
        UNUSED_VARIABLE(ret);
        return nullptr;
    }

    auto boHandle = openFd.handle;
    auto bo = findAndReferenceSharedBufferObject(boHandle);

    if (bo == nullptr) {
        size_t size = lseekFunction(handle, 0, SEEK_END);
        bo = createSharedBufferObject(boHandle, size, requireSpecificBitness, properties.rootDeviceIndex);

        if (!bo) {
            return nullptr;
        }

        pushSharedBufferObject(bo);
    }

    lock.unlock();

    auto drmAllocation = new DrmAllocation(properties.rootDeviceIndex, properties.allocationType, bo, reinterpret_cast<void *>(bo->gpuAddress), bo->size,
                                           handle, MemoryPool::SystemCpuInaccessible);

    if (requireSpecificBitness && this->force32bitAllocations) {
        drmAllocation->set32BitAllocation(true);
        drmAllocation->setGpuBaseAddress(GmmHelper::canonize(getExternalHeapBaseAddress(properties.rootDeviceIndex)));
    }

    if (properties.imgInfo) {
        drm_i915_gem_get_tiling getTiling = {0};
        getTiling.handle = boHandle;
        ret = this->drm->ioctl(DRM_IOCTL_I915_GEM_GET_TILING, &getTiling);

        DEBUG_BREAK_IF(ret != 0);
        UNUSED_VARIABLE(ret);

        if (getTiling.tiling_mode == I915_TILING_NONE) {
            properties.imgInfo->linearStorage = true;
        }

        Gmm *gmm = new Gmm(executionEnvironment.getGmmClientContext(), *properties.imgInfo, createStorageInfoFromProperties(properties));
        drmAllocation->setDefaultGmm(gmm);
    }
    return drmAllocation;
}

GraphicsAllocation *DrmMemoryManager::createPaddedAllocation(GraphicsAllocation *inputGraphicsAllocation, size_t sizeWithPadding) {
    uint64_t gpuRange = 0llu;

    auto rootDeviceIndex = inputGraphicsAllocation->getRootDeviceIndex();
    gpuRange = acquireGpuRange(sizeWithPadding, false, rootDeviceIndex);

    auto srcPtr = inputGraphicsAllocation->getUnderlyingBuffer();
    auto srcSize = inputGraphicsAllocation->getUnderlyingBufferSize();
    auto alignedSrcSize = alignUp(srcSize, MemoryConstants::pageSize);
    auto alignedPtr = (uintptr_t)alignDown(srcPtr, MemoryConstants::pageSize);
    auto offset = (uintptr_t)srcPtr - alignedPtr;

    BufferObject *bo = allocUserptr(alignedPtr, alignedSrcSize, 0, rootDeviceIndex);
    if (!bo) {
        return nullptr;
    }
    bo->gpuAddress = gpuRange;
    auto allocation = new DrmAllocation(rootDeviceIndex, inputGraphicsAllocation->getAllocationType(), bo, srcPtr, GmmHelper::canonize(ptrOffset(gpuRange, offset)), sizeWithPadding,
                                        inputGraphicsAllocation->getMemoryPool());

    allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuRange), sizeWithPadding);
    return allocation;
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
    for (auto handleId = 0u; handleId < EngineLimits::maxHandleCount; handleId++) {
        if (gfxAllocation->getGmm(handleId)) {
            delete gfxAllocation->getGmm(handleId);
        }
    }

    if (gfxAllocation->fragmentsStorage.fragmentCount) {
        cleanGraphicsMemoryCreatedFromHostPtr(gfxAllocation);
    } else {
        auto &bos = static_cast<DrmAllocation *>(gfxAllocation)->getBOs();
        for (auto bo : bos) {
            unreference(bo, bo && bo->isReused ? false : true);
        }
        if (gfxAllocation->peekSharedHandle() != Sharing::nonSharedResource) {
            closeFunction(gfxAllocation->peekSharedHandle());
        }
    }

    releaseGpuRange(gfxAllocation->getReservedAddressPtr(), gfxAllocation->getReservedAddressSize(), gfxAllocation->getRootDeviceIndex());
    alignedFreeWrapper(gfxAllocation->getDriverAllocatedCpuPtr());

    delete gfxAllocation;
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
    UNUSED_VARIABLE(ret);
    uint64_t gpuMemorySize = getContextParam.value;

    return std::min(hostMemorySize, gpuMemorySize);
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
                                                                                    0, 0);
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
        int result = pinBB->pin(allocatedBos, numberOfBosAllocated, getDefaultDrmContextId());

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

void DrmMemoryManager::cleanOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) {
    for (unsigned int i = 0; i < maxFragmentsCount; i++) {
        if (handleStorage.fragmentStorageData[i].freeTheFragment) {
            if (handleStorage.fragmentStorageData[i].osHandleStorage->bo) {
                BufferObject *search = handleStorage.fragmentStorageData[i].osHandleStorage->bo;
                search->wait(-1);
                auto refCount = unreference(search, true);
                DEBUG_BREAK_IF(refCount != 1u);
                UNUSED_VARIABLE(refCount);
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
    if (MemoryPool::LocalMemory == graphicsAllocation.getMemoryPool()) {
        return lockResourceInLocalMemoryImpl(graphicsAllocation);
    }

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

int DrmMemoryManager::obtainFdFromHandle(int boHandle) {
    drm_prime_handle openFd = {0, 0, 0};

    openFd.flags = DRM_CLOEXEC | DRM_RDWR;
    openFd.handle = boHandle;

    drm->ioctl(DRM_IOCTL_PRIME_HANDLE_TO_FD, &openFd);

    return openFd.fd;
}

uint32_t DrmMemoryManager::getDefaultDrmContextId() const {
    auto osContextLinux = static_cast<OsContextLinux *>(registeredEngines[defaultEngineIndex].osContext);
    return osContextLinux->getDrmContextIds()[0];
}

} // namespace NEO
