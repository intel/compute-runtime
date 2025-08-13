/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"

#include <sys/prctl.h>

namespace L0 {

bool ContextImp::isOpaqueHandleSupported(IpcHandleType *handleType) {
    bool useOpaqueHandle = contextSettings.enablePidfdOrSockets;
    NEO::DriverModelType driverModelType = NEO::DriverModelType::unknown;
    auto rootDeviceEnvironment = this->getDriverHandle()->getMemoryManager()->peekExecutionEnvironment().rootDeviceEnvironments[0].get();
    if (rootDeviceEnvironment->osInterface) {
        driverModelType = rootDeviceEnvironment->osInterface->getDriverModel()->getDriverModelType();
    }
    if (driverModelType == NEO::DriverModelType::wddm) {
        *handleType = IpcHandleType::ntHandle;
        useOpaqueHandle = true;
    } else {
        *handleType = IpcHandleType::fdHandle;
        if (useOpaqueHandle) {
            if (NEO::SysCalls::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) == -1) {
                PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "prctl Syscall for PR_SET_PTRACER, PR_SET_PTRACER_ANY failed, using fallback mechanism for IPC handle exchange\n");
                return false;
            }
        }
    }
    return useOpaqueHandle;
}

bool ContextImp::isShareableMemory(const void *exportDesc, bool exportableMemory, NEO::Device *neoDevice, bool shareableWithoutNTHandle) {
    if (exportableMemory) {
        return true;
    }
    if (neoDevice->getRootDeviceEnvironment().osInterface) {
        NEO::DriverModelType driverType = neoDevice->getRootDeviceEnvironment().osInterface->getDriverModel()->getDriverModelType();
        if (!exportDesc && driverType == NEO::DriverModelType::wddm) {
            return true;
        }
    }

    return false;
}

void *ContextImp::getMemHandlePtr(ze_device_handle_t hDevice,
                                  uint64_t handle,
                                  NEO::AllocationType allocationType,
                                  unsigned int processId,
                                  ze_ipc_memory_flags_t flags) {
    L0::Device *device = L0::Device::fromHandle(hDevice);
    auto neoDevice = device->getNEODevice();
    bool useOpaqueHandle = contextSettings.enablePidfdOrSockets;
    NEO::DriverModelType driverType = NEO::DriverModelType::unknown;
    if (neoDevice->getRootDeviceEnvironment().osInterface) {
        driverType = neoDevice->getRootDeviceEnvironment().osInterface->getDriverModel()->getDriverModelType();
    }
    bool isNTHandle = this->getDriverHandle()->getMemoryManager()->isNTHandle(NEO::toOsHandle(reinterpret_cast<void *>(handle)), device->getNEODevice()->getRootDeviceIndex());
    if (isNTHandle) {
        return this->driverHandle->importNTHandle(hDevice,
                                                  reinterpret_cast<void *>(handle),
                                                  allocationType,
                                                  processId);
    } else if (driverType == NEO::DriverModelType::drm) {
        auto neoDevice = Device::fromHandle(hDevice)->getNEODevice();

        NEO::SvmAllocationData allocDataInternal(neoDevice->getRootDeviceIndex());
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

        return this->driverHandle->importFdHandle(neoDevice,
                                                  flags,
                                                  importHandle,
                                                  allocationType,
                                                  nullptr,
                                                  nullptr,
                                                  allocDataInternal);
    } else {
        return nullptr;
    }
}

void ContextImp::getDataFromIpcHandle(ze_device_handle_t hDevice, const ze_ipc_mem_handle_t ipcHandle, uint64_t &handle, uint8_t &type, unsigned int &processId, uint64_t &poolOffset) {
    bool useOpaqueHandle = contextSettings.enablePidfdOrSockets;
    L0::Device *device = L0::Device::fromHandle(hDevice);
    auto neoDevice = device->getNEODevice();
    NEO::DriverModelType driverModelType = NEO::DriverModelType::unknown;
    if (neoDevice->getRootDeviceEnvironment().osInterface) {
        driverModelType = neoDevice->getRootDeviceEnvironment().osInterface->getDriverModel()->getDriverModelType();
    }
    if (driverModelType == NEO::DriverModelType::wddm) {
        useOpaqueHandle = true;
    }

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
