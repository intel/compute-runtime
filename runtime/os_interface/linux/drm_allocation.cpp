/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/drm_allocation.h"

#include "runtime/os_interface/linux/drm_buffer_object.h"
#include "runtime/os_interface/linux/drm_memory_manager.h"

#include <sstream>

namespace NEO {
std::string DrmAllocation::getAllocationInfoString() const {
    std::stringstream ss;
    if (bo != nullptr) {
        ss << " Handle: " << bo->peekHandle();
    }
    return ss.str();
}

uint64_t DrmAllocation::peekInternalHandle(MemoryManager *memoryManager) {
    auto drmMemoryManager = static_cast<DrmMemoryManager *>(memoryManager);
    if (DebugManager.flags.AllowOpenFdOperations.get()) {
        return static_cast<uint64_t>(drmMemoryManager->obtainFdFromHandle(getBO()->peekHandle()));
    } else {
        return 0llu;
    }
}
} // namespace NEO
