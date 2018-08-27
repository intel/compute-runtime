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

#pragma once
#include "runtime/event/event.h"

struct _tagCLGLSyncInfo;
typedef _tagCLGLSyncInfo CL_GL_SYNC_INFO;

namespace OCLRT {
class Context;
class GLSharingFunctions;
class OsInterface;

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
} // namespace OCLRT

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
