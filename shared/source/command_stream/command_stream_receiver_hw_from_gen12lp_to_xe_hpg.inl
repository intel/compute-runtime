/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"

namespace NEO {

template <>
bool CommandStreamReceiverHw<Family>::isPerQueuePrologueEnabled() const {
    return false;
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programEnginePrologue(LinearStream &csr) {
}

template <typename GfxFamily>
inline size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForPrologue() const {
    return 0u;
}

} // namespace NEO
