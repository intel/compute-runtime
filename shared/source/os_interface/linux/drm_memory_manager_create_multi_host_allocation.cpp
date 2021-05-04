/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_memory_manager.h"

namespace NEO {
DrmAllocation *DrmMemoryManager::createMultiHostAllocation(const AllocationData &allocationData) {
    return allocateGraphicsMemoryWithAlignmentImpl(allocationData);
}
} // namespace NEO
