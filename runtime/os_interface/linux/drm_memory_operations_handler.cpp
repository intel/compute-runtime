/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/drm_memory_operations_handler.h"

namespace NEO {

DrmMemoryOperationsHandler::DrmMemoryOperationsHandler() {
}

MemoryOperationsStatus DrmMemoryOperationsHandler::makeResident(GraphicsAllocation &gfxAllocation) {
    return MemoryOperationsStatus::UNSUPPORTED;
}

MemoryOperationsStatus DrmMemoryOperationsHandler::evict(GraphicsAllocation &gfxAllocation) {
    return MemoryOperationsStatus::UNSUPPORTED;
}

MemoryOperationsStatus DrmMemoryOperationsHandler::isResident(GraphicsAllocation &gfxAllocation) {
    return MemoryOperationsStatus::UNSUPPORTED;
}

} // namespace NEO
