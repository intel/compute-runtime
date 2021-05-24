/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"

namespace L0 {

void *DriverHandleImp::importFdHandle(ze_device_handle_t hDevice, ze_ipc_memory_flags_t flags, uint64_t handle, NEO::GraphicsAllocation **pAlloc) {
    auto neoDevice = Device::fromHandle(hDevice)->getNEODevice();
    NEO::osHandle osHandle = static_cast<NEO::osHandle>(handle);
    NEO::AllocationProperties unifiedMemoryProperties{neoDevice->getRootDeviceIndex(),
                                                      MemoryConstants::pageSize,
                                                      NEO::GraphicsAllocation::AllocationType::BUFFER,
                                                      neoDevice->getDeviceBitfield()};
    unifiedMemoryProperties.subDevicesBitfield = neoDevice->getDeviceBitfield();
    NEO::GraphicsAllocation *alloc =
        this->getMemoryManager()->createGraphicsAllocationFromSharedHandle(osHandle,
                                                                           unifiedMemoryProperties,
                                                                           false,
                                                                           false);
    if (alloc == nullptr) {
        return nullptr;
    }

    NEO::SvmAllocationData allocData(neoDevice->getRootDeviceIndex());
    allocData.gpuAllocations.addAllocation(alloc);
    allocData.cpuAllocation = nullptr;
    allocData.size = alloc->getUnderlyingBufferSize();
    allocData.memoryType = InternalMemoryType::DEVICE_UNIFIED_MEMORY;
    allocData.device = neoDevice;
    if (flags & ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED) {
        allocData.allocationFlagsProperty.flags.locallyUncachedResource = 1;
    }

    this->getSvmAllocsManager()->insertSVMAlloc(allocData);

    if (pAlloc) {
        *pAlloc = alloc;
    }

    return reinterpret_cast<void *>(alloc->getGpuAddress());
}

ze_result_t DriverHandleImp::checkMemoryAccessFromDevice(Device *device, const void *ptr) {
    auto allocation = svmAllocsManager->getSVMAlloc(ptr);
    if (allocation == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (allocation->memoryType == InternalMemoryType::HOST_UNIFIED_MEMORY ||
        allocation->memoryType == InternalMemoryType::SHARED_UNIFIED_MEMORY)
        return ZE_RESULT_SUCCESS;

    if (allocation->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex()) != nullptr) {
        return ZE_RESULT_SUCCESS;
    }

    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
}

} // namespace L0
