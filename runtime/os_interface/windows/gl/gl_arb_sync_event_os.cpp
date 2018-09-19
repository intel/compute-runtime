/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/sharings/gl/gl_arb_sync_event.h"

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/context/context.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/os_interface/windows/os_context_win.h"
#include "runtime/os_interface/windows/os_interface.h"
#include "runtime/os_interface/windows/wddm/wddm.h"
#include "runtime/os_interface/windows/gdi_interface.h"
#include "runtime/sharings/gl/gl_sharing.h"

#include <GL/gl.h>
#include "public/cl_gl_private_intel.h"

namespace OCLRT {

void destroySync(Gdi &gdi, D3DKMT_HANDLE sync) {
    if (!sync) {
        return;
    }
    D3DKMT_DESTROYSYNCHRONIZATIONOBJECT destroySyncInfo = {0};
    destroySyncInfo.hSyncObject = sync;
    NTSTATUS status = gdi.destroySynchronizationObject(&destroySyncInfo);
    DEBUG_BREAK_IF(0 != status);
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
    glSyncInfo.hContextToBlock = static_cast<D3DKMT_HANDLE>(sharing.getGLContextHandle());
    auto glDevice = static_cast<D3DKMT_HANDLE>(sharing.getGLDeviceHandle());
    auto wddm = osInterface.get()->getWddm();

    D3DKMT_CREATESYNCHRONIZATIONOBJECT serverSyncInitInfo = {0};
    serverSyncInitInfo.hDevice = glDevice;
    serverSyncInitInfo.Info.Type = D3DDDI_SEMAPHORE;
    serverSyncInitInfo.Info.Semaphore.MaxCount = 32;
    serverSyncInitInfo.Info.Semaphore.InitialCount = 0;
    NTSTATUS serverSyncInitStatus = wddm->getGdi()->createSynchronizationObject(&serverSyncInitInfo);
    glSyncInfo.serverSynchronizationObject = serverSyncInitInfo.hSyncObject;

    glSyncInfo.eventName = createArbSyncEventName();
    glSyncInfo.event = osInterface.get()->createEvent(NULL, TRUE, FALSE, glSyncInfo.eventName);

    D3DKMT_CREATESYNCHRONIZATIONOBJECT2 clientSyncInitInfo = {0};
    clientSyncInitInfo.hDevice = glDevice;
    clientSyncInitInfo.Info.Type = D3DDDI_CPU_NOTIFICATION;
    clientSyncInitInfo.Info.CPUNotification.Event = glSyncInfo.event;
    NTSTATUS clientSyncInitStatus = wddm->getGdi()->createSynchronizationObject2(&clientSyncInitInfo);
    glSyncInfo.clientSynchronizationObject = clientSyncInitInfo.hSyncObject;

    D3DKMT_CREATESYNCHRONIZATIONOBJECT2 submissionSyncEventInfo = {0};
    glSyncInfo.submissionEventName = createArbSyncEventName();
    glSyncInfo.submissionEvent = osInterface.get()->createEvent(NULL, TRUE, FALSE, glSyncInfo.submissionEventName);

    submissionSyncEventInfo.hDevice = glDevice;
    submissionSyncEventInfo.Info.Type = D3DDDI_CPU_NOTIFICATION;
    submissionSyncEventInfo.Info.CPUNotification.Event = glSyncInfo.submissionEvent;
    auto submissionSyncInitStatus = wddm->getGdi()->createSynchronizationObject2(&submissionSyncEventInfo);

    glSyncInfo.submissionSynchronizationObject = submissionSyncEventInfo.hSyncObject;
    glSyncInfo.waitCalled = false;

    bool setupFailed = false;
    setupFailed |= (glSyncInfo.event == nullptr);
    setupFailed |= (glSyncInfo.submissionEvent == nullptr);
    setupFailed |= (0 != serverSyncInitStatus);
    setupFailed |= (0 != clientSyncInitStatus);
    setupFailed |= (0 != submissionSyncInitStatus);

    if (setupFailed) {
        DEBUG_BREAK_IF(true);
        cleanupArbSyncObject(osInterface, &glSyncInfo);
        return false;
    }

    return true;
}

void signalArbSyncObject(OsContext &osContext, CL_GL_SYNC_INFO &glSyncInfo) {
    auto osContextWin = osContext.get();
    UNRECOVERABLE_IF(!osContextWin);
    auto wddm = osContextWin->getWddm();

    D3DKMT_SIGNALSYNCHRONIZATIONOBJECT signalServerClientSyncInfo = {0};
    signalServerClientSyncInfo.hContext = osContextWin->getContext();
    signalServerClientSyncInfo.Flags.SignalAtSubmission = 0; // Wait for GPU to complete processing command buffer
    signalServerClientSyncInfo.ObjectHandleArray[0] = glSyncInfo.serverSynchronizationObject;
    signalServerClientSyncInfo.ObjectHandleArray[1] = glSyncInfo.clientSynchronizationObject;
    signalServerClientSyncInfo.ObjectCount = 2;
    NTSTATUS status = wddm->getGdi()->signalSynchronizationObject(&signalServerClientSyncInfo);
    if (0 != status) {
        DEBUG_BREAK_IF(true);
        return;
    }

    D3DKMT_SIGNALSYNCHRONIZATIONOBJECT signalSubmissionSyncInfo = {0};
    signalSubmissionSyncInfo.hContext = osContext.get()->getContext();
    signalSubmissionSyncInfo.Flags.SignalAtSubmission = 1; // Don't wait for GPU to complete processing command buffer
    signalSubmissionSyncInfo.ObjectHandleArray[0] = glSyncInfo.submissionSynchronizationObject;
    signalSubmissionSyncInfo.ObjectCount = 1;
    status = wddm->getGdi()->signalSynchronizationObject(&signalSubmissionSyncInfo);
    DEBUG_BREAK_IF(0 != status);
}

void serverWaitForArbSyncObject(OSInterface &osInterface, CL_GL_SYNC_INFO &glSyncInfo) {
    auto wddm = osInterface.get()->getWddm();

    D3DKMT_WAITFORSYNCHRONIZATIONOBJECT waitForSyncInfo = {0};
    waitForSyncInfo.hContext = glSyncInfo.hContextToBlock;
    waitForSyncInfo.ObjectCount = 1;
    waitForSyncInfo.ObjectHandleArray[0] = glSyncInfo.serverSynchronizationObject;

    NTSTATUS status = wddm->getGdi()->waitForSynchronizationObject(&waitForSyncInfo);
    if (status != 0) {
        DEBUG_BREAK_IF(true);
        return;
    }
    glSyncInfo.waitCalled = true;
}
} // namespace OCLRT
