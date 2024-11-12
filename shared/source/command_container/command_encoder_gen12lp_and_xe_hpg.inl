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

template <typename Family>
inline void EncodeAtomic<Family>::setMiAtomicAddress(MI_ATOMIC &atomic, uint64_t writeAddress) {
    atomic.setMemoryAddress(static_cast<uint32_t>(writeAddress & 0x0000FFFFFFFFULL));
    atomic.setMemoryAddressHigh(static_cast<uint32_t>(writeAddress >> 32));
}

template <typename Family>
inline size_t EncodeMemoryPrefetch<Family>::getSizeForMemoryPrefetch(size_t size, const RootDeviceEnvironment &rootDeviceEnvironment) { return 0u; }

template <typename Family>
inline void EncodeMemoryPrefetch<Family>::programMemoryPrefetch(LinearStream &commandStream, const GraphicsAllocation &graphicsAllocation, uint32_t size, size_t offset, const RootDeviceEnvironment &rootDeviceEnvironment) {}

} // namespace NEO
