/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/event/event.h"

#include <GL/gl.h>

struct _tagGLCLSyncInfo;
typedef _tagGLCLSyncInfo GL_CL_SYNC_INFO;

namespace NEO {
class Context;

class GlSyncEvent : public Event {
  public:
    GlSyncEvent() = delete;
    GlSyncEvent(Context &context, const GL_CL_SYNC_INFO &sync);
    ~GlSyncEvent() override;

    static GlSyncEvent *create(Context &context, cl_GLsync sync, cl_int *errCode);
    void updateExecutionStatus() override;
    TaskCountType getTaskLevel() override;

    bool isExternallySynchronized() const override { return true; }

  protected:
    std::unique_ptr<GL_CL_SYNC_INFO> glSync;
};
} // namespace NEO
