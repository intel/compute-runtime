/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
