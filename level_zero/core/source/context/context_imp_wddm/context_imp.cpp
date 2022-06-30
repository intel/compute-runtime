/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/context/context_imp.h"

#include "level_zero/core/source/driver/driver_handle_imp.h"

namespace L0 {

bool ContextImp::isShareableMemory(const void *exportDesc, bool exportableMemory, NEO::Device *neoDevice) {
    if (exportableMemory) {
        return true;
    }

    return false;
}

void *ContextImp::getMemHandlePtr(ze_device_handle_t hDevice, uint64_t handle, ze_ipc_memory_flags_t flags) {
    return this->driverHandle->importNTHandle(hDevice, reinterpret_cast<void *>(handle));
}

} // namespace L0
