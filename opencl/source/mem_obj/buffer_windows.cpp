/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/mem_obj/buffer.h"

namespace NEO {
bool Buffer::validateHandleType(const MemoryProperties &memoryProperties, UnifiedSharingMemoryDescription &extMem) {
    if (memoryProperties.handleType == static_cast<uint64_t>(UnifiedSharingHandleType::Win32Nt)) {
        extMem.type = UnifiedSharingHandleType::Win32Nt;
        return true;
    }
    return false;
}
} // namespace NEO
