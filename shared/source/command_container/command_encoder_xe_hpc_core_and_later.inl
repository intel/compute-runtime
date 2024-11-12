/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {

template <typename Family>
size_t EncodeMemoryFence<Family>::getSystemMemoryFenceSize() {
    return sizeof(typename Family::STATE_SYSTEM_MEM_FENCE_ADDRESS);
}

template <typename Family>
void EncodeMemoryFence<Family>::encodeSystemMemoryFence(LinearStream &commandStream, const GraphicsAllocation *globalFenceAllocation) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename Family::STATE_SYSTEM_MEM_FENCE_ADDRESS;

    auto stateSystemFenceAddressSpace = commandStream.getSpaceForCmd<STATE_SYSTEM_MEM_FENCE_ADDRESS>();
    STATE_SYSTEM_MEM_FENCE_ADDRESS stateSystemFenceAddress = Family::cmdInitStateSystemMemFenceAddress;
    stateSystemFenceAddress.setSystemMemoryFenceAddress(globalFenceAllocation->getGpuAddress());
    *stateSystemFenceAddressSpace = stateSystemFenceAddress;
}

template <typename Family>
void EncodeBatchBufferStartOrEnd<Family>::appendBatchBufferStart(MI_BATCH_BUFFER_START &cmd, bool indirect, bool predicate) {
    cmd.setIndirectAddressEnable(indirect);
    cmd.setPredicationEnable(predicate);
}

template <typename Family>
inline void EncodeAtomic<Family>::setMiAtomicAddress(MI_ATOMIC &atomic, uint64_t writeAddress) {
    atomic.setMemoryAddress(writeAddress);
}

} // namespace NEO