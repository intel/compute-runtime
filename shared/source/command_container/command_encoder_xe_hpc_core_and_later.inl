/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/xe_hpc_core/hw_cmds.h"

namespace NEO {

template <>
size_t EncodeMemoryFence<Family>::getSystemMemoryFenceSize() {
    return sizeof(typename Family::STATE_SYSTEM_MEM_FENCE_ADDRESS);
}

template <>
void EncodeMemoryFence<Family>::encodeSystemMemoryFence(LinearStream &commandStream, const GraphicsAllocation *globalFenceAllocation, LogicalStateHelper *logicalStateHelper) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename Family::STATE_SYSTEM_MEM_FENCE_ADDRESS;

    auto stateSystemFenceAddressSpace = commandStream.getSpaceForCmd<STATE_SYSTEM_MEM_FENCE_ADDRESS>();
    STATE_SYSTEM_MEM_FENCE_ADDRESS stateSystemFenceAddress = Family::cmdInitStateSystemMemFenceAddress;
    stateSystemFenceAddress.setSystemMemoryFenceAddress(globalFenceAllocation->getGpuAddress());
    *stateSystemFenceAddressSpace = stateSystemFenceAddress;
}

} // namespace NEO