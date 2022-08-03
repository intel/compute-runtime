/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_memory_manager.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/heap_assigner.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/host_ptr_manager.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/memory_manager/memory_pool.h"
#include "shared/source/memory_manager/residency.h"
#include "shared/source/os_interface/linux/allocator_helper.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_gem_close_worker.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/os_interface.h"

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
        bo = allocUserptr(reinterpret_cast<uintptr_t>(memoryForPinBBs[rootDeviceIndex]), MemoryConstants::pageSize, rootDeviceIndex);
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
        gemCloseWorker->close(true);
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
    auto gmmHelper = getGmmHelper(rootDeviceIndex);
    return gmmHelper->canonize(gfxPartition->heapAllocate(heapIndex, size));
}

void DrmMemoryManager::releaseGpuRange(void *address, size_t unmapSize, uint32_t rootDeviceIndex) {
    uint64_t graphicsAddress = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(address));
    auto gmmHelper = getGmmHelper(rootDeviceIndex);
    graphicsAddress = gmmHelper->decanonize(graphicsAddress);
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

bool DrmMemoryManager::setMemPrefetch(GraphicsAllocation *gfxAllocation, uint32_t subDeviceId, uint32_t rootDeviceIndex) {
    auto drmAllocation = static_cast<DrmAllocation *>(gfxAllocation);
    auto osContextLinux = static_cast<OsContextLinux *>(registeredEngines[defaultEngineIndex[rootDeviceIndex]].osContext);
    auto vmHandleId = subDeviceId;

    auto retVal = drmAllocation->bindBOs(osContextLinux, vmHandleId, nullptr, true);
    if (retVal != 0) {
        DEBUG_BREAK_IF(true);
        return false;
    }

    return drmAllocation->setMemPrefetch(&this->getDrm(rootDeviceIndex), subDeviceId);
}

NEO::BufferObject *DrmMemoryManager::allocUserptr(uintptr_t address, size_t size, uint32_t rootDeviceIndex) {
    GemUserPtr userptr = {};
    userptr.userPtr = address;
    userptr.userSize = size;

    auto &drm = this->getDrm(rootDeviceIndex);
    auto ioctlHelper = drm.getIoctlHelper();

    if (ioctlHelper->ioctl(DrmIoctl::GemUserptr, &userptr) != 0) {
        return nullptr;
    }

    PRINT_DEBUG_STRING(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "Created new BO with GEM_USERPTR, handle: BO-%d\n", userptr.handle);

    auto patIndex = drm.getPatIndex(nullptr, AllocationType::EXTERNAL_HOST_PTR, CacheRegion::Default, CachePolicy::WriteBack, false);

    auto res = new (std::nothrow) BufferObject(&drm, patIndex, userptr.handle, size, maxOsContextCount);
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

GraphicsAllocation *DrmMemoryManager::createGraphicsAllocation(OsHandleStorage &handleStorage, const AllocationData &allocationData) {
    auto hostPtr = const_cast<void *>(allocationData.hostPtr);
    auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(hostPtr));
    auto allocation = std::make_unique<DrmAllocation>(allocationData.rootDeviceIndex, allocationData.type, nullptr, hostPtr, canonizedGpuAddress, allocationData.size, MemoryPool::System4KBPages);
    allocation->fragmentsStorage = handleStorage;
    if (!allocation->setCacheRegion(&this->getDrm(allocationData.rootDeviceIndex), static_cast<CacheRegion>(allocationData.cacheRegion))) {
        return nullptr;
    }
    return allocation.release();
}

