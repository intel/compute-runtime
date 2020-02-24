/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"

#include "opencl/source/command_queue/command_queue_hw.h"

namespace NEO {

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::finish() {
    auto result = getGpgpuCommandStreamReceiver().flushBatchedSubmissions();
    if (!result) {
        return CL_OUT_OF_RESOURCES;
    }

    //as long as queue is blocked we need to stall.
    while (isQueueBlocked())
        ;

    auto taskCountToWaitFor = this->taskCount;
    auto flushStampToWaitFor = this->flushStamp->peekStamp();

    // Stall until HW reaches CQ taskCount
    waitUntilComplete(taskCountToWaitFor, flushStampToWaitFor, false);

    return CL_SUCCESS;
}
} // namespace NEO
