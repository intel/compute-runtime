/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/wddm_memory_manager.h"

namespace OCLRT {
GraphicsAllocation *WddmMemoryManager::allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) {
    return MemoryManager::allocateGraphicsMemoryInDevicePool(allocationData, status);
}
} // namespace OCLRT
