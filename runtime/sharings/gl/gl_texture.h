/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "public/cl_gl_private_intel.h"
#include "runtime/sharings/gl/gl_sharing.h"

#include "CL/cl_gl.h"

namespace NEO {
class Context;
class Image;
class GlTexture : GlSharing {
  public:
    static Image *createSharedGlTexture(Context *context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture,
                                        cl_int *errcodeRet);
    void synchronizeObject(UpdateData &updateData) override;
    cl_int getGlTextureInfo(cl_gl_texture_info paramName, size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet) const;
    cl_GLint getMiplevel() const { return miplevel; }
    CL_GL_RESOURCE_INFO *getTextureInfo() { return &textureInfo; }
    cl_GLenum getTarget() const { return target; }

    static bool setClImageFormat(int glFormat, cl_image_format &clImgFormat);
    static cl_mem_object_type getClMemObjectType(cl_GLenum glType);
    static cl_gl_object_type getClGlObjectType(cl_GLenum glType);
    static cl_GLenum getBaseTargetType(cl_GLenum target);

  protected:
    GlTexture(GLSharingFunctions *sharingFunctions, unsigned int glObjectType, unsigned int glObjectId, CL_GL_RESOURCE_INFO texInfo,
              cl_GLenum target, cl_GLint miplevel)
        : GlSharing(sharingFunctions, glObjectType, glObjectId), target(target), miplevel(miplevel), textureInfo(texInfo){};

    static uint32_t getClObjectType(cl_GLenum glType, bool returnClGlObjectType);

    void releaseResource(MemObj *memObject) override;

    cl_GLenum target;
    cl_GLint miplevel;
    CL_GL_RESOURCE_INFO textureInfo;
};
} // namespace NEO
