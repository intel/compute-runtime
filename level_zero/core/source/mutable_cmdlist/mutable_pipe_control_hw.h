/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/mutable_cmdlist/mutable_pipe_control.h"

namespace L0::MCL {

template <typename GfxFamily>
struct MutablePipeControlHw : public MutablePipeControl {
    using PipeControl = typename GfxFamily::PIPE_CONTROL;

    MutablePipeControlHw(void *pipeControl) : pipeControl(pipeControl) {}
    ~MutablePipeControlHw() override {}

    void setPostSyncAddress(GpuAddress postSyncAddress) override;

  protected:
    void *pipeControl;
};

} // namespace L0::MCL
