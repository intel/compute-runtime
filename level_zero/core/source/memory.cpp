/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "level_zero/core/source/device.h"
#include "level_zero/core/source/driver_handle_imp.h"

namespace L0 {

ze_result_t DriverHandleImp::getIpcMemHandle(const void *ptr, ze_ipc_mem_handle_t *pIpcHandle) {
    NEO::SvmAllocationData *allocData = svmAllocsManager->getSVMAllocs()->get(ptr);
    if (allocData) {
        uint64_t handle = allocData->gpuAllocation->peekInternalHandle(this->getMemoryManager());
        memcpy_s(reinterpret_cast<void *>(pIpcHandle->data),
                 sizeof(ze_ipc_mem_handle_t),
                 &handle,
                 sizeof(handle));

        return ZE_RESULT_SUCCESS;
    }
    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
}

ze_result_t DriverHandleImp::openIpcMemHandle(ze_device_handle_t hDevice, ze_ipc_mem_handle_t pIpcHandle,
                                              ze_ipc_memory_flag_t flags, void **ptr) {
    uint64_t handle = *(pIpcHandle.data);
    NEO::osHandle osHandle = static_cast<NEO::osHandle>(handle);
    NEO::AllocationProperties unifiedMemoryProperties{Device::fromHandle(hDevice)->getRootDeviceIndex(),
                                                      MemoryConstants::pageSize,
                                                      NEO::GraphicsAllocation::AllocationType::BUFFER};
    NEO::GraphicsAllocation *alloc =
        this->getMemoryManager()->createGraphicsAllocationFromSharedHandle(osHandle,
                                                                           unifiedMemoryProperties,
                                                                           false);
    if (alloc == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    NEO::SvmAllocationData allocData;
    allocData.gpuAllocation = alloc;
    allocData.cpuAllocation = nullptr;
    allocData.size = alloc->getUnderlyingBufferSize();
    allocData.memoryType = InternalMemoryType::DEVICE_UNIFIED_MEMORY;
    allocData.device = Device::fromHandle(hDevice)->getNEODevice();

    this->getSvmAllocsManager()->getSVMAllocs()->insert(allocData);

    *ptr = reinterpret_cast<void *>(alloc->getGpuAddress());

    return ZE_RESULT_SUCCESS;
}

ze_result_t DriverHandleImp::closeIpcMemHandle(const void *ptr) {
    return ZE_RESULT_SUCCESS;
}

ze_result_t DriverHandleImp::checkMemoryAccessFromDevice(Device *device, const void *ptr) {
    auto allocation = svmAllocsManager->getSVMAllocs()->get(ptr);
    if (allocation == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (allocation->memoryType == InternalMemoryType::HOST_UNIFIED_MEMORY ||
        allocation->memoryType == InternalMemoryType::SHARED_UNIFIED_MEMORY)
        return ZE_RESULT_SUCCESS;

    if (allocation->gpuAllocation->getRootDeviceIndex() == device->getRootDeviceIndex())
        return ZE_RESULT_SUCCESS;

    ze_bool_t p2pCapable = true;
    device->canAccessPeer(devices[allocation->gpuAllocation->getRootDeviceIndex()], &p2pCapable);

    return p2pCapable ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_INVALID_ARGUMENT;
}

ze_result_t DriverHandleImp::getMemAddressRange(const void *ptr, void **pBase, size_t *pSize) {
    NEO::SvmAllocationData *allocData = svmAllocsManager->getSVMAllocs()->get(ptr);
    if (allocData) {
        NEO::GraphicsAllocation *alloc;
        alloc = allocData->gpuAllocation;
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

ze_result_t DriverHandleImp::allocHostMem(ze_host_mem_alloc_flag_t flags, size_t size, size_t alignment,
                                          void **ptr) {
    if (size > this->devices[0]->getDeviceInfo().maxMemAllocSize) {
        *ptr = nullptr;
        return ZE_RESULT_ERROR_UNSUPPORTED_SIZE;
    }
    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::HOST_UNIFIED_MEMORY);

    auto usmPtr = svmAllocsManager->createUnifiedMemoryAllocation(0u, size, unifiedMemoryProperties);

    if (usmPtr == nullptr) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    *ptr = usmPtr;

    return ZE_RESULT_SUCCESS;
}

ze_result_t DriverHandleImp::allocDeviceMem(ze_device_handle_t hDevice, ze_device_mem_alloc_flag_t flags,
                                            size_t size, size_t alignment, void **ptr) {
    if (size > this->devices[0]->getDeviceInfo().maxMemAllocSize) {
        *ptr = nullptr;
        return ZE_RESULT_ERROR_UNSUPPORTED_SIZE;
    }
    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::DEVICE_UNIFIED_MEMORY);
    unifiedMemoryProperties.allocationFlags.flags.shareable = 1u;
    void *usmPtr =
        svmAllocsManager->createUnifiedMemoryAllocation(Device::fromHandle(hDevice)->getRootDeviceIndex(),
                                                        size, unifiedMemoryProperties);
    if (usmPtr == nullptr) {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    *ptr = usmPtr;

    return ZE_RESULT_SUCCESS;
}

ze_result_t DriverHandleImp::allocSharedMem(ze_device_handle_t hDevice, ze_device_mem_alloc_flag_t deviceFlags,
                                            ze_host_mem_alloc_flag_t hostFlags, size_t size, size_t alignment,
                                            void **ptr) {
    if (size > this->devices[0]->getDeviceInfo().maxMemAllocSize) {
        *ptr = nullptr;
        return ZE_RESULT_ERROR_UNSUPPORTED_SIZE;
    }
    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::SHARED_UNIFIED_MEMORY);

    auto usmPtr =
        svmAllocsManager->createSharedUnifiedMemoryAllocation(Device::fromHandle(hDevice)->getRootDeviceIndex(),
                                                              size,
                                                              unifiedMemoryProperties,
                                                              static_cast<void *>(L0::Device::fromHandle(hDevice)));

    if (usmPtr == nullptr) {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    *ptr = usmPtr;

    return ZE_RESULT_SUCCESS;
}

ze_result_t DriverHandleImp::freeMem(const void *ptr) {
    auto allocation = svmAllocsManager->getSVMAllocs()->get(ptr);
    if (allocation == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    svmAllocsManager->freeSVMAlloc(const_cast<void *>(ptr));
    if (svmAllocsManager->getSvmMapOperation(ptr)) {
        svmAllocsManager->removeSvmMapOperation(ptr);
    }
    return ZE_RESULT_SUCCESS;
}

} // namespace L0
