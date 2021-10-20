/*
 * Copyright (C) 2018-2021 Intel Corporation
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
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/host_ptr_manager.h"
#include "shared/source/memory_manager/residency.h"
#include "shared/source/os_interface/linux/allocator_helper.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/os_interface.h"

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

    alignmentSelector.addCandidateAlignment(MemoryConstants::pageSize64k, true, AlignmentSelector::anyWastage, HeapIndex::HEAP_STANDARD64KB);
    if (DebugManager.flags.AlignLocalMemoryVaTo2MB.get() != 0) {
        alignmentSelector.addCandidateAlignment(MemoryConstants::pageSize2Mb, false, AlignmentSelector::anyWastage, HeapIndex::HEAP_STANDARD2MB);
    }
    const size_t customAlignment = static_cast<size_t>(DebugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.get());
    if (customAlignment > 0) {
        const auto heapIndex = customAlignment >= MemoryConstants::pageSize2Mb ? HeapIndex::HEAP_STANDARD2MB : HeapIndex::HEAP_STANDARD64KB;
        alignmentSelector.addCandidateAlignment(customAlignment, true, AlignmentSelector::anyWastage, heapIndex);
    }

    initialize(mode);
}

void DrmMemoryManager::initialize(gemCloseWorkerMode mode) {
    bool disableGemCloseWorker = true;

    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < gfxPartitions.size(); ++rootDeviceIndex) {
        auto gpuAddressSpace = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo()->capabilityTable.gpuAddressSpace;
        if (!getGfxPartition(rootDeviceIndex)->init(gpuAddressSpace, getSizeToReserve(), rootDeviceIndex, gfxPartitions.size(), heapAssigner.apiAllowExternalHeapForSshAndDsh)) {
            initialized = false;
            return;
        }
        localMemAllocs.emplace_back();
        disableGemCloseWorker &= getDrm(rootDeviceIndex).isVmBindAvailable();
    }
    MemoryManager::virtualPaddingAvailable = true;

    if (disableGemCloseWorker) {
        mode = gemCloseWorkerMode::gemCloseWorkerInactive;
    }

    if (DebugManager.flags.EnableGemCloseWorker.get() != -1) {
        mode = DebugManager.flags.EnableGemCloseWorker.get() ? gemCloseWorkerMode::gemCloseWorkerActive : gemCloseWorkerMode::gemCloseWorkerInactive;
    }

    if (mode != gemCloseWorkerMode::gemCloseWorkerInactive) {
        gemCloseWorker.reset(new DrmGemCloseWorker(*this));
    }

    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < gfxPartitions.size(); ++rootDeviceIndex) {
        if (forcePinEnabled || validateHostPtrMemory) {
            auto cpuAddrBo = alignedMallocWrapper(MemoryConstants::pageSize, MemoryConstants::pageSize);
            UNRECOVERABLE_IF(cpuAddrBo == nullptr);
            // Preprogram the Bo with MI_BATCH_BUFFER_END and MI_NOOP. This BO will be used as the last BB in a series to indicate the end of submission.
            reinterpret_cast<uint32_t *>(cpuAddrBo)[0] = 0x05000000; // MI_BATCH_BUFFER_END
            reinterpret_cast<uint32_t *>(cpuAddrBo)[1] = 0;          // MI_NOOP
            memoryForPinBBs.push_back(cpuAddrBo);
            DEBUG_BREAK_IF(memoryForPinBBs[rootDeviceIndex] == nullptr);
        }
        pinBBs.push_back(createRootDeviceBufferObject(rootDeviceIndex));
    }

    initialized = true;
}

BufferObject *DrmMemoryManager::createRootDeviceBufferObject(uint32_t rootDeviceIndex) {
    BufferObject *bo = nullptr;
    if (forcePinEnabled || validateHostPtrMemory) {
        bo = allocUserptr(reinterpret_cast<uintptr_t>(memoryForPinBBs[rootDeviceIndex]), MemoryConstants::pageSize, 0, rootDeviceIndex);
        if (bo) {
            if (isLimitedRange(rootDeviceIndex)) {
                auto boSize = bo->peekSize();
                bo->setAddress(acquireGpuRange(boSize, rootDeviceIndex, HeapIndex::HEAP_STANDARD));
                UNRECOVERABLE_IF(boSize < bo->peekSize());
            }
        } else {
            alignedFreeWrapper(memoryForPinBBs[rootDeviceIndex]);
            memoryForPinBBs[rootDeviceIndex] = nullptr;
            DEBUG_BREAK_IF(true);
            UNRECOVERABLE_IF(validateHostPtrMemory);
        }
    }
    return bo;
}

void DrmMemoryManager::createDeviceSpecificMemResources(uint32_t rootDeviceIndex) {
    pinBBs[rootDeviceIndex] = createRootDeviceBufferObject(rootDeviceIndex);
}

DrmMemoryManager::~DrmMemoryManager() {
    for (auto &memoryForPinBB : memoryForPinBBs) {
        if (memoryForPinBB) {
            MemoryManager::alignedFreeWrapper(memoryForPinBB);
        }
    }
}

void DrmMemoryManager::releaseDeviceSpecificMemResources(uint32_t rootDeviceIndex) {
    return releaseBufferObject(rootDeviceIndex);
}

void DrmMemoryManager::releaseBufferObject(uint32_t rootDeviceIndex) {
    if (auto bo = pinBBs[rootDeviceIndex]) {
        if (isLimitedRange(rootDeviceIndex)) {
            releaseGpuRange(reinterpret_cast<void *>(bo->peekAddress()), bo->peekSize(), rootDeviceIndex);
        }
        DrmMemoryManager::unreference(bo, true);
        pinBBs[rootDeviceIndex] = nullptr;
    }
}

void DrmMemoryManager::commonCleanup() {
    if (gemCloseWorker) {
        gemCloseWorker->close(false);
    }

    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < pinBBs.size(); ++rootDeviceIndex) {
        releaseBufferObject(rootDeviceIndex);
    }
    pinBBs.clear();
}

void DrmMemoryManager::eraseSharedBufferObject(NEO::BufferObject *bo) {
    auto it = std::find(sharingBufferObjects.begin(), sharingBufferObjects.end(), bo);
    DEBUG_BREAK_IF(it == sharingBufferObjects.end());
    releaseGpuRange(reinterpret_cast<void *>((*it)->peekAddress()), (*it)->peekUnmapSize(), this->getRootDeviceIndex(bo->peekDrm()));
    sharingBufferObjects.erase(it);
}

void DrmMemoryManager::pushSharedBufferObject(NEO::BufferObject *bo) {
    bo->markAsReusableAllocation();
    sharingBufferObjects.push_back(bo);
}

uint32_t DrmMemoryManager::unreference(NEO::BufferObject *bo, bool synchronousDestroy) {
    if (!bo)
        return -1;

    if (synchronousDestroy) {
        while (bo->getRefCount() > 1)
            ;
    }

    std::unique_lock<std::mutex> lock(mtx, std::defer_lock);
    if (bo->peekIsReusableAllocation()) {
        lock.lock();
    }

    uint32_t r = bo->unreference();

    if (r == 1) {
        if (bo->peekIsReusableAllocation()) {
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

uint64_t DrmMemoryManager::acquireGpuRange(size_t &size, uint32_t rootDeviceIndex, HeapIndex heapIndex) {
    auto gfxPartition = getGfxPartition(rootDeviceIndex);
    return GmmHelper::canonize(gfxPartition->heapAllocate(heapIndex, size));
}

void DrmMemoryManager::releaseGpuRange(void *address, size_t unmapSize, uint32_t rootDeviceIndex) {
    uint64_t graphicsAddress = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(address));
    graphicsAddress = GmmHelper::decanonize(graphicsAddress);
    auto gfxPartition = getGfxPartition(rootDeviceIndex);
    gfxPartition->freeGpuAddressRange(graphicsAddress, unmapSize);
}

bool DrmMemoryManager::isKmdMigrationAvailable(uint32_t rootDeviceIndex) {
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
    auto &hwHelper = NEO::HwHelper::get(hwInfo->platform.eRenderCoreFamily);

    auto useKmdMigration = hwHelper.isKmdMigrationSupported(*hwInfo);

    if (DebugManager.flags.UseKmdMigration.get() != -1) {
        useKmdMigration = DebugManager.flags.UseKmdMigration.get();
    }

    return useKmdMigration;
}

bool DrmMemoryManager::setMemAdvise(GraphicsAllocation *gfxAllocation, MemAdviseFlags flags, uint32_t rootDeviceIndex) {
    auto drmAllocation = static_cast<DrmAllocation *>(gfxAllocation);

    return drmAllocation->setMemAdvise(&this->getDrm(rootDeviceIndex), flags);
}

NEO::BufferObject *DrmMemoryManager::allocUserptr(uintptr_t address, size_t size, uint64_t flags, uint32_t rootDeviceIndex) {
    drm_i915_gem_userptr userptr = {};
    userptr.user_ptr = address;
    userptr.user_size = size;
    userptr.flags = static_cast<uint32_t>(flags);

    if (this->getDrm(rootDeviceIndex).ioctl(DRM_IOCTL_I915_GEM_USERPTR, &userptr) != 0) {
        return nullptr;
    }

    PRINT_DEBUG_STRING(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "Created new BO with GEM_USERPTR, handle: BO-%d\n", userptr.handle);

    auto res = new (std::nothrow) BufferObject(&getDrm(rootDeviceIndex), userptr.handle, size, maxOsContextCount);
    if (!res) {
        DEBUG_BREAK_IF(true);
        return nullptr;
    }
    res->setAddress(address);

    return res;
}

void DrmMemoryManager::emitPinningRequest(BufferObject *bo, const AllocationData &allocationData) const {
    auto rootDeviceIndex = allocationData.rootDeviceIndex;
    if (forcePinEnabled && pinBBs.at(rootDeviceIndex) != nullptr && allocationData.flags.forcePin && allocationData.size >= this->pinThreshold) {
        pinBBs.at(rootDeviceIndex)->pin(&bo, 1, registeredEngines[defaultEngineIndex[rootDeviceIndex]].osContext, 0, getDefaultDrmContextId(rootDeviceIndex));
    }
}

DrmAllocation *DrmMemoryManager::createGraphicsAllocation(OsHandleStorage &handleStorage, const AllocationData &allocationData) {
    auto hostPtr = const_cast<void *>(allocationData.hostPtr);
    auto allocation = std::make_unique<DrmAllocation>(allocationData.rootDeviceIndex, allocationData.type, nullptr, hostPtr, castToUint64(hostPtr), allocationData.size, MemoryPool::System4KBPages);
    allocation->fragmentsStorage = handleStorage;
    if (!allocation->setCacheRegion(&this->getDrm(allocationData.rootDeviceIndex), static_cast<CacheRegion>(allocationData.cacheRegion))) {
        return nullptr;
    }
    return allocation.release();
}

DrmAllocation *DrmMemoryManager::allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) {
    if (allocationData.type == NEO::GraphicsAllocation::AllocationType::DEBUG_CONTEXT_SAVE_AREA) {
        return createMultiHostAllocation(allocationData);
    }

    return allocateGraphicsMemoryWithAlignmentImpl(allocationData);
}

DrmAllocation *DrmMemoryManager::allocateGraphicsMemoryWithAlignmentImpl(const AllocationData &allocationData) {
    const size_t minAlignment = getUserptrAlignment();
    size_t cAlignment = alignUp(std::max(allocationData.alignment, minAlignment), minAlignment);
    // When size == 0 allocate allocationAlignment
    // It's needed to prevent overlapping pages with user pointers
    size_t cSize = std::max(alignUp(allocationData.size, minAlignment), minAlignment);

    uint64_t gpuReservationAddress = 0;
    uint64_t alignedGpuAddress = 0;
    size_t alignedStorageSize = cSize;
    size_t alignedVirtualAdressRangeSize = cSize;
    auto svmCpuAllocation = allocationData.type == GraphicsAllocation::AllocationType::SVM_CPU;
    if (svmCpuAllocation) {
        //add padding in case reserved addr is not aligned
        alignedStorageSize = alignUp(cSize, cAlignment);
        alignedVirtualAdressRangeSize = alignedStorageSize + cAlignment;
    }

    // if limitedRangeAlloction is enabled, memory allocation for bo in the limited Range heap is required
    if ((isLimitedRange(allocationData.rootDeviceIndex) || svmCpuAllocation) && !allocationData.flags.isUSMHostAllocation) {
        gpuReservationAddress = acquireGpuRange(alignedVirtualAdressRangeSize, allocationData.rootDeviceIndex, HeapIndex::HEAP_STANDARD);
        if (!gpuReservationAddress) {
            return nullptr;
        }

        alignedGpuAddress = gpuReservationAddress;
        if (svmCpuAllocation) {
            alignedGpuAddress = alignUp(gpuReservationAddress, cAlignment);
        }
    }

    auto drmAllocation = createAllocWithAlignment(allocationData, cSize, cAlignment, alignedStorageSize, alignedGpuAddress);
    if (drmAllocation != nullptr) {
        drmAllocation->setReservedAddressRange(reinterpret_cast<void *>(gpuReservationAddress), alignedVirtualAdressRangeSize);
    }

    return drmAllocation;
}

DrmAllocation *DrmMemoryManager::createAllocWithAlignmentFromUserptr(const AllocationData &allocationData, size_t size, size_t alignment, size_t alignedSVMSize, uint64_t gpuAddress) {
    auto res = alignedMallocWrapper(size, alignment);
    if (!res) {
        return nullptr;
    }

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(allocUserptr(reinterpret_cast<uintptr_t>(res), size, 0, allocationData.rootDeviceIndex));
    if (!bo) {
        alignedFreeWrapper(res);
        return nullptr;
    }

    zeroCpuMemoryIfRequested(allocationData, res, size);
    obtainGpuAddress(allocationData, bo.get(), gpuAddress);
    emitPinningRequest(bo.get(), allocationData);

    auto allocation = std::make_unique<DrmAllocation>(allocationData.rootDeviceIndex, allocationData.type, bo.get(), res, bo->peekAddress(), size, MemoryPool::System4KBPages);
    allocation->setDriverAllocatedCpuPtr(res);
    allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuAddress), alignedSVMSize);
    if (!allocation->setCacheRegion(&this->getDrm(allocationData.rootDeviceIndex), static_cast<CacheRegion>(allocationData.cacheRegion))) {
        alignedFreeWrapper(res);
        return nullptr;
    }

    bo.release();

    return allocation.release();
}

void DrmMemoryManager::obtainGpuAddress(const AllocationData &allocationData, BufferObject *bo, uint64_t gpuAddress) {
    if ((isLimitedRange(allocationData.rootDeviceIndex) || allocationData.type == GraphicsAllocation::AllocationType::SVM_CPU) &&
        !allocationData.flags.isUSMHostAllocation) {
        bo->setAddress(gpuAddress);
    }
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
        gpuAddress = acquireGpuRange(cSize, allocationData.rootDeviceIndex, HeapIndex::HEAP_STANDARD);
        if (!gpuAddress) {
            return nullptr;
        }
        bo->setAddress(gpuAddress);
    }

    emitPinningRequest(bo.get(), allocationData);

    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex,
                                        allocationData.type,
                                        bo.get(),
                                        bufferPtr,
                                        bo->peekAddress(),
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
    bo->setAddress(allocationData.gpuAddress);

    BufferObject *boPtr = bo.get();
    if (forcePinEnabled && pinBBs.at(allocationData.rootDeviceIndex) != nullptr && alignedSize >= this->pinThreshold) {
        pinBBs.at(allocationData.rootDeviceIndex)->pin(&boPtr, 1, osContextLinux, 0, osContextLinux->getDrmContextIds()[0]);
    }

    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, allocationData.type, bo.get(), res, bo->peekAddress(), alignedSize, MemoryPool::System4KBPages);
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
    auto rootDeviceIndex = allocationData.rootDeviceIndex;

    auto gpuVirtualAddress = acquireGpuRange(alignedSize, rootDeviceIndex, HeapIndex::HEAP_STANDARD);
    if (!gpuVirtualAddress) {
        return nullptr;
    }

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(allocUserptr(reinterpret_cast<uintptr_t>(alignedPtr), realAllocationSize, 0, rootDeviceIndex));
    if (!bo) {
        releaseGpuRange(reinterpret_cast<void *>(gpuVirtualAddress), alignedSize, rootDeviceIndex);
        return nullptr;
    }

    bo->setAddress(gpuVirtualAddress);

    if (validateHostPtrMemory) {
        auto boPtr = bo.get();
        auto vmHandleId = Math::getMinLsbSet(static_cast<uint32_t>(allocationData.storageInfo.subDeviceBitfield.to_ulong()));
        int result = pinBBs.at(rootDeviceIndex)->validateHostPtr(&boPtr, 1, registeredEngines[defaultEngineIndex[rootDeviceIndex]].osContext, vmHandleId, getDefaultDrmContextId(rootDeviceIndex));
        if (result != 0) {
            unreference(bo.release(), true);
            releaseGpuRange(reinterpret_cast<void *>(gpuVirtualAddress), alignedSize, rootDeviceIndex);
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

GraphicsAllocation *DrmMemoryManager::allocateMemoryByKMD(const AllocationData &allocationData) {
    auto gmm = std::make_unique<Gmm>(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmClientContext(), allocationData.hostPtr, allocationData.size, 0u, false);
    size_t bufferSize = allocationData.size;
    uint64_t gpuRange = acquireGpuRange(bufferSize, allocationData.rootDeviceIndex, HeapIndex::HEAP_STANDARD64KB);

    drm_i915_gem_create create = {0, 0, 0};
    create.size = bufferSize;

    auto ret = this->getDrm(allocationData.rootDeviceIndex).ioctl(DRM_IOCTL_I915_GEM_CREATE, &create);
    DEBUG_BREAK_IF(ret != 0);
    ((void)(ret));

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(new BufferObject(&getDrm(allocationData.rootDeviceIndex), create.handle, bufferSize, maxOsContextCount));
    bo->setAddress(gpuRange);

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

    uint64_t gpuRange = acquireGpuRange(allocationData.imgInfo->size, allocationData.rootDeviceIndex, HeapIndex::HEAP_STANDARD);

    drm_i915_gem_create create = {0, 0, 0};
    create.size = allocationData.imgInfo->size;

    [[maybe_unused]] auto ret = this->getDrm(allocationData.rootDeviceIndex).ioctl(DRM_IOCTL_I915_GEM_CREATE, &create);
    DEBUG_BREAK_IF(ret != 0);

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(new (std::nothrow) BufferObject(&getDrm(allocationData.rootDeviceIndex), create.handle, allocationData.imgInfo->size, maxOsContextCount));
    if (!bo) {
        return nullptr;
    }
    bo->setAddress(gpuRange);

    [[maybe_unused]] auto ret2 = bo->setTiling(I915_TILING_Y, static_cast<uint32_t>(allocationData.imgInfo->rowPitch));
    DEBUG_BREAK_IF(ret2 != true);

    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, allocationData.type, bo.get(), nullptr, gpuRange, allocationData.imgInfo->size, MemoryPool::SystemCpuInaccessible);
    allocation->setDefaultGmm(gmm.release());

    allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuRange), allocationData.imgInfo->size);
    bo.release();
    return allocation;
}

DrmAllocation *DrmMemoryManager::allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData, bool useLocalMemory) {
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHardwareInfo();
    auto allocatorToUse = heapAssigner.get32BitHeapIndex(allocationData.type, useLocalMemory, *hwInfo, allocationData.flags.use32BitFrontWindow);

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

        bo->setAddress(gpuVirtualAddress);
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
    auto gpuVA = gfxPartition->heapAllocate(allocatorToUse, allocationSize);

    if (!gpuVA) {
        return nullptr;
    }

    auto ptrAlloc = alignedMallocWrapper(alignedAllocationSize, getUserptrAlignment());

    if (!ptrAlloc) {
        gfxPartition->heapFree(allocatorToUse, gpuVA, allocationSize);
        return nullptr;
    }

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(allocUserptr(reinterpret_cast<uintptr_t>(ptrAlloc), alignedAllocationSize, 0, allocationData.rootDeviceIndex));

    if (!bo) {
        alignedFreeWrapper(ptrAlloc);
        gfxPartition->heapFree(allocatorToUse, gpuVA, allocationSize);
        return nullptr;
    }

    bo->setAddress(gpuVA);

    // softpin to the GPU address, res if it uses limitedRange Allocation
    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, allocationData.type, bo.get(), ptrAlloc, GmmHelper::canonize(gpuVA), alignedAllocationSize,
                                        MemoryPool::System4KBPagesWith32BitGpuAddressing);

    allocation->set32BitAllocation(true);
    allocation->setGpuBaseAddress(GmmHelper::canonize(gfxPartition->getHeapBase(allocatorToUse)));
    allocation->setDriverAllocatedCpuPtr(ptrAlloc);
    allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuVA), allocationSize);
    bo.release();
    return allocation;
}

BufferObject *DrmMemoryManager::findAndReferenceSharedBufferObject(int boHandle) {
    BufferObject *bo = nullptr;
    for (const auto &i : sharingBufferObjects) {
        if (i->peekHandle() == boHandle) {
            bo = i;
            bo->reference();
            break;
        }
    }

    return bo;
}

GraphicsAllocation *DrmMemoryManager::createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation) {
    if (isHostIpcAllocation) {
        return createUSMHostAllocationFromSharedHandle(handle, properties, false);
    }

    std::unique_lock<std::mutex> lock(mtx);

    drm_prime_handle openFd = {0, 0, 0};
    openFd.fd = handle;

    auto ret = this->getDrm(properties.rootDeviceIndex).ioctl(DRM_IOCTL_PRIME_FD_TO_HANDLE, &openFd);

    if (ret != 0) {
        [[maybe_unused]] int err = errno;
        PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(PRIME_FD_TO_HANDLE) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
        DEBUG_BREAK_IF(ret != 0);

        return nullptr;
    }

    auto boHandle = openFd.handle;
    auto bo = findAndReferenceSharedBufferObject(boHandle);

    if (bo == nullptr) {
        size_t size = lseekFunction(handle, 0, SEEK_END);

        bo = new (std::nothrow) BufferObject(&getDrm(properties.rootDeviceIndex), boHandle, size, maxOsContextCount);

        if (!bo) {
            return nullptr;
        }

        auto heapIndex = isLocalMemorySupported(properties.rootDeviceIndex) ? HeapIndex::HEAP_STANDARD2MB : HeapIndex::HEAP_STANDARD;
        if (requireSpecificBitness && this->force32bitAllocations) {
            heapIndex = HeapIndex::HEAP_EXTERNAL;
        }
        auto gpuRange = acquireGpuRange(size, properties.rootDeviceIndex, heapIndex);

        bo->setAddress(gpuRange);
        bo->setUnmapSize(size);

        pushSharedBufferObject(bo);
    }

    lock.unlock();

    auto drmAllocation = new DrmAllocation(properties.rootDeviceIndex, properties.allocationType, bo, reinterpret_cast<void *>(bo->peekAddress()), bo->peekSize(),
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

void DrmMemoryManager::closeSharedHandle(GraphicsAllocation *gfxAllocation) {
    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(gfxAllocation);
    if (drmAllocation->peekSharedHandle() != Sharing::nonSharedResource) {
        closeFunction(drmAllocation->peekSharedHandle());
        drmAllocation->setSharedHandle(Sharing::nonSharedResource);
    }
}

GraphicsAllocation *DrmMemoryManager::createPaddedAllocation(GraphicsAllocation *inputGraphicsAllocation, size_t sizeWithPadding) {
    uint64_t gpuRange = 0llu;

    auto rootDeviceIndex = inputGraphicsAllocation->getRootDeviceIndex();
    gpuRange = acquireGpuRange(sizeWithPadding, rootDeviceIndex, HeapIndex::HEAP_STANDARD);

    auto srcPtr = inputGraphicsAllocation->getUnderlyingBuffer();
    auto srcSize = inputGraphicsAllocation->getUnderlyingBufferSize();
    auto alignedSrcSize = alignUp(srcSize, MemoryConstants::pageSize);
    auto alignedPtr = (uintptr_t)alignDown(srcPtr, MemoryConstants::pageSize);
    auto offset = (uintptr_t)srcPtr - alignedPtr;

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(allocUserptr(alignedPtr, alignedSrcSize, 0, rootDeviceIndex));
    if (!bo) {
        return nullptr;
    }
    bo->setAddress(gpuRange);
    auto allocation = new DrmAllocation(rootDeviceIndex, inputGraphicsAllocation->getAllocationType(), bo.get(), srcPtr, GmmHelper::canonize(ptrOffset(gpuRange, offset)), sizeWithPadding,
                                        inputGraphicsAllocation->getMemoryPool());

    allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuRange), sizeWithPadding);
    bo.release();
    return allocation;
}

void DrmMemoryManager::addAllocationToHostPtrManager(GraphicsAllocation *gfxAllocation) {
    DrmAllocation *drmMemory = static_cast<DrmAllocation *>(gfxAllocation);

    FragmentStorage fragment = {};
    fragment.driverAllocation = true;
    fragment.fragmentCpuPointer = gfxAllocation->getUnderlyingBuffer();
    fragment.fragmentSize = alignUp(gfxAllocation->getUnderlyingBufferSize(), MemoryConstants::pageSize);

    auto osHandle = new OsHandleLinux();
    osHandle->bo = drmMemory->getBO();

    fragment.osInternalStorage = osHandle;
    fragment.residency = new ResidencyData(maxOsContextCount);
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
    if (DebugManager.flags.DoNotFreeResources.get()) {
        return;
    }
    DrmAllocation *drmAlloc = static_cast<DrmAllocation *>(gfxAllocation);
    this->unregisterAllocation(gfxAllocation);

    for (auto &engine : this->registeredEngines) {
        auto memoryOperationsInterface = static_cast<DrmMemoryOperationsHandler *>(executionEnvironment.rootDeviceEnvironments[gfxAllocation->getRootDeviceIndex()]->memoryOperationsInterface.get());
        memoryOperationsInterface->evictWithinOsContext(engine.osContext, *gfxAllocation);
    }

    if (drmAlloc->getMmapPtr()) {
        this->munmapFunction(drmAlloc->getMmapPtr(), drmAlloc->getMmapSize());
    }

    for (auto handleId = 0u; handleId < gfxAllocation->getNumGmms(); handleId++) {
        delete gfxAllocation->getGmm(handleId);
    }

    if (gfxAllocation->fragmentsStorage.fragmentCount) {
        cleanGraphicsMemoryCreatedFromHostPtr(gfxAllocation);
    } else {
        auto &bos = static_cast<DrmAllocation *>(gfxAllocation)->getBOs();
        for (auto bo : bos) {
            unreference(bo, bo && bo->peekIsReusableAllocation() ? false : true);
        }
        closeSharedHandle(gfxAllocation);
    }

    releaseGpuRange(gfxAllocation->getReservedAddressPtr(), gfxAllocation->getReservedAddressSize(), gfxAllocation->getRootDeviceIndex());
    alignedFreeWrapper(gfxAllocation->getDriverAllocatedCpuPtr());

    drmAlloc->freeRegisteredBOBindExtHandles(&getDrm(drmAlloc->getRootDeviceIndex()));

    delete gfxAllocation;
}

void DrmMemoryManager::handleFenceCompletion(GraphicsAllocation *allocation) {
    if (this->getDrm(allocation->getRootDeviceIndex()).isVmBindAvailable()) {
        waitForEnginesCompletion(*allocation);
    } else {
        static_cast<DrmAllocation *>(allocation)->getBO()->wait(-1);
    }
}

GraphicsAllocation *DrmMemoryManager::createGraphicsAllocationFromExistingStorage(AllocationProperties &properties, void *ptr, MultiGraphicsAllocation &multiGraphicsAllocation) {
    auto defaultAlloc = multiGraphicsAllocation.getDefaultGraphicsAllocation();
    if (static_cast<DrmAllocation *>(defaultAlloc)->getMmapPtr()) {
        properties.size = defaultAlloc->getUnderlyingBufferSize();
        properties.gpuAddress = castToUint64(ptr);

        auto internalHandle = defaultAlloc->peekInternalHandle(this);
        return createUSMHostAllocationFromSharedHandle(static_cast<osHandle>(internalHandle), properties, true);
    } else {
        return allocateGraphicsMemoryWithProperties(properties, ptr);
    }
}

uint64_t DrmMemoryManager::getSystemSharedMemory(uint32_t rootDeviceIndex) {
    uint64_t hostMemorySize = MemoryConstants::pageSize * (uint64_t)(sysconf(_SC_PHYS_PAGES));

    drm_i915_gem_context_param getContextParam = {};
    getContextParam.param = I915_CONTEXT_PARAM_GTT_SIZE;
    [[maybe_unused]] auto ret = getDrm(rootDeviceIndex).ioctl(DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM, &getContextParam);
    DEBUG_BREAK_IF(ret != 0);

    uint64_t gpuMemorySize = getContextParam.value;

    return std::min(hostMemorySize, gpuMemorySize);
}

double DrmMemoryManager::getPercentOfGlobalMemoryAvailable(uint32_t rootDeviceIndex) {
    if (isLocalMemorySupported(rootDeviceIndex)) {
        return 0.95;
    }
    return 0.8;
}

MemoryManager::AllocationStatus DrmMemoryManager::populateOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) {
    BufferObject *allocatedBos[maxFragmentsCount];
    uint32_t numberOfBosAllocated = 0;
    uint32_t indexesOfAllocatedBos[maxFragmentsCount];

    for (unsigned int i = 0; i < maxFragmentsCount; i++) {
        // If there is no fragment it means it already exists.
        if (!handleStorage.fragmentStorageData[i].osHandleStorage && handleStorage.fragmentStorageData[i].fragmentSize) {
            auto osHandle = new OsHandleLinux();

            handleStorage.fragmentStorageData[i].osHandleStorage = osHandle;
            handleStorage.fragmentStorageData[i].residency = new ResidencyData(maxOsContextCount);

            osHandle->bo = allocUserptr((uintptr_t)handleStorage.fragmentStorageData[i].cpuPtr,
                                        handleStorage.fragmentStorageData[i].fragmentSize,
                                        0, rootDeviceIndex);
            if (!osHandle->bo) {
                handleStorage.fragmentStorageData[i].freeTheFragment = true;
                return AllocationStatus::Error;
            }

            allocatedBos[numberOfBosAllocated] = osHandle->bo;
            indexesOfAllocatedBos[numberOfBosAllocated] = i;
            numberOfBosAllocated++;
        }
    }

    if (validateHostPtrMemory) {
        int result = pinBBs.at(rootDeviceIndex)->validateHostPtr(allocatedBos, numberOfBosAllocated, registeredEngines[defaultEngineIndex[rootDeviceIndex]].osContext, 0, getDefaultDrmContextId(rootDeviceIndex));

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
            auto osHandle = static_cast<OsHandleLinux *>(handleStorage.fragmentStorageData[i].osHandleStorage);
            if (osHandle->bo) {
                BufferObject *search = osHandle->bo;
                search->wait(-1);
                [[maybe_unused]] auto refCount = unreference(search, true);
                DEBUG_BREAK_IF(refCount != 1u);
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

uint32_t DrmMemoryManager::getDefaultDrmContextId(uint32_t rootDeviceIndex) const {
    auto osContextLinux = static_cast<OsContextLinux *>(registeredEngines[defaultEngineIndex[rootDeviceIndex]].osContext);
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
    return *this->executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->as<Drm>();
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
    auto gpuVa = acquireGpuRange(size, rootDeviceIndex, HeapIndex::HEAP_STANDARD);
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

void DrmMemoryManager::registerAllocationInOs(GraphicsAllocation *allocation) {
    if (allocation && getDrm(allocation->getRootDeviceIndex()).resourceRegistrationEnabled()) {
        auto drmAllocation = static_cast<DrmAllocation *>(allocation);
        drmAllocation->registerBOBindExtHandle(&getDrm(drmAllocation->getRootDeviceIndex()));

        if (isAllocationTypeToCapture(drmAllocation->getAllocationType())) {
            drmAllocation->markForCapture();
        }
    }
}

std::unique_ptr<MemoryManager> DrmMemoryManager::create(ExecutionEnvironment &executionEnvironment) {
    bool validateHostPtr = true;

    if (DebugManager.flags.EnableHostPtrValidation.get() != -1) {
        validateHostPtr = DebugManager.flags.EnableHostPtrValidation.get();
    }

    return std::make_unique<DrmMemoryManager>(gemCloseWorkerMode::gemCloseWorkerActive,
                                              DebugManager.flags.EnableForcePin.get(),
                                              validateHostPtr,
                                              executionEnvironment);
}

uint64_t DrmMemoryManager::getLocalMemorySize(uint32_t rootDeviceIndex, uint32_t deviceBitfield) {
    auto memoryInfo = getDrm(rootDeviceIndex).getMemoryInfo();
    if (!memoryInfo) {
        return 0;
    }

    auto hwInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
    uint32_t subDevicesCount = HwHelper::getSubDevicesCount(hwInfo);
    size_t size = 0;

    for (uint32_t i = 0; i < subDevicesCount; i++) {
        auto memoryBank = (1 << i);

        if (deviceBitfield & memoryBank) {
            size += memoryInfo->getMemoryRegionSize(memoryBank);
        }
    }

    return size;
}
void *DrmMemoryManager::lockResourceInLocalMemoryImpl(GraphicsAllocation &graphicsAllocation) {
    if (!isLocalMemorySupported(graphicsAllocation.getRootDeviceIndex())) {
        return nullptr;
    }
    auto bo = static_cast<DrmAllocation &>(graphicsAllocation).getBO();
    if (graphicsAllocation.getAllocationType() == GraphicsAllocation::AllocationType::WRITE_COMBINED) {
        auto addr = lockResourceInLocalMemoryImpl(bo);
        auto alignedAddr = alignUp(addr, MemoryConstants::pageSize64k);
        auto notUsedSize = ptrDiff(alignedAddr, addr);
        //call unmap to free the unaligned pages preceding the BO allocation and
        //adjust the pointer in the CPU mapping to the beginning of the BO allocation
        munmapFunction(addr, notUsedSize);
        bo->setLockedAddress(alignedAddr);
        return bo->peekLockedAddress();
    }
    return lockResourceInLocalMemoryImpl(bo);
}

bool DrmMemoryManager::copyMemoryToAllocation(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy) {
    if (graphicsAllocation->getUnderlyingBuffer() || !isLocalMemorySupported(graphicsAllocation->getRootDeviceIndex())) {
        return MemoryManager::copyMemoryToAllocation(graphicsAllocation, destinationOffset, memoryToCopy, sizeToCopy);
    }
    auto drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    for (auto handleId = 0u; handleId < graphicsAllocation->storageInfo.getNumBanks(); handleId++) {
        auto ptr = lockResourceInLocalMemoryImpl(drmAllocation->getBOs()[handleId]);
        if (!ptr) {
            return false;
        }
        memcpy_s(ptrOffset(ptr, destinationOffset), graphicsAllocation->getUnderlyingBufferSize() - destinationOffset, memoryToCopy, sizeToCopy);
        this->unlockResourceInLocalMemoryImpl(drmAllocation->getBOs()[handleId]);
    }
    return true;
}

void DrmMemoryManager::unlockResourceInLocalMemoryImpl(BufferObject *bo) {
    if (bo == nullptr)
        return;

    releaseReservedCpuAddressRange(bo->peekLockedAddress(), bo->peekSize(), this->getRootDeviceIndex(bo->peekDrm()));

    [[maybe_unused]] auto ret = munmapFunction(bo->peekLockedAddress(), bo->peekSize());
    DEBUG_BREAK_IF(ret != 0);

    bo->setLockedAddress(nullptr);
}

void createColouredGmms(GmmClientContext *clientContext, DrmAllocation &allocation, const StorageInfo &storageInfo, bool compression) {
    DEBUG_BREAK_IF(storageInfo.colouringPolicy == ColouringPolicy::DeviceCountBased && storageInfo.colouringGranularity != MemoryConstants::pageSize64k);

    auto remainingSize = alignUp(allocation.getUnderlyingBufferSize(), storageInfo.colouringGranularity);
    auto handles = storageInfo.getNumBanks();
    auto banksCnt = storageInfo.getTotalBanksCnt();

    if (storageInfo.colouringPolicy == ColouringPolicy::ChunkSizeBased) {
        handles = static_cast<uint32_t>(remainingSize / storageInfo.colouringGranularity);
        allocation.resizeGmms(handles);
    }
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
        auto currentSize = alignUp(remainingSize / (handles - handleId), storageInfo.colouringGranularity);
        remainingSize -= currentSize;
        StorageInfo limitedStorageInfo = storageInfo;
        limitedStorageInfo.memoryBanks &= (1u << (handleId % banksCnt));
        auto gmm = new Gmm(clientContext,
                           nullptr,
                           currentSize,
                           0u,
                           false,
                           compression,
                           false,
                           limitedStorageInfo);
        allocation.setGmm(gmm, handleId);
    }
}

void fillGmmsInAllocation(GmmClientContext *clientContext, DrmAllocation *allocation, const StorageInfo &storageInfo) {
    auto alignedSize = alignUp(allocation->getUnderlyingBufferSize(), MemoryConstants::pageSize64k);
    for (auto handleId = 0u; handleId < storageInfo.getNumBanks(); handleId++) {
        StorageInfo limitedStorageInfo = storageInfo;
        limitedStorageInfo.memoryBanks &= 1u << handleId;
        limitedStorageInfo.pageTablesVisibility &= 1u << handleId;
        auto gmm = new Gmm(clientContext, nullptr, alignedSize, 0u, false, false, false, limitedStorageInfo);
        allocation->setGmm(gmm, handleId);
    }
}

uint64_t getGpuAddress(const AlignmentSelector &alignmentSelector, HeapAssigner &heapAssigner, const HardwareInfo &hwInfo, GraphicsAllocation::AllocationType allocType, GfxPartition *gfxPartition,
                       size_t &sizeAllocated, const void *hostPtr, bool resource48Bit, bool useFrontWindow) {
    uint64_t gpuAddress = 0;
    switch (allocType) {
    case GraphicsAllocation::AllocationType::SVM_GPU:
        gpuAddress = reinterpret_cast<uint64_t>(hostPtr);
        sizeAllocated = 0;
        break;
    case GraphicsAllocation::AllocationType::KERNEL_ISA:
    case GraphicsAllocation::AllocationType::KERNEL_ISA_INTERNAL:
    case GraphicsAllocation::AllocationType::INTERNAL_HEAP:
    case GraphicsAllocation::AllocationType::DEBUG_MODULE_AREA: {
        auto heap = heapAssigner.get32BitHeapIndex(allocType, true, hwInfo, useFrontWindow);
        gpuAddress = GmmHelper::canonize(gfxPartition->heapAllocate(heap, sizeAllocated));
    } break;
    case GraphicsAllocation::AllocationType::WRITE_COMBINED:
        sizeAllocated = 0;
        break;
    default:
        AlignmentSelector::CandidateAlignment alignment = alignmentSelector.selectAlignment(sizeAllocated);
        if (gfxPartition->getHeapLimit(HeapIndex::HEAP_EXTENDED) > 0 && !resource48Bit) {
            alignment.heap = HeapIndex::HEAP_EXTENDED;
        }
        gpuAddress = GmmHelper::canonize(gfxPartition->heapAllocateWithCustomAlignment(alignment.heap, sizeAllocated, alignment.alignment));
        break;
    }
    return gpuAddress;
}

GraphicsAllocation *DrmMemoryManager::allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) {
    status = AllocationStatus::RetryInNonDevicePool;
    if (!this->localMemorySupported[allocationData.rootDeviceIndex] ||
        allocationData.flags.useSystemMemory ||
        (allocationData.flags.allow32Bit && this->force32bitAllocations) ||
        allocationData.type == GraphicsAllocation::AllocationType::SHARED_RESOURCE_COPY) {
        return nullptr;
    }

    if (allocationData.type == GraphicsAllocation::AllocationType::UNIFIED_SHARED_MEMORY) {
        auto allocation = this->createSharedUnifiedMemoryAllocation(allocationData);
        status = allocation ? AllocationStatus::Success : AllocationStatus::Error;
        return allocation;
    }

    std::unique_ptr<Gmm> gmm;
    size_t sizeAligned = 0;
    auto numHandles = allocationData.storageInfo.getNumBanks();
    bool createSingleHandle = 1 == numHandles;
    if (allocationData.type == GraphicsAllocation::AllocationType::IMAGE) {
        allocationData.imgInfo->useLocalMemory = true;
        gmm = std::make_unique<Gmm>(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmClientContext(), *allocationData.imgInfo, allocationData.storageInfo);
        sizeAligned = alignUp(allocationData.imgInfo->size, MemoryConstants::pageSize64k);
    } else {
        if (allocationData.type == GraphicsAllocation::AllocationType::WRITE_COMBINED) {
            sizeAligned = alignUp(allocationData.size + MemoryConstants::pageSize64k, 2 * MemoryConstants::megaByte) + 2 * MemoryConstants::megaByte;
        } else {
            sizeAligned = alignUp(allocationData.size, MemoryConstants::pageSize64k);
        }
        if (createSingleHandle) {
            gmm = std::make_unique<Gmm>(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmClientContext(),
                                        nullptr,
                                        sizeAligned,
                                        0u,
                                        allocationData.flags.uncacheable,
                                        allocationData.flags.preferRenderCompressed,
                                        false,
                                        allocationData.storageInfo);
        }
    }

    auto sizeAllocated = sizeAligned;
    auto gfxPartition = getGfxPartition(allocationData.rootDeviceIndex);
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHardwareInfo();
    auto gpuAddress = getGpuAddress(this->alignmentSelector, this->heapAssigner, *hwInfo,
                                    allocationData.type, gfxPartition, sizeAllocated,
                                    allocationData.hostPtr, allocationData.flags.resource48Bit, allocationData.flags.use32BitFrontWindow);

    auto allocation = std::make_unique<DrmAllocation>(allocationData.rootDeviceIndex, numHandles, allocationData.type, nullptr, nullptr, gpuAddress, sizeAligned, MemoryPool::LocalMemory);
    if (createSingleHandle) {
        allocation->setDefaultGmm(gmm.release());
    } else if (allocationData.storageInfo.multiStorage) {
        createColouredGmms(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmClientContext(),
                           *allocation,
                           allocationData.storageInfo,
                           allocationData.flags.preferRenderCompressed);
    } else {
        fillGmmsInAllocation(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmClientContext(), allocation.get(), allocationData.storageInfo);
    }
    allocation->storageInfo = allocationData.storageInfo;
    allocation->setFlushL3Required(allocationData.flags.flushL3);
    allocation->setUncacheable(allocationData.flags.uncacheable);
    allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuAddress), sizeAllocated);

    if (!createDrmAllocation(&getDrm(allocationData.rootDeviceIndex), allocation.get(), gpuAddress, maxOsContextCount)) {
        for (auto handleId = 0u; handleId < allocationData.storageInfo.getNumBanks(); handleId++) {
            delete allocation->getGmm(handleId);
        }
        gfxPartition->freeGpuAddressRange(GmmHelper::decanonize(gpuAddress), sizeAllocated);
        status = AllocationStatus::Error;
        return nullptr;
    }
    if (allocationData.type == GraphicsAllocation::AllocationType::WRITE_COMBINED) {
        auto cpuAddress = lockResource(allocation.get());
        auto alignedCpuAddress = alignDown(cpuAddress, 2 * MemoryConstants::megaByte);
        auto offset = ptrDiff(cpuAddress, alignedCpuAddress);
        allocation->setAllocationOffset(offset);
        allocation->setCpuPtrAndGpuAddress(cpuAddress, reinterpret_cast<uint64_t>(alignedCpuAddress));
        DEBUG_BREAK_IF(allocation->storageInfo.multiStorage);
        allocation->getBO()->setAddress(reinterpret_cast<uint64_t>(cpuAddress));
    }
    if (allocationData.flags.requiresCpuAccess) {
        auto cpuAddress = lockResource(allocation.get());
        allocation->setCpuPtrAndGpuAddress(cpuAddress, gpuAddress);
    }
    if (heapAssigner.useInternal32BitHeap(allocationData.type)) {
        allocation->setGpuBaseAddress(GmmHelper::canonize(getInternalHeapBaseAddress(allocationData.rootDeviceIndex, true)));
    }
    if (!allocation->setCacheRegion(&getDrm(allocationData.rootDeviceIndex), static_cast<CacheRegion>(allocationData.cacheRegion))) {
        for (auto bo : allocation->getBOs()) {
            delete bo;
        }
        for (auto handleId = 0u; handleId < allocationData.storageInfo.getNumBanks(); handleId++) {
            delete allocation->getGmm(handleId);
        }
        gfxPartition->freeGpuAddressRange(GmmHelper::decanonize(gpuAddress), sizeAllocated);
        status = AllocationStatus::Error;
        return nullptr;
    }

    status = AllocationStatus::Success;
    return allocation.release();
}

BufferObject *DrmMemoryManager::createBufferObjectInMemoryRegion(Drm *drm,
                                                                 uint64_t gpuAddress,
                                                                 size_t size,
                                                                 uint32_t memoryBanks,
                                                                 size_t maxOsContextCount) {
    auto memoryInfo = drm->getMemoryInfo();
    if (!memoryInfo) {
        return nullptr;
    }

    uint32_t handle = 0;
    auto ret = memoryInfo->createGemExtWithSingleRegion(drm, memoryBanks, size, handle);

    if (ret != 0) {
        return nullptr;
    }

    auto bo = new (std::nothrow) BufferObject(drm, handle, size, maxOsContextCount);
    if (!bo) {
        return nullptr;
    }

    bo->setAddress(gpuAddress);

    return bo;
}

bool DrmMemoryManager::createDrmAllocation(Drm *drm, DrmAllocation *allocation, uint64_t gpuAddress, size_t maxOsContextCount) {
    BufferObjects bos{};
    auto &storageInfo = allocation->storageInfo;
    auto boAddress = gpuAddress;
    auto currentBank = 0u;
    auto iterationOffset = 0u;
    auto banksCnt = storageInfo.getTotalBanksCnt();

    auto handles = storageInfo.getNumBanks();
    if (storageInfo.colouringPolicy == ColouringPolicy::ChunkSizeBased) {
        handles = allocation->getNumGmms();
        allocation->resizeBufferObjects(handles);
        bos.resize(handles);
    }

    for (auto handleId = 0u; handleId < handles; handleId++, currentBank++) {
        if (currentBank == banksCnt) {
            currentBank = 0;
            iterationOffset += banksCnt;
        }
        uint32_t memoryBanks = static_cast<uint32_t>(storageInfo.memoryBanks.to_ulong());
        if (storageInfo.getNumBanks() > 1) {
            //check if we have this bank, if not move to next one
            //we may have holes in memoryBanks that we need to skip i.e. memoryBanks 1101 and 3 handle allocation
            while (!(memoryBanks & (1u << currentBank))) {
                currentBank++;
            }
            memoryBanks &= 1u << currentBank;
        }
        auto boSize = alignUp(allocation->getGmm(handleId)->gmmResourceInfo->getSizeAllocation(), MemoryConstants::pageSize64k);
        bos[handleId] = createBufferObjectInMemoryRegion(drm, boAddress, boSize, memoryBanks, maxOsContextCount);
        if (nullptr == bos[handleId]) {
            return false;
        }
        allocation->getBufferObjectToModify(currentBank + iterationOffset) = bos[handleId];
        if (storageInfo.multiStorage) {
            boAddress += boSize;
        }
    }

    if (storageInfo.colouringPolicy == ColouringPolicy::MappingBased) {
        auto size = alignUp(allocation->getUnderlyingBufferSize(), storageInfo.colouringGranularity);
        auto chunks = static_cast<uint32_t>(size / storageInfo.colouringGranularity);
        auto granularity = storageInfo.colouringGranularity;

        for (uint32_t boHandle = 0; boHandle < handles; boHandle++) {
            bos[boHandle]->setColourWithBind();
            bos[boHandle]->setColourChunk(granularity);
            bos[boHandle]->reserveAddressVector(alignUp(chunks, handles) / handles);
        }

        auto boHandle = 0u;
        auto colourAddress = gpuAddress;
        for (auto chunk = 0u; chunk < chunks; chunk++) {
            if (boHandle == handles) {
                boHandle = 0u;
            }

            bos[boHandle]->addColouringAddress(colourAddress);
            colourAddress += granularity;

            boHandle++;
        }
    }

    return true;
}

} // namespace NEO