GraphicsAllocation *DrmMemoryManager::allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) {
    if (allocationData.type == NEO::AllocationType::DEBUG_CONTEXT_SAVE_AREA ||
        (allocationData.type == NEO::AllocationType::DEBUG_SBA_TRACKING_BUFFER &&
         allocationData.storageInfo.subDeviceBitfield.count() > 1)) {
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
    auto svmCpuAllocation = allocationData.type == AllocationType::SVM_CPU;
    if (svmCpuAllocation) {
        // add padding in case reserved addr is not aligned
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

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(allocUserptr(reinterpret_cast<uintptr_t>(res), size, allocationData.rootDeviceIndex));
    if (!bo) {
        alignedFreeWrapper(res);
        return nullptr;
    }

    zeroCpuMemoryIfRequested(allocationData, res, size);
    obtainGpuAddress(allocationData, bo.get(), gpuAddress);
    emitPinningRequest(bo.get(), allocationData);

    auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
    auto canonizedGpuAddress = gmmHelper->canonize(bo->peekAddress());
    auto allocation = std::make_unique<DrmAllocation>(allocationData.rootDeviceIndex, allocationData.type, bo.get(), res, canonizedGpuAddress, size, MemoryPool::System4KBPages);
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
    if ((isLimitedRange(allocationData.rootDeviceIndex) || allocationData.type == AllocationType::SVM_CPU) &&
        !allocationData.flags.isUSMHostAllocation) {
        bo->setAddress(gpuAddress);
    }
}

GraphicsAllocation *DrmMemoryManager::allocateUSMHostGraphicsMemory(const AllocationData &allocationData) {
    const size_t minAlignment = getUserptrAlignment();
    // When size == 0 allocate allocationAlignment
    // It's needed to prevent overlapping pages with user pointers
    size_t cSize = std::max(alignUp(allocationData.size, minAlignment), minAlignment);

    void *bufferPtr = const_cast<void *>(allocationData.hostPtr);
    DEBUG_BREAK_IF(nullptr == bufferPtr);

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(allocUserptr(reinterpret_cast<uintptr_t>(bufferPtr),
                                                                         cSize,
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

GraphicsAllocation *DrmMemoryManager::allocateGraphicsMemoryWithHostPtr(const AllocationData &allocationData) {
    auto res = static_cast<DrmAllocation *>(MemoryManager::allocateGraphicsMemoryWithHostPtr(allocationData));

    if (res != nullptr && !validateHostPtrMemory) {
        emitPinningRequest(res->getBO(), allocationData);
    }
    return res;
}

GraphicsAllocation *DrmMemoryManager::allocateGraphicsMemoryWithGpuVa(const AllocationData &allocationData) {

    if (allocationData.type == NEO::AllocationType::DEBUG_SBA_TRACKING_BUFFER &&
        allocationData.storageInfo.subDeviceBitfield.count() > 1) {
        return createMultiHostAllocation(allocationData);
    }

    auto osContextLinux = static_cast<OsContextLinux *>(allocationData.osContext);

    const size_t minAlignment = getUserptrAlignment();
    size_t alignedSize = alignUp(allocationData.size, minAlignment);

    auto res = alignedMallocWrapper(alignedSize, minAlignment);
    if (!res)
        return nullptr;

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(allocUserptr(reinterpret_cast<uintptr_t>(res), alignedSize, allocationData.rootDeviceIndex));

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

GraphicsAllocation *DrmMemoryManager::allocateGraphicsMemoryForNonSvmHostPtr(const AllocationData &allocationData) {
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

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(allocUserptr(reinterpret_cast<uintptr_t>(alignedPtr), realAllocationSize, rootDeviceIndex));
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

GraphicsAllocation *DrmMemoryManager::allocateGraphicsMemory64kb(const AllocationData &allocationData) {
    return nullptr;
}

GraphicsAllocation *DrmMemoryManager::allocateMemoryByKMD(const AllocationData &allocationData) {
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHardwareInfo();

    StorageInfo systemMemoryStorageInfo = {};
    auto gmm = std::make_unique<Gmm>(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(), allocationData.hostPtr,
                                     allocationData.size, 0u, CacheSettingsHelper::getGmmUsageType(allocationData.type, allocationData.flags.uncacheable, *hwInfo), false, systemMemoryStorageInfo, true);
    size_t bufferSize = allocationData.size;
    uint64_t gpuRange = acquireGpuRange(bufferSize, allocationData.rootDeviceIndex, HeapIndex::HEAP_STANDARD64KB);

    GemCreate create{};
    create.size = bufferSize;

    auto &drm = getDrm(allocationData.rootDeviceIndex);
    auto ioctlHelper = drm.getIoctlHelper();

    [[maybe_unused]] auto ret = ioctlHelper->ioctl(DrmIoctl::GemCreate, &create);
    DEBUG_BREAK_IF(ret != 0);

    auto patIndex = drm.getPatIndex(gmm.get(), allocationData.type, CacheRegion::Default, CachePolicy::WriteBack, false);

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(new BufferObject(&drm, patIndex, create.handle, bufferSize, maxOsContextCount));
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

    GemCreate create{};
    create.size = allocationData.imgInfo->size;

    auto &drm = this->getDrm(allocationData.rootDeviceIndex);
    auto ioctlHelper = drm.getIoctlHelper();

    [[maybe_unused]] auto ret = ioctlHelper->ioctl(DrmIoctl::GemCreate, &create);
    DEBUG_BREAK_IF(ret != 0);

    auto patIndex = drm.getPatIndex(gmm.get(), allocationData.type, CacheRegion::Default, CachePolicy::WriteBack, false);

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(new (std::nothrow) BufferObject(&drm, patIndex, create.handle, allocationData.imgInfo->size, maxOsContextCount));
    if (!bo) {
        return nullptr;
    }
    bo->setAddress(gpuRange);

    [[maybe_unused]] auto ret2 = bo->setTiling(ioctlHelper->getDrmParamValue(DrmParam::TilingY), static_cast<uint32_t>(allocationData.imgInfo->rowPitch));
    DEBUG_BREAK_IF(ret2 != true);

    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, allocationData.type, bo.get(), nullptr, gpuRange, allocationData.imgInfo->size, MemoryPool::SystemCpuInaccessible);
    allocation->setDefaultGmm(gmm.release());

    allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuRange), allocationData.imgInfo->size);
    bo.release();
    return allocation;
}

GraphicsAllocation *DrmMemoryManager::allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData, bool useLocalMemory) {
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

        std::unique_ptr<BufferObject, BufferObject::Deleter> bo(allocUserptr(alignedUserPointer, allocationSize, allocationData.rootDeviceIndex));
        if (!bo) {
            gfxPartition->heapFree(allocatorToUse, gpuVirtualAddress, realAllocationSize);
            return nullptr;
        }

        bo->setAddress(gpuVirtualAddress);
        auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
        auto canonizedGpuAddress = gmmHelper->canonize(ptrOffset(gpuVirtualAddress, inputPointerOffset));
        auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, allocationData.type, bo.get(), const_cast<void *>(allocationData.hostPtr),
                                            canonizedGpuAddress,
                                            allocationSize, MemoryPool::System4KBPagesWith32BitGpuAddressing);
        allocation->set32BitAllocation(true);
        allocation->setGpuBaseAddress(gmmHelper->canonize(gfxPartition->getHeapBase(allocatorToUse)));
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

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(allocUserptr(reinterpret_cast<uintptr_t>(ptrAlloc), alignedAllocationSize, allocationData.rootDeviceIndex));

    if (!bo) {
        alignedFreeWrapper(ptrAlloc);
        gfxPartition->heapFree(allocatorToUse, gpuVA, allocationSize);
        return nullptr;
    }

    bo->setAddress(gpuVA);
    auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);

    // softpin to the GPU address, res if it uses limitedRange Allocation
    auto canonizedGpuAddress = gmmHelper->canonize(gpuVA);
    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, allocationData.type, bo.get(), ptrAlloc,
                                        canonizedGpuAddress, alignedAllocationSize,
                                        MemoryPool::System4KBPagesWith32BitGpuAddressing);

    allocation->set32BitAllocation(true);
    allocation->setGpuBaseAddress(gmmHelper->canonize(gfxPartition->getHeapBase(allocatorToUse)));
    allocation->setDriverAllocatedCpuPtr(ptrAlloc);
    allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuVA), allocationSize);
    bo.release();
    return allocation;
}

BufferObject *DrmMemoryManager::findAndReferenceSharedBufferObject(int boHandle, uint32_t rootDeviceIndex) {
    BufferObject *bo = nullptr;
    for (const auto &i : sharingBufferObjects) {
        if (i->getHandle() == boHandle && i->getRootDeviceIndex() == rootDeviceIndex) {
            bo = i;
            bo->reference();
            break;
        }
    }

    return bo;
}

