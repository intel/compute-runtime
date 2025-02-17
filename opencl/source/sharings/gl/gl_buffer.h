/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "opencl/source/sharings/gl/gl_sharing.h"

#include "CL/cl_gl.h"

struct _tagCLGLBufferInfo;

namespace NEO {
class Buffer;
class Context;

class GlBuffer : public GlSharing, NEO::NonCopyableAndNonMovableClass {
  public:
    static Buffer *createSharedGlBuffer(Context *context, cl_mem_flags flags, unsigned int bufferId, cl_int *errcodeRet);
    void synchronizeObject(UpdateData &updateData) override;
    void releaseReusedGraphicsAllocation() override;

  protected:
    GlBuffer(GLSharingFunctions *sharingFunctions, unsigned int glObjectId)
        : GlSharing(sharingFunctions, CL_GL_OBJECT_BUFFER, glObjectId){};
    ~GlBuffer() override {
        callReleaseResource(true);
    }
    void releaseResource(MemObj *memObject, uint32_t rootDeviceIndex) override;
    void callReleaseResource(bool createOrDestroy);

    void resolveGraphicsAllocationChange(osHandle currentSharedHandle, UpdateData *updateData) override;
    void popGraphicsAllocationFromReuse(GraphicsAllocation *graphicsAllocation);

    static GraphicsAllocation *createGraphicsAllocation(Context *context, unsigned int bufferId, _tagCLGLBufferInfo &bufferInfo);
};

static_assert(NEO::NonCopyableAndNonMovable<GlBuffer>);

} // namespace NEO
