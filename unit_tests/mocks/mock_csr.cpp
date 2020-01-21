/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/mock_csr.h"

#include "core/os_interface/os_interface.h"

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

void MockCommandStreamReceiver::setOSInterface(OSInterface *osInterface) {
    this->osInterface = osInterface;
}
