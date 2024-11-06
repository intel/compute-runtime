/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"

namespace NEO {

template <typename Family>
void EncodeBatchBufferStartOrEnd<Family>::appendBatchBufferStart(MI_BATCH_BUFFER_START &cmd, bool indirect, bool predicate) {
    cmd.setPredicationEnable(predicate);
}

template <typename Family>
size_t EncodeMemoryFence<Family>::getSystemMemoryFenceSize() {
    return 0;
}

template <typename Family>
void EncodeMemoryFence<Family>::encodeSystemMemoryFence(LinearStream &commandStream, const GraphicsAllocation *globalFenceAllocation) {
}

} // namespace NEO