GraphicsAllocation *DrmMemoryManager::createGraphicsAllocationFromMultipleSharedHandles(std::vector<osHandle> handles, AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation) {
    BufferObjects bos;
    std::vector<size_t> sizes;
    size_t totalSize = 0;

    std::unique_lock<std::mutex> lock(mtx);

    uint32_t i = 0;

    if (handles.size() != 1) {
        properties.multiStorageResource = true;
    }

    auto &drm = this->getDrm(properties.rootDeviceIndex);

    bool areBosSharedObjects = true;
    auto ioctlHelper = drm.getIoctlHelper();

    for (auto handle : handles) {
        PrimeHandle openFd = {0, 0, 0};
        openFd.fileDescriptor = handle;

        auto ret = ioctlHelper->ioctl(DrmIoctl::PrimeFdToHandle, &openFd);

        if (ret != 0) {
            [[maybe_unused]] int err = errno;
            PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(PRIME_FD_TO_HANDLE) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));

            return nullptr;
        }

        auto boHandle = openFd.handle;
        auto bo = findAndReferenceSharedBufferObject(boHandle, properties.rootDeviceIndex);

        if (bo == nullptr) {
            areBosSharedObjects = false;

            size_t size = lseekFunction(handle, 0, SEEK_END);
            totalSize += size;

            auto patIndex = drm.getPatIndex(nullptr, properties.allocationType, CacheRegion::Default, CachePolicy::WriteBack, false);

            bo = new (std::nothrow) BufferObject(&drm, patIndex, boHandle, size, maxOsContextCount);
            bo->setRootDeviceIndex(properties.rootDeviceIndex);
            i++;
        }
        bos.push_back(bo);
        sizes.push_back(bo->peekSize());
    }

    auto heapIndex = HeapIndex::HEAP_STANDARD2MB;
    auto gpuRange = acquireGpuRange(totalSize, properties.rootDeviceIndex, heapIndex);

    lock.unlock();

    AllocationData allocationData;
    properties.size = totalSize;
    getAllocationData(allocationData, properties, nullptr, createStorageInfoFromProperties(properties));

    auto drmAllocation = new DrmAllocation(properties.rootDeviceIndex,
                                           handles.size(),
                                           properties.allocationType,
                                           bos,
                                           nullptr,
                                           gpuRange,
                                           totalSize,
                                           MemoryPool::LocalMemory);
    drmAllocation->storageInfo = allocationData.storageInfo;

    auto gmmHelper = executionEnvironment.rootDeviceEnvironments[properties.rootDeviceIndex]->getGmmHelper();
    for (i = 0u; i < handles.size(); i++) {
        auto bo = bos[i];
        StorageInfo limitedStorageInfo = allocationData.storageInfo;
        limitedStorageInfo.memoryBanks &= (1u << (i % handles.size()));
        auto gmm = new Gmm(gmmHelper,
                           nullptr,
                           bo->peekSize(),
                           0u,
                           CacheSettingsHelper::getGmmUsageType(drmAllocation->getAllocationType(), false, *gmmHelper->getHardwareInfo()),
                           false,
                           allocationData.storageInfo,
                           true);
        drmAllocation->setGmm(gmm, i);

        if (areBosSharedObjects == false) {
            bo->setAddress(gpuRange);
            gpuRange += bo->peekSize();
            bo->setUnmapSize(sizes[i]);
            pushSharedBufferObject(bo);
        }
        drmAllocation->getBufferObjectToModify(i) = bo;
    }

    return drmAllocation;
}

GraphicsAllocation *DrmMemoryManager::createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation) {
    if (isHostIpcAllocation) {
        return createUSMHostAllocationFromSharedHandle(handle, properties, false);
    }

    std::unique_lock<std::mutex> lock(mtx);

    PrimeHandle openFd{};
    openFd.fileDescriptor = handle;

    auto &drm = this->getDrm(properties.rootDeviceIndex);
    auto ioctlHelper = drm.getIoctlHelper();

    auto ret = ioctlHelper->ioctl(DrmIoctl::PrimeFdToHandle, &openFd);

    if (ret != 0) {
        [[maybe_unused]] int err = errno;
        PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(PRIME_FD_TO_HANDLE) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));

        return nullptr;
    }

    auto boHandle = openFd.handle;
    auto bo = findAndReferenceSharedBufferObject(boHandle, properties.rootDeviceIndex);

    if (bo == nullptr) {
        size_t size = lseekFunction(handle, 0, SEEK_END);

        auto patIndex = drm.getPatIndex(nullptr, properties.allocationType, CacheRegion::Default, CachePolicy::WriteBack, false);

        bo = new (std::nothrow) BufferObject(&drm, patIndex, boHandle, size, maxOsContextCount);

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
        bo->setRootDeviceIndex(properties.rootDeviceIndex);

        pushSharedBufferObject(bo);
    }

    lock.unlock();

    auto gmmHelper = getGmmHelper(properties.rootDeviceIndex);
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(reinterpret_cast<void *>(bo->peekAddress())));
    auto drmAllocation = new DrmAllocation(properties.rootDeviceIndex, properties.allocationType, bo, reinterpret_cast<void *>(bo->peekAddress()), bo->peekSize(),
                                           handle, MemoryPool::SystemCpuInaccessible, canonizedGpuAddress);

    if (requireSpecificBitness && this->force32bitAllocations) {
        drmAllocation->set32BitAllocation(true);
        auto gmmHelper = getGmmHelper(properties.rootDeviceIndex);
        drmAllocation->setGpuBaseAddress(gmmHelper->canonize(getExternalHeapBaseAddress(properties.rootDeviceIndex, drmAllocation->isAllocatedInLocalMemoryPool())));
    }

    if (properties.imgInfo) {
        GemGetTiling getTiling{};
        getTiling.handle = boHandle;
        auto ioctlHelper = drm.getIoctlHelper();
        ret = ioctlHelper->ioctl(DrmIoctl::GemGetTiling, &getTiling);

        if (ret == 0) {
            auto ioctlHelper = drm.getIoctlHelper();
            if (getTiling.tilingMode == static_cast<uint32_t>(ioctlHelper->getDrmParamValue(DrmParam::TilingNone))) {
                properties.imgInfo->linearStorage = true;
            }
        }

        Gmm *gmm = new Gmm(executionEnvironment.rootDeviceEnvironments[properties.rootDeviceIndex]->getGmmHelper(), *properties.imgInfo,
                           createStorageInfoFromProperties(properties), properties.flags.preferCompressed);

        drmAllocation->setDefaultGmm(gmm);

        bo->setPatIndex(drm.getPatIndex(gmm, properties.allocationType, CacheRegion::Default, CachePolicy::WriteBack, false));
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
    freeGraphicsMemoryImpl(gfxAllocation, false);
}

void DrmMemoryManager::freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation, bool isImported) {
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
        if (isImported == false) {
            closeSharedHandle(gfxAllocation);
        }
    }

    releaseGpuRange(gfxAllocation->getReservedAddressPtr(), gfxAllocation->getReservedAddressSize(), gfxAllocation->getRootDeviceIndex());
    alignedFreeWrapper(gfxAllocation->getDriverAllocatedCpuPtr());

    drmAlloc->freeRegisteredBOBindExtHandles(&getDrm(drmAlloc->getRootDeviceIndex()));

    delete gfxAllocation;
}

void DrmMemoryManager::handleFenceCompletion(GraphicsAllocation *allocation) {
    auto &drm = this->getDrm(allocation->getRootDeviceIndex());
    if (drm.isVmBindAvailable()) {
        if (drm.completionFenceSupport()) {
            waitOnCompletionFence(allocation);
        } else {
            waitForEnginesCompletion(*allocation);
        }
    } else {
        static_cast<DrmAllocation *>(allocation)->getBO()->wait(-1);
    }
}

GraphicsAllocation *DrmMemoryManager::createGraphicsAllocationFromExistingStorage(AllocationProperties &properties, void *ptr, MultiGraphicsAllocation &multiGraphicsAllocation) {
    auto defaultAlloc = multiGraphicsAllocation.getDefaultGraphicsAllocation();
    if (defaultAlloc && static_cast<DrmAllocation *>(defaultAlloc)->getMmapPtr()) {
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

    uint64_t gpuMemorySize = 0u;

    [[maybe_unused]] auto ret = getDrm(rootDeviceIndex).queryGttSize(gpuMemorySize);
    DEBUG_BREAK_IF(ret != 0);

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
                                        handleStorage.fragmentStorageData[i].fragmentSize, rootDeviceIndex);
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
    DEBUG_BREAK_IF(writeEnable); // unsupported path (for CPU writes call SW_FINISH ioctl in unlockResource)

    auto bo = static_cast<DrmAllocation *>(&graphicsAllocation)->getBO();
    if (bo == nullptr)
        return false;

    auto &drm = this->getDrm(graphicsAllocation.getRootDeviceIndex());
    auto ioctlHelper = drm.getIoctlHelper();
    return ioctlHelper->setDomainCpu(bo->peekHandle(), writeEnable);
}

