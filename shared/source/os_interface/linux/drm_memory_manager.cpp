/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_memory_manager.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/heap_assigner.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/memory_manager/host_ptr_manager.h"
#include "shared/source/memory_manager/local_memory_usage.h"
#include "shared/source/memory_manager/memory_pool.h"
#include "shared/source/memory_manager/multi_graphics_allocation.h"
#include "shared/source/memory_manager/residency.h"
#include "shared/source/os_interface/linux/allocator_helper.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_gem_close_worker.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/linux/i915_prelim.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helper/release_helper.h"

#include <cstring>
#include <memory>
#include <sys/ioctl.h>
namespace NEO {

using AllocationStatus = MemoryManager::AllocationStatus;

int debugMunmap(void *ptr, size_t size) noexcept {
    errno = 0;

    int returnVal = munmap(ptr, size);

    printf("\nmunmap(%p, %zu) = %d, errno: %d \n", ptr, size, returnVal, errno);

    return returnVal;
}

void *debugMmap(void *ptr, size_t size, int prot, int flags, int fd, off_t offset) noexcept {
    errno = 0;

    void *returnVal = mmap(ptr, size, prot, flags, fd, offset);

    printf("\nmmap(%p, %zu, %d, %d, %d, %ld) = %p, errno: %d \n", ptr, size, prot, flags, fd, offset, returnVal, errno);
    return returnVal;
}

DrmMemoryManager::DrmMemoryManager(GemCloseWorkerMode mode,
                                   bool forcePinAllowed,
                                   bool validateHostPtrMemory,
                                   ExecutionEnvironment &executionEnvironment) : MemoryManager(executionEnvironment),
                                                                                 forcePinEnabled(forcePinAllowed),
                                                                                 validateHostPtrMemory(validateHostPtrMemory) {

    if (debugManager.flags.PrintMmapAndMunMapCalls.get() == 1) {
        this->munmapFunction = debugMunmap;
        this->mmapFunction = debugMmap;
    }

    alignmentSelector.addCandidateAlignment(MemoryConstants::pageSize64k, true, AlignmentSelector::anyWastage, HeapIndex::heapStandard64KB);
    if (debugManager.flags.AlignLocalMemoryVaTo2MB.get() != 0) {
        alignmentSelector.addCandidateAlignment(MemoryConstants::pageSize2M, false, AlignmentSelector::anyWastage, HeapIndex::heapStandard2MB);
    }
    const size_t customAlignment = static_cast<size_t>(debugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.get());
    if (customAlignment > 0) {
        const auto heapIndex = customAlignment >= MemoryConstants::pageSize2M ? HeapIndex::heapStandard2MB : HeapIndex::heapStandard64KB;
        alignmentSelector.addCandidateAlignment(customAlignment, true, AlignmentSelector::anyWastage, heapIndex);
    }
    osMemory = OSMemory::create();

    initialize(mode);
}

void DrmMemoryManager::initialize(GemCloseWorkerMode mode) {
    bool disableGemCloseWorker = true;

    localMemBanksCount.resize(localMemorySupported.size());
    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < gfxPartitions.size(); ++rootDeviceIndex) {
        auto gpuAddressSpace = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo()->capabilityTable.gpuAddressSpace;
        uint64_t gfxTop{};
        getDrm(rootDeviceIndex).queryGttSize(gfxTop, false);
        if (!getGfxPartition(rootDeviceIndex)->init(gpuAddressSpace, getSizeToReserve(), rootDeviceIndex, gfxPartitions.size(), heapAssigners[rootDeviceIndex]->apiAllowExternalHeapForSshAndDsh, DrmMemoryManager::getSystemSharedMemory(rootDeviceIndex), gfxTop)) {
            initialized = false;
            return;
        }
        localMemAllocs.emplace_back();
        setLocalMemBanksCount(rootDeviceIndex);
        disableGemCloseWorker &= getDrm(rootDeviceIndex).isVmBindAvailable();
        isLocalMemoryUsedForIsa(rootDeviceIndex);
    }

    if (disableGemCloseWorker) {
        mode = GemCloseWorkerMode::gemCloseWorkerInactive;
    }

    if (debugManager.flags.EnableGemCloseWorker.get() != -1) {
        mode = debugManager.flags.EnableGemCloseWorker.get() ? GemCloseWorkerMode::gemCloseWorkerActive : GemCloseWorkerMode::gemCloseWorkerInactive;
    }

    if (mode != GemCloseWorkerMode::gemCloseWorkerInactive && DrmMemoryManager::isGemCloseWorkerSupported()) {
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

void DrmMemoryManager::setLocalMemBanksCount(uint32_t rootDeviceIndex) {
    const auto *memoryInfo = getDrm(rootDeviceIndex).getMemoryInfo();
    if (localMemorySupported[rootDeviceIndex]) {
        localMemBanksCount[rootDeviceIndex] = (memoryInfo ? memoryInfo->getLocalMemoryRegions().size() : 1u);
    }
};

BufferObject *DrmMemoryManager::createRootDeviceBufferObject(uint32_t rootDeviceIndex) {
    BufferObject *bo = nullptr;
    if (forcePinEnabled || validateHostPtrMemory) {
        bo = allocUserptr(reinterpret_cast<uintptr_t>(memoryForPinBBs[rootDeviceIndex]), MemoryConstants::pageSize, AllocationType::externalHostPtr, rootDeviceIndex);
        if (bo) {
            if (isLimitedRange(rootDeviceIndex)) {
                auto boSize = bo->peekSize();
                bo->setAddress(DrmMemoryManager::acquireGpuRange(boSize, rootDeviceIndex, HeapIndex::heapStandard));
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
    if (bo->peekIsReusableAllocation() || bo->isBoHandleShared()) {
        lock.lock();
    }

    uint32_t r = bo->unreference();

    if (r == 1) {
        if (bo->peekIsReusableAllocation()) {
            eraseSharedBufferObject(bo);
        }
        auto rootDeviceIndex = bo->getRootDeviceIndex();
        int boHandle = bo->getHandle();
        bo->close();

        if (bo->isBoHandleShared() && bo->getHandle() != boHandle) {
            // Shared BO was closed - handle was invalidated. Remove weak reference from container.
            eraseSharedBoHandleWrapper(boHandle, rootDeviceIndex);
        }

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

uint64_t DrmMemoryManager::acquireGpuRangeWithCustomAlignment(size_t &size, uint32_t rootDeviceIndex, HeapIndex heapIndex, size_t alignment) {
    auto gfxPartition = getGfxPartition(rootDeviceIndex);
    auto gmmHelper = getGmmHelper(rootDeviceIndex);
    return gmmHelper->canonize(gfxPartition->heapAllocateWithCustomAlignment(heapIndex, size, alignment));
}

void DrmMemoryManager::releaseGpuRange(void *address, size_t unmapSize, uint32_t rootDeviceIndex) {
    uint64_t graphicsAddress = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(address));
    auto gmmHelper = getGmmHelper(rootDeviceIndex);
    graphicsAddress = gmmHelper->decanonize(graphicsAddress);
    auto gfxPartition = getGfxPartition(rootDeviceIndex);
    gfxPartition->freeGpuAddressRange(graphicsAddress, unmapSize);
}

bool DrmMemoryManager::hasPageFaultsEnabled(const Device &neoDevice) {
    auto *drm = neoDevice.getRootDeviceEnvironment().osInterface->getDriverModel()->as<Drm>();
    return drm->hasPageFaultSupport();
}

bool DrmMemoryManager::isKmdMigrationAvailable(uint32_t rootDeviceIndex) {
    auto &drm = this->getDrm(rootDeviceIndex);
    return drm.hasKmdMigrationSupport();
}

bool DrmMemoryManager::setMemAdvise(GraphicsAllocation *gfxAllocation, MemAdviseFlags flags, uint32_t rootDeviceIndex) {
    auto drmAllocation = static_cast<DrmAllocation *>(gfxAllocation);

    return drmAllocation->setMemAdvise(&this->getDrm(rootDeviceIndex), flags);
}

bool DrmMemoryManager::setSharedSystemMemAdvise(const void *ptr, const size_t size, MemAdvise memAdviseOp, SubDeviceIdsVec &subDeviceIds, uint32_t rootDeviceIndex) {

    auto &drm = this->getDrm(rootDeviceIndex);
    auto ioctlHelper = drm.getIoctlHelper();

    uint32_t attribute = ioctlHelper->getPreferredLocationAdvise();
    uint64_t param = ioctlHelper->getPreferredLocationArgs(memAdviseOp);

    // Apply the shared system USM IOCTL to all the VMs of the device
    std::vector<uint32_t> vmIds;
    vmIds.reserve(subDeviceIds.size());
    for (auto subDeviceId : subDeviceIds) {
        vmIds.push_back(drm.getVirtualMemoryAddressSpace(subDeviceId));
    }

    auto result = ioctlHelper->setVmSharedSystemMemAdvise(reinterpret_cast<uint64_t>(ptr), size, attribute, param, vmIds);

    return result;
}

bool DrmMemoryManager::setSharedSystemAtomicAccess(const void *ptr, const size_t size, AtomicAccessMode mode, SubDeviceIdsVec &subDeviceIds, uint32_t rootDeviceIndex) {
    auto &drm = this->getDrm(rootDeviceIndex);
    auto ioctlHelper = drm.getIoctlHelper();

    uint32_t attribute = ioctlHelper->getAtomicAdvise(false);
    uint64_t param = static_cast<uint64_t>(ioctlHelper->getAtomicAccess(mode));

    // Apply the shared system USM IOCTL to all the VMs of the device
    std::vector<uint32_t> vmIds;
    vmIds.reserve(subDeviceIds.size());
    for (auto subDeviceId : subDeviceIds) {
        vmIds.push_back(drm.getVirtualMemoryAddressSpace(subDeviceId));
    }

    auto result = ioctlHelper->setVmSharedSystemMemAdvise(reinterpret_cast<uint64_t>(ptr), size, attribute, param, vmIds);

    return result;
}

AtomicAccessMode DrmMemoryManager::getSharedSystemAtomicAccess(const void *ptr, const size_t size, SubDeviceIdsVec &subDeviceIds, uint32_t rootDeviceIndex) {

    auto &drm = this->getDrm(rootDeviceIndex);
    auto ioctlHelper = drm.getIoctlHelper();

    // Only get the atomic attributes from a single VM since they are replicated in all.
    uint32_t vmId = drm.getVirtualMemoryAddressSpace(subDeviceIds[0]);
    auto result = ioctlHelper->getVmSharedSystemAtomicAttribute(reinterpret_cast<uint64_t>(ptr), size, vmId);

    return result;
}

bool DrmMemoryManager::setAtomicAccess(GraphicsAllocation *gfxAllocation, size_t size, AtomicAccessMode mode, uint32_t rootDeviceIndex) {
    auto drmAllocation = static_cast<DrmAllocation *>(gfxAllocation);

    return drmAllocation->setAtomicAccess(&this->getDrm(rootDeviceIndex), size, mode);
}

bool DrmMemoryManager::prefetchSharedSystemAlloc(const void *ptr, const size_t size, SubDeviceIdsVec &subDeviceIds, uint32_t rootDeviceIndex) {

    auto &drm = this->getDrm(rootDeviceIndex);
    auto ioctlHelper = drm.getIoctlHelper();
    auto memoryClassDevice = ioctlHelper->getDrmParamValue(DrmParam::memoryClassDevice);
    auto region = static_cast<uint32_t>((memoryClassDevice << 16u) | subDeviceIds[0]);
    auto vmId = drm.getVirtualMemoryAddressSpace(subDeviceIds[0]);
    return ioctlHelper->setVmPrefetch(reinterpret_cast<uint64_t>(ptr), size, region, vmId);
}

bool DrmMemoryManager::setMemPrefetch(GraphicsAllocation *gfxAllocation, SubDeviceIdsVec &subDeviceIds, uint32_t rootDeviceIndex) {
    auto drmAllocation = static_cast<DrmAllocation *>(gfxAllocation);
    auto osContextLinux = getDefaultOsContext(rootDeviceIndex);

    for (auto subDeviceId : subDeviceIds) {
        auto vmHandleId = subDeviceId;
        auto retVal = drmAllocation->bindBOs(osContextLinux, vmHandleId, nullptr, true, false);
        if (retVal != 0) {
            DEBUG_BREAK_IF(true);
            return false;
        }
    }

    return drmAllocation->setMemPrefetch(&this->getDrm(rootDeviceIndex), subDeviceIds);
}

NEO::BufferObject *DrmMemoryManager::allocUserptr(uintptr_t address, size_t size, const AllocationType allocationType, uint32_t rootDeviceIndex) {
    GemUserPtr userptr = {};
    userptr.userPtr = address;
    userptr.userSize = size;

    auto &drm = this->getDrm(rootDeviceIndex);
    auto ioctlHelper = drm.getIoctlHelper();

    if (ioctlHelper->ioctl(DrmIoctl::gemUserptr, &userptr) != 0) {
        return nullptr;
    }

    PRINT_DEBUG_STRING(debugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "Created new BO with GEM_USERPTR, handle: BO-%d\n", userptr.handle);

    auto patIndex = drm.getPatIndex(nullptr, allocationType, CacheRegion::defaultRegion, CachePolicy::writeBack, false, true);

    auto res = new (std::nothrow) BufferObject(rootDeviceIndex, &drm, patIndex, userptr.handle, size, maxOsContextCount);
    if (!res) {
        DEBUG_BREAK_IF(true);
        return nullptr;
    }

    res->setAddress(address);
    res->setUserptr(address);
    return res;
}

void DrmMemoryManager::emitPinningRequest(BufferObject *bo, const AllocationData &allocationData) const {
    auto rootDeviceIndex = allocationData.rootDeviceIndex;
    if (forcePinEnabled && pinBBs.at(rootDeviceIndex) != nullptr && allocationData.flags.forcePin && allocationData.size >= this->pinThreshold) {
        emitPinningRequestForBoContainer(&bo, 1, rootDeviceIndex);
    }
}
SubmissionStatus DrmMemoryManager::emitPinningRequestForBoContainer(BufferObject **bo, uint32_t boCount, uint32_t rootDeviceIndex) const {
    auto ret = pinBBs.at(rootDeviceIndex)->pin(bo, boCount, getDefaultOsContext(rootDeviceIndex), 0, getDefaultDrmContextId(rootDeviceIndex));
    return ret == 0 ? SubmissionStatus::success : SubmissionStatus::outOfMemory;
}

StorageInfo DrmMemoryManager::createStorageInfoFromProperties(const AllocationProperties &properties) {

    auto storageInfo{MemoryManager::createStorageInfoFromProperties(properties)};
    auto *memoryInfo = getDrm(properties.rootDeviceIndex).getMemoryInfo();

    if (memoryInfo == nullptr || localMemorySupported[properties.rootDeviceIndex] == false) {
        return storageInfo;
    }

    const auto &localMemoryRegions{memoryInfo->getLocalMemoryRegions()};
    DEBUG_BREAK_IF(localMemoryRegions.empty());

    DeviceBitfield allMemoryBanks{0b0};
    if (storageInfo.tileInstanced) {
        const auto deviceCount = GfxCoreHelper::getSubDevicesCount(executionEnvironment.rootDeviceEnvironments[properties.rootDeviceIndex]->getHardwareInfo());
        const auto subDevicesMask = executionEnvironment.rootDeviceEnvironments[properties.rootDeviceIndex]->deviceAffinityMask.getGenericSubDevicesMask().to_ulong();
        const DeviceBitfield allTilesValue(properties.subDevicesBitfield.count() == 1 ? maxNBitValue(deviceCount) & subDevicesMask : properties.subDevicesBitfield);

        allMemoryBanks = allTilesValue;
    } else {
        for (auto i = 0u; i < localMemoryRegions.size(); ++i) {
            if ((properties.subDevicesBitfield & localMemoryRegions[i].tilesMask).any()) {
                allMemoryBanks.set(i);
            }
        }
    }
    if (allMemoryBanks.none()) {
        return storageInfo;
    }

    DeviceBitfield preferredMemoryBanks{storageInfo.memoryBanks};
    if (localMemoryRegions.size() == 1u) {
        preferredMemoryBanks = allMemoryBanks;
    }
    storageInfo.memoryBanks = computeStorageInfoMemoryBanks(properties, preferredMemoryBanks, allMemoryBanks);

    return storageInfo;
}

GraphicsAllocation *DrmMemoryManager::createGraphicsAllocation(OsHandleStorage &handleStorage, const AllocationData &allocationData) {
    auto hostPtr = const_cast<void *>(allocationData.hostPtr);
    auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(hostPtr));
    auto allocation = std::make_unique<DrmAllocation>(allocationData.rootDeviceIndex, 1u /*num gmms*/, allocationData.type, nullptr, hostPtr, canonizedGpuAddress, allocationData.size, MemoryPool::system4KBPages);
    allocation->fragmentsStorage = handleStorage;
    if (!allocation->setCacheRegion(&this->getDrm(allocationData.rootDeviceIndex), static_cast<CacheRegion>(allocationData.cacheRegion))) {
        return nullptr;
    }
    return allocation.release();
}

GraphicsAllocation *DrmMemoryManager::allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) {
    if (GraphicsAllocation::isDebugSurfaceAllocationType(allocationData.type) &&
        allocationData.storageInfo.subDeviceBitfield.count() > 1) {
        return createMultiHostDebugSurfaceAllocation(allocationData);
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
    size_t alignedVirtualAddressRangeSize = cSize;
    auto svmCpuAllocation = allocationData.type == AllocationType::svmCpu;
    auto is2MBSizeAlignmentRequired = getDrm(allocationData.rootDeviceIndex).getIoctlHelper()->is2MBSizeAlignmentRequired(allocationData.type);
    if (svmCpuAllocation || is2MBSizeAlignmentRequired) {
        // add padding in case reserved addr is not aligned

        auto &productHelper = getGmmHelper(allocationData.rootDeviceIndex)->getRootDeviceEnvironment().getHelper<ProductHelper>();
        if (alignedStorageSize >= 2 * MemoryConstants::megaByte &&
            (is2MBSizeAlignmentRequired || productHelper.is2MBLocalMemAlignmentEnabled()) &&
            cAlignment <= 2 * MemoryConstants::megaByte) {
            alignedStorageSize = alignUp(cSize, MemoryConstants::pageSize2M);
        } else {
            alignedStorageSize = alignUp(cSize, cAlignment);
        }

        alignedVirtualAddressRangeSize = alignedStorageSize + cAlignment;
    }

    // if limitedRangeAlloction is enabled, memory allocation for bo in the limited Range heap is required
    if ((isLimitedRange(allocationData.rootDeviceIndex) || svmCpuAllocation) && !allocationData.flags.isUSMHostAllocation) {
        gpuReservationAddress = acquireGpuRange(alignedVirtualAddressRangeSize, allocationData.rootDeviceIndex, HeapIndex::heapStandard);
        if (!gpuReservationAddress) {
            return nullptr;
        }

        alignedGpuAddress = gpuReservationAddress;
        if (svmCpuAllocation) {
            alignedGpuAddress = alignUp(gpuReservationAddress, cAlignment);
        }
    }

    auto mmapAlignment = cAlignment;
    if (alignedStorageSize >= 2 * MemoryConstants::megaByte && mmapAlignment <= 2 * MemoryConstants::megaByte) {
        mmapAlignment = MemoryConstants::pageSize2M;
    }

    auto drmAllocation = createAllocWithAlignment(allocationData, cSize, mmapAlignment, alignedStorageSize, alignedGpuAddress);
    if (drmAllocation != nullptr && gpuReservationAddress) {
        drmAllocation->setReservedAddressRange(reinterpret_cast<void *>(gpuReservationAddress), alignedVirtualAddressRangeSize);
    }

    return drmAllocation;
}

DrmAllocation *DrmMemoryManager::createAllocWithAlignmentFromUserptr(const AllocationData &allocationData, size_t size, size_t alignment, size_t alignedSVMSize, uint64_t gpuAddress) {
    auto res = alignedMallocWrapper(size, alignment);
    if (!res) {
        return nullptr;
    }

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(allocUserptr(reinterpret_cast<uintptr_t>(res), size, allocationData.type, allocationData.rootDeviceIndex));
    if (!bo) {
        alignedFreeWrapper(res);
        return nullptr;
    }

    zeroCpuMemoryIfRequested(allocationData, res, size);
    obtainGpuAddress(allocationData, bo.get(), gpuAddress);
    emitPinningRequest(bo.get(), allocationData);

    auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
    auto canonizedGpuAddress = gmmHelper->canonize(bo->peekAddress());
    auto allocation = std::make_unique<DrmAllocation>(allocationData.rootDeviceIndex, 1u /*num gmms*/, allocationData.type, bo.get(), res, canonizedGpuAddress, size, MemoryPool::system4KBPages);
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
    if ((isLimitedRange(allocationData.rootDeviceIndex) || allocationData.type == AllocationType::svmCpu) &&
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
                                                                         allocationData.type,
                                                                         allocationData.rootDeviceIndex));
    if (!bo) {
        return nullptr;
    }

    // if limitedRangeAlloction is enabled, memory allocation for bo in the limited Range heap is required
    uint64_t gpuAddress = 0;
    auto svmCpuAllocation = allocationData.type == AllocationType::svmCpu;
    if (isLimitedRange(allocationData.rootDeviceIndex) || svmCpuAllocation) {
        gpuAddress = acquireGpuRange(cSize, allocationData.rootDeviceIndex, HeapIndex::heapStandard);
        if (!gpuAddress) {
            return nullptr;
        }
        bo->setAddress(gpuAddress);
    }

    emitPinningRequest(bo.get(), allocationData);

    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex,
                                        1u /*num gmms*/,
                                        allocationData.type,
                                        bo.get(),
                                        bufferPtr,
                                        bo->peekAddress(),
                                        cSize,
                                        MemoryPool::system4KBPages);
    if (debugManager.flags.EnableHostAllocationMemPolicy.get()) {
        allocation->setUsmHostAllocation(true);
    }
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

    if (allocationData.type == NEO::AllocationType::debugSbaTrackingBuffer &&
        allocationData.storageInfo.subDeviceBitfield.count() > 1) {
        return createMultiHostDebugSurfaceAllocation(allocationData);
    }

    auto osContextLinux = static_cast<OsContextLinux *>(allocationData.osContext);

    const size_t minAlignment = getUserptrAlignment();
    size_t alignedSize = alignUp(allocationData.size, minAlignment);

    auto res = alignedMallocWrapper(alignedSize, minAlignment);
    if (!res)
        return nullptr;

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(allocUserptr(reinterpret_cast<uintptr_t>(res), alignedSize, allocationData.type, allocationData.rootDeviceIndex));

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

    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, 1u /*num gmms*/, allocationData.type, bo.get(), res, bo->peekAddress(), alignedSize, MemoryPool::system4KBPages);
    allocation->storageInfo = allocationData.storageInfo;
    allocation->setDriverAllocatedCpuPtr(res);
    allocation->setOsContext(osContextLinux);

    bo.release();

    return allocation;
}

GraphicsAllocation *DrmMemoryManager::allocateGraphicsMemoryForNonSvmHostPtr(const AllocationData &allocationData) {
    if (allocationData.size == 0 || !allocationData.hostPtr)
        return nullptr;
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex].get();
    auto &productHelper = rootDeviceEnvironment->getHelper<ProductHelper>();
    auto alignedPtr = alignDown(allocationData.hostPtr, MemoryConstants::pageSize);
    auto alignedSize = alignSizeWholePage(allocationData.hostPtr, allocationData.size);
    auto realAllocationSize = alignedSize;
    auto offsetInPage = ptrDiff(allocationData.hostPtr, alignedPtr);
    auto rootDeviceIndex = allocationData.rootDeviceIndex;
    uint64_t gpuVirtualAddress = 0;
    if (this->isLimitedRange(allocationData.rootDeviceIndex)) {
        gpuVirtualAddress = acquireGpuRange(alignedSize, rootDeviceIndex, HeapIndex::heapStandard);
    } else {
        alignedSize = alignUp(alignedSize, MemoryConstants::pageSize2M);
        gpuVirtualAddress = acquireGpuRangeWithCustomAlignment(alignedSize, rootDeviceIndex, HeapIndex::heapStandard, MemoryConstants::pageSize2M);
    }

    if (!gpuVirtualAddress) {
        return nullptr;
    }
    auto ioctlHelper = getDrm(rootDeviceIndex).getIoctlHelper();
    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(ioctlHelper->allocUserptr(*this, allocationData, reinterpret_cast<uintptr_t>(alignedPtr), realAllocationSize, rootDeviceIndex));
    if (!bo) {
        releaseGpuRange(reinterpret_cast<void *>(gpuVirtualAddress), alignedSize, rootDeviceIndex);
        return nullptr;
    }

    bo->setAddress(gpuVirtualAddress);

    auto usageType = CacheSettingsHelper::getGmmUsageTypeForUserPtr(allocationData.flags.flushL3, allocationData.hostPtr, allocationData.size, productHelper);
    auto patIndex = rootDeviceEnvironment->getGmmClientContext()->cachePolicyGetPATIndex(nullptr, usageType, false, true);
    bo->setPatIndex(patIndex);

    if (validateHostPtrMemory) {
        auto boPtr = bo.get();
        auto vmHandleId = Math::getMinLsbSet(static_cast<uint32_t>(allocationData.storageInfo.subDeviceBitfield.to_ulong()));
        auto defaultContext = getDefaultEngineContext(rootDeviceIndex, allocationData.storageInfo.subDeviceBitfield);

        int result = pinBBs.at(rootDeviceIndex)->validateHostPtr(&boPtr, 1, defaultContext, vmHandleId, static_cast<OsContextLinux *>(defaultContext)->getDrmContextIds()[0]);
        if (result != 0) {
            unreference(bo.release(), true);
            releaseGpuRange(reinterpret_cast<void *>(gpuVirtualAddress), alignedSize, rootDeviceIndex);
            return nullptr;
        }
    }

    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, 1u /*num gmms*/, allocationData.type, bo.get(), const_cast<void *>(allocationData.hostPtr),
                                        gpuVirtualAddress, allocationData.size, MemoryPool::system4KBPages);
    allocation->setAllocationOffset(offsetInPage);

    allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuVirtualAddress), alignedSize);
    bo.release();
    return allocation;
}

GraphicsAllocation *DrmMemoryManager::allocateGraphicsMemory64kb(const AllocationData &allocationData) {
    return nullptr;
}

bool DrmMemoryManager::unMapPhysicalDeviceMemoryFromVirtualMemory(GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize, OsContext *osContext, uint32_t rootDeviceIndex) {
    bool result = true;

    DrmAllocation *drmAllocation = reinterpret_cast<DrmAllocation *>(physicalAllocation);
    auto bufferObjects = drmAllocation->getBOs();
    for (auto bufferObject : bufferObjects) {
        if (bufferObject) {
            for (auto drmIterator = 0u; drmIterator < osContext->getDeviceBitfield().size(); drmIterator++) {
                if (bufferObject->unbind(osContext, drmIterator) != 0) {
                    result = false;
                }
            }
            auto address = bufferObject->peekAddress();
            uint64_t offset = address - gpuRange;
            bufferObject->setAddress(offset);
        }
    }
    physicalAllocation->setCpuPtrAndGpuAddress(nullptr, 0u);
    physicalAllocation->setReservedAddressRange(nullptr, 0u);
    return result;
}

bool DrmMemoryManager::unMapPhysicalHostMemoryFromVirtualMemory(MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) {
    void *addressToUnmap = static_cast<DrmAllocation *>(multiGraphicsAllocation.getGraphicsAllocation(physicalAllocation->getRootDeviceIndex()))->getMmapPtr();
    size_t sizeToUnmap = static_cast<DrmAllocation *>(multiGraphicsAllocation.getGraphicsAllocation(physicalAllocation->getRootDeviceIndex()))->getMmapSize();

    for (auto allocation : multiGraphicsAllocation.getGraphicsAllocations()) {
        if (allocation == nullptr) {
            continue;
        }

        const bool isLocked = allocation->isLocked();
        if (isLocked) {
            freeAssociatedResourceImpl(*allocation);
        }

        auto rootDeviceIndex = allocation->getRootDeviceIndex();
        auto memoryOperationsInterface = static_cast<DrmMemoryOperationsHandler *>(executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface.get());
        for (auto &engine : getRegisteredEngines(rootDeviceIndex)) {
            memoryOperationsInterface->evictWithinOsContext(engine.osContext, *allocation);
        }

        DrmAllocation *drmAlloc = static_cast<DrmAllocation *>(allocation);
        if (allocation->getRootDeviceIndex() == physicalAllocation->getRootDeviceIndex()) {
            drmAlloc->setMmapPtr(nullptr);
            drmAlloc->setMmapSize(0u);
            freeGraphicsMemory(allocation);
        } else {
            for (auto bo : drmAlloc->getBOs()) {
                unreference(bo, false);
            }
            delete allocation;
        }
    }
    // Unmap the memory region
    return (this->munmapFunction(addressToUnmap, sizeToUnmap) == 0);
}

bool DrmMemoryManager::mapPhysicalDeviceMemoryToVirtualMemory(GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) {
    DrmAllocation *drmAllocation = reinterpret_cast<DrmAllocation *>(physicalAllocation);
    auto bufferObjects = drmAllocation->getBOs();
    for (auto bufferObject : bufferObjects) {
        if (bufferObject) {
            auto offset = bufferObject->peekAddress();
            bufferObject->setAddress(gpuRange + offset);
        }
    }
    physicalAllocation->setCpuPtrAndGpuAddress(nullptr, gpuRange);
    physicalAllocation->setReservedAddressRange(reinterpret_cast<void *>(gpuRange), bufferSize);
    return true;
}

