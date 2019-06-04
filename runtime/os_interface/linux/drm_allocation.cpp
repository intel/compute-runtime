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

uint64_t DrmAllocation::peekInternalHandle() { return static_cast<uint64_t>(getBO()->peekHandle()); }
} // namespace NEO