void *DrmMemoryManager::lockResourceImpl(GraphicsAllocation &graphicsAllocation) {
    auto cpuPtr = graphicsAllocation.getUnderlyingBuffer();
    if (cpuPtr != nullptr) {
        [[maybe_unused]] auto success = setDomainCpu(graphicsAllocation, false);
        DEBUG_BREAK_IF(!success);
        return cpuPtr;
    }

    auto bo = static_cast<DrmAllocation &>(graphicsAllocation).getBO();

    if (graphicsAllocation.getAllocationType() == AllocationType::WRITE_COMBINED) {
        auto addr = lockBufferObject(bo);
        auto alignedAddr = alignUp(addr, MemoryConstants::pageSize64k);
        auto notUsedSize = ptrDiff(alignedAddr, addr);
        // call unmap to free the unaligned pages preceding the BO allocation and
        // adjust the pointer in the CPU mapping to the beginning of the BO allocation
        munmapFunction(addr, notUsedSize);
        bo->setLockedAddress(alignedAddr);
        return bo->peekLockedAddress();
    }

    return lockBufferObject(bo);
}

void DrmMemoryManager::unlockResourceImpl(GraphicsAllocation &graphicsAllocation) {
    return unlockBufferObject(static_cast<DrmAllocation &>(graphicsAllocation).getBO());
}

