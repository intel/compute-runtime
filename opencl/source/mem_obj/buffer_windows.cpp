/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/mem_obj/buffer.h"

namespace NEO {
bool Buffer::validateHandleType(MemoryProperties &memoryProperties, UnifiedSharingMemoryDescription &extMem) {
    return false;
}
} // namespace NEO
