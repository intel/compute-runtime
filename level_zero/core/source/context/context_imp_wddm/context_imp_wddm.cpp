/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"

namespace L0 {

bool ContextImp::isOpaqueHandleSupported(IpcHandleType *handleType) {
    *handleType = IpcHandleType::ntHandle;
    return true;
}

bool ContextImp::isShareableMemory(const void *exportDesc, bool exportableMemory, NEO::Device *neoDevice, bool shareableWithoutNTHandle) {
    if (exportableMemory) {
        return true;
    }

    if (shareableWithoutNTHandle) {
        return true;
    }

    return false;
}

void *ContextImp::getMemHandlePtr(ze_device_handle_t hDevice, uint64_t handle, NEO::AllocationType allocationType, unsigned int processId, ze_ipc_memory_flags_t flags) {
    return this->driverHandle->importNTHandle(hDevice, reinterpret_cast<void *>(handle), allocationType, processId);
}

void ContextImp::getDataFromIpcHandle(ze_device_handle_t hDevice, const ze_ipc_mem_handle_t ipcHandle, uint64_t &handle, uint8_t &type, unsigned int &processId, uint64_t &poolOffset) {
    const IpcOpaqueMemoryData *ipcData = reinterpret_cast<const IpcOpaqueMemoryData *>(ipcHandle.data);
    handle = static_cast<uint64_t>(ipcData->handle.reserved);
    type = ipcData->memoryType;
    processId = ipcData->processId;
    poolOffset = ipcData->poolOffset;
}

} // namespace L0
