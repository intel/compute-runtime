/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"

#include <sys/prctl.h>
namespace L0 {

bool ContextImp::isOpaqueHandleSupported(IpcHandleType *handleType) {
    bool useOpaqueHandle = contextSettings.enablePidfdOrSockets;
    *handleType = IpcHandleType::fdHandle;
    if (useOpaqueHandle) {
        if (NEO::SysCalls::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) == -1) {
            PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "prctl Syscall for PR_SET_PTRACER, PR_SET_PTRACER_ANY failed, using fallback mechanism for IPC handle exchange\n");
            return false;
        }
    }
    return useOpaqueHandle;
}

bool ContextImp::isShareableMemory(const void *exportDesc, bool exportableMemory, NEO::Device *neoDevice, bool shareableWithoutNTHandle) {
    if (exportableMemory) {
        return true;
    }

    return false;
}

void *ContextImp::getMemHandlePtr(ze_device_handle_t hDevice, uint64_t handle, NEO::AllocationType allocationType, unsigned int processId, ze_ipc_memory_flags_t flags) {
    auto neoDevice = Device::fromHandle(hDevice)->getNEODevice();
    bool useOpaqueHandle = contextSettings.enablePidfdOrSockets;
    uint64_t importHandle = handle;

    if (useOpaqueHandle) {
        // With pidfd approach extract parent pid and target fd before importing handle
        pid_t exporterPid = static_cast<pid_t>(processId);
        unsigned int flags = 0u;
        int pidfd = NEO::SysCalls::pidfdopen(exporterPid, flags);
        if (pidfd == -1) {
            PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "pidfd_open Syscall failed, using fallback mechanism for IPC handle exchange\n");
        } else {
            unsigned int flags = 0u;
            int newfd = NEO::SysCalls::pidfdgetfd(pidfd, static_cast<int>(handle), flags);
            if (newfd < 0) {
                PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "pidfd_getfd Syscall failed, using fallback mechanism for IPC handle exchange\n");
            } else {
                importHandle = static_cast<uint64_t>(newfd);
            }
        }
    }

    NEO::SvmAllocationData allocDataInternal(neoDevice->getRootDeviceIndex());
    return this->driverHandle->importFdHandle(neoDevice, flags, importHandle, allocationType, nullptr, nullptr, allocDataInternal);
}

void ContextImp::getDataFromIpcHandle(ze_device_handle_t hDevice, const ze_ipc_mem_handle_t ipcHandle, uint64_t &handle, uint8_t &type, unsigned int &processId, uint64_t &poolOffset) {
    bool useOpaqueHandle = contextSettings.enablePidfdOrSockets;

    if (useOpaqueHandle) {
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

} // namespace L0
