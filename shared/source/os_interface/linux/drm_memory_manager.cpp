/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_memory_manager.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/heap_assigner.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/host_ptr_manager.h"
#include "shared/source/memory_manager/residency.h"
#include "shared/source/os_interface/linux/allocator_helper.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/os_interface.h"

#include "drm/i915_drm.h"

#include <cstring>
#include <iostream>
#include <memory>

namespace NEO {

DrmMemoryManager::DrmMemoryManager(gemCloseWorkerMode mode,
                                   bool forcePinAllowed,
                                   bool validateHostPtrMemory,
                                   ExecutionEnvironment &executionEnvironment) : MemoryManager(executionEnvironment),
                                                                                 forcePinEnabled(forcePinAllowed),
                                                                                 validateHostPtrMemory(validateHostPtrMemory) {
    if (DebugManager.flags.EnableDirectSubmission.get() == 1) {
        mode = gemCloseWorkerMode::gemCloseWorkerInactive;
        this->forcePinEnabled = false;
        this->validateHostPtrMemory = false;
    }

    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < gfxPartitions.size(); ++rootDeviceIndex) {
        auto gpuAddressSpace = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo()->capabilityTable.gpuAddressSpace;
        getGfxPartition(rootDeviceIndex)->init(gpuAddressSpace, getSizeToReserve(), rootDeviceIndex, gfxPartitions.size());
        localMemAllocs.emplace_back();
    }
    MemoryManager::virtualPaddingAvailable = true;
    if (mode != gemCloseWorkerMode::gemCloseWorkerInactive) {
        gemCloseWorker.reset(new DrmGemCloseWorker(*this));
    }

    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < gfxPartitions.size(); ++rootDeviceIndex) {
        BufferObject *bo = nullptr;
        if (forcePinEnabled || validateHostPtrMemory) {
            auto cpuAddrBo = alignedMallocWrapper(MemoryConstants::pageSize, MemoryConstants::pageSize);
            UNRECOVERABLE_IF(cpuAddrBo == nullptr);
            // Preprogram the Bo with MI_BATCH_BUFFER_END and MI_NOOP. This BO will be used as the last BB in a series to indicate the end of submission.
            reinterpret_cast<uint32_t *>(cpuAddrBo)[0] = 0x05000000; // MI_BATCH_BUFFER_END
            reinterpret_cast<uint32_t *>(cpuAddrBo)[1] = 0;          // MI_NOOP
            memoryForPinBBs.push_back(cpuAddrBo);
            DEBUG_BREAK_IF(memoryForPinBBs[rootDeviceIndex] == nullptr);
            bo = allocUserptr(reinterpret_cast<uintptr_t>(memoryForPinBBs[rootDeviceIndex]), MemoryConstants::pageSize, 0, rootDeviceIndex);
            if (bo) {
                if (isLimitedRange(rootDeviceIndex)) {
                    bo->gpuAddress = acquireGpuRange(bo->size, false, rootDeviceIndex, false);
                }
            } else {
                alignedFreeWrapper(memoryForPinBBs[rootDeviceIndex]);
                memoryForPinBBs[rootDeviceIndex] = nullptr;
                DEBUG_BREAK_IF(true);
                UNRECOVERABLE_IF(validateHostPtrMemory);
            }
        }

        pinBBs.push_back(bo);
    }
}

DrmMemoryManager::~DrmMemoryManager() {
    for (auto &memoryForPinBB : memoryForPinBBs) {
        if (memoryForPinBB) {
            MemoryManager::alignedFreeWrapper(memoryForPinBB);
        }
    }
}

void DrmMemoryManager::commonCleanup() {
    if (gemCloseWorker) {
        gemCloseWorker->close(false);
    }

    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < pinBBs.size(); ++rootDeviceIndex) {
        if (auto bo = pinBBs[rootDeviceIndex]) {
            if (isLimitedRange(rootDeviceIndex)) {
                releaseGpuRange(reinterpret_cast<void *>(bo->gpuAddress), bo->size, rootDeviceIndex);
            }
            DrmMemoryManager::unreference(bo, true);
        }
    }
    pinBBs.clear();
}

