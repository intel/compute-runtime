/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/timestamp_packet.h"

#include "level_zero/api/internal/l0_cmdlist.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/sharings/gl/leo_gl_context_guard.h"
#include "level_zero/api/opencl/source/sharings/gl/leo_gl_sync_event.h"
#include "level_zero/api/opencl/source/sharings/gl/windows/leo_gl_sharing_windows.h"

namespace NEO {
namespace LEO {

GlSyncEvent::GlSyncEvent(Context &context, const GL_CL_SYNC_INFO &sync)
    : Event(&context),
      glSync(std::make_unique<GL_CL_SYNC_INFO>(sync)) {

    this->updateCommandType(CL_COMMAND_GL_FENCE_SYNC_OBJECT_KHR);
    this->incRefInternal();

    ze_command_list_handle_t internalComputeCmdList = nullptr;
    [[maybe_unused]] auto result = context.getInternalComputeCmdList(internalComputeCmdList);
    DEBUG_BREAK_IF(result != ZE_RESULT_SUCCESS);

    auto lock = this->getContext()->lockInternalCompute();
    L0::zeCommandListAppendHostFunction(internalComputeCmdList,
                                        reinterpret_cast<ze_host_function_callback_t>(signalGlEvent),
                                        this,
                                        nullptr,
                                        nullptr,
                                        0u,
                                        nullptr);
}

GlSyncEvent::~GlSyncEvent() {
    getContext()->getSharing<GLSharingFunctionsWindows>()->releaseSync(glSync->pSync);
}

GlSyncEvent *GlSyncEvent::create(Context &context, cl_GLsync sync, cl_int *errCode) {
    ze_command_list_handle_t internalComputeCmdList = nullptr;
    auto result = context.getInternalComputeCmdList(internalComputeCmdList);
    if (result != ZE_RESULT_SUCCESS) {
        ErrorCodeHelper err(errCode, L0ToClResultMapper(result));
        return nullptr;
    }

    GLContextGuard guard(*context.getSharing<GLSharingFunctionsWindows>());

    GL_CL_SYNC_INFO syncInfo = {sync, nullptr};

    context.getSharing<GLSharingFunctionsWindows>()->retainSync(&syncInfo);
    DEBUG_BREAK_IF(!syncInfo.pSync);

    return new GlSyncEvent(context, syncInfo);
}

void GlSyncEvent::signalGlEvent(void *userData) {
    auto glEvent = static_cast<GlSyncEvent *>(userData);
    GLContextGuard guard(*glEvent->getContext()->getSharing<GLSharingFunctionsWindows>());
    int retVal = 0;

    while (retVal != GL_SIGNALED) {
        glEvent->getContext()->getSharing<GLSharingFunctionsWindows>()->getSynciv(glEvent->glSync->pSync, GL_SYNC_STATUS, &retVal);
    }

    clSetUserEventStatus(glEvent, CL_COMPLETE);

    glEvent->decRefInternal();
}

} // namespace LEO
} // namespace NEO
