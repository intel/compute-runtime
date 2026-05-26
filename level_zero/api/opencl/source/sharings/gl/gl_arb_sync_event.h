/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <CL/cl.h>

#include <memory>

struct _tagCLGLSyncInfo;
typedef _tagCLGLSyncInfo CL_GL_SYNC_INFO;

namespace NEO {
class OSInterface;
class OsContext;
} // namespace NEO

namespace NEO {
namespace LEO {

class GLSharingFunctions;
class Event;

using OSInterface = NEO::OSInterface;
using OsContext = NEO::OsContext;

char *createArbSyncEventName();
void destroyArbSyncEventName(char *name);

void cleanupArbSyncObject(OSInterface &osInterface, CL_GL_SYNC_INFO *glSyncInfo);
bool setupArbSyncObject(GLSharingFunctions &sharing, OSInterface &osInterface, CL_GL_SYNC_INFO &glSyncInfo);
void signalArbSyncObject(OsContext &osContext, CL_GL_SYNC_INFO &glSyncInfo);
void serverWaitForArbSyncObject(OSInterface &osInterface, CL_GL_SYNC_INFO &glSyncInfo, bool cpuSync);

class GlArbSyncEvent {
  public:
    GlArbSyncEvent() = delete;
    ~GlArbSyncEvent();

    static void unblockGlArbEvent(cl_event event, cl_int status, void *userData);
    static GlArbSyncEvent *create(Event &baseEvent);

    CL_GL_SYNC_INFO *getSyncInfo() {
        return glSyncInfo.get();
    }

  protected:
    GlArbSyncEvent(Event &ev);

    Event *baseEvent = nullptr;
    std::unique_ptr<CL_GL_SYNC_INFO> glSyncInfo;
};

} // namespace LEO
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
