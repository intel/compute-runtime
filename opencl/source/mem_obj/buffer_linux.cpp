/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/sharings/unified/unified_sharing_types.h"

namespace NEO {
bool Buffer::validateHandleType(const MemoryProperties &memoryProperties, UnifiedSharingMemoryDescription &extMem) {
    if (memoryProperties.handleType == static_cast<uint64_t>(UnifiedSharingHandleType::linuxFd)) {
        extMem.type = UnifiedSharingHandleType::linuxFd;
        return true;
    }
    return false;
}
} // namespace NEO
