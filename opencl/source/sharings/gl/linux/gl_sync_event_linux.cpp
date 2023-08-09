/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/timestamp_packet.h"

#include "opencl/extensions/public/cl_gl_private_intel.h"
#include "opencl/source/context/context.h"
#include "opencl/source/event/async_events_handler.h"
#include "opencl/source/event/event_builder.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/sharings/gl/gl_context_guard.h"
#include "opencl/source/sharings/gl/gl_sync_event.h"
#include "opencl/source/sharings/gl/linux/gl_sharing_linux.h"

namespace NEO {
GlSyncEvent::GlSyncEvent(Context &context, const GL_CL_SYNC_INFO &sync)
    : Event(&context, nullptr, CL_COMMAND_GL_FENCE_SYNC_OBJECT_KHR, CompletionStamp::notReady, CompletionStamp::notReady),
      glSync(std::make_unique<GL_CL_SYNC_INFO>(sync)) {
    transitionExecutionStatus(CL_SUBMITTED);
}

GlSyncEvent::~GlSyncEvent() {
}

GlSyncEvent *GlSyncEvent::create(Context &context, cl_GLsync sync, cl_int *errCode) {
    /* Not supported */
    return nullptr;
}

void GlSyncEvent::updateExecutionStatus() {
    /* Not supported */
}

TaskCountType GlSyncEvent::getTaskLevel() {
    if (peekExecutionStatus() == CL_COMPLETE) {
        return 0;
    }
    return CompletionStamp::notReady;
}
} // namespace NEO
