/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/heap_assigner.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/memory_info_impl.h"

namespace NEO {

BufferObject *DrmMemoryManager::createBufferObjectInMemoryRegion(Drm *drm,
                                                                 uint64_t gpuAddress,
                                                                 size_t size,
                                                                 uint32_t memoryBanks,
                                                                 size_t maxOsContextCount) {
    auto memoryInfo = static_cast<MemoryInfoImpl *>(drm->getMemoryInfo());
    if (!memoryInfo) {
        return nullptr;
    }
    auto pHwInfo = drm->getRootDeviceEnvironment().getHardwareInfo();
    auto regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(memoryBanks, *pHwInfo);

    drm_i915_gem_memory_class_instance memRegions{};
    memRegions.memory_class = regionClassAndInstance.memory_class;
    memRegions.memory_instance = regionClassAndInstance.memory_instance;

    drm_i915_gem_object_param regionParam{};
    regionParam.size = 1;
    regionParam.data = reinterpret_cast<uintptr_t>(&memRegions);
    regionParam.param = I915_OBJECT_PARAM | I915_PARAM_MEMORY_REGIONS;

    drm_i915_gem_create_ext_setparam setparamRegion{};
    setparamRegion.base.name = I915_GEM_CREATE_EXT_SETPARAM;
    setparamRegion.param = regionParam;

    drm_i915_gem_create_ext createExt{};
    createExt.size = size;
    createExt.extensions = reinterpret_cast<uintptr_t>(&setparamRegion);
    auto ret = drm->ioctl(DRM_IOCTL_I915_GEM_CREATE_EXT, &createExt);
    if (ret != 0) {
        return nullptr;
    }

    auto bo = new (std::nothrow) BufferObject(drm, createExt.handle, size, maxOsContextCount);
    if (!bo) {
        return nullptr;
    }

    bo->setAddress(gpuAddress);

    return bo;
}

GraphicsAllocation *DrmMemoryManager::createSharedUnifiedMemoryAllocation(const AllocationData &allocationData) {
    return nullptr;
}

DrmAllocation *DrmMemoryManager::createUSMHostAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool hasMappedPtr) {
    drm_prime_handle openFd = {0, 0, 0};
    openFd.fd = handle;

    auto ret = this->getDrm(properties.rootDeviceIndex).ioctl(DRM_IOCTL_PRIME_FD_TO_HANDLE, &openFd);

    if (ret != 0) {
        int err = this->getDrm(properties.rootDeviceIndex).getErrno();
        PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(PRIME_FD_TO_HANDLE) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
        DEBUG_BREAK_IF(ret != 0);
        return nullptr;
    }

    auto bo = new BufferObject(&getDrm(properties.rootDeviceIndex), openFd.handle, properties.size, maxOsContextCount);
    bo->setAddress(properties.gpuAddress);

    return new DrmAllocation(properties.rootDeviceIndex, properties.allocationType, bo, reinterpret_cast<void *>(bo->gpuAddress), bo->size,
                             handle, MemoryPool::SystemCpuInaccessible);
}

