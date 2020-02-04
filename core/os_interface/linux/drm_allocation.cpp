/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/linux/drm_allocation.h"

#include "core/os_interface/linux/drm_buffer_object.h"
#include "runtime/os_interface/linux/drm_memory_manager.h"

#include <sstream>

namespace NEO {
std::string DrmAllocation::getAllocationInfoString() const {
    std::stringstream ss;
    for (auto bo : bufferObjects) {
        if (bo != nullptr) {
            ss << " Handle: " << bo->peekHandle();
        }
    }
    return ss.str();
}

uint64_t DrmAllocation::peekInternalHandle(MemoryManager *memoryManager) {
    return static_cast<uint64_t>((static_cast<DrmMemoryManager *>(memoryManager))->obtainFdFromHandle(getBO()->peekHandle(), this->rootDeviceIndex));
}
} // namespace NEO
