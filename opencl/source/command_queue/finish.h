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
cl_int CommandQueueHw<GfxFamily>::finish() {

    auto &csr = getGpgpuCommandStreamReceiver();

    auto result = csr.flushBatchedSubmissions();
    if (!result) {
        return CL_OUT_OF_RESOURCES;
    }

    if (!l3FlushedAfterCpuRead && l3FlushAfterPostSyncEnabled) {
        csr.flushTagUpdate();
        this->l3FlushedAfterCpuRead = true;

        CompletionStamp completionStamp = {
            csr.peekTaskCount(),
            csr.peekTaskLevel(),
            csr.obtainCurrentFlushStamp()};

        this->updateFromCompletionStamp(completionStamp, nullptr);
    }

    // Stall until HW reaches taskCount on all its engines
    const auto waitStatus = waitForAllEngines(true, nullptr);
    if (waitStatus == WaitStatus::gpuHang) {
        return CL_OUT_OF_RESOURCES;
    }

    return CL_SUCCESS;
}
} // namespace NEO
