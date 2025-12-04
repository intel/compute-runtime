/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/utilities/cpuintrinsics.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"

#include <sys/mman.h>
#include <sys/prctl.h>

namespace L0 {

bool ContextImp::isOpaqueHandleSupported(IpcHandleType *handleType) {
    *handleType = IpcHandleType::fdHandle;
    for (auto &device : this->driverHandle->devices) {
        auto &productHelper = device->getNEODevice()->getProductHelper();
        if (!productHelper.isPidFdOrSocketForIpcSupported()) {
            return false;
        }
    }
    if (NEO::SysCalls::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) == -1) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "prctl Syscall for PR_SET_PTRACER, PR_SET_PTRACER_ANY failed, using fallback mechanism for IPC handle exchange\n");
        return false;
    }
    return true;
}

bool ContextImp::isShareableMemory(const void *exportDesc, bool exportableMemory, NEO::Device *neoDevice, bool shareableWithoutNTHandle) {
    if (exportableMemory) {
        return true;
    }

    return false;
}

void *ContextImp::getMemHandlePtr(ze_device_handle_t hDevice, uint64_t handle, NEO::AllocationType allocationType, unsigned int processId, ze_ipc_memory_flags_t flags) {
    auto neoDevice = Device::fromHandle(hDevice)->getNEODevice();
    uint64_t importHandle = handle;

    if (settings.useOpaqueHandle) {
        // With pidfd approach extract parent pid and target fd before importing handle
        pid_t exporterPid = static_cast<pid_t>(processId);
        unsigned int flags = 0u;
        int pidfd = NEO::SysCalls::pidfdopen(exporterPid, flags);
        if (pidfd == -1) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "pidfd_open Syscall failed, using fallback mechanism for IPC handle exchange\n");
        } else {
            unsigned int flags = 0u;
            int newfd = NEO::SysCalls::pidfdgetfd(pidfd, static_cast<int>(handle), flags);
            if (newfd < 0) {
                PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "pidfd_getfd Syscall failed, using fallback mechanism for IPC handle exchange\n");
            } else {
                importHandle = static_cast<uint64_t>(newfd);
            }
        }
    }

    NEO::SvmAllocationData allocDataInternal(neoDevice->getRootDeviceIndex());
    return this->driverHandle->importFdHandle(neoDevice, flags, importHandle, allocationType, nullptr, nullptr, allocDataInternal);
}

void ContextImp::getDataFromIpcHandle(ze_device_handle_t hDevice, const ze_ipc_mem_handle_t ipcHandle, uint64_t &handle, uint8_t &type, unsigned int &processId, uint64_t &poolOffset) {
    if (settings.useOpaqueHandle) {
        const IpcOpaqueMemoryData *ipcData = reinterpret_cast<const IpcOpaqueMemoryData *>(ipcHandle.data);
        handle = static_cast<uint64_t>(ipcData->handle.fd);
        type = ipcData->memoryType;
        processId = ipcData->processId;
        poolOffset = ipcData->poolOffset;
    } else {
        const IpcMemoryData *ipcData = reinterpret_cast<const IpcMemoryData *>(ipcHandle.data);
        handle = ipcData->handle;
        type = ipcData->type;
        poolOffset = ipcData->poolOffset;
    }
}

ze_result_t ContextImp::systemBarrier(ze_device_handle_t hDevice) {
    if (hDevice == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto deviceImp = static_cast<DeviceImp *>(Device::fromHandle(hDevice));
    if (!deviceImp) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto neoDevice = deviceImp->getNEODevice();

    auto *drmMemoryManager = static_cast<NEO::DrmMemoryManager *>(this->driverHandle->getMemoryManager());

    if (neoDevice->getHardwareInfo().capabilityTable.isIntegratedDevice) {
        return ZE_RESULT_SUCCESS;
    }

    void *ptr = drmMemoryManager->getDrm(neoDevice->getRootDeviceIndex()).getIoctlHelper()->pciBarrierMmap();
    if (ptr == MAP_FAILED || ptr == nullptr) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    NEO::CpuIntrinsics::sfence();
    auto pciBarrierPtr = static_cast<uint32_t *>(ptr);
    *pciBarrierPtr = 0u;
    NEO::CpuIntrinsics::sfence();

    NEO::SysCalls::munmap(ptr, MemoryConstants::pageSize);
    return ZE_RESULT_SUCCESS;
}
} // namespace L0
