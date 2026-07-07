/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/opencl/extensions/public/cl_gl_private_intel.h"
#include "level_zero/api/opencl/source/event/leo_event.h"

#include <GL/gl.h>

struct _tagGLCLSyncInfo;
typedef _tagGLCLSyncInfo GL_CL_SYNC_INFO;

namespace NEO {
namespace LEO {

class Context;

class GlSyncEvent : public Event {
  public:
    GlSyncEvent() = delete;
    GlSyncEvent(Context &context, const GL_CL_SYNC_INFO &sync);
    ~GlSyncEvent() override;

    static GlSyncEvent *create(Context &context, cl_GLsync sync, cl_int *errCode);
    static void signalGlEvent(void *userData);

  protected:
    std::unique_ptr<GL_CL_SYNC_INFO> glSync;
};

} // namespace LEO
} // namespace NEO
