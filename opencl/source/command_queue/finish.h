/*
 * Copyright (C) 2018-2021 Intel Corporation
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

    // Stall until HW reaches CQ taskCount
    waitForLatestTaskCount();

    return CL_SUCCESS;
}
} // namespace NEO
