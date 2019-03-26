/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/drm_allocation.h"

#include "runtime/os_interface/linux/drm_buffer_object.h"

#include <sstream>

namespace NEO {
std::string DrmAllocation::getAllocationInfoString() const {
    std::stringstream ss;
    if (bo != nullptr) {
        ss << " Handle: " << bo->peekHandle();
    }
    return ss.str();
}
} // namespace NEO
