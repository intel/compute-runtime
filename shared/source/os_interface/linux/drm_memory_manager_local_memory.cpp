/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"

namespace NEO {

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
    return createAllocWithAlignmentFromUserptr(allocationData, size, alignment, alignedSize, gpuAddress);
}

GraphicsAllocation *DrmMemoryManager::createSharedUnifiedMemoryAllocation(const AllocationData &allocationData) {
    return nullptr;
}

void *DrmMemoryManager::lockResourceInLocalMemoryImpl(BufferObject *bo) {
    return nullptr;
}

} // namespace NEO
