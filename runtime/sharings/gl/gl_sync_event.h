/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/event/event.h"

#include <GL/gl.h>

struct _tagGLCLSyncInfo;
typedef _tagGLCLSyncInfo GL_CL_SYNC_INFO;

namespace OCLRT {
class Context;

class GlSyncEvent : public Event {
  public:
    GlSyncEvent() = delete;
    GlSyncEvent(Context &context, const GL_CL_SYNC_INFO &sync);
    ~GlSyncEvent() override;

    static GlSyncEvent *create(Context &context, cl_GLsync sync, cl_int *errCode);
    void updateExecutionStatus() override;
    uint32_t getTaskLevel() override;

    bool isExternallySynchronized() const override { return true; }

  protected:
    std::unique_ptr<GL_CL_SYNC_INFO> glSync;
};
} // namespace OCLRT