int DrmMemoryManager::obtainFdFromHandle(int boHandle, uint32_t rootDeviceIndex) {
    auto &drm = this->getDrm(rootDeviceIndex);
    auto ioctlHelper = drm.getIoctlHelper();
    PrimeHandle openFd{};

    openFd.flags = ioctlHelper->getFlagsForPrimeHandleToFd();
    openFd.handle = boHandle;

    ioctlHelper->ioctl(DrmIoctl::PrimeHandleToFd, &openFd);

    return openFd.fileDescriptor;
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

void DrmMemoryManager::makeAllocationResident(GraphicsAllocation *allocation) {
    if (DebugManager.flags.MakeEachAllocationResident.get() == 1) {
        auto drmAllocation = static_cast<DrmAllocation *>(allocation);
        for (uint32_t i = 0; getDrm(allocation->getRootDeviceIndex()).getVirtualMemoryAddressSpace(i) > 0u; i++) {
            drmAllocation->makeBOsResident(registeredEngines[defaultEngineIndex[allocation->getRootDeviceIndex()]].osContext, i, nullptr, true);
            getDrm(allocation->getRootDeviceIndex()).waitForBind(i);
        }
    }
}

void DrmMemoryManager::registerSysMemAlloc(GraphicsAllocation *allocation) {
    makeAllocationResident(allocation);
    std::lock_guard<std::mutex> lock(this->allocMutex);
    this->sysMemAllocs.push_back(allocation);
}

void DrmMemoryManager::registerLocalMemAlloc(GraphicsAllocation *allocation, uint32_t rootDeviceIndex) {
    makeAllocationResident(allocation);
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

bool DrmMemoryManager::copyMemoryToAllocation(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy) {
    if (graphicsAllocation->getUnderlyingBuffer() || !isLocalMemorySupported(graphicsAllocation->getRootDeviceIndex())) {
        return MemoryManager::copyMemoryToAllocation(graphicsAllocation, destinationOffset, memoryToCopy, sizeToCopy);
    }
    return copyMemoryToAllocationBanks(graphicsAllocation, destinationOffset, memoryToCopy, sizeToCopy, maxNBitValue(graphicsAllocation->storageInfo.getNumBanks()));
}
bool DrmMemoryManager::copyMemoryToAllocationBanks(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy, DeviceBitfield handleMask) {
    if (MemoryPoolHelper::isSystemMemoryPool(graphicsAllocation->getMemoryPool())) {
        return false;
    }
    auto drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    for (auto handleId = 0u; handleId < graphicsAllocation->storageInfo.getNumBanks(); handleId++) {
        if (!handleMask.test(handleId)) {
            continue;
        }
        auto ptr = lockBufferObject(drmAllocation->getBOs()[handleId]);
        if (!ptr) {
            return false;
        }
        memcpy_s(ptrOffset(ptr, destinationOffset), graphicsAllocation->getUnderlyingBufferSize() - destinationOffset, memoryToCopy, sizeToCopy);
        this->unlockBufferObject(drmAllocation->getBOs()[handleId]);
    }
    return true;
}

void DrmMemoryManager::unlockBufferObject(BufferObject *bo) {
    if (bo == nullptr)
        return;

    releaseReservedCpuAddressRange(bo->peekLockedAddress(), bo->peekSize(), this->getRootDeviceIndex(bo->peekDrm()));

    [[maybe_unused]] auto ret = munmapFunction(bo->peekLockedAddress(), bo->peekSize());
    DEBUG_BREAK_IF(ret != 0);

    bo->setLockedAddress(nullptr);
}

void createColouredGmms(GmmHelper *gmmHelper, DrmAllocation &allocation, const StorageInfo &storageInfo, bool compression) {
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
        auto gmm = new Gmm(gmmHelper,
                           nullptr,
                           currentSize,
                           0u,
                           CacheSettingsHelper::getGmmUsageType(allocation.getAllocationType(), false, *gmmHelper->getHardwareInfo()),
                           compression,
                           limitedStorageInfo,
                           true);
        allocation.setGmm(gmm, handleId);
    }
}

void fillGmmsInAllocation(GmmHelper *gmmHelper, DrmAllocation *allocation, const StorageInfo &storageInfo) {
    auto alignedSize = alignUp(allocation->getUnderlyingBufferSize(), MemoryConstants::pageSize64k);
    for (auto handleId = 0u; handleId < storageInfo.getNumBanks(); handleId++) {
        StorageInfo limitedStorageInfo = storageInfo;
        limitedStorageInfo.memoryBanks &= 1u << handleId;
        limitedStorageInfo.pageTablesVisibility &= 1u << handleId;
        auto gmm = new Gmm(gmmHelper, nullptr, alignedSize, 0u,
                           CacheSettingsHelper::getGmmUsageType(allocation->getAllocationType(), false, *gmmHelper->getHardwareInfo()), false, limitedStorageInfo, true);
        allocation->setGmm(gmm, handleId);
    }
}

uint64_t getGpuAddress(const AlignmentSelector &alignmentSelector, HeapAssigner &heapAssigner, const HardwareInfo &hwInfo, AllocationType allocType, GfxPartition *gfxPartition,
                       size_t &sizeAllocated, const void *hostPtr, bool resource48Bit, bool useFrontWindow, GmmHelper &gmmHelper) {
    uint64_t gpuAddress = 0;
    switch (allocType) {
    case AllocationType::SVM_GPU:
        gpuAddress = reinterpret_cast<uint64_t>(hostPtr);
        sizeAllocated = 0;
        break;
    case AllocationType::KERNEL_ISA:
    case AllocationType::KERNEL_ISA_INTERNAL:
    case AllocationType::INTERNAL_HEAP:
    case AllocationType::DEBUG_MODULE_AREA: {
        auto heap = heapAssigner.get32BitHeapIndex(allocType, true, hwInfo, useFrontWindow);
        gpuAddress = gmmHelper.canonize(gfxPartition->heapAllocate(heap, sizeAllocated));
    } break;
    case AllocationType::WRITE_COMBINED:
        sizeAllocated = 0;
        break;
    default:
        AlignmentSelector::CandidateAlignment alignment = alignmentSelector.selectAlignment(sizeAllocated);
        if (gfxPartition->getHeapLimit(HeapIndex::HEAP_EXTENDED) > 0 && !resource48Bit) {
            alignment.heap = HeapIndex::HEAP_EXTENDED;
        }
        gpuAddress = gmmHelper.canonize(gfxPartition->heapAllocateWithCustomAlignment(alignment.heap, sizeAllocated, alignment.alignment));
        break;
    }
    return gpuAddress;
}

void DrmMemoryManager::cleanupBeforeReturn(const AllocationData &allocationData, GfxPartition *gfxPartition, DrmAllocation *drmAllocation, GraphicsAllocation *graphicsAllocation, uint64_t &gpuAddress, size_t &sizeAllocated) {
    for (auto bo : drmAllocation->getBOs()) {
        delete bo;
    }
    for (auto handleId = 0u; handleId < allocationData.storageInfo.getNumBanks(); handleId++) {
        delete graphicsAllocation->getGmm(handleId);
    }
    auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
    gfxPartition->freeGpuAddressRange(gmmHelper->decanonize(gpuAddress), sizeAllocated);
}

GraphicsAllocation *DrmMemoryManager::allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) {
    status = AllocationStatus::RetryInNonDevicePool;
    if (!this->localMemorySupported[allocationData.rootDeviceIndex] ||
        allocationData.flags.useSystemMemory ||
        (allocationData.flags.allow32Bit && this->force32bitAllocations) ||
        allocationData.type == AllocationType::SHARED_RESOURCE_COPY) {
        return nullptr;
    }

    if (allocationData.type == AllocationType::UNIFIED_SHARED_MEMORY) {
        auto allocation = this->createSharedUnifiedMemoryAllocation(allocationData);
        status = allocation ? AllocationStatus::Success : AllocationStatus::Error;
        return allocation;
    }

    auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHardwareInfo();

    std::unique_ptr<Gmm> gmm;
    size_t sizeAligned = 0;
    auto numHandles = allocationData.storageInfo.getNumBanks();
    bool createSingleHandle = 1 == numHandles;
    auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);

    if (allocationData.type == AllocationType::IMAGE) {
        allocationData.imgInfo->useLocalMemory = true;
        gmm = std::make_unique<Gmm>(gmmHelper, *allocationData.imgInfo,
                                    allocationData.storageInfo, allocationData.flags.preferCompressed);
        sizeAligned = alignUp(allocationData.imgInfo->size, MemoryConstants::pageSize64k);
    } else {
        if (allocationData.type == AllocationType::WRITE_COMBINED) {
            sizeAligned = alignUp(allocationData.size + MemoryConstants::pageSize64k, 2 * MemoryConstants::megaByte) + 2 * MemoryConstants::megaByte;
        } else {
            sizeAligned = alignUp(allocationData.size, MemoryConstants::pageSize64k);
        }
        if (createSingleHandle) {

            gmm = std::make_unique<Gmm>(gmmHelper,
                                        nullptr,
                                        sizeAligned,
                                        0u,
                                        CacheSettingsHelper::getGmmUsageType(allocationData.type, !!allocationData.flags.uncacheable, *hwInfo),
                                        allocationData.flags.preferCompressed,
                                        allocationData.storageInfo,
                                        true);
        }
    }

    auto sizeAllocated = sizeAligned;
    auto gfxPartition = getGfxPartition(allocationData.rootDeviceIndex);
    auto gpuAddress = getGpuAddress(this->alignmentSelector, this->heapAssigner, *hwInfo,
                                    allocationData.type, gfxPartition, sizeAllocated,
                                    allocationData.hostPtr, allocationData.flags.resource48Bit, allocationData.flags.use32BitFrontWindow, *gmmHelper);
    auto canonizedGpuAddress = gmmHelper->canonize(gpuAddress);
    auto allocation = std::make_unique<DrmAllocation>(allocationData.rootDeviceIndex, numHandles, allocationData.type, nullptr, nullptr, canonizedGpuAddress, sizeAligned, MemoryPool::LocalMemory);
    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(allocation.get());
    GraphicsAllocation *graphicsAllocation = static_cast<GraphicsAllocation *>(allocation.get());

    if (createSingleHandle) {
        allocation->setDefaultGmm(gmm.release());
    } else if (allocationData.storageInfo.multiStorage) {
        createColouredGmms(gmmHelper,
                           *allocation,
                           allocationData.storageInfo,
                           allocationData.flags.preferCompressed);
    } else {
        fillGmmsInAllocation(gmmHelper, allocation.get(), allocationData.storageInfo);
    }
    allocation->storageInfo = allocationData.storageInfo;
    allocation->setFlushL3Required(allocationData.flags.flushL3);
    allocation->setUncacheable(allocationData.flags.uncacheable);
    allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuAddress), sizeAllocated);

    if (!createDrmAllocation(&getDrm(allocationData.rootDeviceIndex), allocation.get(), gpuAddress, maxOsContextCount)) {
        for (auto handleId = 0u; handleId < allocationData.storageInfo.getNumBanks(); handleId++) {
            delete allocation->getGmm(handleId);
        }
        gfxPartition->freeGpuAddressRange(gmmHelper->decanonize(gpuAddress), sizeAllocated);
        status = AllocationStatus::Error;
        return nullptr;
    }
    if (allocationData.type == AllocationType::WRITE_COMBINED) {
        auto cpuAddress = lockResource(allocation.get());
        if (!cpuAddress) {
            cleanupBeforeReturn(allocationData, gfxPartition, drmAllocation, graphicsAllocation, gpuAddress, sizeAllocated);
            status = AllocationStatus::Error;
            return nullptr;
        }
        auto alignedCpuAddress = alignDown(cpuAddress, 2 * MemoryConstants::megaByte);
        auto offset = ptrDiff(cpuAddress, alignedCpuAddress);
        auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
        auto canonizedGpuAddress = gmmHelper->canonize(reinterpret_cast<uint64_t>(alignedCpuAddress));

        allocation->setAllocationOffset(offset);
        allocation->setCpuPtrAndGpuAddress(cpuAddress, canonizedGpuAddress);
        DEBUG_BREAK_IF(allocation->storageInfo.multiStorage);
        allocation->getBO()->setAddress(reinterpret_cast<uint64_t>(cpuAddress));
    }
    if (allocationData.flags.requiresCpuAccess) {
        auto cpuAddress = lockResource(allocation.get());
        if (!cpuAddress) {
            cleanupBeforeReturn(allocationData, gfxPartition, drmAllocation, graphicsAllocation, gpuAddress, sizeAllocated);
            status = AllocationStatus::Error;
            return nullptr;
        }
        auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
        auto canonizedGpuAddress = gmmHelper->canonize(gpuAddress);
        allocation->setCpuPtrAndGpuAddress(cpuAddress, canonizedGpuAddress);
    }
    if (heapAssigner.useInternal32BitHeap(allocationData.type)) {
        allocation->setGpuBaseAddress(gmmHelper->canonize(getInternalHeapBaseAddress(allocationData.rootDeviceIndex, true)));
    }
    if (!allocation->setCacheRegion(&getDrm(allocationData.rootDeviceIndex), static_cast<CacheRegion>(allocationData.cacheRegion))) {
        cleanupBeforeReturn(allocationData, gfxPartition, drmAllocation, graphicsAllocation, gpuAddress, sizeAllocated);
        status = AllocationStatus::Error;
        return nullptr;
    }

    status = AllocationStatus::Success;
    return allocation.release();
}

