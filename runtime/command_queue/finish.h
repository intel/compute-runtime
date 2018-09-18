/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "hw_cmds.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/device/device.h"

namespace OCLRT {

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::finish(bool dcFlush) {
    auto &commandStreamReceiver = device->getCommandStreamReceiver();
    commandStreamReceiver.flushBatchedSubmissions();

    //as long as queue is blocked we need to stall.
    while (isQueueBlocked())
        ;

    auto taskCountToWaitFor = this->taskCount;
    auto flushStampToWaitFor = this->flushStamp->peekStamp();

    // Stall until HW reaches CQ taskCount
    waitUntilComplete(taskCountToWaitFor, flushStampToWaitFor, false);

    commandStreamReceiver.waitForTaskCountAndCleanAllocationList(taskCountToWaitFor, TEMPORARY_ALLOCATION);

    return CL_SUCCESS;
}
} // namespace OCLRT
