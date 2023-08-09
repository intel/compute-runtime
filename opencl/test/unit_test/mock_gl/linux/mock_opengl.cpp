/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/gl/linux/mock_gl_sharing_linux.h"

#include "GL/gl.h"

#include <string.h>

extern "C" {
const char *glString = "Intel";
const char *glVersion = "4.0";
const char *arrayStringi[2]{"GL_OES_framebuffer_object", "GL_EXT_framebuffer_object"};
CL_GL_BUFFER_INFO bufferInfoInput = {0};
CL_GL_BUFFER_INFO bufferInfoOutput = {0};
CL_GL_RESOURCE_INFO textureInfoInput = {0};
CL_GL_RESOURCE_INFO textureInfoOutput = {0};
GLboolean glSetSharedOCLContextStateReturnedValue = 1u;

const unsigned char *glGetString(unsigned int name) {
    if (name == GL_VENDOR)
        return reinterpret_cast<const unsigned char *>(glString);
    if (name == GL_VERSION)
        return reinterpret_cast<const unsigned char *>(glVersion);
    return reinterpret_cast<const unsigned char *>("");
};

void glGetIntegerv(GLenum pname, GLint *params) { return NEO::MockGLSharingFunctions::glGetIntegervTest(pname, params); };

void resetParam(const char *name){};
int getParam(const char *name) {
    return 0;
};
CL_GL_BUFFER_INFO getBufferInfo() { return bufferInfoInput; };
CL_GL_RESOURCE_INFO getTextureInfo() { return textureInfoInput; };
void memParam() {
    memset(&bufferInfoInput, 0, sizeof(CL_GL_BUFFER_INFO));
    memset(&bufferInfoOutput, 0, sizeof(CL_GL_BUFFER_INFO));
    memset(&textureInfoInput, 0, sizeof(CL_GL_RESOURCE_INFO));
    memset(&textureInfoOutput, 0, sizeof(CL_GL_RESOURCE_INFO));
};
void loadBuffer(CL_GL_BUFFER_INFO buff) { bufferInfoOutput = buff; };
void loadTexture(CL_GL_RESOURCE_INFO texture) { textureInfoOutput = texture; };
void glSetString(const char *name, unsigned int var) {
    if (var == GL_VENDOR) {
        glString = name;
    } else if (var == GL_VERSION) {
        glVersion = name;
    }
};
void glSetStringi(const char *name, unsigned int index) { arrayStringi[index] = name; };
void setGLSetSharedOCLContextStateReturnedValue(GLboolean value) { glSetSharedOCLContextStateReturnedValue = static_cast<GLboolean>(value); };
GLboolean getGLSetSharedOCLContextStateReturnedValue() { return glSetSharedOCLContextStateReturnedValue; };

EGLAPI __eglMustCastToProperFunctionPointerType EGLAPIENTRY eglGetProcAddress(const char *procname) {
    return nullptr;
}
}
