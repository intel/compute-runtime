/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/os_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

#include "opencl/extensions/public/cl_gl_private_intel.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/source/helpers/base_object.h"
#include "opencl/source/sharings/gl/gl_arb_sync_event.h"
#include "opencl/source/sharings/gl/windows/gl_sharing_windows.h"

#include <GL/gl.h>

namespace NEO {

void destroySync(Gdi &gdi, D3DKMT_HANDLE sync) {
    if (!sync) {
        return;
    }
    D3DKMT_DESTROYSYNCHRONIZATIONOBJECT destroySyncInfo = {};
    destroySyncInfo.hSyncObject = sync;
    NTSTATUS status = gdi.destroySynchronizationObject(&destroySyncInfo);
    DEBUG_BREAK_IF(STATUS_SUCCESS != status);
}

void destroyEvent(OSInterface &osInterface, HANDLE event) {
    if (!event) {
        return;
    }

    auto ret = osInterface.get()->closeHandle(event);
    DEBUG_BREAK_IF(TRUE != ret);
}

void cleanupArbSyncObject(OSInterface &osInterface, CL_GL_SYNC_INFO *glSyncInfo) {
    if (nullptr == glSyncInfo) {
        return;
    }

    auto gdi = osInterface.get()->getWddm()->getGdi();
    UNRECOVERABLE_IF(nullptr == gdi);

    destroySync(*gdi, glSyncInfo->serverSynchronizationObject);
    destroySync(*gdi, glSyncInfo->clientSynchronizationObject);
    destroySync(*gdi, glSyncInfo->submissionSynchronizationObject);
    destroyEvent(osInterface, glSyncInfo->event);
    destroyEvent(osInterface, glSyncInfo->submissionEvent);

    destroyArbSyncEventName(glSyncInfo->eventName);
    destroyArbSyncEventName(glSyncInfo->submissionEventName);
}

bool setupArbSyncObject(GLSharingFunctions &sharing, OSInterface &osInterface, CL_GL_SYNC_INFO &glSyncInfo) {
    auto &sharingFunctions = static_cast<GLSharingFunctionsWindows &>(sharing);

    glSyncInfo.hContextToBlock = static_cast<D3DKMT_HANDLE>(sharingFunctions.getGLContextHandle());
    auto glDevice = static_cast<D3DKMT_HANDLE>(sharingFunctions.getGLDeviceHandle());
    auto wddm = osInterface.get()->getWddm();

    D3DKMT_CREATESYNCHRONIZATIONOBJECT serverSyncInitInfo = {};
    serverSyncInitInfo.hDevice = glDevice;
    serverSyncInitInfo.Info.Type = D3DDDI_SEMAPHORE;
    serverSyncInitInfo.Info.Semaphore.MaxCount = 32;
    serverSyncInitInfo.Info.Semaphore.InitialCount = 0;
    NTSTATUS serverSyncInitStatus = wddm->getGdi()->createSynchronizationObject(&serverSyncInitInfo);
    glSyncInfo.serverSynchronizationObject = serverSyncInitInfo.hSyncObject;

    glSyncInfo.eventName = createArbSyncEventName();
    glSyncInfo.event = osInterface.get()->createEvent(NULL, TRUE, FALSE, glSyncInfo.eventName);

    D3DKMT_CREATESYNCHRONIZATIONOBJECT2 clientSyncInitInfo = {};
    clientSyncInitInfo.hDevice = glDevice;
    clientSyncInitInfo.Info.Type = D3DDDI_CPU_NOTIFICATION;
    clientSyncInitInfo.Info.CPUNotification.Event = glSyncInfo.event;
    NTSTATUS clientSyncInitStatus = wddm->getGdi()->createSynchronizationObject2(&clientSyncInitInfo);
    glSyncInfo.clientSynchronizationObject = clientSyncInitInfo.hSyncObject;

    D3DKMT_CREATESYNCHRONIZATIONOBJECT2 submissionSyncEventInfo = {};
    glSyncInfo.submissionEventName = createArbSyncEventName();
    glSyncInfo.submissionEvent = osInterface.get()->createEvent(NULL, TRUE, FALSE, glSyncInfo.submissionEventName);

    submissionSyncEventInfo.hDevice = glDevice;
    submissionSyncEventInfo.Info.Type = D3DDDI_CPU_NOTIFICATION;
    submissionSyncEventInfo.Info.CPUNotification.Event = glSyncInfo.submissionEvent;
    auto submissionSyncInitStatus = wddm->getGdi()->createSynchronizationObject2(&submissionSyncEventInfo);

    glSyncInfo.submissionSynchronizationObject = submissionSyncEventInfo.hSyncObject;
    glSyncInfo.waitCalled = false;

    bool setupFailed =
        (glSyncInfo.event == nullptr) ||
        (glSyncInfo.submissionEvent == nullptr) ||
        (STATUS_SUCCESS != serverSyncInitStatus) ||
        (STATUS_SUCCESS != clientSyncInitStatus) ||
        (STATUS_SUCCESS != submissionSyncInitStatus);

    if (setupFailed) {
        DEBUG_BREAK_IF(true);
        cleanupArbSyncObject(osInterface, &glSyncInfo);
        return false;
    }

    return true;
}

void signalArbSyncObject(OsContext &osContext, CL_GL_SYNC_INFO &glSyncInfo) {
    auto osContextWin = static_cast<OsContextWin *>(&osContext);
    UNRECOVERABLE_IF(!osContextWin);
    auto wddm = osContextWin->getWddm();

    D3DKMT_SIGNALSYNCHRONIZATIONOBJECT signalServerClientSyncInfo = {};
    signalServerClientSyncInfo.hContext = osContextWin->getWddmContextHandle();
    signalServerClientSyncInfo.Flags.SignalAtSubmission = 0; // Wait for GPU to complete processing command buffer
    signalServerClientSyncInfo.ObjectHandleArray[0] = glSyncInfo.serverSynchronizationObject;
    signalServerClientSyncInfo.ObjectHandleArray[1] = glSyncInfo.clientSynchronizationObject;
    signalServerClientSyncInfo.ObjectCount = 2;
    NTSTATUS status = wddm->getGdi()->signalSynchronizationObject(&signalServerClientSyncInfo);
    if (STATUS_SUCCESS != status) {
        DEBUG_BREAK_IF(true);
        return;
    }

    D3DKMT_SIGNALSYNCHRONIZATIONOBJECT signalSubmissionSyncInfo = {};
    signalSubmissionSyncInfo.hContext = osContextWin->getWddmContextHandle();
    signalSubmissionSyncInfo.Flags.SignalAtSubmission = 1; // Don't wait for GPU to complete processing command buffer
    signalSubmissionSyncInfo.ObjectHandleArray[0] = glSyncInfo.submissionSynchronizationObject;
    signalSubmissionSyncInfo.ObjectCount = 1;
    status = wddm->getGdi()->signalSynchronizationObject(&signalSubmissionSyncInfo);
    DEBUG_BREAK_IF(STATUS_SUCCESS != status);
}

void serverWaitForArbSyncObject(OSInterface &osInterface, CL_GL_SYNC_INFO &glSyncInfo) {
    auto wddm = osInterface.get()->getWddm();

    D3DKMT_WAITFORSYNCHRONIZATIONOBJECT waitForSyncInfo = {};
    waitForSyncInfo.hContext = glSyncInfo.hContextToBlock;
    waitForSyncInfo.ObjectCount = 1;
    waitForSyncInfo.ObjectHandleArray[0] = glSyncInfo.serverSynchronizationObject;

    NTSTATUS status = wddm->getGdi()->waitForSynchronizationObject(&waitForSyncInfo);
    if (status != STATUS_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return;
    }
    glSyncInfo.waitCalled = true;
}

GlArbSyncEvent::GlArbSyncEvent(Context &context)
    : Event(&context, nullptr, CL_COMMAND_GL_FENCE_SYNC_OBJECT_KHR, CompletionStamp::levelNotReady, CompletionStamp::levelNotReady),
      glSyncInfo(std::make_unique<CL_GL_SYNC_INFO>()) {
}

bool GlArbSyncEvent::setBaseEvent(Event &ev) {
    UNRECOVERABLE_IF(this->baseEvent != nullptr);
    UNRECOVERABLE_IF(ev.getContext() == nullptr);
    UNRECOVERABLE_IF(ev.getCommandQueue() == nullptr);
    auto cmdQueue = ev.getCommandQueue();
    auto osInterface = cmdQueue->getGpgpuCommandStreamReceiver().getOSInterface();
    UNRECOVERABLE_IF(osInterface == nullptr);
    if (false == ctx->getSharing<GLSharingFunctionsWindows>()->glArbSyncObjectSetup(*osInterface, *glSyncInfo)) {
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
        ctx->getSharing<GLSharingFunctionsWindows>()->glArbSyncObjectCleanup(*osInterface, glSyncInfo.get());
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

    ctx->getSharing<GLSharingFunctionsWindows>()->glArbSyncObjectSignal(event.getCommandQueue()->getGpgpuCommandStreamReceiver().getOsContext(), *glSyncInfo);
    ctx->getSharing<GLSharingFunctionsWindows>()->glArbSyncObjectWaitServer(*osInterface, *glSyncInfo);
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

    auto sharing = neoEvent->getContext()->getSharing<NEO::GLSharingFunctionsWindows>();
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
    auto arbSyncEvent = neoEvent->getContext()->getSharing<NEO::GLSharingFunctionsWindows>()->getGlArbSyncEvent(*neoEvent);
    neoEvent->getContext()->getSharing<NEO::GLSharingFunctionsWindows>()->removeGlArbSyncEventMapping(*neoEvent);
    if (nullptr != arbSyncEvent) {
        arbSyncEvent->release();
    }
    neoEvent->release();

    return CL_SUCCESS;
}
