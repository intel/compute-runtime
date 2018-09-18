/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/experimental_command_buffer.h"

namespace OCLRT {

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

} // namespace OCLRT
