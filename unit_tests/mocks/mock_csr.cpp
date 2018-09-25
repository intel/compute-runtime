/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/mock_csr.h"
#include "runtime/os_interface/os_interface.h"

FlushStamp MockCommandStreamReceiver::flush(BatchBuffer &batchBuffer, EngineType engineType, ResidencyContainer &allocationsForResidency, OsContext &osContext) {
    FlushStamp stamp = 0;
    return stamp;
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
    CompletionStamp stamp = {taskCount, taskLevel, flushStamp->peekStamp(), 0, EngineType::ENGINE_RCS};
    return stamp;
}

void MockCommandStreamReceiver::setOSInterface(OSInterface *osInterface) {
    this->osInterface = osInterface;
}
