/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/os_interface/windows/wddm_allocation.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"

#include <sstream>

namespace NEO {

void *WddmAllocation::getAlignedCpuPtr() const {
    return alignDown(this->cpuPtr, MemoryConstants::pageSize);
}

size_t WddmAllocation::getAlignedSize() const {
    return alignSizeWholePage(this->cpuPtr, this->size);
}

std::string WddmAllocation::getHandleInfoString() const {
    std::stringstream ss;
    for (auto &handle : handles) {
        ss << " Handle: " << handle;
    }
    return ss.str();
}

GraphicsAllocation *WddmAllocation::createView(size_t offsetInParentAllocation, size_t viewSize) {
    return new WddmAllocation(this, offsetInParentAllocation, viewSize);
}

int WddmAllocation::createInternalHandle(MemoryManager *memoryManager, uint32_t handleId, uint64_t &handle, void *reservedHandleData) {
    if (parentAllocation) {
        return static_cast<WddmAllocation *>(parentAllocation)->createInternalHandle(memoryManager, handleId, handle, reservedHandleData);
    }
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
    if (parentAllocation) {
        static_cast<WddmAllocation *>(parentAllocation)->clearInternalHandle(handleId);
        return;
    }
    ntSecureHandle = 0u;
}
} // namespace NEO