void DrmMemoryManager::eraseSharedBufferObject(NEO::BufferObject *bo) {
    auto it = std::find(sharingBufferObjects.begin(), sharingBufferObjects.end(), bo);
    DEBUG_BREAK_IF(it == sharingBufferObjects.end());
    releaseGpuRange(reinterpret_cast<void *>((*it)->gpuAddress), (*it)->peekUnmapSize(), this->getRootDeviceIndex(bo->drm));
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

uint64_t DrmMemoryManager::acquireGpuRange(size_t &size, bool specificBitness, uint32_t rootDeviceIndex, bool requiresStandard64KBHeap) {
    auto gfxPartition = getGfxPartition(rootDeviceIndex);
    if (specificBitness && this->force32bitAllocations) {
        return GmmHelper::canonize(gfxPartition->heapAllocate(HeapIndex::HEAP_EXTERNAL, size));
    }
    if (requiresStandard64KBHeap) {
        return GmmHelper::canonize(gfxPartition->heapAllocate(HeapIndex::HEAP_STANDARD64KB, size));
    }
    return GmmHelper::canonize(gfxPartition->heapAllocate(HeapIndex::HEAP_STANDARD, size));
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

    if (this->getDrm(rootDeviceIndex).ioctl(DRM_IOCTL_I915_GEM_USERPTR, &userptr) != 0) {
        return nullptr;
    }

    printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "Created new BO with GEM_USERPTR, handle: BO-%d\n", userptr.handle);

    auto res = new (std::nothrow) BufferObject(&getDrm(rootDeviceIndex), userptr.handle, size, maxOsContextCount);
    if (!res) {
        DEBUG_BREAK_IF(true);
        return nullptr;
    }
    res->gpuAddress = address;

    return res;
}

void DrmMemoryManager::emitPinningRequest(BufferObject *bo, const AllocationData &allocationData) const {
    if (forcePinEnabled && pinBBs.at(allocationData.rootDeviceIndex) != nullptr && allocationData.flags.forcePin && allocationData.size >= this->pinThreshold) {
        pinBBs.at(allocationData.rootDeviceIndex)->pin(&bo, 1, registeredEngines[defaultEngineIndex].osContext, 0, getDefaultDrmContextId());
    }
}

DrmAllocation *DrmMemoryManager::createGraphicsAllocation(OsHandleStorage &handleStorage, const AllocationData &allocationData) {
    auto hostPtr = const_cast<void *>(allocationData.hostPtr);
    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, allocationData.type, nullptr, hostPtr, castToUint64(hostPtr), allocationData.size, MemoryPool::System4KBPages);
    allocation->fragmentsStorage = handleStorage;
    return allocation;
}

DrmAllocation *DrmMemoryManager::allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) {
    const size_t minAlignment = getUserptrAlignment();
    size_t cAlignment = alignUp(std::max(allocationData.alignment, minAlignment), minAlignment);
    // When size == 0 allocate allocationAlignment
    // It's needed to prevent overlapping pages with user pointers
    size_t cSize = std::max(alignUp(allocationData.size, minAlignment), minAlignment);

    auto res = alignedMallocWrapper(cSize, cAlignment);

    if (!res)
        return nullptr;

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(allocUserptr(reinterpret_cast<uintptr_t>(res), cSize, 0, allocationData.rootDeviceIndex));

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
        gpuAddress = acquireGpuRange(alignedSize, false, allocationData.rootDeviceIndex, false);
        if (!gpuAddress) {
            alignedFreeWrapper(res);
            return nullptr;
        }

        if (svmCpuAllocation) {
            bo->gpuAddress = alignUp(gpuAddress, cAlignment);
        } else {
            bo->gpuAddress = gpuAddress;
        }
    }

    emitPinningRequest(bo.get(), allocationData);

    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, allocationData.type, bo.get(), res, bo->gpuAddress, cSize, MemoryPool::System4KBPages);
    allocation->setDriverAllocatedCpuPtr(res);

    allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuAddress), alignedSize);
    bo.release();

    return allocation;
}

