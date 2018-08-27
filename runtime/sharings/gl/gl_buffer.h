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

#include "runtime/sharings/gl/gl_sharing.h"
#include "CL/cl_gl.h"

struct _tagCLGLBufferInfo;

namespace OCLRT {
class Buffer;
class Context;

class GlBuffer : public GlSharing {
  public:
    static Buffer *createSharedGlBuffer(Context *context, cl_mem_flags flags, unsigned int bufferId, cl_int *errcodeRet);
    void synchronizeObject(UpdateData &updateData) override;
    void releaseReusedGraphicsAllocation() override;

  protected:
    GlBuffer(GLSharingFunctions *sharingFunctions, unsigned int glObjectId)
        : GlSharing(sharingFunctions, CL_GL_OBJECT_BUFFER, glObjectId){};

    void releaseResource(MemObj *memObject) override;

    void resolveGraphicsAllocationChange(osHandle currentSharedHandle, UpdateData *updateData) override;
    void popGraphicsAllocationFromReuse(GraphicsAllocation *graphicsAllocation);

    static GraphicsAllocation *createGraphicsAllocation(Context *context, unsigned int bufferId, _tagCLGLBufferInfo &bufferInfo);
};
} // namespace OCLRT
