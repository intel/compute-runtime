/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"

#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/sharings/gl/leo_gl_arb_sync_event.h"
#include "level_zero/api/opencl/source/sharings/gl/linux/leo_gl_sharing_linux.h"

#include <GL/gl.h>

namespace NEO {
namespace LEO {

void cleanupArbSyncObject(OSInterface &osInterface, CL_GL_SYNC_INFO *glSyncInfo) {}

bool setupArbSyncObject(GLSharingFunctions &sharing, OSInterface &osInterface, CL_GL_SYNC_INFO &glSyncInfo) {
    return false;
}

void signalArbSyncObject(OsContext &osContext, CL_GL_SYNC_INFO &glSyncInfo) {}

void serverWaitForArbSyncObject(OSInterface &osInterface, CL_GL_SYNC_INFO &glSyncInfo, bool cpuSync) {}

GlArbSyncEvent::~GlArbSyncEvent() {
}

GlArbSyncEvent *GlArbSyncEvent::create(Event &baseEvent) {
    return nullptr;
}

} // namespace LEO
} // namespace NEO

extern "C" CL_API_ENTRY cl_int CL_API_CALL
clEnqueueMarkerWithSyncObjectINTEL(cl_command_queue commandQueue,
                                   cl_event *event,
                                   cl_context *context) {
    return CL_INVALID_OPERATION;
}

extern "C" CL_API_ENTRY cl_int CL_API_CALL
clGetCLObjectInfoINTEL(cl_mem memObj,
                       void *pResourceInfo) {
    return CL_INVALID_OPERATION;
}

extern "C" CL_API_ENTRY cl_int CL_API_CALL
clGetCLEventInfoINTEL(cl_event event, PCL_GL_SYNC_INFO *pSyncInfoHandleRet, cl_context *pClContextRet) {
    return CL_INVALID_OPERATION;
}

extern "C" CL_API_ENTRY cl_int CL_API_CALL
clReleaseGlSharedEventINTEL(cl_event event) {
    return CL_INVALID_EVENT;
}