DrmAllocation *DrmMemoryManager::allocateUSMHostGraphicsMemory(const AllocationData &allocationData) {
    const size_t minAlignment = getUserptrAlignment();
    // When size == 0 allocate allocationAlignment
    // It's needed to prevent overlapping pages with user pointers
    size_t cSize = std::max(alignUp(allocationData.size, minAlignment), minAlignment);

    void *bufferPtr = const_cast<void *>(allocationData.hostPtr);
    DEBUG_BREAK_IF(nullptr == bufferPtr);

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(allocUserptr(reinterpret_cast<uintptr_t>(bufferPtr),
                                                                         cSize,
                                                                         0,
                                                                         allocationData.rootDeviceIndex));
    if (!bo) {
        return nullptr;
    }

    // if limitedRangeAlloction is enabled, memory allocation for bo in the limited Range heap is required
    uint64_t gpuAddress = 0;
    if (isLimitedRange(allocationData.rootDeviceIndex)) {
        gpuAddress = acquireGpuRange(cSize, false, allocationData.rootDeviceIndex, false);
        if (!gpuAddress) {
            return nullptr;
        }
        bo->gpuAddress = gpuAddress;
    }

    emitPinningRequest(bo.get(), allocationData);

    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex,
                                        allocationData.type,
                                        bo.get(),
                                        bufferPtr,
                                        bo->gpuAddress,
                                        cSize,
                                        MemoryPool::System4KBPages);

    allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuAddress), cSize);
    bo.release();

    return allocation;
}

DrmAllocation *DrmMemoryManager::allocateGraphicsMemoryWithHostPtr(const AllocationData &allocationData) {
    auto res = static_cast<DrmAllocation *>(MemoryManager::allocateGraphicsMemoryWithHostPtr(allocationData));

    if (res != nullptr && !validateHostPtrMemory) {
        emitPinningRequest(res->getBO(), allocationData);
    }
    return res;
}

GraphicsAllocation *DrmMemoryManager::allocateGraphicsMemoryWithGpuVa(const AllocationData &allocationData) {
    auto osContextLinux = static_cast<OsContextLinux *>(allocationData.osContext);

    const size_t minAlignment = getUserptrAlignment();
    size_t alignedSize = alignUp(allocationData.size, minAlignment);

    auto res = alignedMallocWrapper(alignedSize, minAlignment);
    if (!res)
        return nullptr;

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(allocUserptr(reinterpret_cast<uintptr_t>(res), alignedSize, 0, allocationData.rootDeviceIndex));

    if (!bo) {
        alignedFreeWrapper(res);
        return nullptr;
    }

    UNRECOVERABLE_IF(allocationData.gpuAddress == 0);
    bo->gpuAddress = allocationData.gpuAddress;

    BufferObject *boPtr = bo.get();
    if (forcePinEnabled && pinBBs.at(allocationData.rootDeviceIndex) != nullptr && alignedSize >= this->pinThreshold) {
        pinBBs.at(allocationData.rootDeviceIndex)->pin(&boPtr, 1, osContextLinux, 0, osContextLinux->getDrmContextIds()[0]);
    }

    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, allocationData.type, bo.get(), res, bo->gpuAddress, alignedSize, MemoryPool::System4KBPages);
    allocation->setDriverAllocatedCpuPtr(res);
    bo.release();

    return allocation;
}