BufferObject *DrmMemoryManager::createBufferObjectInMemoryRegion(Drm *drm, Gmm *gmm, AllocationType allocationType, uint64_t gpuAddress,
                                                                 size_t size, uint32_t memoryBanks, size_t maxOsContextCount) {
    auto memoryInfo = drm->getMemoryInfo();
    if (!memoryInfo) {
        return nullptr;
    }

    uint32_t handle = 0;
    uint32_t ret = 0;

    auto banks = std::bitset<32>(memoryBanks);
    if (banks.count() > 1) {
        ret = memoryInfo->createGemExtWithMultipleRegions(memoryBanks, size, handle);
    } else {
        ret = memoryInfo->createGemExtWithSingleRegion(memoryBanks, size, handle);
    }

    if (ret != 0) {
        return nullptr;
    }

    auto patIndex = drm->getPatIndex(gmm, allocationType, CacheRegion::Default, CachePolicy::WriteBack, false);

    auto bo = new (std::nothrow) BufferObject(drm, patIndex, handle, size, maxOsContextCount);
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
    auto useKmdMigrationForBuffers = (AllocationType::BUFFER == allocation->getAllocationType() && (DebugManager.flags.UseKmdMigrationForBuffers.get() > 0));

    auto handles = storageInfo.getNumBanks();
    if (storageInfo.colouringPolicy == ColouringPolicy::ChunkSizeBased) {
        handles = allocation->getNumGmms();
        allocation->resizeBufferObjects(handles);
        bos.resize(handles);
    }
    allocation->setNumHandles(handles);

    for (auto handleId = 0u; handleId < handles; handleId++, currentBank++) {
        if (currentBank == banksCnt) {
            currentBank = 0;
            iterationOffset += banksCnt;
        }
        auto memoryBanks = static_cast<uint32_t>(storageInfo.memoryBanks.to_ulong());
        if (!useKmdMigrationForBuffers) {
            if (storageInfo.getNumBanks() > 1) {
                // check if we have this bank, if not move to next one
                // we may have holes in memoryBanks that we need to skip i.e. memoryBanks 1101 and 3 handle allocation
                while (!(memoryBanks & (1u << currentBank))) {
                    currentBank++;
                }
                memoryBanks &= 1u << currentBank;
            }
        }
        auto gmm = allocation->getGmm(handleId);
        auto boSize = alignUp(gmm->gmmResourceInfo->getSizeAllocation(), MemoryConstants::pageSize64k);
        bos[handleId] = createBufferObjectInMemoryRegion(drm, gmm, allocation->getAllocationType(), boAddress, boSize, memoryBanks, maxOsContextCount);
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

bool DrmMemoryManager::retrieveMmapOffsetForBufferObject(uint32_t rootDeviceIndex, BufferObject &bo, uint64_t flags, uint64_t &offset) {
    constexpr uint64_t mmapOffsetFixed = 4;

    GemMmapOffset mmapOffset = {};
    mmapOffset.handle = bo.peekHandle();
    mmapOffset.flags = isLocalMemorySupported(rootDeviceIndex) ? mmapOffsetFixed : flags;
    auto &drm = this->getDrm(rootDeviceIndex);
    auto ioctlHelper = drm.getIoctlHelper();
    auto ret = ioctlHelper->ioctl(DrmIoctl::GemMmapOffset, &mmapOffset);
    if (ret != 0 && isLocalMemorySupported(rootDeviceIndex)) {
        mmapOffset.flags = flags;
        ret = ioctlHelper->ioctl(DrmIoctl::GemMmapOffset, &mmapOffset);
    }
    if (ret != 0) {
        int err = drm.getErrno();
        PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(DRM_IOCTL_I915_GEM_MMAP_OFFSET) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
        DEBUG_BREAK_IF(ret != 0);
        return false;
    }

    offset = mmapOffset.offset;
    return true;
}

bool DrmMemoryManager::allocationTypeForCompletionFence(AllocationType allocationType) {
    int32_t overrideAllowAllAllocations = DebugManager.flags.UseDrmCompletionFenceForAllAllocations.get();
    bool allowAllAllocations = overrideAllowAllAllocations == -1 ? false : !!overrideAllowAllAllocations;
    if (allowAllAllocations) {
        return true;
    }
    if (allocationType == AllocationType::COMMAND_BUFFER ||
        allocationType == AllocationType::RING_BUFFER ||
        allocationType == AllocationType::SEMAPHORE_BUFFER ||
        allocationType == AllocationType::TAG_BUFFER) {
        return true;
    }
    return false;
}

void DrmMemoryManager::waitOnCompletionFence(GraphicsAllocation *allocation) {
    auto allocationType = allocation->getAllocationType();
    if (allocationTypeForCompletionFence(allocationType)) {
        for (auto &engine : getRegisteredEngines()) {
            OsContext *osContext = engine.osContext;
            CommandStreamReceiver *csr = engine.commandStreamReceiver;

            auto osContextId = osContext->getContextId();
            auto allocationTaskCount = csr->getCompletionValue(*allocation);
            uint64_t completionFenceAddress = csr->getCompletionAddress();
            if (completionFenceAddress == 0) {
                continue;
            }

            if (allocation->isUsedByOsContext(osContextId)) {
                Drm &drm = getDrm(csr->getRootDeviceIndex());
                drm.waitOnUserFences(static_cast<const OsContextLinux &>(*osContext), completionFenceAddress, allocationTaskCount, csr->getActivePartitions(), csr->getPostSyncWriteOffset());
            }
        }
    } else {
        waitForEnginesCompletion(*allocation);
    }
}

DrmAllocation *DrmMemoryManager::createAllocWithAlignment(const AllocationData &allocationData, size_t size, size_t alignment, size_t alignedSize, uint64_t gpuAddress) {
    auto &drm = this->getDrm(allocationData.rootDeviceIndex);
    bool useBooMmap = drm.getMemoryInfo() && allocationData.useMmapObject;

    if (DebugManager.flags.EnableBOMmapCreate.get() != -1) {
        useBooMmap = DebugManager.flags.EnableBOMmapCreate.get();
    }

    if (useBooMmap) {
        auto totalSizeToAlloc = alignedSize + alignment;
        auto cpuPointer = this->mmapFunction(0, totalSizeToAlloc, PROT_NONE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

        auto cpuBasePointer = cpuPointer;
        cpuPointer = alignUp(cpuPointer, alignment);

        auto pointerDiff = ptrDiff(cpuPointer, cpuBasePointer);
        std::unique_ptr<BufferObject, BufferObject::Deleter> bo(this->createBufferObjectInMemoryRegion(&drm, nullptr, allocationData.type,
                                                                                                       reinterpret_cast<uintptr_t>(cpuPointer), alignedSize, 0u, maxOsContextCount));

        if (!bo) {
            this->munmapFunction(cpuBasePointer, totalSizeToAlloc);
            return nullptr;
        }

        uint64_t offset = 0;
        auto ioctlHelper = drm.getIoctlHelper();
        uint64_t mmapOffsetWb = ioctlHelper->getDrmParamValue(DrmParam::MmapOffsetWb);
        if (!retrieveMmapOffsetForBufferObject(allocationData.rootDeviceIndex, *bo, mmapOffsetWb, offset)) {
            this->munmapFunction(cpuPointer, size);
            return nullptr;
        }

        [[maybe_unused]] auto retPtr = this->mmapFunction(cpuPointer, alignedSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, drm.getFileDescriptor(), static_cast<off_t>(offset));
        DEBUG_BREAK_IF(retPtr != cpuPointer);

        obtainGpuAddress(allocationData, bo.get(), gpuAddress);
        emitPinningRequest(bo.get(), allocationData);

        auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
        auto canonizedGpuAddress = gmmHelper->canonize(bo->peekAddress());
        auto allocation = std::make_unique<DrmAllocation>(allocationData.rootDeviceIndex, allocationData.type, bo.get(), cpuPointer, canonizedGpuAddress, alignedSize, MemoryPool::System4KBPages);
        allocation->setMmapPtr(cpuPointer);
        allocation->setMmapSize(alignedSize);
        if (pointerDiff != 0) {
            allocation->registerMemoryToUnmap(cpuBasePointer, pointerDiff, this->munmapFunction);
        }
        [[maybe_unused]] int retCode = this->munmapFunction(ptrOffset(cpuPointer, alignedSize), alignment - pointerDiff);
        DEBUG_BREAK_IF(retCode != 0);
        allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuAddress), alignedSize);
        if (!allocation->setCacheRegion(&drm, static_cast<CacheRegion>(allocationData.cacheRegion))) {
            if (pointerDiff == 0) {
                allocation->registerMemoryToUnmap(cpuBasePointer, totalSizeToAlloc, this->munmapFunction);
            }
            return nullptr;
        }

        bo.release();
        allocation->isShareableHostMemory = true;
        return allocation.release();
    } else {
        return createAllocWithAlignmentFromUserptr(allocationData, size, alignment, alignedSize, gpuAddress);
    }
}

void *DrmMemoryManager::lockBufferObject(BufferObject *bo) {
    if (bo == nullptr) {
        return nullptr;
    }

    auto drm = bo->peekDrm();
    auto rootDeviceIndex = this->getRootDeviceIndex(drm);

    auto ioctlHelper = drm->getIoctlHelper();
    uint64_t mmapOffsetWc = ioctlHelper->getDrmParamValue(DrmParam::MmapOffsetWc);
    uint64_t offset = 0;
    if (!retrieveMmapOffsetForBufferObject(rootDeviceIndex, *bo, mmapOffsetWc, offset)) {
        return nullptr;
    }

    auto addr = mmapFunction(nullptr, bo->peekSize(), PROT_WRITE | PROT_READ, MAP_SHARED, drm->getFileDescriptor(), static_cast<off_t>(offset));
    DEBUG_BREAK_IF(addr == MAP_FAILED);

    if (addr == MAP_FAILED) {
        PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "mmap return of MAP_FAILED\n");
        return nullptr;
    }

    bo->setLockedAddress(addr);

    return bo->peekLockedAddress();
}

