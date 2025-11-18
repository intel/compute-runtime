/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/driver/driver_handle.h"

namespace L0 {

uint8_t Context::isOpaqueHandleSupported(IpcHandleType *handleType) {
    *handleType = IpcHandleType::ntHandle;
    return OpaqueHandlingType::nthandle;
}

bool Context::isIPCHandleSharingSupported() {
    if (NEO::debugManager.flags.EnableShareableWithoutNTHandle.get()) {
        return true;
    }
    return false;
}

bool Context::isShareableMemory(const void *exportDesc, bool exportableMemory, NEO::Device *neoDevice, bool shareableWithoutNTHandle) {
    if (exportableMemory) {
        return true;
    }

    if (shareableWithoutNTHandle) {
        return true;
    }

    return false;
}

void Context::closeExternalHandle(uint64_t) {
}

std::pair<NEO::GraphicsAllocation *, void *> Context::getMemHandlePtr(ze_device_handle_t hDevice, uint64_t handle, NEO::AllocationType allocationType, bool isHostIpcAllocation, unsigned int processId, ze_ipc_memory_flags_t flags, uint64_t cacheID, void *reservedHandleData, bool compressedMemory) {
    return this->driverHandle->importNTHandle(hDevice, reinterpret_cast<void *>(handle), allocationType, isHostIpcAllocation, processId, compressedMemory);
}

void Context::getDataFromIpcHandle(ze_device_handle_t hDevice, const ze_ipc_mem_handle_t &ipcHandle, uint64_t &handle, uint8_t &type, unsigned int &processId, uint64_t &poolOffset, uint64_t &cacheID, void *&reservedHandleData, bool &compressedMemory) {
    const IpcOpaqueMemoryData *ipcData = reinterpret_cast<const IpcOpaqueMemoryData *>(ipcHandle.data);
    handle = static_cast<uint64_t>(ipcData->handle.reserved);
    type = ipcData->memoryType;
    processId = ipcData->processId;
    poolOffset = ipcData->poolOffset;
    cacheID = ipcData->computeCacheID();
    compressedMemory = ipcData->compressedMemory;
    reservedHandleData = nullptr;
}

ze_result_t Context::systemBarrier(ze_device_handle_t hDevice) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
} // namespace L0
