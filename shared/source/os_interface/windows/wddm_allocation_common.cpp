/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm_allocation.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"

namespace NEO {
int WddmAllocation::createInternalHandle(MemoryManager *memoryManager, uint32_t handleId, uint64_t &handle) {
    handle = ntSecureHandle;
    if (handle == 0) {
        HANDLE ntSharedHandle = NULL;
        WddmMemoryManager *wddmMemoryManager = reinterpret_cast<WddmMemoryManager *>(memoryManager);
        auto status = wddmMemoryManager->createInternalNTHandle(&resourceHandle, &ntSharedHandle, this->getRootDeviceIndex());
        if (status != STATUS_SUCCESS) {
            return handle == 0;
        }
        ntSecureHandle = castToUint64(ntSharedHandle);
        handle = ntSecureHandle;
    }
    return handle == 0;
}
void WddmAllocation::clearInternalHandle(uint32_t handleId) {
    ntSecureHandle = 0u;
}
} // namespace NEO