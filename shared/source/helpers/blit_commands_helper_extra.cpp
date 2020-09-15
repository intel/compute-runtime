/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/blit_commands_helper.h"

namespace NEO {

BlitOperationResult BlitHelper::blitMemoryToAllocation(const Device &device, GraphicsAllocation *memory, size_t offset, const void *hostPtr,
                                                       Vec3<size_t> size) {
    return BlitOperationResult::Unsupported;
}

} // namespace NEO
