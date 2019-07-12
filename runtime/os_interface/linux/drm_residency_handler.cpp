/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/drm_residency_handler.h"

namespace NEO {

DrmResidencyHandler::DrmResidencyHandler() {
}

bool DrmResidencyHandler::makeResident(GraphicsAllocation &gfxAllocation) {
    return false;
}

bool DrmResidencyHandler::evict(GraphicsAllocation &gfxAllocation) {
    return false;
}

bool DrmResidencyHandler::isResident(GraphicsAllocation &gfxAllocation) {
    return false;
}

} // namespace NEO
