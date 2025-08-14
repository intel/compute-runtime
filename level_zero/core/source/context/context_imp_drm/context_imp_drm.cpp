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
namespace L0 {

bool ContextImp::isShareableMemory(const void *exportDesc, bool exportableMemory, NEO::Device *neoDevice) {
    if (exportableMemory) {
        return true;
    }

    return false;
}

void *ContextImp::getMemHandlePtr(ze_device_handle_t hDevice, uint64_t handle, NEO::AllocationType allocationType, ze_ipc_memory_flags_t flags) {
    auto neoDevice = Device::fromHandle(hDevice)->getNEODevice();
    auto &productHelper = neoDevice->getProductHelper();

    bool pidfdOrSocket = false;
    pidfdOrSocket = productHelper.isPidFdOrSocketForIpcSupported();

    if (pidfdOrSocket) {
        // With pidfd approach extract parent pid and target fd before importing handle
        pid_t exporterPid = 0;
        unsigned int flags = 0u;
        int pidfd = NEO::SysCalls::pidfdopen(exporterPid, flags);
        if (pidfd == -1) {
            PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "pidfd_open Syscall failed, using fallback mechanism for IPC handle exchange\n");
        } else {
            unsigned int flags = 0u;
            int newfd = NEO::SysCalls::pidfdgetfd(pidfd, 0, flags);
            if (newfd < 0) {
                PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "pidfd_getfd Syscall failed, using fallback mechanism for IPC handle exchange\n");
            }
        }
    }

    NEO::SvmAllocationData allocDataInternal(neoDevice->getRootDeviceIndex());
    return this->driverHandle->importFdHandle(neoDevice, flags, handle, allocationType, nullptr, nullptr, allocDataInternal);
}

} // namespace L0
