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