DrmAllocation *DrmMemoryManager::allocateGraphicsMemoryForNonSvmHostPtr(const AllocationData &allocationData) {
    if (allocationData.size == 0 || !allocationData.hostPtr)
        return nullptr;

    auto alignedPtr = alignDown(allocationData.hostPtr, MemoryConstants::pageSize);
    auto alignedSize = alignSizeWholePage(allocationData.hostPtr, allocationData.size);
    auto realAllocationSize = alignedSize;
    auto offsetInPage = ptrDiff(allocationData.hostPtr, alignedPtr);

    auto gpuVirtualAddress = acquireGpuRange(alignedSize, false, allocationData.rootDeviceIndex, false);
    if (!gpuVirtualAddress) {
        return nullptr;
    }

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(allocUserptr(reinterpret_cast<uintptr_t>(alignedPtr), realAllocationSize, 0, allocationData.rootDeviceIndex));
    if (!bo) {
        releaseGpuRange(reinterpret_cast<void *>(gpuVirtualAddress), alignedSize, allocationData.rootDeviceIndex);
        return nullptr;
    }

    bo->gpuAddress = gpuVirtualAddress;

    if (validateHostPtrMemory) {
        auto boPtr = bo.get();
        int result = pinBBs.at(allocationData.rootDeviceIndex)->pin(&boPtr, 1, registeredEngines[defaultEngineIndex].osContext, 0, getDefaultDrmContextId());
        if (result != SUCCESS) {
            unreference(bo.release(), true);
            releaseGpuRange(reinterpret_cast<void *>(gpuVirtualAddress), alignedSize, allocationData.rootDeviceIndex);
            return nullptr;
        }
    }

    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, allocationData.type, bo.get(), const_cast<void *>(allocationData.hostPtr),
                                        gpuVirtualAddress, allocationData.size, MemoryPool::System4KBPages);
    allocation->setAllocationOffset(offsetInPage);

    allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuVirtualAddress), alignedSize);
    bo.release();
    return allocation;
}

DrmAllocation *DrmMemoryManager::allocateGraphicsMemory64kb(const AllocationData &allocationData) {
    return nullptr;
}

GraphicsAllocation *DrmMemoryManager::allocateShareableMemory(const AllocationData &allocationData) {
    auto gmm = std::make_unique<Gmm>(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmClientContext(), allocationData.hostPtr, allocationData.size, false);
    size_t bufferSize = allocationData.size;
    uint64_t gpuRange = acquireGpuRange(bufferSize, false, allocationData.rootDeviceIndex, true);

    drm_i915_gem_create create = {0, 0, 0};
    create.size = bufferSize;

    auto ret = this->getDrm(allocationData.rootDeviceIndex).ioctl(DRM_IOCTL_I915_GEM_CREATE, &create);
    DEBUG_BREAK_IF(ret != 0);
    ((void)(ret));

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(new BufferObject(&getDrm(allocationData.rootDeviceIndex), create.handle, bufferSize, maxOsContextCount));
    bo->gpuAddress = gpuRange;

    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, allocationData.type, bo.get(), nullptr, gpuRange, bufferSize, MemoryPool::SystemCpuInaccessible);
    allocation->setDefaultGmm(gmm.release());

    allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuRange), bufferSize);
    bo.release();
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

    uint64_t gpuRange = acquireGpuRange(allocationData.imgInfo->size, false, allocationData.rootDeviceIndex, false);

    drm_i915_gem_create create = {0, 0, 0};
    create.size = allocationData.imgInfo->size;

    auto ret = this->getDrm(allocationData.rootDeviceIndex).ioctl(DRM_IOCTL_I915_GEM_CREATE, &create);
    DEBUG_BREAK_IF(ret != 0);
    UNUSED_VARIABLE(ret);

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(new (std::nothrow) BufferObject(&getDrm(allocationData.rootDeviceIndex), create.handle, allocationData.imgInfo->size, maxOsContextCount));
    if (!bo) {
        return nullptr;
    }
    bo->gpuAddress = gpuRange;

    auto ret2 = bo->setTiling(I915_TILING_Y, static_cast<uint32_t>(allocationData.imgInfo->rowPitch));
    DEBUG_BREAK_IF(ret2 != true);
    UNUSED_VARIABLE(ret2);

    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, allocationData.type, bo.get(), nullptr, gpuRange, allocationData.imgInfo->size, MemoryPool::SystemCpuInaccessible);
    allocation->setDefaultGmm(gmm.release());

    allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuRange), allocationData.imgInfo->size);
    bo.release();
    return allocation;
}

