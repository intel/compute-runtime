/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"
#include "level_zero/api/opencl/source/sharings/unified/leo_unified_sharing_types.h"

namespace NEO {
namespace LEO {

bool Buffer::validateHandleType(const MemoryProperties &memoryProperties, UnifiedSharingMemoryDescription &extMem) {
    if (memoryProperties.handleType == static_cast<uint64_t>(UnifiedSharingHandleType::win32Nt)) {
        extMem.type = UnifiedSharingHandleType::win32Nt;
        return true;
    }
    return false;
}

} // namespace LEO
} // namespace NEO
