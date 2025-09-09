/*
 * Copyright (C) 2018-2025 Intel Corporation
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
cl_int CommandQueueHw<GfxFamily>::finish(bool resolvePendingL3Flushes) {

    auto &csr = getGpgpuCommandStreamReceiver();

    auto result = csr.flushBatchedSubmissions();
    if (!result) {
        return CL_OUT_OF_RESOURCES;
    }

    bool waitForTaskCountRequired = false;
    programPendingL3Flushes(csr, waitForTaskCountRequired, resolvePendingL3Flushes);

    // Stall until HW reaches taskCount on all its engines
    const auto waitStatus = waitForAllEngines(true, nullptr, waitForTaskCountRequired);
    if (waitStatus == WaitStatus::gpuHang) {
        return CL_OUT_OF_RESOURCES;
    }

    return CL_SUCCESS;
}

template <typename GfxFamily>
void CommandQueueHw<GfxFamily>::programPendingL3Flushes(CommandStreamReceiver &csr, bool &waitForTaskCountRequired, bool resolvePendingL3Flushes) {
}

} // namespace NEO
