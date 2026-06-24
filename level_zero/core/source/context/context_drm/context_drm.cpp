/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/cpuintrinsics.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"

#include <sys/mman.h>

namespace L0 {

uint8_t Context::isOpaqueHandleSupported(IpcHandleType *handleType) {
    return isDrmOpaqueHandleSupported(handleType);
}

bool Context::isIPCHandleSharingSupported() {
    return true;
}

bool Context::isShareableMemory(const void *exportDesc, bool exportableMemory, NEO::Device *neoDevice, bool shareableWithoutNTHandle) {
    if (exportableMemory) {
        return true;
    }

    return false;
}

void Context::closeExternalHandle(uint64_t handle) {
    NEO::SysCalls::close(static_cast<int>(handle));
}

std::pair<NEO::GraphicsAllocation *, void *> Context::getMemHandlePtr(ze_device_handle_t hDevice, uint64_t handle, NEO::AllocationType allocationType, bool isHostIpcAllocation, unsigned int processId, ze_ipc_memory_flags_t flags, uint64_t cacheID, void *reservedHandleData, bool compressedMemory, bool isOpaqueHandle) {
    auto neoDevice = Device::fromHandle(hDevice)->getNEODevice();
    uint64_t effectiveCacheID = cacheID;
    uint64_t importHandle = handle;

    bool opaqueHandlesAttempted = false;
    if (isOpaqueHandle && settings.useOpaqueHandle) {
        // Use helper to import opaque handle with fallback
        auto importResult = importOpaqueHandleWithFallback(handle, processId, cacheID, reservedHandleData, neoDevice);
        if (!importResult.success) {
            return {nullptr, nullptr};
        }
        importHandle = importResult.importHandle;
        opaqueHandlesAttempted = importResult.opaqueHandlesAttempted;
    }

    NEO::GraphicsAllocation *alloc = nullptr;
    NEO::SvmAllocationData allocDataInternal(neoDevice->getRootDeviceIndex());
    auto result = this->driverHandle->importFdHandle(neoDevice, flags, importHandle, allocationType, isHostIpcAllocation, nullptr, &alloc, allocDataInternal, compressedMemory);
    if (opaqueHandlesAttempted && !alloc && reservedHandleData) {
        result = importHandleFromReservedHandleData(reservedHandleData, cacheID, neoDevice, flags, allocationType, isHostIpcAllocation, compressedMemory, importHandle, alloc);
    }

    // Store cacheID in IPC handle tracking if opaque handles are used
    if (result && isOpaqueHandle && settings.useOpaqueHandle && effectiveCacheID != 0) {
        auto lock = driverHandle->lockIPCHandleMap();
        auto &ipcMap = driverHandle->getIPCHandleMap();
        auto ipcIter = ipcMap.find(importHandle);
        if (ipcIter != ipcMap.end()) {
            ipcIter->second->cacheID = effectiveCacheID;
        }
    }

    return {alloc, result};
}

ze_result_t Context::systemBarrier(ze_device_handle_t hDevice) {
    if (hDevice == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto device = Device::fromHandle(hDevice);
    if (!device) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto neoDevice = device->getNEODevice();

    if (!neoDevice->getRootDeviceEnvironment().osInterface) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

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
