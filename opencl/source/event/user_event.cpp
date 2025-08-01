/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "user_event.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/wait_status.h"

#include "opencl/source/command_queue/command_queue.h"

namespace NEO {

UserEvent::UserEvent(Context *ctx)
    : Event(ctx, nullptr, CL_COMMAND_USER, CompletionStamp::notReady, CompletionStamp::notReady) {}

void UserEvent::updateExecutionStatus() {
    return;
}

WaitStatus UserEvent::wait(bool blocking, bool useQuickKmdSleep) {
    while (updateStatusAndCheckCompletion() == false) {
        if (blocking == false) {
            return WaitStatus::notReady;
        }
    }
    return WaitStatus::ready;
}

TaskCountType UserEvent::getTaskLevel() {
    if (peekExecutionStatus() == CL_COMPLETE) {
        return 0;
    }
    return CompletionStamp::notReady;
}

bool UserEvent::isInitialEventStatus() const {
    return executionStatus == CL_QUEUED;
}

VirtualEvent::VirtualEvent(CommandQueue *cmdQ, Context *ctx)
    : Event(ctx, cmdQ, -1, CompletionStamp::notReady, CompletionStamp::notReady) {
    transitionExecutionStatus(CL_QUEUED);

    // internal object - no need for API refcount
    convertToInternalObject();
}

void VirtualEvent::updateExecutionStatus() {
}

WaitStatus VirtualEvent::wait(bool blocking, bool useQuickKmdSleep) {
    while (updateStatusAndCheckCompletion() == false) {
        if (blocking == false) {
            return WaitStatus::notReady;
        }
    }
    return WaitStatus::ready;
}

TaskCountType VirtualEvent::getTaskLevel() {
    TaskCountType taskLevel = 0;
    if (cmdQueue != nullptr) {
        auto &csr = cmdQueue->getGpgpuCommandStreamReceiver();
        taskLevel = csr.peekTaskLevel();
    }
    return taskLevel;
}

bool VirtualEvent::setStatus(cl_int status) {
    // virtual events are just helper events and will have either
    // "waiting" (after construction) or "complete" (on change if not blocked) execution state
    if (isStatusCompletedByTermination(status) == false) {
        status = CL_COMPLETE;
    }
    return Event::setStatus(status);
}
} // namespace NEO
