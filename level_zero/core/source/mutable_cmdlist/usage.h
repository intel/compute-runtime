/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/stackvec.h"

#include "level_zero/core/source/mutable_cmdlist/mcl_types.h"

namespace L0 {
namespace MCL {
constexpr size_t stackUsageSize = 2;
struct BufferUsages {
    StackVec<SurfaceStateHeapOffset, stackUsageSize> bindful;
    StackVec<SurfaceStateHeapOffset, stackUsageSize> bindfulWithoutOffset;
    StackVec<IndirectObjectHeapOffset, stackUsageSize> bindless;
    StackVec<IndirectObjectHeapOffset, stackUsageSize> bindlessWithoutOffset;
    StackVec<IndirectObjectHeapOffset, stackUsageSize> statelessIndirect;
    StackVec<IndirectObjectHeapOffset, stackUsageSize> statelessWithoutOffset;
    StackVec<IndirectObjectHeapOffset, stackUsageSize> bufferOffset;
    StackVec<CommandBufferOffset, stackUsageSize> commandBufferOffsets;
    StackVec<CommandBufferOffset, stackUsageSize> commandBufferWithoutOffset;
};

struct ValueUsages {
    StackVec<IndirectObjectHeapOffset, stackUsageSize> statelessIndirect;
    StackVec<IndirectObjectHeapOffset, stackUsageSize> statelessWithoutOffset;
    StackVec<CommandBufferOffset, stackUsageSize> commandBufferOffsets;
    StackVec<CommandBufferOffset, stackUsageSize> commandBufferWithoutOffset;
    StackVec<size_t, stackUsageSize> commandBufferPatchSize;
    StackVec<size_t, stackUsageSize> statelessIndirectPatchSize;
};

} // namespace MCL
} // namespace L0