DrmAllocation *DrmMemoryManager::allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData, bool useLocalMemory) {
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHardwareInfo();
    auto allocatorToUse = heapAssigner.get32BitHeapIndex(allocationData.type, useLocalMemory, *hwInfo);

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

        std::unique_ptr<BufferObject, BufferObject::Deleter> bo(allocUserptr(alignedUserPointer, allocationSize, 0, allocationData.rootDeviceIndex));
        if (!bo) {
            gfxPartition->heapFree(allocatorToUse, gpuVirtualAddress, realAllocationSize);
            return nullptr;
        }

        bo->gpuAddress = GmmHelper::canonize(gpuVirtualAddress);
        auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, allocationData.type, bo.get(), const_cast<void *>(allocationData.hostPtr), GmmHelper::canonize(ptrOffset(gpuVirtualAddress, inputPointerOffset)),
                                            allocationSize, MemoryPool::System4KBPagesWith32BitGpuAddressing);
        allocation->set32BitAllocation(true);
        allocation->setGpuBaseAddress(GmmHelper::canonize(gfxPartition->getHeapBase(allocatorToUse)));
        allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuVirtualAddress), realAllocationSize);
        bo.release();
        return allocation;
    }

    size_t alignedAllocationSize = alignUp(allocationData.size, MemoryConstants::pageSize);
    auto allocationSize = alignedAllocationSize;
    auto gfxPartition = getGfxPartition(allocationData.rootDeviceIndex);
    auto res = gfxPartition->heapAllocate(allocatorToUse, allocationSize);

    if (!res) {
        return nullptr;
    }

    auto ptrAlloc = alignedMallocWrapper(alignedAllocationSize, getUserptrAlignment());

    if (!ptrAlloc) {
        gfxPartition->heapFree(allocatorToUse, res, allocationSize);
        return nullptr;
    }

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(allocUserptr(reinterpret_cast<uintptr_t>(ptrAlloc), alignedAllocationSize, 0, allocationData.rootDeviceIndex));

    if (!bo) {
        alignedFreeWrapper(ptrAlloc);
        gfxPartition->heapFree(allocatorToUse, res, allocationSize);
        return nullptr;
    }

    bo->gpuAddress = GmmHelper::canonize(res);

    // softpin to the GPU address, res if it uses limitedRange Allocation
    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, allocationData.type, bo.get(), ptrAlloc, GmmHelper::canonize(res), alignedAllocationSize,
                                        MemoryPool::System4KBPagesWith32BitGpuAddressing);

    allocation->set32BitAllocation(true);
    allocation->setGpuBaseAddress(GmmHelper::canonize(gfxPartition->getHeapBase(allocatorToUse)));
    allocation->setDriverAllocatedCpuPtr(ptrAlloc);
    allocation->setReservedAddressRange(reinterpret_cast<void *>(res), allocationSize);
    bo.release();
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

    gpuRange = acquireGpuRange(size, requireSpecificBitness, rootDeviceIndex, isLocalMemorySupported(rootDeviceIndex));

    auto bo = new (std::nothrow) BufferObject(&getDrm(rootDeviceIndex), boHandle, size, maxOsContextCount);
    if (!bo) {
        return nullptr;
    }

    bo->gpuAddress = gpuRange;
    bo->setUnmapSize(size);
    return bo;
}

GraphicsAllocation *DrmMemoryManager::createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness) {
    std::unique_lock<std::mutex> lock(mtx);

    drm_prime_handle openFd = {0, 0, 0};
    openFd.fd = handle;

    auto ret = this->getDrm(properties.rootDeviceIndex).ioctl(DRM_IOCTL_PRIME_FD_TO_HANDLE, &openFd);

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
        drmAllocation->setGpuBaseAddress(GmmHelper::canonize(getExternalHeapBaseAddress(properties.rootDeviceIndex, drmAllocation->isAllocatedInLocalMemoryPool())));
    }

    if (properties.imgInfo) {
        drm_i915_gem_get_tiling getTiling = {0};
        getTiling.handle = boHandle;
        ret = this->getDrm(properties.rootDeviceIndex).ioctl(DRM_IOCTL_I915_GEM_GET_TILING, &getTiling);

        if (ret == 0) {
            if (getTiling.tiling_mode == I915_TILING_NONE) {
                properties.imgInfo->linearStorage = true;
            }
        }

        Gmm *gmm = new Gmm(executionEnvironment.rootDeviceEnvironments[properties.rootDeviceIndex]->getGmmClientContext(), *properties.imgInfo, createStorageInfoFromProperties(properties));
        drmAllocation->setDefaultGmm(gmm);
    }
    return drmAllocation;
}