bool DrmMemoryManager::mapPhysicalHostMemoryToVirtualMemory(RootDeviceIndicesContainer &rootDeviceIndices, MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) {
    auto drmPhysicalAllocation = static_cast<DrmAllocation *>(physicalAllocation);
    auto &drm = this->getDrm(drmPhysicalAllocation->getRootDeviceIndex());
    BufferObject *physicalBo = drmPhysicalAllocation->getBO();
    uint64_t mmapOffset = physicalBo->getMmapOffset();
    uint64_t internalHandle = 0;
    if ((rootDeviceIndices.size() > 1) && (physicalAllocation->peekInternalHandle(this, internalHandle) < 0)) {
        return false;
    }

    [[maybe_unused]] auto retPtr = this->mmapFunction(addrToPtr(gpuRange), bufferSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, drm.getFileDescriptor(), static_cast<off_t>(mmapOffset));
    DEBUG_BREAK_IF(retPtr != addrToPtr(gpuRange));

    int physicalBoHandle = physicalBo->peekHandle();
    auto physicalBoHandleWrapper = tryToGetBoHandleWrapperWithSharedOwnership(physicalBoHandle, physicalAllocation->getRootDeviceIndex());

    auto memoryPool = MemoryPool::system4KBPages;
    auto patIndex = drm.getPatIndex(nullptr, AllocationType::bufferHostMemory, CacheRegion::defaultRegion, CachePolicy::writeBack, false, MemoryPoolHelper::isSystemMemoryPool(memoryPool));
    auto bo = new BufferObject(physicalAllocation->getRootDeviceIndex(), &drm, patIndex, std::move(physicalBoHandleWrapper), bufferSize, maxOsContextCount);

    bo->setMmapOffset(mmapOffset);
    bo->setAddress(gpuRange);
    bo->bind(getDefaultOsContext(drmPhysicalAllocation->getRootDeviceIndex()), 0, false);

    auto drmAllocation = new DrmAllocation(drmPhysicalAllocation->getRootDeviceIndex(), 1u, AllocationType::bufferHostMemory, bo, addrToPtr(bo->peekAddress()), bo->peekSize(),
                                           static_cast<osHandle>(internalHandle), memoryPool, getGmmHelper(drmPhysicalAllocation->getRootDeviceIndex())->canonize(bo->peekAddress()));

    drmAllocation->setUsmHostAllocation(true);
    drmAllocation->setShareableHostMemory(true);
    drmAllocation->setMmapPtr(addrToPtr(gpuRange));
    drmAllocation->setMmapSize(bufferSize);
    drmAllocation->setCpuPtrAndGpuAddress(retPtr, gpuRange);
    drmAllocation->setReservedAddressRange(addrToPtr(gpuRange), bufferSize);
    multiGraphicsAllocation.addAllocation(drmAllocation);
    this->registerSysMemAlloc(drmAllocation);

    for (size_t i = 0; i < rootDeviceIndices.size(); i++) {
        if (rootDeviceIndices[i] == physicalAllocation->getRootDeviceIndex()) {
            continue;
        }
        PrimeHandle openFd{};
        openFd.fileDescriptor = static_cast<osHandle>(internalHandle);

        auto &drmToImport = this->getDrm(rootDeviceIndices[i]);

        auto ioctlHelper = drm.getIoctlHelper();
        if (0 != ioctlHelper->ioctl(DrmIoctl::primeFdToHandle, &openFd)) {
            for (auto allocation : multiGraphicsAllocation.getGraphicsAllocations()) {
                if (allocation != nullptr) {
                    multiGraphicsAllocation.removeAllocation(allocation->getRootDeviceIndex());
                    freeGraphicsMemory(allocation);
                }
            }
            return false;
        }

        auto bo = new BufferObject(rootDeviceIndices[i], &drmToImport, patIndex, openFd.handle, bufferSize, maxOsContextCount);
        bo->setAddress(gpuRange);

        auto canonizedGpuAddress = getGmmHelper(rootDeviceIndices[i])->canonize(bo->peekAddress());
        auto allocation = new DrmAllocation(rootDeviceIndices[i], 1u, AllocationType::bufferHostMemory, bo, addrToPtr(bo->peekAddress()), bo->peekSize(),
                                            static_cast<osHandle>(internalHandle), memoryPool, canonizedGpuAddress);
        allocation->setImportedMmapPtr(addrToPtr(gpuRange));
        multiGraphicsAllocation.addAllocation(allocation);
    }

    return true;
}

GraphicsAllocation *DrmMemoryManager::allocatePhysicalDeviceMemory(const AllocationData &allocationData, AllocationStatus &status) {
    const auto memoryPool = MemoryPool::systemCpuInaccessible;
    StorageInfo systemMemoryStorageInfo = {};
    auto &productHelper = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHelper<ProductHelper>();
    auto gmmHelper = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper();
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;

    auto gmm = std::make_unique<Gmm>(gmmHelper, nullptr,
                                     allocationData.size, 0u, CacheSettingsHelper::getGmmUsageType(allocationData.type, allocationData.flags.uncacheable, productHelper, gmmHelper->getHardwareInfo()), systemMemoryStorageInfo, gmmRequirements);
    size_t bufferSize = allocationData.size;

    auto &drm = getDrm(allocationData.rootDeviceIndex);
    auto ioctlHelper = drm.getIoctlHelper();
    auto patIndex = drm.getPatIndex(gmm.get(), allocationData.type, CacheRegion::defaultRegion, CachePolicy::writeBack, false, MemoryPoolHelper::isSystemMemoryPool(memoryPool));
    auto isCoherent = productHelper.isCoherentAllocation(patIndex);
    uint32_t handle = ioctlHelper->createGem(bufferSize, static_cast<uint32_t>(allocationData.storageInfo.memoryBanks.to_ulong()), isCoherent);

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(new BufferObject(allocationData.rootDeviceIndex, &drm, patIndex, handle, bufferSize, maxOsContextCount));

    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, 1u /*num gmms*/, allocationData.type, bo.get(), nullptr, 0u, bufferSize, memoryPool);
    allocation->setDefaultGmm(gmm.release());

    bo.release();
    status = AllocationStatus::Success;
    return allocation;
}

GraphicsAllocation *DrmMemoryManager::allocateMemoryByKMD(const AllocationData &allocationData) {
    const auto memoryPool = MemoryPool::systemCpuInaccessible;

    auto &productHelper = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHelper<ProductHelper>();
    StorageInfo systemMemoryStorageInfo = {};
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = allocationData.flags.preferCompressed;
    auto gmmHelper = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper();
    auto gmm = std::make_unique<Gmm>(gmmHelper, allocationData.hostPtr,
                                     allocationData.size, allocationData.alignment, CacheSettingsHelper::getGmmUsageType(allocationData.type, allocationData.flags.uncacheable, productHelper, gmmHelper->getHardwareInfo()), systemMemoryStorageInfo, gmmRequirements);
    size_t bufferSize = allocationData.size;
    auto &drm = getDrm(allocationData.rootDeviceIndex);
    auto alignment = allocationData.alignment;
    if (bufferSize >= 2 * MemoryConstants::megaByte) {
        alignment = MemoryConstants::pageSize2M;
        if (drm.getIoctlHelper()->is2MBSizeAlignmentRequired(allocationData.type)) {
            bufferSize = alignUp(bufferSize, MemoryConstants::pageSize2M);
        }
    }
    uint64_t gpuRange = acquireGpuRangeWithCustomAlignment(bufferSize, allocationData.rootDeviceIndex, HeapIndex::heapStandard64KB, alignment);

    int ret = -1;
    uint32_t handle;
    auto patIndex = drm.getPatIndex(gmm.get(), allocationData.type, CacheRegion::defaultRegion, CachePolicy::writeBack, false, MemoryPoolHelper::isSystemMemoryPool(memoryPool));
    bool tryToUseGemCreateExt = productHelper.useGemCreateExtInAllocateMemoryByKMD();
    if (debugManager.flags.UseGemCreateExtInAllocateMemoryByKMD.get() != -1) {
        tryToUseGemCreateExt = debugManager.flags.UseGemCreateExtInAllocateMemoryByKMD.get() == 1;
    }
    BufferObject::BOType boType{};
    if (tryToUseGemCreateExt && drm.getMemoryInfo()) {
        ret = drm.getMemoryInfo()->createGemExtWithSingleRegion(allocationData.storageInfo.memoryBanks, bufferSize, handle, patIndex, -1, allocationData.flags.isUSMHostAllocation);
        boType = getBOTypeFromPatIndex(patIndex, productHelper.isVmBindPatIndexProgrammingSupported());
    }

    if (0 != ret) {
        auto ioctlHelper = drm.getIoctlHelper();
        auto isCoherent = productHelper.isCoherentAllocation(patIndex);
        handle = ioctlHelper->createGem(bufferSize, static_cast<uint32_t>(allocationData.storageInfo.memoryBanks.to_ulong()), isCoherent);
        boType = BufferObject::BOType::legacy;
    }

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(new BufferObject(allocationData.rootDeviceIndex, &drm, patIndex, handle, bufferSize, maxOsContextCount));
    bo->setAddress(gpuRange);
    bo->setBOType(boType);

    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, 1u /*num gmms*/, allocationData.type, bo.get(), nullptr, gpuRange, bufferSize, memoryPool);
    if (!allocation) {
        return nullptr;
    }
    bo.release();

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

    const auto memoryPool = MemoryPool::systemCpuInaccessible;

    uint64_t gpuRange = acquireGpuRange(allocationData.imgInfo->size, allocationData.rootDeviceIndex, HeapIndex::heapStandard);

    auto &drm = this->getDrm(allocationData.rootDeviceIndex);
    auto ioctlHelper = drm.getIoctlHelper();
    auto patIndex = drm.getPatIndex(gmm.get(), allocationData.type, CacheRegion::defaultRegion, CachePolicy::writeBack, false, MemoryPoolHelper::isSystemMemoryPool(memoryPool));
    auto &productHelper = drm.getRootDeviceEnvironment().getProductHelper();
    auto isCoherent = productHelper.isCoherentAllocation(patIndex);
    uint32_t handle = ioctlHelper->createGem(allocationData.imgInfo->size, static_cast<uint32_t>(allocationData.storageInfo.memoryBanks.to_ulong()), isCoherent);

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(new (std::nothrow) BufferObject(allocationData.rootDeviceIndex, &drm, patIndex, handle, allocationData.imgInfo->size, maxOsContextCount));
    if (!bo) {
        return nullptr;
    }
    bo->setAddress(gpuRange);

    [[maybe_unused]] auto ret2 = bo->setTiling(ioctlHelper->getDrmParamValue(DrmParam::tilingY), static_cast<uint32_t>(allocationData.imgInfo->rowPitch));
    DEBUG_BREAK_IF(ret2 != true);

    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, 1u /*num gmms*/, allocationData.type, bo.get(), nullptr, gpuRange, allocationData.imgInfo->size, memoryPool);
    allocation->setDefaultGmm(gmm.release());

    allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuRange), allocationData.imgInfo->size);
    bo.release();
    return allocation;
}

GraphicsAllocation *DrmMemoryManager::allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData) {
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHardwareInfo();
    auto allocatorToUse = heapAssigners[allocationData.rootDeviceIndex]->get32BitHeapIndex(allocationData.type, false, *hwInfo, allocationData.flags.use32BitFrontWindow);

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

        std::unique_ptr<BufferObject, BufferObject::Deleter> bo(allocUserptr(alignedUserPointer, allocationSize, allocationData.type, allocationData.rootDeviceIndex));
        if (!bo) {
            gfxPartition->heapFree(allocatorToUse, gpuVirtualAddress, realAllocationSize);
            return nullptr;
        }

        bo->setAddress(gpuVirtualAddress);
        auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
        auto canonizedGpuAddress = gmmHelper->canonize(ptrOffset(gpuVirtualAddress, inputPointerOffset));
        auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, 1u /*num gmms*/, allocationData.type, bo.get(), const_cast<void *>(allocationData.hostPtr),
                                            canonizedGpuAddress,
                                            allocationSize, MemoryPool::system4KBPagesWith32BitGpuAddressing);
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

    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(allocUserptr(reinterpret_cast<uintptr_t>(ptrAlloc), alignedAllocationSize, allocationData.type, allocationData.rootDeviceIndex));

    if (!bo) {
        alignedFreeWrapper(ptrAlloc);
        gfxPartition->heapFree(allocatorToUse, gpuVA, allocationSize);
        return nullptr;
    }

    bo->setAddress(gpuVA);
    auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);

    // softpin to the GPU address, res if it uses limitedRange Allocation
    auto canonizedGpuAddress = gmmHelper->canonize(gpuVA);
    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, 1u /*num gmms*/, allocationData.type, bo.get(), ptrAlloc,
                                        canonizedGpuAddress, alignedAllocationSize,
                                        MemoryPool::system4KBPagesWith32BitGpuAddressing);

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

GraphicsAllocation *DrmMemoryManager::createGraphicsAllocationFromMultipleSharedHandles(const std::vector<osHandle> &handles,
                                                                                        AllocationProperties &properties,
                                                                                        bool requireSpecificBitness,
                                                                                        bool isHostIpcAllocation,
                                                                                        bool reuseSharedAllocation,
                                                                                        void *mapPointer) {
    BufferObjects bos;
    std::vector<size_t> sizes;
    size_t totalSize = 0;
    uint64_t gpuRange = 0;

    std::unique_lock<std::mutex> lock(mtx);

    uint32_t i = 0;

    if (handles.size() != 1) {
        properties.multiStorageResource = true;
    }

    auto &drm = this->getDrm(properties.rootDeviceIndex);

    bool areBosSharedObjects = true;
    auto ioctlHelper = drm.getIoctlHelper();

    const auto memoryPool = MemoryPool::localMemory;

    for (auto handle : handles) {
        PrimeHandle openFd = {0, 0, 0};
        openFd.fileDescriptor = handle;

        auto ret = ioctlHelper->ioctl(DrmIoctl::primeFdToHandle, &openFd);

        if (ret != 0) {
            [[maybe_unused]] int err = errno;
            PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(PRIME_FD_TO_HANDLE) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));

            return nullptr;
        }

        auto boHandle = static_cast<int>(openFd.handle);
        BufferObject *bo = nullptr;
        if (reuseSharedAllocation) {
            bo = findAndReferenceSharedBufferObject(boHandle, properties.rootDeviceIndex);
        }

        if (bo == nullptr) {
            areBosSharedObjects = false;

            size_t size = SysCalls::lseek(handle, 0, SEEK_END);
            UNRECOVERABLE_IF(size == std::numeric_limits<size_t>::max());
            totalSize += size;

            auto patIndex = drm.getPatIndex(nullptr, properties.allocationType, CacheRegion::defaultRegion, CachePolicy::writeBack, false, MemoryPoolHelper::isSystemMemoryPool(memoryPool));
            auto boHandleWrapper = reuseSharedAllocation ? BufferObjectHandleWrapper{boHandle, properties.rootDeviceIndex} : tryToGetBoHandleWrapperWithSharedOwnership(boHandle, properties.rootDeviceIndex);

            bo = new (std::nothrow) BufferObject(properties.rootDeviceIndex, &drm, patIndex, std::move(boHandleWrapper), size, maxOsContextCount);
            i++;
        }
        bos.push_back(bo);
        sizes.push_back(bo->peekSize());
    }

    if (mapPointer) {
        gpuRange = reinterpret_cast<uint64_t>(mapPointer);
    } else {
        auto gfxPartition = getGfxPartition(properties.rootDeviceIndex);
        auto prefer57bitAddressing = (gfxPartition->getHeapLimit(HeapIndex::heapExtended) > 0);
        auto heapIndex = prefer57bitAddressing ? HeapIndex::heapExtended : HeapIndex::heapStandard2MB;
        gpuRange = acquireGpuRange(totalSize, properties.rootDeviceIndex, heapIndex);
    }

    if (reuseSharedAllocation) {
        lock.unlock();
    }

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
                                           memoryPool);
    drmAllocation->storageInfo = allocationData.storageInfo;
    auto gmmHelper = executionEnvironment.rootDeviceEnvironments[properties.rootDeviceIndex]->getGmmHelper();
    auto &productHelper = gmmHelper->getRootDeviceEnvironment().getHelper<ProductHelper>();
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    for (i = 0u; i < handles.size(); i++) {
        auto bo = bos[i];
        StorageInfo limitedStorageInfo = allocationData.storageInfo;
        limitedStorageInfo.memoryBanks &= (1u << (i % handles.size()));

        auto gmm = new Gmm(gmmHelper,
                           nullptr,
                           bo->peekSize(),
                           0u,
                           CacheSettingsHelper::getGmmUsageType(drmAllocation->getAllocationType(), false, productHelper, gmmHelper->getHardwareInfo()),
                           allocationData.storageInfo,
                           gmmRequirements);
        drmAllocation->setGmm(gmm, i);

        if (areBosSharedObjects == false) {
            bo->setAddress(gpuRange);
            gpuRange += bo->peekSize();
            bo->setUnmapSize(sizes[i]);
            pushSharedBufferObject(bo);
        }
        drmAllocation->getBufferObjectToModify(i) = bo;
    }

    if (!reuseSharedAllocation) {
        registerSharedBoHandleAllocation(drmAllocation);
    }

    return drmAllocation;
}

