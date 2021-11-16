/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {
template <typename GfxFamily>
struct EncodeMemoryFence {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename GfxFamily::STATE_SYSTEM_MEM_FENCE_ADDRESS;

    static size_t getSystemMemoryFenceSize() {
        return sizeof(STATE_SYSTEM_MEM_FENCE_ADDRESS);
    }
    static void encodeSystemMemoryFence(LinearStream &commandStream, const GraphicsAllocation *globalFenceAllocation) {
        auto stateSystemFenceAddressSpace = commandStream.getSpaceForCmd<STATE_SYSTEM_MEM_FENCE_ADDRESS>();
        STATE_SYSTEM_MEM_FENCE_ADDRESS stateSystemFenceAddress = GfxFamily::cmdInitStateSystemMemFenceAddress;
        stateSystemFenceAddress.setSystemMemoryFenceAddress(globalFenceAllocation->getGpuAddress());
        *stateSystemFenceAddressSpace = stateSystemFenceAddress;
    }
};
} // namespace NEO