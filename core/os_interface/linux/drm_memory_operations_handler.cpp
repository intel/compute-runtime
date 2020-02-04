/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/linux/drm_memory_operations_handler.h"

namespace NEO {

DrmMemoryOperationsHandler::DrmMemoryOperationsHandler() {
}

MemoryOperationsStatus DrmMemoryOperationsHandler::makeResident(ArrayRef<GraphicsAllocation *> gfxAllocations) {
    return MemoryOperationsStatus::UNSUPPORTED;
}

MemoryOperationsStatus DrmMemoryOperationsHandler::evict(GraphicsAllocation &gfxAllocation) {
    return MemoryOperationsStatus::UNSUPPORTED;
}

MemoryOperationsStatus DrmMemoryOperationsHandler::isResident(GraphicsAllocation &gfxAllocation) {
    return MemoryOperationsStatus::UNSUPPORTED;
}

} // namespace NEO
