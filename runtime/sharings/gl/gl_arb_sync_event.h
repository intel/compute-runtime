/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/event/event.h"

struct _tagCLGLSyncInfo;
typedef _tagCLGLSyncInfo CL_GL_SYNC_INFO;

namespace NEO {
class Context;
class GLSharingFunctions;
class OsInterface;
class OsContext;

char *createArbSyncEventName();
void destroyArbSyncEventName(char *name);

void cleanupArbSyncObject(OSInterface &osInterface, CL_GL_SYNC_INFO *glSyncInfo);
bool setupArbSyncObject(GLSharingFunctions &sharing, OSInterface &osInterface, CL_GL_SYNC_INFO &glSyncInfo);
void signalArbSyncObject(OsContext &osContext, CL_GL_SYNC_INFO &glSyncInfo);
void serverWaitForArbSyncObject(OSInterface &osInterface, CL_GL_SYNC_INFO &glSyncInfo);

class GlArbSyncEvent : public Event {
  public:
    GlArbSyncEvent() = delete;
    ~GlArbSyncEvent() override;

    void unblockEventBy(Event &event, uint32_t taskLevel, int32_t transitionStatus) override;

    static GlArbSyncEvent *create(Event &baseEvent);

    CL_GL_SYNC_INFO *getSyncInfo() {
        return glSyncInfo.get();
    }

  protected:
    GlArbSyncEvent(Context &context);
    MOCKABLE_VIRTUAL bool setBaseEvent(Event &ev);

    Event *baseEvent = nullptr;
    OSInterface *osInterface = nullptr;
    std::unique_ptr<CL_GL_SYNC_INFO> glSyncInfo;
};
} // namespace NEO

extern "C" CL_API_ENTRY cl_int CL_API_CALL
clEnqueueMarkerWithSyncObjectINTEL(cl_command_queue commandQueue,
                                   cl_event *event,
                                   cl_context *context);

extern "C" CL_API_ENTRY cl_int CL_API_CALL
clGetCLObjectInfoINTEL(cl_mem memObj,
                       void *pResourceInfo);

extern "C" CL_API_ENTRY cl_int CL_API_CALL
clGetCLEventInfoINTEL(cl_event event, CL_GL_SYNC_INFO **pSyncInfoHandleRet, cl_context *pClContextRet);

extern "C" CL_API_ENTRY cl_int CL_API_CALL
clReleaseGlSharedEventINTEL(cl_event event);