GraphicsAllocation *DrmMemoryManager::createPaddedAllocation(GraphicsAllocation *inputGraphicsAllocation, size_t sizeWithPadding) {
    uint64_t gpuRange = 0llu;

    auto rootDeviceIndex = inputGraphicsAllocation->getRootDeviceIndex();
    gpuRange = acquireGpuRange(sizeWithPadding, false, rootDeviceIndex, false);

    auto srcPtr = inputGraphicsAllocation->getUnderlyingBuffer();
    auto srcSize = inputGraphicsAllocation->getUnderlyingBufferSize();
    auto alignedSrcSize = alignUp(srcSize, MemoryConstants::pageSize);
    auto alignedPtr = (uintptr_t)alignDown(srcPtr, MemoryConstants::pageSize);
    auto offset = (uintptr_t)srcPtr - alignedPtr;

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(allocUserptr(alignedPtr, alignedSrcSize, 0, rootDeviceIndex));
    if (!bo) {
        return nullptr;
    }
    bo->gpuAddress = gpuRange;
    auto allocation = new DrmAllocation(rootDeviceIndex, inputGraphicsAllocation->getAllocationType(), bo.get(), srcPtr, GmmHelper::canonize(ptrOffset(gpuRange, offset)), sizeWithPadding,
                                        inputGraphicsAllocation->getMemoryPool());

    allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuRange), sizeWithPadding);
    bo.release();
    return allocation;
}

void DrmMemoryManager::addAllocationToHostPtrManager(GraphicsAllocation *gfxAllocation) {
    DrmAllocation *drmMemory = static_cast<DrmAllocation *>(gfxAllocation);
    auto maxOsContextCount = 1u;
    FragmentStorage fragment = {};
    fragment.driverAllocation = true;
    fragment.fragmentCpuPointer = gfxAllocation->getUnderlyingBuffer();
    fragment.fragmentSize = alignUp(gfxAllocation->getUnderlyingBufferSize(), MemoryConstants::pageSize);
    fragment.osInternalStorage = new OsHandle();
    fragment.residency = new ResidencyData(maxOsContextCount);
    fragment.osInternalStorage->bo = drmMemory->getBO();
    hostPtrManager->storeFragment(gfxAllocation->getRootDeviceIndex(), fragment);
}

void DrmMemoryManager::removeAllocationFromHostPtrManager(GraphicsAllocation *gfxAllocation) {
    auto buffer = gfxAllocation->getUnderlyingBuffer();
    auto fragment = hostPtrManager->getFragment({buffer, gfxAllocation->getRootDeviceIndex()});
    if (fragment && fragment->driverAllocation) {
        OsHandle *osStorageToRelease = fragment->osInternalStorage;
        ResidencyData *residencyDataToRelease = fragment->residency;
        if (hostPtrManager->releaseHostPtr(gfxAllocation->getRootDeviceIndex(), buffer)) {
            delete osStorageToRelease;
            delete residencyDataToRelease;
        }
    }
}

