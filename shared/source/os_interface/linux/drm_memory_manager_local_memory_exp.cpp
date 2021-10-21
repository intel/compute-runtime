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

#include "drm_tip.h"

namespace NEO {

bool retrieveMmapOffsetForBufferObject(Drm &drm, BufferObject &bo, uint64_t flags, uint64_t &offset) {
    drm_i915_gem_mmap_offset mmapOffset = {};
    mmapOffset.handle = bo.peekHandle();
    mmapOffset.flags = I915_MMAP_OFFSET_FIXED;

    auto ret = drm.ioctl(DRM_IOCTL_I915_GEM_MMAP_OFFSET, &mmapOffset);
    if (ret != 0) {
        mmapOffset.flags = flags;
        ret = drm.ioctl(DRM_IOCTL_I915_GEM_MMAP_OFFSET, &mmapOffset);
        if (ret != 0) {
            int err = drm.getErrno();
            PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(DRM_IOCTL_I915_GEM_MMAP_OFFSET) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
            DEBUG_BREAK_IF(ret != 0);
            return false;
        }
    }

    offset = mmapOffset.offset;
    return true;
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

    return new DrmAllocation(properties.rootDeviceIndex, properties.allocationType, bo, reinterpret_cast<void *>(bo->peekAddress()), bo->peekSize(),
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

        uint64_t offset = 0;
        if (!retrieveMmapOffsetForBufferObject(this->getDrm(allocationData.rootDeviceIndex), *bo, I915_MMAP_OFFSET_WB, offset)) {
            this->munmapFunction(cpuPointer, size);
            return nullptr;
        }

        [[maybe_unused]] auto retPtr = this->mmapFunction(cpuPointer, alignedSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, getDrm(allocationData.rootDeviceIndex).getFileDescriptor(), static_cast<off_t>(offset));
        DEBUG_BREAK_IF(retPtr != cpuPointer);

        obtainGpuAddress(allocationData, bo.get(), gpuAddress);
        emitPinningRequest(bo.get(), allocationData);

        auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, allocationData.type, bo.get(), cpuPointer, bo->peekAddress(), alignedSize, MemoryPool::System4KBPages);
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

void *DrmMemoryManager::lockResourceInLocalMemoryImpl(BufferObject *bo) {
    if (bo == nullptr)
        return nullptr;

    auto rootDeviceIndex = this->getRootDeviceIndex(bo->peekDrm());

    uint64_t offset = 0;
    if (!retrieveMmapOffsetForBufferObject(this->getDrm(rootDeviceIndex), *bo, I915_MMAP_OFFSET_WC, offset)) {
        return nullptr;
    }

    auto addr = mmapFunction(nullptr, bo->peekSize(), PROT_WRITE | PROT_READ, MAP_SHARED, getDrm(rootDeviceIndex).getFileDescriptor(), static_cast<off_t>(offset));
    DEBUG_BREAK_IF(addr == MAP_FAILED);

    bo->setLockedAddress(addr);

    return bo->peekLockedAddress();
}

} // namespace NEO
