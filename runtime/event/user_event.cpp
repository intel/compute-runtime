/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "user_event.h"

#include "runtime/command_queue/command_queue.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"

namespace NEO {

UserEvent::UserEvent(Context *ctx)
    : Event(ctx, nullptr, CL_COMMAND_USER, eventNotReady, eventNotReady) {
    transitionExecutionStatus(CL_QUEUED);
}

void UserEvent::updateExecutionStatus() {
    return;
}

bool UserEvent::wait(bool blocking, bool useQuickKmdSleep) {
    while (updateStatusAndCheckCompletion() == false) {
        if (blocking == false) {
            return false;
        }
    }
    return true;
}

uint32_t UserEvent::getTaskLevel() {
    if (peekExecutionStatus() == CL_COMPLETE) {
        return 0;
    }
    return Event::eventNotReady;
}

bool UserEvent::isInitialEventStatus() const {
    return executionStatus == CL_QUEUED;
}

VirtualEvent::VirtualEvent(CommandQueue *cmdQ, Context *ctx)
    : Event(ctx, cmdQ, -1, eventNotReady, eventNotReady) {
    transitionExecutionStatus(CL_QUEUED);

    // internal object - no need for API refcount
    convertToInternalObject();
}

void VirtualEvent::updateExecutionStatus() {
    ;
}

bool VirtualEvent::wait(bool blocking, bool useQuickKmdSleep) {
    while (updateStatusAndCheckCompletion() == false) {
        if (blocking == false) {
            return false;
        }
    }
    return true;
}

uint32_t VirtualEvent::getTaskLevel() {
    uint32_t taskLevel = 0;
    if (cmdQueue != nullptr) {
        auto &csr = cmdQueue->getCommandStreamReceiver();
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
