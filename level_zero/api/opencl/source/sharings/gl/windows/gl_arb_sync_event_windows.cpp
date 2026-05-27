/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

#include "level_zero/api/opencl/extensions/public/cl_gl_private_intel.h"
#include "level_zero/api/opencl/source/command_queue/command_queue.h"
#include "level_zero/api/opencl/source/context/context.h"
#include "level_zero/api/opencl/source/event/event.h"
#include "level_zero/api/opencl/source/helpers/base_object.h"
#include "level_zero/api/opencl/source/sharings/gl/gl_arb_sync_event.h"
#include "level_zero/api/opencl/source/sharings/gl/windows/gl_sharing_windows.h"

#include <GL/gl.h>

namespace NEO {
namespace LEO {

void destroySync(NEO::Gdi &gdi, D3DKMT_HANDLE sync) {
    if (!sync) {
        return;
    }
    D3DKMT_DESTROYSYNCHRONIZATIONOBJECT destroySyncInfo = {};
    destroySyncInfo.hSyncObject = sync;
    [[maybe_unused]] NTSTATUS status = gdi.destroySynchronizationObject(&destroySyncInfo);
    DEBUG_BREAK_IF(STATUS_SUCCESS != status);
}

void destroyEvent(NEO::OSInterface &osInterface, HANDLE event) {
    if (!event) {
        return;
    }

    [[maybe_unused]] auto ret = NEO::SysCalls::closeHandle(event);
    DEBUG_BREAK_IF(TRUE != ret);
}

void cleanupArbSyncObject(NEO::OSInterface &osInterface, CL_GL_SYNC_INFO *glSyncInfo) {
    if (nullptr == glSyncInfo) {
        return;
    }

    auto gdi = osInterface.getDriverModel()->as<NEO::Wddm>()->getGdi();
    UNRECOVERABLE_IF(nullptr == gdi);

    destroySync(*gdi, glSyncInfo->serverSynchronizationObject);
    destroySync(*gdi, glSyncInfo->clientSynchronizationObject);
    destroySync(*gdi, glSyncInfo->submissionSynchronizationObject);
    destroyEvent(osInterface, glSyncInfo->event);
    destroyEvent(osInterface, glSyncInfo->submissionEvent);

    destroyArbSyncEventName(glSyncInfo->eventName);
    destroyArbSyncEventName(glSyncInfo->submissionEventName);
}

bool setupArbSyncObject(GLSharingFunctions &sharing, NEO::OSInterface &osInterface, CL_GL_SYNC_INFO &glSyncInfo) {
    auto &sharingFunctions = static_cast<GLSharingFunctionsWindows &>(sharing);

    glSyncInfo.hContextToBlock = static_cast<D3DKMT_HANDLE>(sharingFunctions.getGLContextHandle());
    auto glDevice = static_cast<D3DKMT_HANDLE>(sharingFunctions.getGLDeviceHandle());
    auto wddm = osInterface.getDriverModel()->as<NEO::Wddm>();

    D3DKMT_CREATESYNCHRONIZATIONOBJECT serverSyncInitInfo = {};
    serverSyncInitInfo.hDevice = glDevice;
    serverSyncInitInfo.Info.Type = D3DDDI_SEMAPHORE;
    serverSyncInitInfo.Info.Semaphore.MaxCount = 32;
    serverSyncInitInfo.Info.Semaphore.InitialCount = 0;
    NTSTATUS serverSyncInitStatus = wddm->getGdi()->createSynchronizationObject(&serverSyncInitInfo);
    glSyncInfo.serverSynchronizationObject = serverSyncInitInfo.hSyncObject;

    glSyncInfo.eventName = createArbSyncEventName();
    glSyncInfo.event = SysCalls::createEvent(nullptr, TRUE, FALSE, glSyncInfo.eventName);

    D3DKMT_CREATESYNCHRONIZATIONOBJECT2 clientSyncInitInfo = {};
    clientSyncInitInfo.hDevice = glDevice;
    clientSyncInitInfo.Info.Type = D3DDDI_CPU_NOTIFICATION;
    clientSyncInitInfo.Info.CPUNotification.Event = glSyncInfo.event;
    NTSTATUS clientSyncInitStatus = wddm->getGdi()->createSynchronizationObject2(&clientSyncInitInfo);
    glSyncInfo.clientSynchronizationObject = clientSyncInitInfo.hSyncObject;

    D3DKMT_CREATESYNCHRONIZATIONOBJECT2 submissionSyncEventInfo = {};
    glSyncInfo.submissionEventName = createArbSyncEventName();
    glSyncInfo.submissionEvent = SysCalls::createEvent(nullptr, TRUE, FALSE, glSyncInfo.submissionEventName);

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

void signalArbSyncObject(NEO::OsContext &osContext, CL_GL_SYNC_INFO &glSyncInfo) {
    auto osContextWin = static_cast<OsContextWin *>(&osContext);
    UNRECOVERABLE_IF(!osContextWin);
    auto wddm = osContextWin->getWddm();

    D3DKMT_SIGNALSYNCHRONIZATIONOBJECT signalServerClientSyncInfo = {};
    signalServerClientSyncInfo.hContext = osContextWin->getWddmContextHandle();
    signalServerClientSyncInfo.Flags.SignalAtSubmission = 0; // Wait for GPU to complete processing command buffer
    signalServerClientSyncInfo.ObjectHandleArray[0] = glSyncInfo.serverSynchronizationObject;
    signalServerClientSyncInfo.ObjectHandleArray[1] = glSyncInfo.clientSynchronizationObject;
    signalServerClientSyncInfo.ObjectCount = 2;
    [[maybe_unused]] NTSTATUS status = wddm->getGdi()->signalSynchronizationObject(&signalServerClientSyncInfo);
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

void serverWaitForArbSyncObject(NEO::OSInterface &osInterface, CL_GL_SYNC_INFO &glSyncInfo, bool cpuSync) {
    auto wddm = osInterface.getDriverModel()->as<Wddm>();

    D3DKMT_WAITFORSYNCHRONIZATIONOBJECT waitForSyncInfo = {};
    waitForSyncInfo.hContext = glSyncInfo.hContextToBlock;
    waitForSyncInfo.ObjectCount = 1;
    waitForSyncInfo.ObjectHandleArray[0] = glSyncInfo.serverSynchronizationObject;

    NTSTATUS status = wddm->getGdi()->waitForSynchronizationObject(&waitForSyncInfo);
    if (status != STATUS_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return;
    }

    if (cpuSync) {
        [[maybe_unused]] auto ret = SysCalls::waitForSingleObject(glSyncInfo.submissionEvent, INFINITE);
        if (ret != WAIT_OBJECT_0) {
            DEBUG_BREAK_IF(true);
            return;
        }
    }

    glSyncInfo.waitCalled = true;
}

GlArbSyncEvent::GlArbSyncEvent(Event &ev) : baseEvent(&ev), glSyncInfo(std::make_unique<CL_GL_SYNC_INFO>()) {
    ev.getContext()->getSharing<GLSharingFunctionsWindows>()->glArbSyncObjectSetup(*ev.getL0Object()->getDevice()->getNEODevice()->getDefaultEngine().commandStreamReceiver->getOSInterface(), *glSyncInfo);
}

GlArbSyncEvent::~GlArbSyncEvent() {
    baseEvent->getContext()->getSharing<GLSharingFunctionsWindows>()->glArbSyncObjectCleanup(*baseEvent->getL0Object()->getDevice()->getNEODevice()->getDefaultEngine().commandStreamReceiver->getOSInterface(), glSyncInfo.get());
}

GlArbSyncEvent *GlArbSyncEvent::create(Event &baseEvent) {
    auto arbSyncEvent = new GlArbSyncEvent(baseEvent);
    baseEvent.setGlArbSyncEvent(arbSyncEvent);

    auto alreadyCompletedData = new bool(baseEvent.queryAndUpdateEventStatus() == CL_COMPLETE);

    clSetEventCallback(&baseEvent, CL_COMPLETE, GlArbSyncEvent::unblockGlArbEvent, alreadyCompletedData);

    return arbSyncEvent;
}

void GlArbSyncEvent::unblockGlArbEvent(cl_event event, cl_int status, void *userData) {
    auto alreadyCompletedData = reinterpret_cast<bool *>(userData);

    auto pEvent = NEO::LEO::castToObject<NEO::LEO::Event>(event);

    if (*alreadyCompletedData) {
        auto lock = pEvent->getL0Object()->getDevice()->getNEODevice()->getDefaultEngine().commandStreamReceiver->obtainUniqueOwnership();
        pEvent->getL0Object()->getDevice()->getNEODevice()->getDefaultEngine().commandStreamReceiver->stopDirectSubmission(true, true);
    }

    pEvent->getContext()->getSharing<GLSharingFunctionsWindows>()->glArbSyncObjectSignal(*pEvent->getL0Object()->getDevice()->getNEODevice()->getDefaultEngine().osContext, *pEvent->getGlArbSyncEvent()->getSyncInfo());
    pEvent->getContext()->getSharing<GLSharingFunctionsWindows>()->glArbSyncObjectWaitServer(*pEvent->getL0Object()->getDevice()->getNEODevice()->getDefaultEngine().commandStreamReceiver->getOSInterface(), *pEvent->getGlArbSyncEvent()->getSyncInfo(), *alreadyCompletedData);

    delete alreadyCompletedData;
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
    if ((nullptr == pSyncInfoHandleRet) || (nullptr == pClContextRet)) {
        return CL_INVALID_ARG_VALUE;
    }

    auto neoEvent = NEO::LEO::castToObject<NEO::LEO::Event>(event);
    if (nullptr == neoEvent) {
        return CL_INVALID_EVENT;
    }

    if (neoEvent->getCommandType() != CL_COMMAND_RELEASE_GL_OBJECTS) {
        *pSyncInfoHandleRet = nullptr;
        *pClContextRet = static_cast<cl_context>(neoEvent->getContext());
        return CL_SUCCESS;
    }

    auto sharing = neoEvent->getContext()->getSharing<NEO::LEO::GLSharingFunctionsWindows>();
    if (sharing == nullptr) {
        return CL_INVALID_OPERATION;
    }

    NEO::LEO::GlArbSyncEvent *arbSyncEvent = sharing->getOrCreateGlArbSyncEvent(*neoEvent);
    if (nullptr == arbSyncEvent) {
        return CL_OUT_OF_RESOURCES;
    }

    CL_GL_SYNC_INFO *syncInfo = arbSyncEvent->getSyncInfo();
    *pSyncInfoHandleRet = syncInfo;
    *pClContextRet = static_cast<cl_context>(neoEvent->getContext());

    return CL_SUCCESS;
}

extern "C" CL_API_ENTRY cl_int CL_API_CALL
clReleaseGlSharedEventINTEL(cl_event event) {
    auto neoEvent = NEO::LEO::castToObject<NEO::LEO::Event>(event);
    if (nullptr == neoEvent) {
        return CL_INVALID_EVENT;
    }
    neoEvent->release();

    return CL_SUCCESS;
}
