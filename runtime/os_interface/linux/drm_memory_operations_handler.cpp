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

bool DrmMemoryOperationsHandler::makeResident(GraphicsAllocation &gfxAllocation) {
    return false;
}

bool DrmMemoryOperationsHandler::evict(GraphicsAllocation &gfxAllocation) {
    return false;
}

bool DrmMemoryOperationsHandler::isResident(GraphicsAllocation &gfxAllocation) {
    return false;
}

} // namespace NEO
