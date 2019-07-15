/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/device/device.h"

#include "hw_cmds.h"

namespace NEO {

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::finish(bool dcFlush) {
    getGpgpuCommandStreamReceiver().flushBatchedSubmissions();

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
