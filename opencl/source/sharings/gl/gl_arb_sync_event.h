/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/mockable_virtual_defaults.h"

#include "opencl/source/event/event.h"
#include "opencl/source/sharings/gl/cl_gl_api_intel.h"

namespace NEO {
class Context;
class GLSharingFunctions;
class OSInterface;
class OsContext;

char *createArbSyncEventName();
void destroyArbSyncEventName(char *name);

void cleanupArbSyncObject(OSInterface &osInterface, CL_GL_SYNC_INFO *glSyncInfo);
bool setupArbSyncObject(GLSharingFunctions &sharing, OSInterface &osInterface, CL_GL_SYNC_INFO &glSyncInfo);
void signalArbSyncObject(OsContext &osContext, CL_GL_SYNC_INFO &glSyncInfo, bool signalAtSubmission);
void serverWaitForArbSyncObject(OSInterface &osInterface, CL_GL_SYNC_INFO &glSyncInfo);

class GlArbSyncEvent : public Event {
  public:
    GlArbSyncEvent() = delete;
    ~GlArbSyncEvent() override;

    void unblockEventBy(Event &event, TaskCountType taskLevel, int32_t transitionStatus) override;

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
