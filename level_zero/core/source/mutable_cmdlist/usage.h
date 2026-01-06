/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/stackvec.h"

#include "level_zero/core/source/mutable_cmdlist/mcl_types.h"

namespace L0 {
namespace MCL {
struct BufferUsages {
    StackVec<SurfaceStateHeapOffset, 8> bindful;
    StackVec<SurfaceStateHeapOffset, 8> bindfulWithoutOffset;
    StackVec<IndirectObjectHeapOffset, 8> bindless;
    StackVec<IndirectObjectHeapOffset, 8> bindlessWithoutOffset;
    StackVec<IndirectObjectHeapOffset, 8> statelessIndirect;
    StackVec<IndirectObjectHeapOffset, 8> statelessWithoutOffset;
    StackVec<IndirectObjectHeapOffset, 8> bufferOffset;
    StackVec<CommandBufferOffset, 8> commandBufferOffsets;
    StackVec<CommandBufferOffset, 8> commandBufferWithoutOffset;
};

struct ValueUsages {
    StackVec<IndirectObjectHeapOffset, 8> statelessIndirect;
    StackVec<IndirectObjectHeapOffset, 8> statelessWithoutOffset;
    StackVec<CommandBufferOffset, 8> commandBufferOffsets;
    StackVec<CommandBufferOffset, 8> commandBufferWithoutOffset;
    StackVec<size_t, 8> commandBufferPatchSize;
    StackVec<size_t, 8> statelessIndirectPatchSize;
};

} // namespace MCL
} // namespace L0