void DrmMemoryManager::freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) {
    DrmAllocation *drmAlloc = static_cast<DrmAllocation *>(gfxAllocation);
    this->unregisterAllocation(gfxAllocation);

    for (auto &engine : this->registeredEngines) {
        auto memoryOperationsInterface = static_cast<DrmMemoryOperationsHandler *>(executionEnvironment.rootDeviceEnvironments[gfxAllocation->getRootDeviceIndex()]->memoryOperationsInterface.get());
        memoryOperationsInterface->evictWithinOsContext(engine.osContext, *gfxAllocation);
    }

    for (auto handleId = 0u; handleId < gfxAllocation->getNumGmms(); handleId++) {
        delete gfxAllocation->getGmm(handleId);
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

    drmAlloc->freeRegisteredBOBindExtHandles(&getDrm(drmAlloc->getRootDeviceIndex()));

    delete gfxAllocation;
}

void DrmMemoryManager::handleFenceCompletion(GraphicsAllocation *allocation) {
    static_cast<DrmAllocation *>(allocation)->getBO()->wait(-1);
}

uint64_t DrmMemoryManager::getSystemSharedMemory(uint32_t rootDeviceIndex) {
    uint64_t hostMemorySize = MemoryConstants::pageSize * (uint64_t)(sysconf(_SC_PHYS_PAGES));

    drm_i915_gem_context_param getContextParam = {};
    getContextParam.param = I915_CONTEXT_PARAM_GTT_SIZE;
    auto ret = getDrm(rootDeviceIndex).ioctl(DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM, &getContextParam);
    DEBUG_BREAK_IF(ret != 0);
    UNUSED_VARIABLE(ret);
    uint64_t gpuMemorySize = getContextParam.value;

    return std::min(hostMemorySize, gpuMemorySize);
}

MemoryManager::AllocationStatus DrmMemoryManager::populateOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) {
    BufferObject *allocatedBos[maxFragmentsCount];
    uint32_t numberOfBosAllocated = 0;
    uint32_t indexesOfAllocatedBos[maxFragmentsCount];
    auto maxOsContextCount = 1u;

    for (unsigned int i = 0; i < maxFragmentsCount; i++) {
        // If there is no fragment it means it already exists.
        if (!handleStorage.fragmentStorageData[i].osHandleStorage && handleStorage.fragmentStorageData[i].fragmentSize) {
            handleStorage.fragmentStorageData[i].osHandleStorage = new OsHandle();
            handleStorage.fragmentStorageData[i].residency = new ResidencyData(maxOsContextCount);

            handleStorage.fragmentStorageData[i].osHandleStorage->bo = allocUserptr((uintptr_t)handleStorage.fragmentStorageData[i].cpuPtr,
                                                                                    handleStorage.fragmentStorageData[i].fragmentSize,
                                                                                    0, rootDeviceIndex);
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
        int result = pinBBs.at(rootDeviceIndex)->pin(allocatedBos, numberOfBosAllocated, registeredEngines[defaultEngineIndex].osContext, 0, getDefaultDrmContextId());

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
        hostPtrManager->storeFragment(rootDeviceIndex, handleStorage.fragmentStorageData[indexesOfAllocatedBos[i]]);
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

    return getDrm(graphicsAllocation.getRootDeviceIndex()).ioctl(DRM_IOCTL_I915_GEM_SET_DOMAIN, &set_domain) == 0;
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
    if (getDrm(graphicsAllocation.getRootDeviceIndex()).ioctl(DRM_IOCTL_I915_GEM_MMAP, &mmap_arg) != 0) {
        return nullptr;
    }

    bo->setLockedAddress(reinterpret_cast<void *>(mmap_arg.addr_ptr));

    auto success = setDomainCpu(graphicsAllocation, false);
    DEBUG_BREAK_IF(!success);
    (void)success;

    return bo->peekLockedAddress();
}

void DrmMemoryManager::unlockResourceImpl(GraphicsAllocation &graphicsAllocation) {
    if (MemoryPool::LocalMemory == graphicsAllocation.getMemoryPool()) {
        return unlockResourceInLocalMemoryImpl(static_cast<DrmAllocation &>(graphicsAllocation).getBO());
    }

    auto cpuPtr = graphicsAllocation.getUnderlyingBuffer();
    if (cpuPtr != nullptr) {
        return;
    }

    auto bo = static_cast<DrmAllocation &>(graphicsAllocation).getBO();
    if (bo == nullptr)
        return;

    releaseReservedCpuAddressRange(bo->peekLockedAddress(), bo->peekSize(), graphicsAllocation.getRootDeviceIndex());

    bo->setLockedAddress(nullptr);
}

int DrmMemoryManager::obtainFdFromHandle(int boHandle, uint32_t rootDeviceindex) {
    drm_prime_handle openFd = {0, 0, 0};

    openFd.flags = DRM_CLOEXEC | DRM_RDWR;
    openFd.handle = boHandle;

    getDrm(rootDeviceindex).ioctl(DRM_IOCTL_PRIME_HANDLE_TO_FD, &openFd);

    return openFd.fd;
}

uint32_t DrmMemoryManager::getDefaultDrmContextId() const {
    auto osContextLinux = static_cast<OsContextLinux *>(registeredEngines[defaultEngineIndex].osContext);
    return osContextLinux->getDrmContextIds()[0];
}

size_t DrmMemoryManager::getUserptrAlignment() {
    auto alignment = MemoryConstants::allocationAlignment;

    if (DebugManager.flags.ForceUserptrAlignment.get() != -1) {
        alignment = DebugManager.flags.ForceUserptrAlignment.get() * MemoryConstants::kiloByte;
    }

    return alignment;
}

Drm &DrmMemoryManager::getDrm(uint32_t rootDeviceIndex) const {
    return *this->executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->get()->getDrm();
}

uint32_t DrmMemoryManager::getRootDeviceIndex(const Drm *drm) {
    auto rootDeviceCount = this->executionEnvironment.rootDeviceEnvironments.size();

    for (auto rootDeviceIndex = 0u; rootDeviceIndex < rootDeviceCount; rootDeviceIndex++) {
        if (&getDrm(rootDeviceIndex) == drm) {
            return rootDeviceIndex;
        }
    }
    return CommonConstants::unspecifiedDeviceIndex;
}

AddressRange DrmMemoryManager::reserveGpuAddress(size_t size, uint32_t rootDeviceIndex) {
    auto gpuVa = acquireGpuRange(size, false, rootDeviceIndex, false);
    return AddressRange{gpuVa, size};
}

void DrmMemoryManager::freeGpuAddress(AddressRange addressRange, uint32_t rootDeviceIndex) {
    releaseGpuRange(reinterpret_cast<void *>(addressRange.address), addressRange.size, rootDeviceIndex);
}

std::unique_lock<std::mutex> DrmMemoryManager::acquireAllocLock() {
    return std::unique_lock<std::mutex>(this->allocMutex);
}

std::vector<GraphicsAllocation *> &DrmMemoryManager::getSysMemAllocs() {
    return this->sysMemAllocs;
}

std::vector<GraphicsAllocation *> &DrmMemoryManager::getLocalMemAllocs(uint32_t rootDeviceIndex) {
    return this->localMemAllocs[rootDeviceIndex];
}

void DrmMemoryManager::registerSysMemAlloc(GraphicsAllocation *allocation) {
    std::lock_guard<std::mutex> lock(this->allocMutex);
    this->sysMemAllocs.push_back(allocation);
}

void DrmMemoryManager::registerLocalMemAlloc(GraphicsAllocation *allocation, uint32_t rootDeviceIndex) {
    std::lock_guard<std::mutex> lock(this->allocMutex);
    this->localMemAllocs[rootDeviceIndex].push_back(allocation);
}
void DrmMemoryManager::unregisterAllocation(GraphicsAllocation *allocation) {
    std::lock_guard<std::mutex> lock(this->allocMutex);
    sysMemAllocs.erase(std::remove(sysMemAllocs.begin(), sysMemAllocs.end(), allocation),
                       sysMemAllocs.end());
    localMemAllocs[allocation->getRootDeviceIndex()].erase(std::remove(localMemAllocs[allocation->getRootDeviceIndex()].begin(),
                                                                       localMemAllocs[allocation->getRootDeviceIndex()].end(),
                                                                       allocation),
                                                           localMemAllocs[allocation->getRootDeviceIndex()].end());
}

void DrmMemoryManager::registerAllocation(GraphicsAllocation *allocation) {
    if (allocation) {
        auto drmAllocation = static_cast<DrmAllocation *>(allocation);
        drmAllocation->registerBOBindExtHandle(&getDrm(drmAllocation->getRootDeviceIndex()));
    }
}
} // namespace NEO
