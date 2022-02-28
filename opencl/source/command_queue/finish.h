/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/wait_status.h"

#include "opencl/source/command_queue/command_queue_hw.h"

namespace NEO {

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::finish() {
    auto result = getGpgpuCommandStreamReceiver().flushBatchedSubmissions();
    if (!result) {
        return CL_OUT_OF_RESOURCES;
    }

    // Stall until HW reaches taskCount on all its engines
    const auto waitStatus = waitForAllEngines(true, nullptr);
    if (waitStatus == WaitStatus::GpuHang) {
        return CL_OUT_OF_RESOURCES;
    }

    return CL_SUCCESS;
}
} // namespace NEO
