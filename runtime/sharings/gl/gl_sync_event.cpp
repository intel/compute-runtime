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

#include "runtime/sharings/gl/gl_sync_event.h"

#include "runtime/sharings/gl/gl_sharing.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/event/async_events_handler.h"
#include "runtime/event/event_builder.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/get_info.h"
#include "runtime/platform/platform.h"

#include "CLGLShr.h"

namespace OCLRT {
GlSyncEvent::GlSyncEvent(Context &context, const GL_CL_SYNC_INFO &sync)
    : Event(&context, nullptr, CL_COMMAND_GL_FENCE_SYNC_OBJECT_KHR, eventNotReady, eventNotReady), glSync(new GL_CL_SYNC_INFO(sync)) {
    transitionExecutionStatus(CL_SUBMITTED);
}

GlSyncEvent::~GlSyncEvent() { ctx->getSharing<GLSharingFunctions>()->releaseSync(glSync->pSync); }

GlSyncEvent *GlSyncEvent::create(Context &context, cl_GLsync sync, cl_int *errCode) {
    GLContextGuard guard(*context.getSharing<GLSharingFunctions>());

    ErrorCodeHelper err(errCode, CL_SUCCESS);
    GL_CL_SYNC_INFO syncInfo = {sync, nullptr};

    context.getSharing<GLSharingFunctions>()->retainSync(&syncInfo);
    DEBUG_BREAK_IF(!syncInfo.pSync);

    EventBuilder eventBuilder;
    eventBuilder.create<GlSyncEvent>(context, syncInfo);
    return static_cast<GlSyncEvent *>(eventBuilder.finalizeAndRelease());
}

void GlSyncEvent::updateExecutionStatus() {
    GLContextGuard guard(*ctx->getSharing<GLSharingFunctions>());
    int retVal = 0;

    ctx->getSharing<GLSharingFunctions>()->getSynciv(glSync->pSync, GL_SYNC_STATUS, &retVal);
    if (retVal == GL_SIGNALED) {
        setStatus(CL_COMPLETE);
    }
}

uint32_t GlSyncEvent::getTaskLevel() {
    auto &csr = ctx->getDevice(0)->getCommandStreamReceiver();
    return csr.peekTaskLevel();
}
} // namespace OCLRT