void DrmMemoryManager::registerIpcExportedAllocation(GraphicsAllocation *graphicsAllocation) {
    std::lock_guard lock{mtx};
    registerSharedBoHandleAllocation(static_cast<DrmAllocation *>(graphicsAllocation));
}

void DrmMemoryManager::registerSharedBoHandleAllocation(DrmAllocation *drmAllocation) {
    if (!drmAllocation) {
        return;
    }

    auto &bos = drmAllocation->getBOs();
    auto rootDeviceIndex = drmAllocation->getRootDeviceIndex();

    for (auto *bo : bos) {
        if (bo == nullptr) {
            continue;
        }

        auto foundHandleWrapperIt = sharedBoHandles.find(std::pair<int, uint32_t>(bo->getHandle(), rootDeviceIndex));
        if (foundHandleWrapperIt == std::end(sharedBoHandles)) {
            sharedBoHandles.insert({std::make_pair(bo->getHandle(), rootDeviceIndex), bo->acquireWeakOwnershipOfBoHandle()});
        } else {
            bo->markAsSharedBoHandle();
        }
    }
}

BufferObjectHandleWrapper DrmMemoryManager::tryToGetBoHandleWrapperWithSharedOwnership(int boHandle, uint32_t rootDeviceIndex) {
    auto foundHandleWrapperIt = sharedBoHandles.find(std::make_pair(boHandle, rootDeviceIndex));
    if (foundHandleWrapperIt == std::end(sharedBoHandles)) {
        return BufferObjectHandleWrapper{boHandle, rootDeviceIndex};
    }
    return foundHandleWrapperIt->second.acquireSharedOwnership();
}

void DrmMemoryManager::eraseSharedBoHandleWrapper(int boHandle, uint32_t rootDeviceIndex) {
    auto foundHandleWrapperIt = sharedBoHandles.find(std::make_pair(boHandle, rootDeviceIndex));
    if (foundHandleWrapperIt != std::end(sharedBoHandles)) {
        sharedBoHandles.erase(foundHandleWrapperIt);
    }
}

GraphicsAllocation *DrmMemoryManager::createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData,
                                                                               const AllocationProperties &properties,
                                                                               bool requireSpecificBitness,
                                                                               bool isHostIpcAllocation,
                                                                               bool reuseSharedAllocation,
                                                                               void *mapPointer) {
    if (isHostIpcAllocation) {
        return createUSMHostAllocationFromSharedHandle(osHandleData.handle, properties, nullptr, reuseSharedAllocation);
    }

    std::unique_lock<std::mutex> lock(mtx);

    PrimeHandle openFd{};
    uint64_t gpuRange = 0;
    openFd.fileDescriptor = osHandleData.handle;

    auto &drm = this->getDrm(properties.rootDeviceIndex);
    auto ioctlHelper = drm.getIoctlHelper();

    auto ret = ioctlHelper->ioctl(DrmIoctl::primeFdToHandle, &openFd);

    if (ret != 0) {
        [[maybe_unused]] int err = errno;
        PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(PRIME_FD_TO_HANDLE) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));

        return nullptr;
    }

    auto boHandle = static_cast<int>(openFd.handle);
    BufferObject *bo = nullptr;
    if (reuseSharedAllocation) {
        bo = findAndReferenceSharedBufferObject(boHandle, properties.rootDeviceIndex);
    }

    const auto memoryPool = MemoryPool::systemCpuInaccessible;

    if (bo == nullptr) {
        size_t size = SysCalls::lseek(osHandleData.handle, 0, SEEK_END);
        UNRECOVERABLE_IF(size == std::numeric_limits<size_t>::max());

        auto patIndex = drm.getPatIndex(nullptr, properties.allocationType, CacheRegion::defaultRegion, CachePolicy::writeBack, false, MemoryPoolHelper::isSystemMemoryPool(memoryPool));
        auto boHandleWrapper = reuseSharedAllocation ? BufferObjectHandleWrapper{boHandle, properties.rootDeviceIndex} : tryToGetBoHandleWrapperWithSharedOwnership(boHandle, properties.rootDeviceIndex);

        bo = new (std::nothrow) BufferObject(properties.rootDeviceIndex, &drm, patIndex, std::move(boHandleWrapper), size, maxOsContextCount);

        if (!bo) {
            return nullptr;
        }

        auto getHeapIndex = [&] {
            if (requireSpecificBitness && this->force32bitAllocations) {
                return HeapIndex::heapExternal;
            }

            auto gfxPartition = getGfxPartition(properties.rootDeviceIndex);
            auto prefer57bitAddressing = (gfxPartition->getHeapLimit(HeapIndex::heapExtended) > 0);
            if (prefer57bitAddressing) {
                return HeapIndex::heapExtended;
            }

            if (isLocalMemorySupported(properties.rootDeviceIndex)) {
                return HeapIndex::heapStandard2MB;
            }

            return HeapIndex::heapStandard;
        };

        if (mapPointer) {
            gpuRange = reinterpret_cast<uint64_t>(mapPointer);
        } else {
            auto heapIndex = getHeapIndex();
            gpuRange = acquireGpuRange(size, properties.rootDeviceIndex, heapIndex);
        }

        bo->setAddress(gpuRange);
        bo->setUnmapSize(size);

        printDebugString(debugManager.flags.PrintBOCreateDestroyResult.get(), stdout,
                         "Created BO-%d range: %llx - %llx, size: %lld from PRIME_FD_TO_HANDLE\n",
                         bo->peekHandle(),
                         bo->peekAddress(),
                         ptrOffset(bo->peekAddress(), bo->peekSize()),
                         bo->peekSize());

        pushSharedBufferObject(bo);
    }

    if (reuseSharedAllocation) {
        lock.unlock();
    }

    auto gmmHelper = getGmmHelper(properties.rootDeviceIndex);
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(reinterpret_cast<void *>(bo->peekAddress())));
    auto drmAllocation = new DrmAllocation(properties.rootDeviceIndex, 1u /*num gmms*/, properties.allocationType, bo, reinterpret_cast<void *>(bo->peekAddress()), bo->peekSize(),
                                           osHandleData.handle, memoryPool, canonizedGpuAddress);
    this->makeAllocationResident(drmAllocation);

    if (requireSpecificBitness && this->force32bitAllocations) {
        drmAllocation->set32BitAllocation(true);
        auto gmmHelper = getGmmHelper(properties.rootDeviceIndex);
        drmAllocation->setGpuBaseAddress(gmmHelper->canonize(getExternalHeapBaseAddress(properties.rootDeviceIndex, drmAllocation->isAllocatedInLocalMemoryPool())));
    }

    if (properties.imgInfo) {
        GemGetTiling getTiling{};
        getTiling.handle = boHandle;
        ret = ioctlHelper->getGemTiling(&getTiling);

        if (ret) {
            if (getTiling.tilingMode == static_cast<uint32_t>(ioctlHelper->getDrmParamValue(DrmParam::tilingNone))) {
                properties.imgInfo->linearStorage = true;
            }
        }

        Gmm *gmm = new Gmm(executionEnvironment.rootDeviceEnvironments[properties.rootDeviceIndex]->getGmmHelper(), *properties.imgInfo,
                           createStorageInfoFromProperties(properties), properties.flags.preferCompressed);

        gmm->updateImgInfoAndDesc(*properties.imgInfo, 0, NEO::ImagePlane::noPlane);
        drmAllocation->setDefaultGmm(gmm);

        bo->setPatIndex(drm.getPatIndex(gmm, properties.allocationType, CacheRegion::defaultRegion, CachePolicy::writeBack, false, MemoryPoolHelper::isSystemMemoryPool(memoryPool)));
    }

    if (!reuseSharedAllocation) {
        registerSharedBoHandleAllocation(drmAllocation);
    }

    return drmAllocation;
}

void DrmMemoryManager::closeInternalHandle(uint64_t &handle, uint32_t handleId, GraphicsAllocation *graphicsAllocation) {
    if (graphicsAllocation) {
        DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
        drmAllocation->clearInternalHandle(handleId);
    }
    [[maybe_unused]] auto status = this->closeFunction(static_cast<int>(handle));
    DEBUG_BREAK_IF(status != 0);
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
    if (debugManager.flags.DoNotFreeResources.get()) {
        return;
    }
    DrmAllocation *drmAlloc = static_cast<DrmAllocation *>(gfxAllocation);
    if (Sharing::nonSharedResource == gfxAllocation->peekSharedHandle()) {
        this->unregisterAllocation(gfxAllocation);
    }
    auto rootDeviceIndex = gfxAllocation->getRootDeviceIndex();
    for (auto &engine : getRegisteredEngines(rootDeviceIndex)) {
        std::unique_lock<CommandStreamReceiver::MutexType> lock;
        if (engine.osContext->isDirectSubmissionLightActive()) {
            lock = engine.commandStreamReceiver->obtainUniqueOwnership();
            engine.commandStreamReceiver->stopDirectSubmission(true, false);
            handleFenceCompletion(gfxAllocation);
        }

        auto memoryOperationsInterface = static_cast<DrmMemoryOperationsHandler *>(executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface.get());
        memoryOperationsInterface->evictWithinOsContext(engine.osContext, *gfxAllocation);
    }

    auto ioctlHelper = getDrm(drmAlloc->getRootDeviceIndex()).getIoctlHelper();
    if (drmAlloc->getMmapPtr()) {
        ioctlHelper->munmapFunction(*this, drmAlloc->getMmapPtr(), drmAlloc->getMmapSize());
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

    ioctlHelper->syncUserptrAlloc(*this, *gfxAllocation);
    ioctlHelper->releaseGpuRange(*this, gfxAllocation->getReservedAddressPtr(), gfxAllocation->getReservedAddressSize(), gfxAllocation->getRootDeviceIndex(), gfxAllocation->getAllocationType());
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

        uint64_t internalHandle = 0;
        int ret = defaultAlloc->peekInternalHandle(this, internalHandle);
        if (ret < 0) {
            return nullptr;
        }
        return createUSMHostAllocationFromSharedHandle(static_cast<osHandle>(internalHandle), properties, ptr, true);
    } else {
        return allocateGraphicsMemoryWithProperties(properties, ptr);
    }
}

uint64_t DrmMemoryManager::getSystemSharedMemory(uint32_t rootDeviceIndex) {
    uint64_t hostMemorySize = MemoryConstants::pageSize * static_cast<uint64_t>(SysCalls::sysconf(_SC_PHYS_PAGES));

    uint64_t gpuMemorySize = 0u;

    [[maybe_unused]] auto ret = getDrm(rootDeviceIndex).queryGttSize(gpuMemorySize, false);
    DEBUG_BREAK_IF(ret != 0);

    auto memoryInfo = getDrm(rootDeviceIndex).getMemoryInfo();
    if (memoryInfo) {
        auto systemMemoryRegionSize = memoryInfo->getMemoryRegion(static_cast<uint32_t>(systemMemoryBitfield.to_ulong())).probedSize;
        gpuMemorySize = std::min(gpuMemorySize, systemMemoryRegionSize);
    }

    return std::min(hostMemorySize, gpuMemorySize);
}

double DrmMemoryManager::getPercentOfGlobalMemoryAvailable(uint32_t rootDeviceIndex) {
    if (isLocalMemorySupported(rootDeviceIndex)) {
        return 0.95;
    }
    return 0.94;
}

AllocationStatus DrmMemoryManager::populateOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) {
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
                                        handleStorage.fragmentStorageData[i].fragmentSize, AllocationType::externalHostPtr, rootDeviceIndex);
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
        int result = pinBBs.at(rootDeviceIndex)->validateHostPtr(allocatedBos, numberOfBosAllocated, getDefaultOsContext(rootDeviceIndex), 0, getDefaultDrmContextId(rootDeviceIndex));

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

    auto rootDeviceIndex = graphicsAllocation.getRootDeviceIndex();
    auto ioctlHelper = this->getDrm(rootDeviceIndex).getIoctlHelper();

    if (ioctlHelper->makeResidentBeforeLockNeeded()) {
        auto memoryOperationsInterface = static_cast<DrmMemoryOperationsHandler *>(executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface.get());
        auto graphicsAllocationPtr = &graphicsAllocation;
        [[maybe_unused]] auto ret = memoryOperationsInterface->makeResidentWithinOsContext(getDefaultOsContext(rootDeviceIndex), ArrayRef<NEO::GraphicsAllocation *>(&graphicsAllocationPtr, 1), false, false, true) == MemoryOperationsStatus::success;
        DEBUG_BREAK_IF(!ret);
    }

    auto bo = static_cast<DrmAllocation &>(graphicsAllocation).getBO();
    if (graphicsAllocation.getAllocationType() == AllocationType::writeCombined) {
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

    int ret = ioctlHelper->ioctl(DrmIoctl::primeHandleToFd, &openFd);
    if (ret < 0) {
        return -1;
    }

    return openFd.fileDescriptor;
}

OsContextLinux *DrmMemoryManager::getDefaultOsContext(uint32_t rootDeviceIndex) const {
    return static_cast<OsContextLinux *>(allRegisteredEngines.at(rootDeviceIndex)[defaultEngineIndex[rootDeviceIndex]].osContext);
}
uint32_t DrmMemoryManager::getDefaultDrmContextId(uint32_t rootDeviceIndex) const {
    auto osContextLinux = getDefaultOsContext(rootDeviceIndex);
    return osContextLinux->getDrmContextIds()[0];
}

