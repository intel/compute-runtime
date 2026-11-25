/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/mutable_cmdlist/mutable_pipe_control_hw.h"

namespace L0::MCL {

template <typename GfxFamily>
void MutablePipeControlHw<GfxFamily>::setPostSyncAddress(GpuAddress postSyncAddress) {
    auto pipeControlCmd = reinterpret_cast<PipeControl *>(pipeControl);
    pipeControlCmd->setAddress(postSyncAddress);
}

} // namespace L0::MCL