void createMemoryRegionsForSharedAllocation(const HardwareInfo &hwInfo, MemoryInfo &memoryInfo, const AllocationData &allocationData, MemRegionsVec &memRegions) {
    auto memoryBanks = allocationData.storageInfo.memoryBanks;

    if (allocationData.usmInitialPlacement == GraphicsAllocation::UsmInitialPlacement::CPU) {
        //System memory region
        auto regionClassAndInstance = memoryInfo.getMemoryRegionClassAndInstance(0u, hwInfo);
        memRegions.push_back(regionClassAndInstance);
    }

    //All local memory regions
    size_t currentBank = 0;
    size_t i = 0;

    while (i < memoryBanks.count()) {
        if (memoryBanks.test(currentBank)) {
            auto regionClassAndInstance = memoryInfo.getMemoryRegionClassAndInstance(1u << currentBank, hwInfo);
            memRegions.push_back(regionClassAndInstance);
            i++;
        }
        currentBank++;
    }

    if (allocationData.usmInitialPlacement == GraphicsAllocation::UsmInitialPlacement::GPU) {
        //System memory region
        auto regionClassAndInstance = memoryInfo.getMemoryRegionClassAndInstance(0u, hwInfo);
        memRegions.push_back(regionClassAndInstance);
    }
}

