/*
 * Copyright (c) 2017, Intel Corporation
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

#include "user_event.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/command_queue/command_queue.h"

namespace OCLRT {

UserEvent::UserEvent(Context *ctx)
    : Event(ctx, nullptr, CL_COMMAND_USER, eventNotReady, eventNotReady) {
    transitionExecutionStatus(CL_QUEUED);
}

void UserEvent::updateExecutionStatus() {
    return;
}

bool UserEvent::wait(bool blocking) {
    while (updateStatusAndCheckCompletion() == false) {
        if (blocking == false) {
            return false;
        }
    }
    return true;
}

uint32_t UserEvent::getTaskLevel() {
    uint32_t taskLevel = 0;
    if (ctx != nullptr) {
        Device *pDevice = ctx->getDevice(0);
        auto &csr = pDevice->getCommandStreamReceiver();
        taskLevel = csr.peekTaskLevel();
    }
    return taskLevel;
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

bool VirtualEvent::wait(bool blocking) {
    while (updateStatusAndCheckCompletion() == false) {
        if (blocking == false) {
            return false;
        }
    }
    return true;
}

uint32_t VirtualEvent::getTaskLevel() {
    uint32_t taskLevel = 0;
    if (ctx != nullptr) {
        Device *pDevice = ctx->getDevice(0);
        auto &csr = pDevice->getCommandStreamReceiver();
        taskLevel = csr.peekTaskLevel();
    }
    return taskLevel;
}

bool VirtualEvent::setStatus(cl_int status) {
    // virtual events are just helper events and will have either
    // "waiting" (after construction) or "complete" (on change if not blocked) execution state
    if (isStatusCompletedByTermination(&status) == false) {
        status = CL_COMPLETE;
    }
    return Event::setStatus(status);
}
} // namespace OCLRT
