/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_memory_manager.h"

namespace NEO {

void DrmMemoryManager::obtainReservedHandleData(int fd, void *reservedHandleData) {
    // Default implementation: No reserved handle data
}

void DrmMemoryManager::closeInternalHandleWithReservedData(uint64_t &handle, uint32_t handleId, GraphicsAllocation *graphicsAllocation, void *reservedHandleData) {
    closeInternalHandle(handle, handleId, graphicsAllocation);
}

} // namespace NEO
