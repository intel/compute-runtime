/*
 * Copyright (C) 2019-2021 Intel Corporation
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

ze_result_t DriverHandleImp::getIpcMemHandle(const void *ptr, ze_ipc_mem_handle_t *pIpcHandle) {
    NEO::SvmAllocationData *allocData = svmAllocsManager->getSVMAlloc(ptr);
    if (allocData) {
        uint64_t handle = allocData->gpuAllocations.getDefaultGraphicsAllocation()->peekInternalHandle(this->getMemoryManager());
        memcpy_s(reinterpret_cast<void *>(pIpcHandle->data),
                 sizeof(ze_ipc_mem_handle_t),
                 &handle,
                 sizeof(handle));

        return ZE_RESULT_SUCCESS;
    }
    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
}

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

ze_result_t DriverHandleImp::openIpcMemHandle(ze_device_handle_t hDevice, ze_ipc_mem_handle_t pIpcHandle,
                                              ze_ipc_memory_flags_t flags, void **ptr) {
    uint64_t handle = 0u;
    memcpy_s(&handle,
             sizeof(handle),
             reinterpret_cast<void *>(pIpcHandle.data),
             sizeof(handle));

    *ptr = this->importFdHandle(hDevice, flags, handle, nullptr);
    if (nullptr == *ptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DriverHandleImp::closeIpcMemHandle(const void *ptr) {
    return freeMem(ptr);
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

ze_result_t DriverHandleImp::getMemAddressRange(const void *ptr, void **pBase, size_t *pSize) {
    NEO::SvmAllocationData *allocData = svmAllocsManager->getSVMAlloc(ptr);
    if (allocData) {
        NEO::GraphicsAllocation *alloc;
        alloc = allocData->gpuAllocations.getDefaultGraphicsAllocation();
        if (pBase) {
            uint64_t *allocBase = reinterpret_cast<uint64_t *>(pBase);
            *allocBase = alloc->getGpuAddress();
        }

        if (pSize) {
            *pSize = alloc->getUnderlyingBufferSize();
        }

        return ZE_RESULT_SUCCESS;
    }
    DEBUG_BREAK_IF(true);
    return ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t DriverHandleImp::freeMem(const void *ptr) {
    auto allocation = svmAllocsManager->getSVMAlloc(ptr);
    if (allocation == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    for (auto device : this->devices) {
        DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

        std::unique_lock<NEO::SpinLock> lock(deviceImp->peerAllocationsMutex);

        auto iter = deviceImp->peerAllocations.allocations.find(ptr);
        if (iter != deviceImp->peerAllocations.allocations.end()) {
            auto peerAllocData = &iter->second;
            auto peerAlloc = peerAllocData->gpuAllocations.getDefaultGraphicsAllocation();
            auto peerPtr = reinterpret_cast<void *>(peerAlloc->getGpuAddress());
            svmAllocsManager->freeSVMAlloc(peerPtr);
        }
    }

    svmAllocsManager->freeSVMAlloc(const_cast<void *>(ptr));
    if (svmAllocsManager->getSvmMapOperation(ptr)) {
        svmAllocsManager->removeSvmMapOperation(ptr);
    }
    return ZE_RESULT_SUCCESS;
}

} // namespace L0
