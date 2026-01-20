/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"

namespace NEO {

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::programEnginePrologue(LinearStream &csr) {
    if (!this->isEnginePrologueSent) {
        if (getGlobalFenceAllocation()) {
            EncodeMemoryFence<GfxFamily>::encodeSystemMemoryFence(csr, getGlobalFenceAllocation());
        }
        this->isEnginePrologueSent = true;
    }
}

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForPrologue() const {
    if (!this->isEnginePrologueSent) {
        if (getGlobalFenceAllocation()) {
            return EncodeMemoryFence<GfxFamily>::getSystemMemoryFenceSize();
        }
    }
    return 0u;
}

} // namespace NEO