GraphicsAllocation *DrmMemoryManager::createSharedUnifiedMemoryAllocation(const AllocationData &allocationData) {
    auto &drm = this->getDrm(allocationData.rootDeviceIndex);
    auto ioctlHelper = drm.getIoctlHelper();

    const auto vmAdviseAttribute = ioctlHelper->getVmAdviseAtomicAttribute();
    if (vmAdviseAttribute == 0) {
        return nullptr;
    }

    auto memoryInfo = drm.getMemoryInfo();
    const bool useBooMmap = memoryInfo && allocationData.useMmapObject;

    if (!useBooMmap) {
        return nullptr;
    }

    auto size = allocationData.size;
    auto alignment = allocationData.alignment;

    auto pHwInfo = drm.getRootDeviceEnvironment().getHardwareInfo();

    MemRegionsVec memRegions;
    createMemoryRegionsForSharedAllocation(*pHwInfo, *memoryInfo, allocationData, memRegions);

    uint32_t handle = 0;
    auto ret = memoryInfo->createGemExt(memRegions, size, handle, {});

    if (ret) {
        return nullptr;
    }

    auto patIndex = drm.getPatIndex(nullptr, allocationData.type, CacheRegion::Default, CachePolicy::WriteBack, false);

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(new BufferObject(&drm, patIndex, handle, size, maxOsContextCount));

    if (!ioctlHelper->setVmBoAdvise(bo->peekHandle(), vmAdviseAttribute, nullptr)) {
        return nullptr;
    }

    uint64_t mmapOffsetWb = ioctlHelper->getDrmParamValue(DrmParam::MmapOffsetWb);
    uint64_t offset = 0;
    if (!retrieveMmapOffsetForBufferObject(allocationData.rootDeviceIndex, *bo, mmapOffsetWb, offset)) {
        return nullptr;
    }

    auto totalSizeToAlloc = size + alignment;
    auto cpuPointer = this->mmapFunction(0, totalSizeToAlloc, PROT_NONE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if (cpuPointer == MAP_FAILED) {
        PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "mmap return of MAP_FAILED\n");
        return nullptr;
    }

    auto cpuBasePointer = cpuPointer;
    cpuPointer = alignUp(cpuPointer, alignment);

    this->mmapFunction(cpuPointer, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, drm.getFileDescriptor(), static_cast<off_t>(offset));

    bo->setAddress(reinterpret_cast<uintptr_t>(cpuPointer));

    auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
    auto canonizedGpuAddress = gmmHelper->canonize(bo->peekAddress());
    auto allocation = std::make_unique<DrmAllocation>(allocationData.rootDeviceIndex, allocationData.type, bo.get(), cpuPointer, canonizedGpuAddress, size, MemoryPool::System4KBPages);
    allocation->setMmapPtr(cpuBasePointer);
    allocation->setMmapSize(totalSizeToAlloc);
    if (!allocation->setCacheRegion(&drm, static_cast<CacheRegion>(allocationData.cacheRegion))) {
        this->munmapFunction(cpuPointer, totalSizeToAlloc);
        return nullptr;
    }

    bo.release();

    return allocation.release();
}

DrmAllocation *DrmMemoryManager::createUSMHostAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool hasMappedPtr) {
    PrimeHandle openFd{};
    openFd.fileDescriptor = handle;

    auto &drm = this->getDrm(properties.rootDeviceIndex);
    auto patIndex = drm.getPatIndex(nullptr, properties.allocationType, CacheRegion::Default, CachePolicy::WriteBack, false);
    auto ioctlHelper = drm.getIoctlHelper();

    auto ret = ioctlHelper->ioctl(DrmIoctl::PrimeFdToHandle, &openFd);
    if (ret != 0) {
        int err = drm.getErrno();
        PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(PRIME_FD_TO_HANDLE) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
        DEBUG_BREAK_IF(ret != 0);
        return nullptr;
    }

    if (hasMappedPtr) {
        auto bo = new BufferObject(&drm, patIndex, openFd.handle, properties.size, maxOsContextCount);
        bo->setAddress(properties.gpuAddress);

        auto gmmHelper = getGmmHelper(properties.rootDeviceIndex);
        auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(reinterpret_cast<void *>(bo->peekAddress())));
        return new DrmAllocation(properties.rootDeviceIndex, properties.allocationType, bo, reinterpret_cast<void *>(bo->peekAddress()), bo->peekSize(),
                                 handle, MemoryPool::SystemCpuInaccessible, canonizedGpuAddress);
    }

    const bool useBooMmap = drm.getMemoryInfo() && properties.useMmapObject;
    if (!useBooMmap) {
        auto bo = new BufferObject(&drm, patIndex, openFd.handle, properties.size, maxOsContextCount);
        bo->setAddress(properties.gpuAddress);

        auto gmmHelper = getGmmHelper(properties.rootDeviceIndex);
        auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(reinterpret_cast<void *>(bo->peekAddress())));
        return new DrmAllocation(properties.rootDeviceIndex, properties.allocationType, bo, reinterpret_cast<void *>(bo->peekAddress()), bo->peekSize(),
                                 handle, MemoryPool::SystemCpuInaccessible, canonizedGpuAddress);
    }

    auto boHandle = openFd.handle;
    auto bo = findAndReferenceSharedBufferObject(boHandle, properties.rootDeviceIndex);

    if (bo == nullptr) {
        void *cpuPointer = nullptr;
        size_t size = lseekFunction(handle, 0, SEEK_END);

        bo = new BufferObject(&drm, patIndex, boHandle, size, maxOsContextCount);
        cpuPointer = this->mmapFunction(0, size, PROT_NONE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

        if (cpuPointer == MAP_FAILED) {
            PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "mmap return of MAP_FAILED\n");
            delete bo;
            return nullptr;
        }

        bo->setAddress(reinterpret_cast<uintptr_t>(cpuPointer));

        uint64_t mmapOffsetWb = ioctlHelper->getDrmParamValue(DrmParam::MmapOffsetWb);
        uint64_t offset = 0;
        if (!retrieveMmapOffsetForBufferObject(properties.rootDeviceIndex, *bo, mmapOffsetWb, offset)) {
            this->munmapFunction(cpuPointer, size);
            delete bo;
            return nullptr;
        }

        [[maybe_unused]] auto retPtr = this->mmapFunction(cpuPointer, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, drm.getFileDescriptor(), static_cast<off_t>(offset));
        DEBUG_BREAK_IF(retPtr != cpuPointer);

        AllocationData allocationData = {};
        allocationData.rootDeviceIndex = properties.rootDeviceIndex;
        allocationData.size = size;
        emitPinningRequest(bo, allocationData);

        bo->setUnmapSize(size);
        bo->setRootDeviceIndex(properties.rootDeviceIndex);

        pushSharedBufferObject(bo);

        DrmAllocation *drmAllocation = nullptr;
        drmAllocation = new DrmAllocation(properties.rootDeviceIndex, properties.allocationType, bo, cpuPointer, bo->peekAddress(), bo->peekSize(), MemoryPool::System4KBPages);
        drmAllocation->setMmapPtr(cpuPointer);
        drmAllocation->setMmapSize(size);
        drmAllocation->setReservedAddressRange(reinterpret_cast<void *>(cpuPointer), size);
        drmAllocation->setCacheRegion(&drm, static_cast<CacheRegion>(properties.cacheRegion));

        return drmAllocation;
    }

    auto gmmHelper = getGmmHelper(properties.rootDeviceIndex);
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(reinterpret_cast<void *>(bo->peekAddress())));
    return new DrmAllocation(properties.rootDeviceIndex, properties.allocationType, bo, reinterpret_cast<void *>(bo->peekAddress()), bo->peekSize(),
                             handle, MemoryPool::SystemCpuInaccessible, canonizedGpuAddress);
}
bool DrmMemoryManager::allowIndirectAllocationsAsPack(uint32_t rootDeviceIndex) {
    return this->getDrm(rootDeviceIndex).isVmBindAvailable();
}

} // namespace NEO