size_t DrmMemoryManager::getUserptrAlignment() {
    auto alignment = MemoryConstants::allocationAlignment;

    if (debugManager.flags.ForceUserptrAlignment.get() != -1) {
        alignment = debugManager.flags.ForceUserptrAlignment.get() * MemoryConstants::kiloByte;
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

size_t DrmMemoryManager::selectAlignmentAndHeap(size_t size, HeapIndex *heap) {
    AlignmentSelector::CandidateAlignment alignmentBase = alignmentSelector.selectAlignment(size);
    size_t pageSizeAlignment = alignmentBase.alignment;
    auto rootDeviceCount = this->executionEnvironment.rootDeviceEnvironments.size();

    // If all devices can support HEAP EXTENDED, then that heap is used, otherwise the HEAP based on the size is used.
    for (auto rootDeviceIndex = 0u; rootDeviceIndex < rootDeviceCount; rootDeviceIndex++) {
        auto gfxPartition = getGfxPartition(rootDeviceIndex);
        if (gfxPartition->getHeapLimit(HeapIndex::heapExtended) > 0) {
            auto alignSize = size >= 8 * MemoryConstants::gigaByte && Math::isPow2(size);
            if (debugManager.flags.UseHighAlignmentForHeapExtended.get() != -1) {
                alignSize = !!debugManager.flags.UseHighAlignmentForHeapExtended.get();
            }

            if (alignSize) {
                pageSizeAlignment = Math::prevPowerOfTwo(size);
            }

            *heap = HeapIndex::heapExtended;
        } else {
            pageSizeAlignment = alignmentBase.alignment;
            *heap = alignmentBase.heap;
            break;
        }
    }
    return pageSizeAlignment;
}

AddressRange DrmMemoryManager::reserveGpuAddress(const uint64_t requiredStartAddress, size_t size, const RootDeviceIndicesContainer &rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex) {
    return reserveGpuAddressOnHeap(requiredStartAddress, size, rootDeviceIndices, reservedOnRootDeviceIndex, HeapIndex::heapStandard, MemoryConstants::pageSize64k);
}

AddressRange DrmMemoryManager::reserveGpuAddressOnHeap(const uint64_t requiredStartAddress, size_t size, const RootDeviceIndicesContainer &rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex, HeapIndex heap, size_t alignment) {
    uint64_t gpuVa = 0u;
    *reservedOnRootDeviceIndex = 0;
    for (auto rootDeviceIndex : rootDeviceIndices) {
        if (heap == HeapIndex::heapExtended) {
            gpuVa = acquireGpuRangeWithCustomAlignment(size, rootDeviceIndex, heap, alignment);
        } else {
            gpuVa = acquireGpuRange(size, rootDeviceIndex, heap);
        }
        if (gpuVa != 0u) {
            *reservedOnRootDeviceIndex = rootDeviceIndex;
            break;
        }
    }
    return AddressRange{gpuVa, size};
}

void DrmMemoryManager::freeGpuAddress(AddressRange addressRange, uint32_t rootDeviceIndex) {
    releaseGpuRange(reinterpret_cast<void *>(addressRange.address), addressRange.size, rootDeviceIndex);
}

AddressRange DrmMemoryManager::reserveCpuAddress(const uint64_t requiredStartAddress, size_t size) {
    void *ptr = osMemory->osReserveCpuAddressRange(addrToPtr(requiredStartAddress), size, false);
    if (ptr == MAP_FAILED) {
        ptr = nullptr;
    }
    return {castToUint64(ptr), size};
}

void DrmMemoryManager::freeCpuAddress(AddressRange addressRange) {
    osMemory->osReleaseCpuAddressRange(addrToPtr(addressRange.address), addressRange.size);
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

bool DrmMemoryManager::makeAllocationResident(GraphicsAllocation *allocation) {
    auto rootDeviceIndex = allocation->getRootDeviceIndex();
    auto memoryOperationsInterface = static_cast<DrmMemoryOperationsHandler *>(executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface.get());
    const auto &engines = this->getRegisteredEngines(rootDeviceIndex);
    for (const auto &engine : engines) {
        if (engine.osContext->isDirectSubmissionLightActive() &&
            allocation->getAllocationType() != AllocationType::ringBuffer &&
            allocation->getAllocationType() != AllocationType::semaphoreBuffer) {
            memoryOperationsInterface->makeResidentWithinOsContext(engine.osContext, ArrayRef<NEO::GraphicsAllocation *>(&allocation, 1), false, false, true);
        }
    }

    if (debugManager.flags.MakeEachAllocationResident.get() == 1) {
        auto drmAllocation = static_cast<DrmAllocation *>(allocation);
        auto rootDeviceIndex = allocation->getRootDeviceIndex();
        for (uint32_t i = 0; getDrm(rootDeviceIndex).getVirtualMemoryAddressSpace(i) > 0u; i++) {
            if (drmAllocation->makeBOsResident(getDefaultOsContext(rootDeviceIndex), i, nullptr, true, false)) {
                return false;
            }
            getDrm(rootDeviceIndex).waitForBind(i);
        }
    }
    return true;
}

AllocationStatus DrmMemoryManager::registerSysMemAlloc(GraphicsAllocation *allocation) {
    if (!makeAllocationResident(allocation)) {
        return AllocationStatus::Error;
    }
    this->sysMemAllocsSize += allocation->getUnderlyingBufferSize();
    std::lock_guard<std::mutex> lock(this->allocMutex);
    this->sysMemAllocs.push_back(allocation);
    return AllocationStatus::Success;
}

AllocationStatus DrmMemoryManager::registerLocalMemAlloc(GraphicsAllocation *allocation, uint32_t rootDeviceIndex) {
    if (!makeAllocationResident(allocation)) {
        return AllocationStatus::Error;
    }
    this->localMemAllocsSize[rootDeviceIndex] += allocation->getUnderlyingBufferSize();
    std::lock_guard<std::mutex> lock(this->allocMutex);
    this->localMemAllocs[rootDeviceIndex].push_back(allocation);
    return AllocationStatus::Success;
}

void DrmMemoryManager::unregisterAllocation(GraphicsAllocation *allocation) {
    if (allocation->isAllocatedInLocalMemoryPool()) {
        DEBUG_BREAK_IF(allocation->getUnderlyingBufferSize() > localMemAllocsSize[allocation->getRootDeviceIndex()]);
        localMemAllocsSize[allocation->getRootDeviceIndex()] -= allocation->getUnderlyingBufferSize();
    } else if (MemoryPool::memoryNull != allocation->getMemoryPool()) {
        DEBUG_BREAK_IF(allocation->getUnderlyingBufferSize() > sysMemAllocsSize);
        sysMemAllocsSize -= allocation->getUnderlyingBufferSize();
    }
    std::lock_guard<std::mutex> lock(this->allocMutex);
    sysMemAllocs.erase(std::remove(sysMemAllocs.begin(), sysMemAllocs.end(), allocation),
                       sysMemAllocs.end());
    localMemAllocs[allocation->getRootDeviceIndex()].erase(std::remove(localMemAllocs[allocation->getRootDeviceIndex()].begin(),
                                                                       localMemAllocs[allocation->getRootDeviceIndex()].end(),
                                                                       allocation),
                                                           localMemAllocs[allocation->getRootDeviceIndex()].end());
}

void DrmMemoryManager::registerAllocationInOs(GraphicsAllocation *allocation) {

    if (allocation) {
        auto drmAllocation = static_cast<DrmAllocation *>(allocation);
        auto drm = &getDrm(drmAllocation->getRootDeviceIndex());
        if (drm->getRootDeviceEnvironment().executionEnvironment.isDebuggingEnabled()) {
            drmAllocation->registerBOBindExtHandle(drm);
            if (isAllocationTypeToCapture(drmAllocation->getAllocationType())) {
                drmAllocation->markForCapture();
            }
        }
    }
}

std::unique_ptr<MemoryManager> DrmMemoryManager::create(ExecutionEnvironment &executionEnvironment) {
    bool validateHostPtr = true;

    if (debugManager.flags.EnableHostPtrValidation.get() != -1) {
        validateHostPtr = debugManager.flags.EnableHostPtrValidation.get();
    }

    return std::make_unique<DrmMemoryManager>(GemCloseWorkerMode::gemCloseWorkerActive,
                                              debugManager.flags.EnableForcePin.get(),
                                              validateHostPtr,
                                              executionEnvironment);
}

uint64_t DrmMemoryManager::getLocalMemorySize(uint32_t rootDeviceIndex, uint32_t deviceBitfield) {
    auto memoryInfo = getDrm(rootDeviceIndex).getMemoryInfo();
    if (!memoryInfo) {
        return 0;
    }
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
    uint32_t subDevicesCount = GfxCoreHelper::getSubDevicesCount(hwInfo);

    auto ioctlHelper = getDrm(rootDeviceIndex).getIoctlHelper();
    return ioctlHelper->getLocalMemoryRegionsSize(memoryInfo, subDevicesCount, deviceBitfield);
}

void DrmMemoryManager::drainGemCloseWorker() const {
    if (this->peekGemCloseWorker()) {
        this->peekGemCloseWorker()->close(true);
    }
}

void DrmMemoryManager::disableForcePin() {
    this->forcePinEnabled = false;
}

bool DrmMemoryManager::copyMemoryToAllocation(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy) {
    if (graphicsAllocation->getUnderlyingBuffer() && (graphicsAllocation->storageInfo.getNumBanks() == 1 || GraphicsAllocation::isDebugSurfaceAllocationType(graphicsAllocation->getAllocationType()))) {
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

void createColouredGmms(GmmHelper *gmmHelper, DrmAllocation &allocation, bool compression) {
    const StorageInfo &storageInfo = allocation.storageInfo;
    DEBUG_BREAK_IF(storageInfo.colouringPolicy == ColouringPolicy::deviceCountBased && storageInfo.colouringGranularity != MemoryConstants::pageSize64k);

    auto remainingSize = alignUp(allocation.getUnderlyingBufferSize(), storageInfo.colouringGranularity);
    auto handles = storageInfo.getNumBanks();
    auto banksCnt = storageInfo.getTotalBanksCnt();

    if (storageInfo.colouringPolicy == ColouringPolicy::chunkSizeBased) {
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
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = compression;
    for (auto handleId = 0u; handleId < handles; handleId++) {
        auto currentSize = alignUp(remainingSize / (handles - handleId), storageInfo.colouringGranularity);
        remainingSize -= currentSize;
        StorageInfo limitedStorageInfo = storageInfo;
        limitedStorageInfo.memoryBanks &= (1u << (handleId % banksCnt));
        auto &productHelper = gmmHelper->getRootDeviceEnvironment().getHelper<ProductHelper>();
        auto gmm = new Gmm(gmmHelper,
                           nullptr,
                           currentSize,
                           0u,
                           CacheSettingsHelper::getGmmUsageType(allocation.getAllocationType(), false, productHelper, gmmHelper->getHardwareInfo()),
                           limitedStorageInfo,
                           gmmRequirements);
        allocation.setGmm(gmm, handleId);
    }
}

void fillGmmsInAllocation(GmmHelper *gmmHelper, DrmAllocation *allocation) {
    auto alignedSize = alignUp(allocation->getUnderlyingBufferSize(), MemoryConstants::pageSize64k);
    auto &productHelper = gmmHelper->getRootDeviceEnvironment().getHelper<ProductHelper>();
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;

    const StorageInfo &storageInfo = allocation->storageInfo;
    for (auto handleId = 0u; handleId < storageInfo.getNumBanks(); handleId++) {
        StorageInfo limitedStorageInfo = storageInfo;
        limitedStorageInfo.memoryBanks &= 1u << handleId;
        limitedStorageInfo.pageTablesVisibility &= 1u << handleId;
        auto gmm = new Gmm(gmmHelper, nullptr, alignedSize, 0u,
                           CacheSettingsHelper::getGmmUsageType(allocation->getAllocationType(), false, productHelper, gmmHelper->getHardwareInfo()), limitedStorageInfo, gmmRequirements);
        allocation->setGmm(gmm, handleId);
    }
}

inline uint64_t getCanonizedHeapAllocationAddress(HeapIndex heap, GmmHelper *gmmHelper, GfxPartition *gfxPartition, size_t &sizeAllocated, bool packed) {
    size_t alignment = 0;

    if (debugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.get() != -1) {
        alignment = static_cast<size_t>(debugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.get());
    }
    auto address = gfxPartition->heapAllocateWithCustomAlignment(heap, sizeAllocated, alignment);
    return gmmHelper->canonize(address);
}

AllocationStatus getGpuAddress(const AlignmentSelector &alignmentSelector, HeapAssigner &heapAssigner, const HardwareInfo &hwInfo,
                               GfxPartition *gfxPartition, const AllocationData &allocationData, size_t &sizeAllocated,
                               GmmHelper *gmmHelper, uint64_t &gpuAddress) {

    auto allocType = allocationData.type;

    switch (allocType) {
    case AllocationType::svmGpu:
        gpuAddress = reinterpret_cast<uint64_t>(allocationData.hostPtr);
        sizeAllocated = 0;
        break;
    case AllocationType::kernelIsa:
    case AllocationType::kernelIsaInternal:
    case AllocationType::internalHeap:
    case AllocationType::debugModuleArea:
        gpuAddress = getCanonizedHeapAllocationAddress(heapAssigner.get32BitHeapIndex(allocType, true, hwInfo, allocationData.flags.use32BitFrontWindow),
                                                       gmmHelper, gfxPartition, sizeAllocated, false);
        break;
    case AllocationType::writeCombined:
        sizeAllocated = 0;
        break;
    default:
        if (heapAssigner.useExternal32BitHeap(allocType)) {
            auto heapIndex = allocationData.flags.use32BitFrontWindow ? HeapAssigner::mapExternalWindowIndex(HeapIndex::heapExternalDeviceMemory) : HeapIndex::heapExternalDeviceMemory;
            gpuAddress = gmmHelper->canonize(gfxPartition->heapAllocateWithCustomAlignment(heapIndex, sizeAllocated, std::max(allocationData.alignment, MemoryConstants::pageSize64k)));
            break;
        }

        AlignmentSelector::CandidateAlignment alignment = alignmentSelector.selectAlignment(sizeAllocated);
        if (gfxPartition->getHeapLimit(HeapIndex::heapExtended) > 0 && !allocationData.flags.resource48Bit) {
            auto alignSize = sizeAllocated >= 8 * MemoryConstants::gigaByte && Math::isPow2(sizeAllocated);
            if (debugManager.flags.UseHighAlignmentForHeapExtended.get() != -1) {
                alignSize = !!debugManager.flags.UseHighAlignmentForHeapExtended.get();
            }

            if (alignSize) {
                alignment.alignment = Math::prevPowerOfTwo(sizeAllocated);
            }

            alignment.heap = HeapIndex::heapExtended;
        }
        if (alignment.alignment < allocationData.alignment) {
            alignment.alignment = allocationData.alignment;
        }
        gpuAddress = gmmHelper->canonize(gfxPartition->heapAllocateWithCustomAlignment(alignment.heap, sizeAllocated, alignment.alignment));
        if (alignment.heap == HeapIndex::heapExtended) {
            gpuAddress = MemoryManager::adjustToggleBitFlagForGpuVa(allocationData.type, gpuAddress);
        }
        break;
    }

    if (allocType != NEO::AllocationType::writeCombined && gpuAddress == 0llu) {
        return AllocationStatus::Error;
    } else {
        return AllocationStatus::Success;
    }
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

inline std::unique_ptr<Gmm> DrmMemoryManager::makeGmmIfSingleHandle(const AllocationData &allocationData, size_t sizeAligned) {
    if (1 != allocationData.storageInfo.getNumBanks()) {
        return nullptr;
    }
    auto gmmHelper = this->getGmmHelper(allocationData.rootDeviceIndex);
    auto &productHelper = gmmHelper->getRootDeviceEnvironment().getHelper<ProductHelper>();
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = allocationData.flags.preferCompressed;

    if (productHelper.overrideAllocationCpuCacheable(allocationData)) {
        gmmRequirements.overriderCacheable.enableOverride = true;
        gmmRequirements.overriderCacheable.value = true;
    }

    return std::make_unique<Gmm>(gmmHelper,
                                 nullptr,
                                 sizeAligned,
                                 0u,
                                 CacheSettingsHelper::getGmmUsageType(allocationData.type, allocationData.flags.uncacheable, productHelper, gmmHelper->getHardwareInfo()),
                                 allocationData.storageInfo,
                                 gmmRequirements);
}

inline std::unique_ptr<DrmAllocation> DrmMemoryManager::makeDrmAllocation(const AllocationData &allocationData, std::unique_ptr<Gmm> gmm, uint64_t gpuAddress, size_t sizeAligned) {
    auto gmmHelper = this->getGmmHelper(allocationData.rootDeviceIndex);
    bool createSingleHandle = (1 == allocationData.storageInfo.getNumBanks());

    auto allocation = std::make_unique<DrmAllocation>(allocationData.rootDeviceIndex, allocationData.storageInfo.getNumBanks(),
                                                      allocationData.type, nullptr, nullptr, gmmHelper->canonize(gpuAddress),
                                                      sizeAligned, MemoryPool::localMemory);
    allocation->storageInfo = allocationData.storageInfo;

    if (createSingleHandle) {
        allocation->setDefaultGmm(gmm.release());
    } else if (allocationData.storageInfo.multiStorage) {
        createColouredGmms(gmmHelper, *allocation, allocationData.flags.preferCompressed);
    } else {
        fillGmmsInAllocation(gmmHelper, allocation.get());
    }

    allocation->setFlushL3Required(allocationData.flags.flushL3);
    allocation->setUncacheable(allocationData.flags.uncacheable);
    if (debugManager.flags.EnableHostAllocationMemPolicy.get()) {
        allocation->setUsmHostAllocation(allocationData.flags.isUSMHostAllocation);
    }

    return allocation;
}

GraphicsAllocation *DrmMemoryManager::allocatePhysicalLocalDeviceMemory(const AllocationData &allocationData, AllocationStatus &status) {

    size_t sizeAligned = alignUp(allocationData.size, MemoryConstants::pageSize64k);
    auto gmm = this->makeGmmIfSingleHandle(allocationData, sizeAligned);

    auto allocation = this->makeDrmAllocation(allocationData, std::move(gmm), 0u, sizeAligned);
    auto *drmAllocation = static_cast<DrmAllocation *>(allocation.get());

    if (!createDrmAllocation(&getDrm(allocationData.rootDeviceIndex), allocation.get(), 0u, maxOsContextCount, MemoryConstants::pageSize64k)) {
        for (auto handleId = 0u; handleId < allocationData.storageInfo.getNumBanks(); handleId++) {
            delete allocation->getGmm(handleId);
        }
        status = AllocationStatus::Error;
        return nullptr;
    }
    if (!allocation->setCacheRegion(&getDrm(allocationData.rootDeviceIndex), static_cast<CacheRegion>(allocationData.cacheRegion))) {
        for (auto bo : drmAllocation->getBOs()) {
            delete bo;
        }
        for (auto handleId = 0u; handleId < allocationData.storageInfo.getNumBanks(); handleId++) {
            delete allocation->getGmm(handleId);
        }
        status = AllocationStatus::Error;
        return nullptr;
    }

    status = AllocationStatus::Success;
    return allocation.release();
}

GraphicsAllocation *DrmMemoryManager::allocatePhysicalHostMemory(const AllocationData &allocationData, AllocationStatus &status) {
    const size_t alignedSize = alignUp<size_t>(allocationData.size, MemoryConstants::pageSize);

    auto gmm = makeGmmIfSingleHandle(allocationData, alignedSize);
    std::unique_ptr<BufferObject, BufferObject::Deleter> bo(this->createBufferObjectInMemoryRegion(allocationData.rootDeviceIndex, gmm.get(),
                                                                                                   allocationData.type, 0u, alignedSize,
                                                                                                   0u, maxOsContextCount, -1, true, true));
    if (!bo) {
        return nullptr;
    }

    uint64_t offset = 0;
    auto ioctlHelper = this->getDrm(allocationData.rootDeviceIndex).getIoctlHelper();
    uint64_t mmapOffsetWb = ioctlHelper->getDrmParamValue(DrmParam::mmapOffsetWb);
    if (!ioctlHelper->retrieveMmapOffsetForBufferObject(*bo, mmapOffsetWb, offset)) {
        return nullptr;
    }
    bo->setMmapOffset(offset);

    auto allocation = std::make_unique<DrmAllocation>(allocationData.rootDeviceIndex, 1u, allocationData.type, bo.get(),
                                                      nullptr, 0u, alignedSize, MemoryPool::system4KBPages);
    allocation->setMmapPtr(nullptr);
    allocation->setMmapSize(0u);

    registerSharedBoHandleAllocation(allocation.get());

    bo.release();
    allocation->setDefaultGmm(gmm.release());
    allocation->setUsmHostAllocation(true);
    allocation->setShareableHostMemory(true);
    allocation->storageInfo = allocationData.storageInfo;
    status = AllocationStatus::Success;
    return allocation.release();
}

GraphicsAllocation *DrmMemoryManager::allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) {
    status = AllocationStatus::RetryInNonDevicePool;
    if (!this->localMemorySupported[allocationData.rootDeviceIndex] ||
        allocationData.flags.useSystemMemory ||
        (allocationData.flags.allow32Bit && this->force32bitAllocations) ||
        allocationData.type == AllocationType::sharedResourceCopy) {
        return nullptr;
    }

    if (allocationData.type == AllocationType::unifiedSharedMemory) {
        auto allocation = this->createSharedUnifiedMemoryAllocation(allocationData);
        status = allocation ? AllocationStatus::Success : AllocationStatus::Error;
        return allocation;
    }

    auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHardwareInfo();

    std::unique_ptr<Gmm> gmm;
    auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
    auto &productHelper = gmmHelper->getRootDeviceEnvironment().getHelper<ProductHelper>();

    size_t baseSize = 0u;
    if (allocationData.type == AllocationType::image) {
        allocationData.imgInfo->useLocalMemory = true;
        gmm = std::make_unique<Gmm>(gmmHelper, *allocationData.imgInfo,
                                    allocationData.storageInfo, allocationData.flags.preferCompressed);

        baseSize = allocationData.imgInfo->size;
    } else if (allocationData.type == AllocationType::writeCombined) {
        baseSize = alignUp(allocationData.size + MemoryConstants::pageSize64k, 2 * MemoryConstants::megaByte) + 2 * MemoryConstants::megaByte;
    } else {
        baseSize = allocationData.size;
    }

    size_t finalAlignment = MemoryConstants::pageSize64k;
    if (debugManager.flags.ExperimentalAlignLocalMemorySizeTo2MB.get()) {
        finalAlignment = MemoryConstants::pageSize2M;
    } else if (productHelper.is2MBLocalMemAlignmentEnabled()) {
        if (baseSize >= MemoryConstants::pageSize2M ||
            GraphicsAllocation::is2MBPageAllocationType(allocationData.type)) {
            finalAlignment = MemoryConstants::pageSize2M;
        }
    }

    size_t sizeAligned = alignUp(baseSize, finalAlignment);

    if (allocationData.type != AllocationType::image) {
        gmm = this->makeGmmIfSingleHandle(allocationData, sizeAligned);
    }

    auto sizeAllocated = sizeAligned;
    auto gfxPartition = getGfxPartition(allocationData.rootDeviceIndex);
    uint64_t gpuAddress = 0lu;

    status = getGpuAddress(this->alignmentSelector, *this->heapAssigners[allocationData.rootDeviceIndex], *hwInfo, gfxPartition, allocationData, sizeAllocated, gmmHelper, gpuAddress);
    if (status == AllocationStatus::Error) {
        return nullptr;
    }

    auto allocation = makeDrmAllocation(allocationData, std::move(gmm), gpuAddress, sizeAligned);
    allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuAddress), sizeAllocated);
    auto *drmAllocation = static_cast<DrmAllocation *>(allocation.get());
    auto *graphicsAllocation = static_cast<GraphicsAllocation *>(allocation.get());

    if (!createDrmAllocation(&getDrm(allocationData.rootDeviceIndex), allocation.get(), gpuAddress, maxOsContextCount, finalAlignment)) {
        for (auto handleId = 0u; handleId < allocationData.storageInfo.getNumBanks(); handleId++) {
            delete allocation->getGmm(handleId);
        }
        gfxPartition->freeGpuAddressRange(gmmHelper->decanonize(gpuAddress), sizeAllocated);
        status = AllocationStatus::Error;
        return nullptr;
    }
    if (allocationData.type == AllocationType::writeCombined) {
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
    if (heapAssigners[allocationData.rootDeviceIndex]->useInternal32BitHeap(allocationData.type)) {
        allocation->setGpuBaseAddress(gmmHelper->canonize(getInternalHeapBaseAddress(allocationData.rootDeviceIndex, true)));
    } else if (heapAssigners[allocationData.rootDeviceIndex]->useExternal32BitHeap(allocationData.type)) {
        allocation->setGpuBaseAddress(gmmHelper->canonize(getExternalHeapBaseAddress(allocationData.rootDeviceIndex, true)));
    }

    if (!allocation->setCacheRegion(&getDrm(allocationData.rootDeviceIndex), static_cast<CacheRegion>(allocationData.cacheRegion))) {
        cleanupBeforeReturn(allocationData, gfxPartition, drmAllocation, graphicsAllocation, gpuAddress, sizeAllocated);
        status = AllocationStatus::Error;
        return nullptr;
    }

    status = AllocationStatus::Success;
    return allocation.release();
}

BufferObject *DrmMemoryManager::createBufferObjectInMemoryRegion(uint32_t rootDeviceIndex, Gmm *gmm, AllocationType allocationType, uint64_t gpuAddress,
                                                                 size_t size, DeviceBitfield memoryBanks, size_t maxOsContextCount, int32_t pairHandle, bool isSystemMemoryPool, bool isUsmHostAllocation) {
    auto drm = &getDrm(rootDeviceIndex);
    auto memoryInfo = drm->getMemoryInfo();
    if (!memoryInfo) {
        return nullptr;
    }

    uint32_t handle = 0;
    int ret = 0;

    auto patIndex = drm->getPatIndex(gmm, allocationType, CacheRegion::defaultRegion, CachePolicy::writeBack, false, isSystemMemoryPool);

    if (memoryBanks.count() > 1) {
        ret = memoryInfo->createGemExtWithMultipleRegions(memoryBanks, size, handle, patIndex, isUsmHostAllocation);
    } else {
        ret = memoryInfo->createGemExtWithSingleRegion(memoryBanks, size, handle, patIndex, pairHandle, isUsmHostAllocation);
    }

    if (ret != 0) {
        return nullptr;
    }

    auto bo = new (std::nothrow) BufferObject(rootDeviceIndex, drm, patIndex, handle, size, maxOsContextCount);
    if (!bo) {
        return nullptr;
    }
    auto &productHelper = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHelper<ProductHelper>();
    bo->setBOType(getBOTypeFromPatIndex(patIndex, productHelper.isVmBindPatIndexProgrammingSupported()));
    bo->setAddress(gpuAddress);

    return bo;
}

size_t DrmMemoryManager::getSizeOfChunk(size_t allocSize) {

    size_t chunkSize = MemoryConstants::chunkThreshold;
    size_t chunkMask = (~(MemoryConstants::chunkThreshold - 1));
    size_t numChunk = debugManager.flags.NumberOfBOChunks.get();
    size_t alignSize = alignUp(allocSize, MemoryConstants::pageSize64k);
    if (debugManager.flags.SetBOChunkingSize.get() != -1) {
        chunkSize = debugManager.flags.SetBOChunkingSize.get() & chunkMask;
        if (chunkSize == 0) {
            chunkSize = MemoryConstants::chunkThreshold;
        }
        numChunk = alignSize / chunkSize;
        if (numChunk < 2) {
            numChunk = 2;
        }
    }
    if (numChunk > 1) {
        chunkSize = (alignSize / numChunk) & chunkMask;
        if (chunkSize == 0) {
            chunkSize = MemoryConstants::chunkThreshold;
        }
        while ((alignSize % chunkSize) && ((alignSize / chunkSize) > 1)) {
            chunkSize += MemoryConstants::chunkThreshold;
        }
        while ((alignSize % chunkSize) && (chunkSize >= (2 * MemoryConstants::chunkThreshold))) {
            chunkSize -= MemoryConstants::chunkThreshold;
        }
    }
    return chunkSize;
}

bool DrmMemoryManager::checkAllocationForChunking(size_t allocSize, size_t minSize, bool subDeviceEnabled, bool debugDisabled, bool modeEnabled, bool bufferEnabled) {
    return ((allocSize >= minSize) && (allocSize >= (2 * MemoryConstants::chunkThreshold)) && ((allocSize % MemoryConstants::chunkThreshold) == 0) &&
            (((allocSize / MemoryConstants::chunkThreshold) % 2) == 0) && subDeviceEnabled && debugDisabled && modeEnabled && bufferEnabled);
}

void DrmMemoryManager::checkUnexpectedGpuPageFault() {
    for (auto &engineContainer : allRegisteredEngines) {
        for (auto &engine : engineContainer) {
            CommandStreamReceiver *csr = engine.commandStreamReceiver;
            Drm &drm = getDrm(csr->getRootDeviceIndex());
            drm.checkResetStatus(*engine.osContext);
        }
    }
}

bool DrmMemoryManager::createDrmChunkedAllocation(Drm *drm, DrmAllocation *allocation, uint64_t boAddress, size_t boSize, size_t maxOsContextCount) {
    auto &storageInfo = allocation->storageInfo;
    auto memoryInfo = drm->getMemoryInfo();
    uint32_t handle = 0;
    auto alignSize = alignUp(boSize, MemoryConstants::pageSize64k);
    uint32_t numOfChunks = static_cast<uint32_t>(alignSize / getSizeOfChunk(alignSize));

    auto gmm = allocation->getGmm(0u);
    auto patIndex = drm->getPatIndex(gmm, allocation->getAllocationType(), CacheRegion::defaultRegion, CachePolicy::writeBack, false, !allocation->isAllocatedInLocalMemoryPool());
    int ret = memoryInfo->createGemExtWithMultipleRegions(storageInfo.memoryBanks, boSize, handle, patIndex, -1, true, numOfChunks, allocation->isUsmHostAllocation());
    if (ret != 0) {
        return false;
    }

    auto bo = new (std::nothrow) BufferObject(allocation->getRootDeviceIndex(), drm, patIndex, handle, boSize, maxOsContextCount);
    UNRECOVERABLE_IF(bo == nullptr);
    auto &productHelper = executionEnvironment.rootDeviceEnvironments[allocation->getRootDeviceIndex()]->getHelper<ProductHelper>();
    bo->setBOType(getBOTypeFromPatIndex(patIndex, productHelper.isVmBindPatIndexProgrammingSupported()));
    bo->setAddress(boAddress);

    allocation->getBufferObjectToModify(0) = bo;

    bo->setChunked(true);

    storageInfo.isChunked = true;
    storageInfo.numOfChunks = numOfChunks;

    return true;
}

bool DrmMemoryManager::createDrmAllocation(Drm *drm, DrmAllocation *allocation, uint64_t gpuAddress, size_t maxOsContextCount, size_t preferredAlignment) {
    BufferObjects bos{};
    auto &storageInfo = allocation->storageInfo;
    auto boAddress = gpuAddress;
    auto currentBank = 0u;
    auto iterationOffset = 0u;
    auto banksCnt = storageInfo.getTotalBanksCnt();
    auto useKmdMigrationForBuffers = (AllocationType::buffer == allocation->getAllocationType() && (debugManager.flags.UseKmdMigrationForBuffers.get() > 0));

    auto handles = storageInfo.getNumBanks();
    size_t boTotalChunkSize = 0;

    if (checkAllocationForChunking(allocation->getUnderlyingBufferSize(), drm->getMinimalSizeForChunking(),
                                   (allocation->storageInfo.subDeviceBitfield.count() > 1),
                                   (!executionEnvironment.isDebuggingEnabled()), (drm->getChunkingMode() & chunkingModeDevice),
                                   (AllocationType::buffer == allocation->getAllocationType()))) {
        handles = 1;
        allocation->resizeBufferObjects(handles);
        bos.resize(handles);
        boTotalChunkSize = allocation->getUnderlyingBufferSize();
        allocation->setNumHandles(handles);
        return createDrmChunkedAllocation(drm, allocation, gpuAddress, boTotalChunkSize, maxOsContextCount);
    }

    if (storageInfo.colouringPolicy == ColouringPolicy::chunkSizeBased) {
        handles = allocation->getNumGmms();
        allocation->resizeBufferObjects(handles);
        bos.resize(handles);
    }
    allocation->setNumHandles(handles);

    int32_t pairHandle = -1;
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
        auto boSize = alignUp(gmm->gmmResourceInfo->getSizeAllocation(), preferredAlignment);
        bos[handleId] = createBufferObjectInMemoryRegion(allocation->getRootDeviceIndex(), gmm, allocation->getAllocationType(), boAddress, boSize, memoryBanks, maxOsContextCount, pairHandle,
                                                         !allocation->isAllocatedInLocalMemoryPool(), allocation->isUsmHostAllocation());
        if (nullptr == bos[handleId]) {
            return false;
        }
        allocation->getBufferObjectToModify(currentBank + iterationOffset) = bos[handleId];
        if (storageInfo.multiStorage) {
            boAddress += boSize;
        }

        // only support pairing of handles with PRELIM_I915_PARAM_SET_PAIR for implicit scaling scenarios, which
        // have 2 handles
        if (AllocationType::buffer == allocation->getAllocationType() && handles == 2 && drm->getSetPairAvailable()) {
            pairHandle = bos[handleId]->peekHandle();
        }
    }

    if (storageInfo.colouringPolicy == ColouringPolicy::mappingBased) {
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

BufferObject::BOType DrmMemoryManager::getBOTypeFromPatIndex(uint64_t patIndex, bool isPatIndexSupported) const {
    if (!isPatIndexSupported) {
        return BufferObject::BOType::legacy;
    }
    if (patIndex < 3) {
        return BufferObject::BOType::nonCoherent;
    } else {
        return BufferObject::BOType::coherent;
    }
}

bool DrmMemoryManager::allocationTypeForCompletionFence(AllocationType allocationType) {
    int32_t overrideAllowAllAllocations = debugManager.flags.UseDrmCompletionFenceForAllAllocations.get();
    bool allowAllAllocations = overrideAllowAllAllocations == -1 ? false : !!overrideAllowAllAllocations;
    if (allowAllAllocations) {
        return true;
    }
    if (allocationType == AllocationType::commandBuffer ||
        allocationType == AllocationType::deferredTasksList ||
        allocationType == AllocationType::ringBuffer ||
        allocationType == AllocationType::semaphoreBuffer ||
        allocationType == AllocationType::tagBuffer) {
        return true;
    }
    return false;
}

void DrmMemoryManager::waitOnCompletionFence(GraphicsAllocation *allocation) {
    auto allocationType = allocation->getAllocationType();
    if (allocationTypeForCompletionFence(allocationType)) {
        for (auto &engine : getRegisteredEngines(allocation->getRootDeviceIndex())) {
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
                drm.waitOnUserFences(static_cast<OsContextLinux &>(*osContext), completionFenceAddress, allocationTaskCount, csr->getActivePartitions(), -1,
                                     csr->getImmWritePostSyncWriteOffset(), false, NEO::InterruptId::notUsed, nullptr);
            }
        }
    } else {
        waitForEnginesCompletion(*allocation);
    }
}

DrmAllocation *DrmMemoryManager::createAllocWithAlignment(const AllocationData &allocationData, size_t size, size_t alignment, size_t alignedSize, uint64_t gpuAddress) {
    auto &drm = this->getDrm(allocationData.rootDeviceIndex);
    bool useBooMmap = drm.getMemoryInfo() && allocationData.useMmapObject;

    if (debugManager.flags.EnableBOMmapCreate.get() != -1) {
        useBooMmap = debugManager.flags.EnableBOMmapCreate.get();
    }

    if (useBooMmap) {
        const auto memoryPool = MemoryPool::system4KBPages;
        auto ioctlHelper = drm.getIoctlHelper();

        auto totalSizeToAlloc = alignedSize + alignment;
        uint64_t preferredAddress = 0;
        auto gfxPartition = getGfxPartition(allocationData.rootDeviceIndex);
        auto canAllocateInHeapExtended = debugManager.flags.AllocateHostAllocationsInHeapExtendedHost.get();
        if (canAllocateInHeapExtended && allocationData.flags.isUSMHostAllocation && gfxPartition->getHeapLimit(HeapIndex::heapExtendedHost) > 0u) {
            preferredAddress = ioctlHelper->acquireGpuRange(*this, totalSizeToAlloc, allocationData.rootDeviceIndex, allocationData.type, HeapIndex::heapExtendedHost);
        }
        if (0 == preferredAddress) {
            preferredAddress = ioctlHelper->acquireGpuRange(*this, totalSizeToAlloc, allocationData.rootDeviceIndex, allocationData.type, HeapIndex::totalHeaps);
        }

        auto cpuPointer = ioctlHelper->mmapFunction(*this, reinterpret_cast<void *>(preferredAddress), totalSizeToAlloc, PROT_NONE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (castToUint64(cpuPointer) != preferredAddress) {
            ioctlHelper->releaseGpuRange(*this, reinterpret_cast<void *>(preferredAddress), totalSizeToAlloc, allocationData.rootDeviceIndex, allocationData.type);
            preferredAddress = 0;
        }

        auto cpuBasePointer = cpuPointer;
        cpuPointer = alignUp(cpuPointer, alignment);

        auto pointerDiff = ptrDiff(cpuPointer, cpuBasePointer);
        auto gmm = makeGmmIfSingleHandle(allocationData, alignedSize);
        std::unique_ptr<BufferObject, BufferObject::Deleter> bo(this->createBufferObjectInMemoryRegion(allocationData.rootDeviceIndex, gmm.get(), allocationData.type,
                                                                                                       reinterpret_cast<uintptr_t>(cpuPointer), alignedSize, 0u, maxOsContextCount, -1,
                                                                                                       MemoryPoolHelper::isSystemMemoryPool(memoryPool), allocationData.flags.isUSMHostAllocation));

        if (!bo) {
            ioctlHelper->releaseGpuRange(*this, reinterpret_cast<void *>(preferredAddress), totalSizeToAlloc, allocationData.rootDeviceIndex, allocationData.type);
            ioctlHelper->munmapFunction(*this, cpuBasePointer, totalSizeToAlloc);
            return nullptr;
        }

        uint64_t offset = 0;
        uint64_t mmapOffsetWb = ioctlHelper->getDrmParamValue(DrmParam::mmapOffsetWb);
        if (!ioctlHelper->retrieveMmapOffsetForBufferObject(*bo, mmapOffsetWb, offset)) {
            ioctlHelper->releaseGpuRange(*this, reinterpret_cast<void *>(preferredAddress), totalSizeToAlloc, allocationData.rootDeviceIndex, allocationData.type);
            ioctlHelper->munmapFunction(*this, cpuPointer, size);
            return nullptr;
        }

        [[maybe_unused]] auto retPtr = ioctlHelper->mmapFunction(*this, cpuPointer, alignedSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, drm.getFileDescriptor(), static_cast<off_t>(offset));
        DEBUG_BREAK_IF(retPtr != cpuPointer);

        obtainGpuAddress(allocationData, bo.get(), gpuAddress);
        emitPinningRequest(bo.get(), allocationData);

        auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
        auto canonizedGpuAddress = gmmHelper->canonize(bo->peekAddress());
        auto allocation = std::make_unique<DrmAllocation>(allocationData.rootDeviceIndex, 1u /*num gmms*/, allocationData.type, bo.get(), cpuPointer, canonizedGpuAddress, alignedSize, memoryPool);
        allocation->setMmapPtr(cpuPointer);
        allocation->setMmapSize(alignedSize);
        if (pointerDiff != 0) {
            ioctlHelper->registerMemoryToUnmap(*allocation.get(), cpuBasePointer, pointerDiff, this->munmapFunction);
        }
        [[maybe_unused]] int retCode = ioctlHelper->munmapFunction(*this, ptrOffset(cpuPointer, alignedSize), alignment - pointerDiff);
        DEBUG_BREAK_IF(retCode != 0);
        if (preferredAddress) {
            allocation->setReservedAddressRange(reinterpret_cast<void *>(preferredAddress), totalSizeToAlloc);
        } else {
            allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuAddress), alignedSize);
        }
        if (!allocation->setCacheRegion(&drm, static_cast<CacheRegion>(allocationData.cacheRegion))) {
            if (pointerDiff == 0) {
                ioctlHelper->registerMemoryToUnmap(*allocation.get(), cpuBasePointer, pointerDiff, this->munmapFunction);
            }
            return nullptr;
        }

        bo.release();
        allocation->setDefaultGmm(gmm.release());
        allocation->setShareableHostMemory(true);
        allocation->storageInfo = allocationData.storageInfo;
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
    auto ioctlHelper = drm->getIoctlHelper();
    uint64_t mmapOffsetWc = ioctlHelper->getDrmParamValue(DrmParam::mmapOffsetWc);
    uint64_t offset = 0;
    if (!ioctlHelper->retrieveMmapOffsetForBufferObject(*bo, mmapOffsetWc, offset)) {
        return nullptr;
    }

    auto addr = mmapFunction(nullptr, bo->peekSize(), PROT_WRITE | PROT_READ, MAP_SHARED, drm->getFileDescriptor(), static_cast<off_t>(offset));
    DEBUG_BREAK_IF(addr == MAP_FAILED);

    if (addr == MAP_FAILED) {
        PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "mmap return of MAP_FAILED\n");
        return nullptr;
    }

    bo->setLockedAddress(addr);

    return bo->peekLockedAddress();
}

MemRegionsVec createMemoryRegionsForSharedAllocation(const HardwareInfo &hwInfo, MemoryInfo &memoryInfo, const AllocationData &allocationData, DeviceBitfield memoryBanks) {
    MemRegionsVec memRegions;

    if (allocationData.usmInitialPlacement == GraphicsAllocation::UsmInitialPlacement::CPU) {
        // System memory region
        auto regionClassAndInstance = memoryInfo.getMemoryRegionClassAndInstance(0u, hwInfo);
        memRegions.push_back(regionClassAndInstance);
    }

    // All local memory regions
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
        // System memory region
        auto regionClassAndInstance = memoryInfo.getMemoryRegionClassAndInstance(0u, hwInfo);
        memRegions.push_back(regionClassAndInstance);
    }

    return memRegions;
}

GraphicsAllocation *DrmMemoryManager::createSharedUnifiedMemoryAllocation(const AllocationData &allocationData) {
    auto &drm = this->getDrm(allocationData.rootDeviceIndex);
    auto ioctlHelper = drm.getIoctlHelper();

    const auto vmAdviseAttribute = ioctlHelper->getVmAdviseAtomicAttribute();
    if (vmAdviseAttribute.has_value() && vmAdviseAttribute.value() == 0) {
        return nullptr;
    }

    auto memoryInfo = drm.getMemoryInfo();
    const bool useBooMmap = memoryInfo && allocationData.useMmapObject;

    if (!useBooMmap) {
        return nullptr;
    }

    auto size = allocationData.size;
    auto alignment = allocationData.alignment;

    auto totalSizeToAlloc = size + alignment;

    uint64_t preferredAddress = 0;
    auto gfxPartition = getGfxPartition(allocationData.rootDeviceIndex);
    auto canAllocateInHeapExtended = debugManager.flags.AllocateSharedAllocationsInHeapExtendedHost.get();
    if (canAllocateInHeapExtended && gfxPartition->getHeapLimit(HeapIndex::heapExtendedHost) > 0u && !allocationData.flags.resource48Bit) {
        preferredAddress = ioctlHelper->acquireGpuRange(*this, totalSizeToAlloc, allocationData.rootDeviceIndex, allocationData.type, HeapIndex::heapExtendedHost);
    }
    if (0 == preferredAddress) {
        preferredAddress = ioctlHelper->acquireGpuRange(*this, totalSizeToAlloc, allocationData.rootDeviceIndex, allocationData.type, HeapIndex::totalHeaps);
    }

    auto cpuPointer = ioctlHelper->mmapFunction(*this, reinterpret_cast<void *>(preferredAddress), totalSizeToAlloc, PROT_NONE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (castToUint64(cpuPointer) != preferredAddress) {
        ioctlHelper->releaseGpuRange(*this, reinterpret_cast<void *>(preferredAddress), totalSizeToAlloc, allocationData.rootDeviceIndex, allocationData.type);
        preferredAddress = 0;
    }

    if (cpuPointer == MAP_FAILED) {
        PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "mmap return of MAP_FAILED\n");
        return nullptr;
    }

    auto cpuBasePointer = cpuPointer;
    cpuPointer = alignUp(cpuPointer, alignment);

    auto pHwInfo = drm.getRootDeviceEnvironment().getHardwareInfo();

    BufferObjects bos{};
    auto currentAddress = cpuPointer;
    auto remainingSize = size;
    auto alignSize = alignUp(remainingSize, MemoryConstants::pageSize64k);
    auto remainingMemoryBanks = allocationData.storageInfo.memoryBanks;
    auto numHandles = GraphicsAllocation::getNumHandlesForKmdSharedAllocation(allocationData.storageInfo.getNumBanks());

    bool useChunking = false;
    uint32_t numOfChunks = 0;

    if (checkAllocationForChunking(alignSize, drm.getMinimalSizeForChunking(),
                                   true, (!executionEnvironment.isDebuggingEnabled()),
                                   (drm.getChunkingMode() & chunkingModeShared), true)) {
        numHandles = 1;
        useChunking = true;
        numOfChunks = static_cast<uint32_t>(alignSize / getSizeOfChunk(alignSize));
    }

    const auto memoryPool = MemoryPool::localMemory;
    auto &productHelper = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHelper<ProductHelper>();
    for (auto handleId = 0u; handleId < numHandles; handleId++) {
        uint32_t handle = 0;

        auto currentSize = alignUp(remainingSize / (numHandles - handleId), MemoryConstants::pageSize64k);
        if (currentSize == 0) {
            break;
        }

        auto memoryInstance = Math::getMinLsbSet(static_cast<uint32_t>(remainingMemoryBanks.to_ulong()));
        auto memoryBanks = (debugManager.flags.KMDSupportForCrossTileMigrationPolicy.get() > 0 || useChunking) ? allocationData.storageInfo.memoryBanks : DeviceBitfield(1 << memoryInstance);
        auto memRegions = createMemoryRegionsForSharedAllocation(*pHwInfo, *memoryInfo, allocationData, memoryBanks);

        auto patIndex = drm.getPatIndex(nullptr, allocationData.type, CacheRegion::defaultRegion, CachePolicy::writeBack, false, MemoryPoolHelper::isSystemMemoryPool(memoryPool));

        int ret = memoryInfo->createGemExt(memRegions, currentSize, handle, patIndex, {}, -1, useChunking, numOfChunks, allocationData.flags.isUSMHostAllocation);

        if (ret) {
            ioctlHelper->munmapFunction(*this, cpuPointer, totalSizeToAlloc);
            ioctlHelper->releaseGpuRange(*this, reinterpret_cast<void *>(preferredAddress), totalSizeToAlloc, allocationData.rootDeviceIndex, allocationData.type);
            return nullptr;
        }

        std::unique_ptr<BufferObject, BufferObject::Deleter> bo(new BufferObject(allocationData.rootDeviceIndex, &drm, patIndex, handle, currentSize, maxOsContextCount));

        if (vmAdviseAttribute.has_value() && !ioctlHelper->setVmBoAdvise(bo->peekHandle(), vmAdviseAttribute.value(), nullptr)) {
            ioctlHelper->munmapFunction(*this, cpuBasePointer, totalSizeToAlloc);
            ioctlHelper->releaseGpuRange(*this, reinterpret_cast<void *>(preferredAddress), totalSizeToAlloc, allocationData.rootDeviceIndex, allocationData.type);
            return nullptr;
        }

        uint64_t mmapOffsetWb = ioctlHelper->getDrmParamValue(DrmParam::mmapOffsetWb);
        uint64_t offset = 0;
        bo->setBOType(getBOTypeFromPatIndex(patIndex, productHelper.isVmBindPatIndexProgrammingSupported()));
        if (!ioctlHelper->retrieveMmapOffsetForBufferObject(*bo, mmapOffsetWb, offset)) {
            ioctlHelper->munmapFunction(*this, cpuBasePointer, totalSizeToAlloc);
            ioctlHelper->releaseGpuRange(*this, reinterpret_cast<void *>(preferredAddress), totalSizeToAlloc, allocationData.rootDeviceIndex, allocationData.type);
            return nullptr;
        }

        ioctlHelper->mmapFunction(*this, currentAddress, currentSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, drm.getFileDescriptor(), static_cast<off_t>(offset));

        bo->setAddress(castToUint64(currentAddress));

        bo->setChunked(useChunking);

        bos.push_back(bo.release());

        currentAddress = reinterpret_cast<void *>(castToUint64(currentAddress) + currentSize);
        remainingSize -= currentSize;
        remainingMemoryBanks.reset(memoryInstance);
    }

    auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
    auto canonizedGpuAddress = gmmHelper->canonize(reinterpret_cast<uintptr_t>(cpuPointer));
    auto allocation = std::make_unique<DrmAllocation>(allocationData.rootDeviceIndex, 1u /*num gmms*/, allocationData.type, bos, cpuPointer, canonizedGpuAddress, size, memoryPool);
    allocation->setMmapPtr(cpuBasePointer);
    allocation->setMmapSize(totalSizeToAlloc);
    allocation->setReservedAddressRange(reinterpret_cast<void *>(preferredAddress), totalSizeToAlloc);
    allocation->setNumHandles(static_cast<uint32_t>(bos.size()));
    allocation->storageInfo = allocationData.storageInfo;
    allocation->storageInfo.isChunked = useChunking;
    allocation->storageInfo.numOfChunks = numOfChunks;
    if (!allocation->setCacheRegion(&drm, static_cast<CacheRegion>(allocationData.cacheRegion))) {
        ioctlHelper->munmapFunction(*this, cpuBasePointer, totalSizeToAlloc);
        for (auto bo : bos) {
            delete bo;
        }
        return nullptr;
    }

    PreferredLocation preferredLocation = PreferredLocation::defaultLocation;
    if (NEO::debugManager.flags.CreateContextWithAccessCounters.get() > 0) {
        preferredLocation = PreferredLocation::none;
    }
    [[maybe_unused]] auto success = allocation->setPreferredLocation(&drm, preferredLocation);
    DEBUG_BREAK_IF(!success);

    if (allocationData.usmInitialPlacement == GraphicsAllocation::UsmInitialPlacement::GPU) {
        auto getSubDeviceIds = [](const DeviceBitfield &subDeviceBitfield) {
            SubDeviceIdsVec subDeviceIds;
            for (auto subDeviceId = 0u; subDeviceId < subDeviceBitfield.size(); subDeviceId++) {
                if (subDeviceBitfield.test(subDeviceId)) {
                    subDeviceIds.push_back(subDeviceId);
                }
            }
            return subDeviceIds;
        };
        auto subDeviceIds = getSubDeviceIds(allocationData.storageInfo.subDeviceBitfield);
        success = setMemPrefetch(allocation.get(), subDeviceIds, allocationData.rootDeviceIndex);
        DEBUG_BREAK_IF(!success);
    }

    return allocation.release();
}

DrmAllocation *DrmMemoryManager::createUSMHostAllocationFromSharedHandle(osHandle handle,
                                                                         const AllocationProperties &properties,
                                                                         void *mappedPtr,
                                                                         bool reuseSharedAllocation) {
    PrimeHandle openFd{};
    openFd.fileDescriptor = handle;

    auto memoryPool = MemoryPool::systemCpuInaccessible;

    auto &drm = this->getDrm(properties.rootDeviceIndex);
    auto patIndex = drm.getPatIndex(nullptr, properties.allocationType, CacheRegion::defaultRegion, CachePolicy::writeBack, false, MemoryPoolHelper::isSystemMemoryPool(memoryPool));
    auto ioctlHelper = drm.getIoctlHelper();

    auto ret = ioctlHelper->ioctl(DrmIoctl::primeFdToHandle, &openFd);
    if (ret != 0) {
        int err = drm.getErrno();

        CREATE_DEBUG_STRING(str, "ioctl(PRIME_FD_TO_HANDLE) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
        drm.getRootDeviceEnvironment().executionEnvironment.setErrorDescription(std::string(str.get()));
        PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, str.get());
        DEBUG_BREAK_IF(true);

        return nullptr;
    }

    if (mappedPtr) {
        auto bo = new BufferObject(properties.rootDeviceIndex, &drm, patIndex, openFd.handle, properties.size, maxOsContextCount);
        bo->setAddress(properties.gpuAddress);

        auto gmmHelper = getGmmHelper(properties.rootDeviceIndex);
        auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(reinterpret_cast<void *>(bo->peekAddress())));
        auto allocation = new DrmAllocation(properties.rootDeviceIndex, 1u /*num gmms*/, properties.allocationType, bo, reinterpret_cast<void *>(bo->peekAddress()), bo->peekSize(),
                                            handle, memoryPool, canonizedGpuAddress);
        allocation->setImportedMmapPtr(mappedPtr);
        return allocation;
    }

    auto boHandle = static_cast<int>(openFd.handle);
    auto boHandleWrapper = reuseSharedAllocation ? BufferObjectHandleWrapper{boHandle, properties.rootDeviceIndex} : tryToGetBoHandleWrapperWithSharedOwnership(boHandle, properties.rootDeviceIndex);

    const bool useBooMmap = drm.getMemoryInfo() && properties.useMmapObject;
    if (!useBooMmap) {
        auto bo = new BufferObject(properties.rootDeviceIndex, &drm, patIndex, std::move(boHandleWrapper), properties.size, maxOsContextCount);
        bo->setAddress(properties.gpuAddress);

        auto gmmHelper = getGmmHelper(properties.rootDeviceIndex);
        auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(reinterpret_cast<void *>(bo->peekAddress())));
        auto drmAllocation = std::make_unique<DrmAllocation>(properties.rootDeviceIndex,
                                                             1u /*num gmms*/,
                                                             properties.allocationType,
                                                             bo,
                                                             reinterpret_cast<void *>(bo->peekAddress()),
                                                             bo->peekSize(),
                                                             handle,
                                                             memoryPool,
                                                             canonizedGpuAddress);
        if (!reuseSharedAllocation) {
            registerSharedBoHandleAllocation(drmAllocation.get());
        }
        return drmAllocation.release();
    }

    BufferObject *bo = nullptr;
    if (reuseSharedAllocation) {
        bo = findAndReferenceSharedBufferObject(boHandle, properties.rootDeviceIndex);
    }

    if (bo == nullptr) {
        void *cpuPointer = nullptr;
        size_t size = SysCalls::lseek(handle, 0, SEEK_END);
        UNRECOVERABLE_IF(size == std::numeric_limits<size_t>::max());

        memoryPool = MemoryPool::system4KBPages;
        patIndex = drm.getPatIndex(nullptr, properties.allocationType, CacheRegion::defaultRegion, CachePolicy::writeBack, false, MemoryPoolHelper::isSystemMemoryPool(memoryPool));
        bo = new BufferObject(properties.rootDeviceIndex, &drm, patIndex, std::move(boHandleWrapper), size, maxOsContextCount);

        if (properties.allocationType == AllocationType::gpuTimestampDeviceBuffer) {
            cpuPointer = this->mmapFunction(0, size + MemoryConstants::pageSize64k, PROT_NONE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
            auto alignedAddr = alignUp(cpuPointer, MemoryConstants::pageSize64k);
            auto notUsedSize = ptrDiff(alignedAddr, cpuPointer);
            // call unmap to free the unaligned pages preceding the system allocation and
            // adjust the pointer to the correct aligned Address.
            munmapFunction(cpuPointer, notUsedSize);
            cpuPointer = alignedAddr;
        } else {
            cpuPointer = this->mmapFunction(0, size, PROT_NONE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        }

        if (cpuPointer == MAP_FAILED) {
            PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "mmap return of MAP_FAILED\n");
            delete bo;
            return nullptr;
        }

        bo->setAddress(reinterpret_cast<uintptr_t>(cpuPointer));

        uint64_t mmapOffsetWb = ioctlHelper->getDrmParamValue(DrmParam::mmapOffsetWb);
        uint64_t offset = 0;
        if (!ioctlHelper->retrieveMmapOffsetForBufferObject(*bo, mmapOffsetWb, offset)) {
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

        printDebugString(debugManager.flags.PrintBOCreateDestroyResult.get(), stdout,
                         "Created BO-%d range: %llx - %llx, size: %lld from PRIME_FD_TO_HANDLE\n",
                         bo->peekHandle(),
                         bo->peekAddress(),
                         ptrOffset(bo->peekAddress(), bo->peekSize()),
                         bo->peekSize());

        pushSharedBufferObject(bo);

        auto drmAllocation = std::make_unique<DrmAllocation>(properties.rootDeviceIndex, 1u /*num gmms*/, properties.allocationType, bo, cpuPointer, bo->peekAddress(), bo->peekSize(), memoryPool);
        drmAllocation->setMmapPtr(cpuPointer);
        drmAllocation->setMmapSize(size);
        drmAllocation->setReservedAddressRange(reinterpret_cast<void *>(cpuPointer), size);
        if (!drmAllocation->setCacheRegion(&drm, static_cast<CacheRegion>(properties.cacheRegion))) {
            this->munmapFunction(cpuPointer, size);
            delete bo;
            return nullptr;
        }

        if (!reuseSharedAllocation) {
            registerSharedBoHandleAllocation(drmAllocation.get());
        }

        this->registerSysMemAlloc(drmAllocation.get());

        return drmAllocation.release();
    }

    auto gmmHelper = getGmmHelper(properties.rootDeviceIndex);
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(reinterpret_cast<void *>(bo->peekAddress())));
    return new DrmAllocation(properties.rootDeviceIndex, 1u /*num gmms*/, properties.allocationType, bo, reinterpret_cast<void *>(bo->peekAddress()), bo->peekSize(),
                             handle, memoryPool, canonizedGpuAddress);
}
bool DrmMemoryManager::allowIndirectAllocationsAsPack(uint32_t rootDeviceIndex) {
    return this->getDrm(rootDeviceIndex).isVmBindAvailable();
}

bool DrmMemoryManager::allocateInterrupt(uint32_t &outHandle, uint32_t rootDeviceIndex) {
    auto &productHelper = getGmmHelper(rootDeviceIndex)->getRootDeviceEnvironment().getHelper<ProductHelper>();
    if (productHelper.isInterruptSupported()) {
        return getDrm(rootDeviceIndex).getIoctlHelper()->allocateInterrupt(outHandle);
    }
    return false;
}

bool DrmMemoryManager::releaseInterrupt(uint32_t outHandle, uint32_t rootDeviceIndex) {
    auto &productHelper = getGmmHelper(rootDeviceIndex)->getRootDeviceEnvironment().getHelper<ProductHelper>();
    if (productHelper.isInterruptSupported()) {
        return getDrm(rootDeviceIndex).getIoctlHelper()->releaseInterrupt(outHandle);
    }
    return false;
}

bool DrmMemoryManager::createMediaContext(uint32_t rootDeviceIndex, void *controlSharedMemoryBuffer, uint32_t controlSharedMemoryBufferSize, void *controlBatchBuffer, uint32_t controlBatchBufferSize, void *&outDoorbell) {
    auto &drm = getDrm(rootDeviceIndex);
    return drm.getIoctlHelper()->createMediaContext(drm.getVirtualMemoryAddressSpace(0), controlSharedMemoryBuffer, controlSharedMemoryBufferSize, controlBatchBuffer, controlBatchBufferSize, outDoorbell);
}

bool DrmMemoryManager::releaseMediaContext(uint32_t rootDeviceIndex, void *doorbellHandle) {
    return getDrm(rootDeviceIndex).getIoctlHelper()->releaseMediaContext(doorbellHandle);
}

uint32_t DrmMemoryManager::getNumMediaDecoders(uint32_t rootDeviceIndex) const {
    return getDrm(rootDeviceIndex).getIoctlHelper()->getNumMediaDecoders();
}

uint32_t DrmMemoryManager::getNumMediaEncoders(uint32_t rootDeviceIndex) const {
    return getDrm(rootDeviceIndex).getIoctlHelper()->getNumMediaEncoders();
}

bool DrmMemoryManager::isCompressionSupportedForShareable(bool isShareable) {
    // Currently KMD does not support compression with allocation sharing
    return !isShareable;
}

void DrmMemoryManager::getExtraDeviceProperties(uint32_t rootDeviceIndex, uint32_t *moduleId, uint16_t *serverType) {
    getDrm(rootDeviceIndex).getIoctlHelper()->queryDeviceParams(moduleId, serverType);
}

bool DrmMemoryManager::reInitDeviceSpecificGfxPartition(uint32_t rootDeviceIndex) {
    if (gfxPartitions.at(rootDeviceIndex) == nullptr) {
        auto gpuAddressSpace = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo()->capabilityTable.gpuAddressSpace;

        gfxPartitions.at(rootDeviceIndex) = std::make_unique<GfxPartition>(reservedCpuAddressRange);

        uint64_t gfxTop{};
        getDrm(rootDeviceIndex).queryGttSize(gfxTop, false);

        if (getGfxPartition(rootDeviceIndex)->init(gpuAddressSpace, getSizeToReserve(), rootDeviceIndex, gfxPartitions.size(), heapAssigners[rootDeviceIndex]->apiAllowExternalHeapForSshAndDsh, DrmMemoryManager::getSystemSharedMemory(rootDeviceIndex), gfxTop)) {
            return true;
        }
    }
    return false;
}

void DrmMemoryManager::releaseDeviceSpecificGfxPartition(uint32_t rootDeviceIndex) {
    gfxPartitions.at(rootDeviceIndex).reset();
}

bool DrmMemoryManager::getLocalOnlyRequired(AllocationType allocationType, const ProductHelper &productHelper, const ReleaseHelper *releaseHelper, bool preferCompressed) const {
    const bool enabledForRelease{!releaseHelper || releaseHelper->isLocalOnlyAllowed()};

    if (preferCompressed || allocationType == AllocationType::buffer || allocationType == AllocationType::svmGpu) {
        return enabledForRelease;
    }

    return false;
}
} // namespace NEO