DrmAllocation *DrmMemoryManager::createAllocWithAlignment(const AllocationData &allocationData, size_t size, size_t alignment, size_t alignedSize, uint64_t gpuAddress) {
    bool useBooMmap = this->getDrm(allocationData.rootDeviceIndex).getMemoryInfo() && allocationData.useMmapObject;

    if (DebugManager.flags.EnableBOMmapCreate.get() != -1) {
        useBooMmap = DebugManager.flags.EnableBOMmapCreate.get();
    }

    if (useBooMmap) {
        auto totalSizeToAlloc = alignedSize + alignment;
        auto cpuPointer = this->mmapFunction(0, totalSizeToAlloc, PROT_NONE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

        auto cpuBasePointer = cpuPointer;
        cpuPointer = alignUp(cpuPointer, alignment);

        auto pointerDiff = ptrDiff(cpuPointer, cpuBasePointer);
        std::unique_ptr<BufferObject, BufferObject::Deleter> bo(this->createBufferObjectInMemoryRegion(&this->getDrm(allocationData.rootDeviceIndex), reinterpret_cast<uintptr_t>(cpuPointer), alignedSize, 0u, maxOsContextCount));

        if (!bo) {
            this->munmapFunction(cpuBasePointer, totalSizeToAlloc);
            return nullptr;
        }

        drm_i915_gem_mmap_offset gemMmap{};
        gemMmap.handle = bo->peekHandle();
        gemMmap.flags = I915_MMAP_OFFSET_WB;

        auto ret = this->getDrm(allocationData.rootDeviceIndex).ioctl(DRM_IOCTL_I915_GEM_MMAP_OFFSET, &gemMmap);
        if (ret != 0) {
            this->munmapFunction(cpuBasePointer, totalSizeToAlloc);
            return nullptr;
        }

        [[maybe_unused]] auto retPtr = this->mmapFunction(cpuPointer, alignedSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, getDrm(allocationData.rootDeviceIndex).getFileDescriptor(), static_cast<off_t>(gemMmap.offset));
        DEBUG_BREAK_IF(retPtr != cpuPointer);

        obtainGpuAddress(allocationData, bo.get(), gpuAddress);
        emitPinningRequest(bo.get(), allocationData);

        auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, allocationData.type, bo.get(), cpuPointer, bo->gpuAddress, alignedSize, MemoryPool::System4KBPages);
        allocation->setMmapPtr(cpuPointer);
        allocation->setMmapSize(alignedSize);
        if (pointerDiff != 0) {
            [[maybe_unused]] auto retCode = this->munmapFunction(cpuBasePointer, pointerDiff);
            DEBUG_BREAK_IF(retCode != 0);
        }
        [[maybe_unused]] auto retCode = this->munmapFunction(ptrOffset(cpuPointer, alignedSize), alignment - pointerDiff);
        DEBUG_BREAK_IF(retCode != 0);
        allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuAddress), alignedSize);

        bo.release();

        return allocation;
    } else {
        return createAllocWithAlignmentFromUserptr(allocationData, size, alignment, alignedSize, gpuAddress);
    }
}

bool DrmMemoryManager::createDrmAllocation(Drm *drm, DrmAllocation *allocation, uint64_t gpuAddress, size_t maxOsContextCount) {
    std::array<std::unique_ptr<BufferObject>, EngineLimits::maxHandleCount> bos{};
    auto &storageInfo = allocation->storageInfo;
    auto boAddress = gpuAddress;
    for (auto handleId = 0u; handleId < storageInfo.getNumBanks(); handleId++) {
        uint32_t memoryBanks = 1u;
        if (storageInfo.getNumBanks() > 1) {
            memoryBanks &= 1u << handleId;
        }
        auto boSize = alignUp(allocation->getGmm(handleId)->gmmResourceInfo->getSizeAllocation(), MemoryConstants::pageSize64k);
        bos[handleId] = std::unique_ptr<BufferObject>(createBufferObjectInMemoryRegion(drm, boAddress, boSize, memoryBanks, maxOsContextCount));
        if (nullptr == bos[handleId]) {
            return false;
        }
        allocation->getBufferObjectToModify(handleId) = bos[handleId].get();
        if (storageInfo.multiStorage) {
            boAddress += boSize;
        }
    }
    for (auto &bo : bos) {
        bo.release();
    }
    return true;
}

void *DrmMemoryManager::lockResourceInLocalMemoryImpl(BufferObject *bo) {
    if (bo == nullptr)
        return nullptr;

    drm_i915_gem_mmap_offset mmapOffset = {};
    mmapOffset.handle = bo->peekHandle();
    mmapOffset.flags = I915_MMAP_OFFSET_WC;
    auto rootDeviceIndex = this->getRootDeviceIndex(bo->drm);

    if (getDrm(rootDeviceIndex).ioctl(DRM_IOCTL_I915_GEM_MMAP_OFFSET, &mmapOffset) != 0) {
        return nullptr;
    }

    auto addr = mmapFunction(nullptr, bo->peekSize(), PROT_WRITE | PROT_READ, MAP_SHARED, getDrm(rootDeviceIndex).getFileDescriptor(), static_cast<off_t>(mmapOffset.offset));
    DEBUG_BREAK_IF(addr == MAP_FAILED);

    bo->setLockedAddress(addr);

    return bo->peekLockedAddress();
}

} // namespace NEO
