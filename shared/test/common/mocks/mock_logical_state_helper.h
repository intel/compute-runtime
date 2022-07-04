/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/logical_state_helper.h"

namespace NEO {

template <typename GfxFamily>
class LogicalStateHelperMock : public GfxFamily::LogicalStateHelperHw {
  public:
    LogicalStateHelperMock() : GfxFamily::LogicalStateHelperHw() {
    }

    void writeStreamInline(LinearStream &linearStream, bool pipelinedState) override {
        writeStreamInlineCalledCounter++;

        auto cmd = linearStream.getSpaceForCmd<typename GfxFamily::MI_NOOP>();
        *cmd = GfxFamily::cmdInitNoop;
        cmd->setIdentificationNumber(0x123);
    }

    uint32_t writeStreamInlineCalledCounter = 0;
};
} // namespace NEO
