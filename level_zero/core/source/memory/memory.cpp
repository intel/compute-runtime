/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "level_zero/core/source/device/device.h"
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

void *DriverHandleImp::importFdHandle(ze_device_handle_t hDevice, uint64_t handle) {
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

    this->getSvmAllocsManager()->insertSVMAlloc(allocData);

    return reinterpret_cast<void *>(alloc->getGpuAddress());
}

ze_result_t DriverHandleImp::openIpcMemHandle(ze_device_handle_t hDevice, ze_ipc_mem_handle_t pIpcHandle,
                                              ze_ipc_memory_flag_t flags, void **ptr) {
    uint64_t handle = 0u;
    memcpy_s(&handle,
             sizeof(handle),
             reinterpret_cast<void *>(pIpcHandle.data),
             sizeof(handle));

    *ptr = this->importFdHandle(hDevice, handle);
    if (nullptr == *ptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DriverHandleImp::closeIpcMemHandle(const void *ptr) {
    return ZE_RESULT_SUCCESS;
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

ze_result_t DriverHandleImp::allocHostMem(const ze_host_mem_alloc_desc_t *hostDesc, size_t size, size_t alignment,
                                          void **ptr) {
    if (size > this->devices[0]->getDeviceInfo().maxMemAllocSize) {
        *ptr = nullptr;
        return ZE_RESULT_ERROR_UNSUPPORTED_SIZE;
    }

    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::HOST_UNIFIED_MEMORY, this->rootDeviceIndices, this->deviceBitfields);

    auto usmPtr = svmAllocsManager->createHostUnifiedMemoryAllocation(size, unifiedMemoryProperties);
    if (usmPtr == nullptr) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    *ptr = usmPtr;

    return ZE_RESULT_SUCCESS;
}

ze_result_t DriverHandleImp::allocDeviceMem(ze_device_handle_t hDevice, const ze_device_mem_alloc_desc_t *deviceDesc,
                                            size_t size, size_t alignment, void **ptr) {

    bool relaxedSizeAllowed = false;
    if (deviceDesc->pNext) {
        const ze_base_desc_t *extendedDesc = reinterpret_cast<const ze_base_desc_t *>(deviceDesc->pNext);
        if (extendedDesc->stype == ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC) {
            const ze_external_memory_export_desc_t *externalMemoryExportDesc =
                reinterpret_cast<const ze_external_memory_export_desc_t *>(extendedDesc);
            // ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF supported by default for all
            // device allocations for the purpose of IPC, so just check correct
            // flag is passed.
            if (externalMemoryExportDesc->flags & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD) {
                return ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
            }
        } else if (extendedDesc->stype == ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD) {
            const ze_external_memory_import_fd_t *externalMemoryImportDesc =
                reinterpret_cast<const ze_external_memory_import_fd_t *>(extendedDesc);
            if (externalMemoryImportDesc->flags & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD) {
                return ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
            }
            *ptr = this->importFdHandle(hDevice, externalMemoryImportDesc->fd);
            if (nullptr == *ptr) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
            return ZE_RESULT_SUCCESS;
        } else if (extendedDesc->stype == ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC) {
            const ze_relaxed_allocation_limits_exp_desc_t *relaxedLimitsDesc =
                reinterpret_cast<const ze_relaxed_allocation_limits_exp_desc_t *>(extendedDesc);
            if (!(relaxedLimitsDesc->flags & ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE)) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
            relaxedSizeAllowed = true;
        }
    }

    if (relaxedSizeAllowed == false &&
        (size > this->devices[0]->getNEODevice()->getHardwareCapabilities().maxMemAllocSize)) {
        *ptr = nullptr;
        return ZE_RESULT_ERROR_UNSUPPORTED_SIZE;
    }

    auto neoDevice = Device::fromHandle(hDevice)->getNEODevice();
    auto rootDeviceIndex = neoDevice->getRootDeviceIndex();
    auto deviceBitfields = this->deviceBitfields;

    deviceBitfields[rootDeviceIndex] = neoDevice->getDeviceBitfield();

    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::DEVICE_UNIFIED_MEMORY, this->rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.allocationFlags.flags.shareable = 1u;
    unifiedMemoryProperties.device = neoDevice;

    if (deviceDesc->flags & ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED) {
        unifiedMemoryProperties.allocationFlags.flags.locallyUncachedResource = 1;
    }

    void *usmPtr =
        svmAllocsManager->createUnifiedMemoryAllocation(size, unifiedMemoryProperties);
    if (usmPtr == nullptr) {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    *ptr = usmPtr;

    return ZE_RESULT_SUCCESS;
}

ze_result_t DriverHandleImp::allocSharedMem(ze_device_handle_t hDevice, const ze_device_mem_alloc_desc_t *deviceDesc,
                                            const ze_host_mem_alloc_desc_t *hostDesc,
                                            size_t size,
                                            size_t alignment,
                                            void **ptr) {
    bool relaxedSizeAllowed = false;
    if (deviceDesc->pNext) {
        const ze_base_desc_t *extendedDesc = reinterpret_cast<const ze_base_desc_t *>(deviceDesc->pNext);
        if (extendedDesc->stype == ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC) {
            const ze_relaxed_allocation_limits_exp_desc_t *relaxedLimitsDesc =
                reinterpret_cast<const ze_relaxed_allocation_limits_exp_desc_t *>(extendedDesc);
            if (!(relaxedLimitsDesc->flags & ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE)) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
            relaxedSizeAllowed = true;
        }
    }

    if (relaxedSizeAllowed == false &&
        (size > this->devices[0]->getNEODevice()->getHardwareCapabilities().maxMemAllocSize)) {
        *ptr = nullptr;
        return ZE_RESULT_ERROR_UNSUPPORTED_SIZE;
    }

    auto neoDevice = this->devices[0]->getNEODevice();

    auto deviceBitfields = this->deviceBitfields;
    NEO::Device *unifiedMemoryPropertiesDevice = nullptr;
    if (hDevice) {
        neoDevice = Device::fromHandle(hDevice)->getNEODevice();
        auto rootDeviceIndex = neoDevice->getRootDeviceIndex();
        unifiedMemoryPropertiesDevice = neoDevice;
        deviceBitfields[rootDeviceIndex] = neoDevice->getDeviceBitfield();
    }

    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::SHARED_UNIFIED_MEMORY, this->rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = unifiedMemoryPropertiesDevice;

    if (deviceDesc->flags & ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED) {
        unifiedMemoryProperties.allocationFlags.flags.locallyUncachedResource = 1;
    }

    auto usmPtr =
        svmAllocsManager->createSharedUnifiedMemoryAllocation(size,
                                                              unifiedMemoryProperties,
                                                              static_cast<void *>(neoDevice->getSpecializedDevice<L0::Device>()));

    if (usmPtr == nullptr) {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    *ptr = usmPtr;

    return ZE_RESULT_SUCCESS;
}

ze_result_t DriverHandleImp::freeMem(const void *ptr) {
    auto allocation = svmAllocsManager->getSVMAlloc(ptr);
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
