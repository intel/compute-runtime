/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_command_stream_receiver.h"

bool MockCommandStreamReceiver::flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) {
    return true;
}

CompletionStamp MockCommandStreamReceiver::flushTask(
    LinearStream &commandStream,
    size_t commandStreamStart,
    const IndirectHeap &dsh,
    const IndirectHeap &ioh,
    const IndirectHeap &ssh,
    uint32_t taskLevel,
    DispatchFlags &dispatchFlags,
    Device &device) {
    ++taskCount;
    CompletionStamp stamp = {taskCount, taskLevel, flushStamp->peekStamp()};
    return stamp;
}
