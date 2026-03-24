/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/linux/drm_fabric.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_neo.h"

namespace NEO {

void DrmMemoryManager::obtainReservedHandleData(int fd, uint32_t rootDeviceindex, void *reservedHandleData) {
    if (reservedHandleData) {
        auto &drm = getDrm(rootDeviceindex);
        drm.getDrmFabric()->fdToHandle(fd, reservedHandleData);
    }
}

void DrmMemoryManager::closeInternalHandleWithReservedData(uint64_t &handle, uint32_t handleId, GraphicsAllocation *graphicsAllocation, void *reservedHandleData) {
    closeInternalHandle(handle, handleId, graphicsAllocation);
    if (reservedHandleData && graphicsAllocation) {
        auto &drm = getDrm(graphicsAllocation->getRootDeviceIndex());
        int ret = drm.getDrmFabric()->handleClose(reservedHandleData);
        PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "handleClose for reservedHandleData returned %d\n", ret);
        DEBUG_BREAK_IF(ret != 0);
    }
}

int DrmMemoryManager::getImportHandleFromReservedHandleData(void *reservedHandleData, uint32_t rootDeviceIndex) {
    if (!reservedHandleData) {
        return -1;
    }

    auto &drm = getDrm(rootDeviceIndex);
    int32_t dmabufFd = -1;
    int ret = drm.getDrmFabric()->handleToFd(reservedHandleData, dmabufFd);

    if (ret != 0) {
        return -1;
    }

    return dmabufFd;
}

} // namespace NEO
