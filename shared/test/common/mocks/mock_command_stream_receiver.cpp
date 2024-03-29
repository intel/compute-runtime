/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_command_stream_receiver.h"

volatile TagAddressType MockCommandStreamReceiver::mockTagAddress[MockCommandStreamReceiver::tagSize];

SubmissionStatus MockCommandStreamReceiver::flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) {
    this->latestFlushedBatchBuffer = batchBuffer;
    return SubmissionStatus::success;
}

CompletionStamp MockCommandStreamReceiver::flushTask(
    LinearStream &commandStream,
    size_t commandStreamStart,
    const IndirectHeap *dsh,
    const IndirectHeap *ioh,
    const IndirectHeap *ssh,
    TaskCountType taskLevel,
    DispatchFlags &dispatchFlags,
    Device &device) {
    ++taskCount;
    CompletionStamp stamp = {taskCount, taskLevel, flushStamp->peekStamp()};
    return stamp;
}

CompletionStamp MockCommandStreamReceiver::flushTaskStateless(
    LinearStream &commandStream,
    size_t commandStreamStart,
    const IndirectHeap *dsh,
    const IndirectHeap *ioh,
    const IndirectHeap *ssh,
    TaskCountType taskLevel,
    DispatchFlags &dispatchFlags,
    Device &device) {
    ++taskCount;
    CompletionStamp stamp = {taskCount, taskLevel, flushStamp->peekStamp()};
    return stamp;
}

CompletionStamp MockCommandStreamReceiver::flushBcsTask(LinearStream &commandStreamTask, size_t commandStreamTaskStart,
                                                        const DispatchBcsFlags &dispatchBcsFlags, const HardwareInfo &hwInfo) {
    ++taskCount;
    CompletionStamp stamp = {taskCount, taskLevel, flushStamp->peekStamp()};
    return stamp;
}

CompletionStamp MockCommandStreamReceiver::flushImmediateTask(
    LinearStream &immediateCommandStream,
    size_t immediateCommandStreamStart,
    ImmediateDispatchFlags &dispatchFlags,
    Device &device) {
    ++taskCount;
    CompletionStamp stamp = {taskCount, taskLevel, flushStamp->peekStamp()};
    return stamp;
}

CompletionStamp MockCommandStreamReceiver::flushImmediateTaskStateless(
    LinearStream &immediateCommandStream,
    size_t immediateCommandStreamStart,
    ImmediateDispatchFlags &dispatchFlags,
    Device &device) {
    ++taskCount;
    CompletionStamp stamp = {taskCount, taskLevel, flushStamp->peekStamp()};
    return stamp;
}
