/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/experimental_command_buffer.h"

namespace NEO {

class MockExperimentalCommandBuffer : public ExperimentalCommandBuffer {
    using BaseClass = ExperimentalCommandBuffer;

  public:
    using BaseClass::currentStream;
    using BaseClass::experimentalAllocation;
    using BaseClass::experimentalAllocationOffset;
    using BaseClass::timestamps;
    using BaseClass::timestampsOffset;

    MockExperimentalCommandBuffer(CommandStreamReceiver *csr) : ExperimentalCommandBuffer(csr, 80.0) {
        defaultPrint = false;
    }
};

} // namespace NEO
