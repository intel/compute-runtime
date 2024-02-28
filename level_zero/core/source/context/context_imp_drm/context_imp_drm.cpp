/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

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
    NEO::SvmAllocationData allocDataInternal(neoDevice->getRootDeviceIndex());
    return this->driverHandle->importFdHandle(neoDevice, flags, handle, allocationType, nullptr, nullptr, allocDataInternal);
}

} // namespace L0
