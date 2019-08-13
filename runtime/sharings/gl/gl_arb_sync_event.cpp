/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/sharings/gl/gl_arb_sync_event.h"

#include "public/cl_gl_private_intel.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/base_object.h"
#include "runtime/sharings/gl/gl_sharing.h"

#include <GL/gl.h>

namespace NEO {
GlArbSyncEvent::GlArbSyncEvent(Context &context)
    : Event(&context, nullptr, CL_COMMAND_GL_FENCE_SYNC_OBJECT_KHR, eventNotReady, eventNotReady),
      glSyncInfo(std::make_unique<CL_GL_SYNC_INFO>()) {
}

bool GlArbSyncEvent::setBaseEvent(Event &ev) {
    UNRECOVERABLE_IF(this->baseEvent != nullptr);
    UNRECOVERABLE_IF(ev.getContext() == nullptr);
    UNRECOVERABLE_IF(ev.getCommandQueue() == nullptr);
    auto cmdQueue = ev.getCommandQueue();
    auto osInterface = cmdQueue->getGpgpuCommandStreamReceiver().getOSInterface();
    UNRECOVERABLE_IF(osInterface == nullptr);
    if (false == ctx->getSharing<NEO::GLSharingFunctions>()->glArbSyncObjectSetup(*osInterface, *glSyncInfo)) {
        return false;
    }

    this->baseEvent = &ev;
    this->cmdQueue = cmdQueue;
    this->cmdQueue->incRefInternal();
    this->baseEvent->incRefInternal();
    this->osInterface = osInterface;
    ev.addChild(*this);
    return true;
}

GlArbSyncEvent::~GlArbSyncEvent() {
    if (baseEvent != nullptr) {
        ctx->getSharing<NEO::GLSharingFunctions>()->glArbSyncObjectCleanup(*osInterface, glSyncInfo.get());
        baseEvent->decRefInternal();
    }
}

GlArbSyncEvent *GlArbSyncEvent::create(Event &baseEvent) {
    if (baseEvent.getContext() == nullptr) {
        return nullptr;
    }
    auto arbSyncEvent = new GlArbSyncEvent(*baseEvent.getContext());
    if (false == arbSyncEvent->setBaseEvent(baseEvent)) {
        delete arbSyncEvent;
        arbSyncEvent = nullptr;
    }

    return arbSyncEvent;
}

void GlArbSyncEvent::unblockEventBy(Event &event, uint32_t taskLevel, int32_t transitionStatus) {
    DEBUG_BREAK_IF(&event != this->baseEvent);
    if ((transitionStatus > CL_SUBMITTED) || (transitionStatus < 0)) {
        return;
    }

    ctx->getSharing<NEO::GLSharingFunctions>()->glArbSyncObjectSignal(event.getCommandQueue()->getGpgpuCommandStreamReceiver().getOsContext(), *glSyncInfo);
    ctx->getSharing<NEO::GLSharingFunctions>()->glArbSyncObjectWaitServer(*osInterface, *glSyncInfo);
}
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
    if ((nullptr == pSyncInfoHandleRet) || (nullptr == pClContextRet)) {
        return CL_INVALID_ARG_VALUE;
    }

    auto neoEvent = NEO::castToObject<NEO::Event>(event);
    if (nullptr == neoEvent) {
        return CL_INVALID_EVENT;
    }

    if (neoEvent->getCommandType() != CL_COMMAND_RELEASE_GL_OBJECTS) {
        *pSyncInfoHandleRet = nullptr;
        *pClContextRet = static_cast<cl_context>(neoEvent->getContext());
        return CL_SUCCESS;
    }

    auto sharing = neoEvent->getContext()->getSharing<NEO::GLSharingFunctions>();
    if (sharing == nullptr) {
        return CL_INVALID_OPERATION;
    }

    NEO::GlArbSyncEvent *arbSyncEvent = sharing->getOrCreateGlArbSyncEvent(*neoEvent);
    if (nullptr == arbSyncEvent) {
        return CL_OUT_OF_RESOURCES;
    }

    neoEvent->updateExecutionStatus();
    CL_GL_SYNC_INFO *syncInfo = arbSyncEvent->getSyncInfo();
    *pSyncInfoHandleRet = syncInfo;
    *pClContextRet = static_cast<cl_context>(neoEvent->getContext());

    return CL_SUCCESS;
}

extern "C" CL_API_ENTRY cl_int CL_API_CALL
clReleaseGlSharedEventINTEL(cl_event event) {
    auto neoEvent = NEO::castToObject<NEO::Event>(event);
    if (nullptr == neoEvent) {
        return CL_INVALID_EVENT;
    }
    auto arbSyncEvent = neoEvent->getContext()->getSharing<NEO::GLSharingFunctions>()->getGlArbSyncEvent(*neoEvent);
    neoEvent->getContext()->getSharing<NEO::GLSharingFunctions>()->removeGlArbSyncEventMapping(*neoEvent);
    if (nullptr != arbSyncEvent) {
        arbSyncEvent->release();
    }
    neoEvent->release();

    return CL_SUCCESS;
}
