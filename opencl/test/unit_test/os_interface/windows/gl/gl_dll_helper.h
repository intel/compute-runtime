/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/os_library.h"

#include "opencl/extensions/public/cl_gl_private_intel.h"

#include "Gl/gl.h"

#include <memory>

using namespace NEO;
namespace Os {
extern const char *openglDllName;
}
namespace NEO {
struct GLMockReturnedValues;
}

using GLString = void (*)(const char *, unsigned int);
using GLSharedOCLContext = void (*)(GLboolean);
using glBoolean = GLboolean (*)();
using Void = void (*)(const char *);
using Int = int (*)(const char *);
using BufferParam = void (*)(CL_GL_BUFFER_INFO);
using TextureParam = void (*)(CL_GL_RESOURCE_INFO);
using BuffInfo = CL_GL_BUFFER_INFO (*)();
using TextureInfo = CL_GL_RESOURCE_INFO (*)();
using GLMockValue = GLMockReturnedValues (*)();
using setGLMockValue = void (*)(GLMockReturnedValues);

struct glDllHelper {
  public:
    glDllHelper() {
        glDllLoad.reset(OsLibrary::load(Os::openglDllName));
        if (glDllLoad) {
            glSetString = (*glDllLoad)["glSetString"];
            UNRECOVERABLE_IF(glSetString == nullptr);
            glSetStringi = (*glDllLoad)["glSetStringi"];
            UNRECOVERABLE_IF(glSetStringi == nullptr);
            setGLSetSharedOCLContextStateReturnedValue = (*glDllLoad)["setGLSetSharedOCLContextStateReturnedValue"];
            UNRECOVERABLE_IF(setGLSetSharedOCLContextStateReturnedValue == nullptr);
            getGLSetSharedOCLContextStateReturnedValue = (*glDllLoad)["getGLSetSharedOCLContextStateReturnedValue"];
            UNRECOVERABLE_IF(getGLSetSharedOCLContextStateReturnedValue == nullptr);
            resetParam = (*glDllLoad)["resetParam"];
            UNRECOVERABLE_IF(resetParam == nullptr);
            getParam = (*glDllLoad)["getParam"];
            UNRECOVERABLE_IF(getParam == nullptr);
            loadBuffer = (*glDllLoad)["loadBuffer"];
            UNRECOVERABLE_IF(loadBuffer == nullptr);
            getBufferInfo = (*glDllLoad)["getBufferInfo"];
            UNRECOVERABLE_IF(getBufferInfo == nullptr);
            getTextureInfo = (*glDllLoad)["getTextureInfo"];
            UNRECOVERABLE_IF(getTextureInfo == nullptr);
            Void memParam = (*glDllLoad)["memParam"];
            UNRECOVERABLE_IF(memParam == nullptr);
            loadTexture = (*glDllLoad)["loadTexture"];
            UNRECOVERABLE_IF(loadTexture == nullptr);
            getGlMockReturnedValues = (*glDllLoad)["getGlMockReturnedValues"];
            UNRECOVERABLE_IF(getGlMockReturnedValues == nullptr);
            setGlMockReturnedValues = (*glDllLoad)["setGlMockReturnedValues"];
            UNRECOVERABLE_IF(setGlMockReturnedValues == nullptr);
        }
    }
    ~glDllHelper() {
        if (glDllLoad) {
            glSetString("Intel", GL_VENDOR);
            glSetString("4.0", GL_VERSION);
            glSetStringi("GL_OES_framebuffer_object", 0);
            glSetStringi("GL_EXT_framebuffer_object", 1);
        }
    }
    GLString glSetString;
    GLString glSetStringi;
    GLSharedOCLContext setGLSetSharedOCLContextStateReturnedValue;
    glBoolean getGLSetSharedOCLContextStateReturnedValue;
    Void resetParam;
    Int getParam;

    BufferParam loadBuffer;
    TextureParam loadTexture;
    BuffInfo getBufferInfo;
    TextureInfo getTextureInfo;
    GLMockValue getGlMockReturnedValues;
    setGLMockValue setGlMockReturnedValues;

  private:
    std::unique_ptr<OsLibrary> glDllLoad;
};
