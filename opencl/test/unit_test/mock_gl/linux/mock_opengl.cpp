/*
 * Copyright (C) 2018-2023 Intel Corporation
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
int glAcquireSharedBufferCalled = 0;
int glAcquireSharedRenderBufferCalled = 0;
int glAcquireSharedTextureCalled = 0;
int glDeleteContextCalled = 0;
int glGetCurrentContextCalled = 0;
int glGetCurrentDisplayCalled = 0;
int glGetSyncivCalled = 0;
int glMakeCurrentCalled = 0;
int glReleaseSharedBufferCalled = 0;
int glReleaseSharedRenderBufferCalled = 0;
int glReleaseSharedTextureCalled = 0;
int glReleaseSyncCalled = 0;
int glRetainSyncCalled = 0;
int eglCreateContextCalled = 0;
int eglDeleteContextCalled = 0;
int eglShareListsCalled = 0;
CL_GL_BUFFER_INFO bufferInfoInput = {0};
CL_GL_BUFFER_INFO bufferInfoOutput = {0};
CL_GL_RESOURCE_INFO textureInfoInput = {0};
CL_GL_RESOURCE_INFO textureInfoOutput = {0};
GLboolean glSetSharedOCLContextStateReturnedValue = 1u;
static const auto mockImage = reinterpret_cast<void *>(0xEEEE);

const unsigned char *glGetString(unsigned int name) {
    if (name == GL_VENDOR)
        return reinterpret_cast<const unsigned char *>(glString);
    if (name == GL_VERSION)
        return reinterpret_cast<const unsigned char *>(glVersion);
    return reinterpret_cast<const unsigned char *>("");
};

EGLBoolean eglExportDMABUFImageMESA(EGLDisplay dpy, EGLImageKHR image, int *fds, EGLint *strides, EGLint *offsets) {
    glAcquireSharedTextureCalled++;
    if (image == mockImage) {
        *fds = 10;
        textureInfoInput.name = 1;
        return GL_TRUE;
    } else {
        return GL_FALSE;
    }
};

void *eglGetCurrentContext() {
    glGetCurrentContextCalled++;
    return nullptr;
};
void *eglGetCurrentDisplay() {
    glGetCurrentDisplayCalled++;
    return nullptr;
};

EGLAPI EGLContext EGLAPIENTRY eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext shareContext, const EGLint *attribList) {
    eglCreateContextCalled++;
    return (GLContext)0x101;
};
EGLBoolean eglDestroyContext(EGLDisplay display, EGLContext context) {
    eglDeleteContextCalled++;
    glDeleteContextCalled++;
    return (GLboolean)1;
};
void glGetIntegerv(GLenum pname, GLint *params) { return NEO::MockGLSharingFunctions::glGetIntegervTest(pname, params); };
unsigned char eglShareLists(void *arg1, void *arg2) {
    eglShareListsCalled++;
    return 1;
};

EGLAPI EGLBoolean EGLAPIENTRY eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext arg2) {
    glMakeCurrentCalled++;
    return EGL_TRUE;
};

void resetParam(const char *name) {
    if (strcmp(name, "GLAcquireSharedBufferCalled") == 0) {
        glAcquireSharedBufferCalled = 0;
    }
    if (strcmp(name, "GLAcquireSharedRenderBufferCalled") == 0) {
        glAcquireSharedRenderBufferCalled = 0;
    }
    if (strcmp(name, "GLAcquireSharedTextureCalled") == 0) {
        glAcquireSharedTextureCalled = 0;
    }
    if (strcmp(name, "GLDeleteContextCalled") == 0) {
        glDeleteContextCalled = 0;
    }
    if (strcmp(name, "GLGetCurrentContextCalled") == 0) {
        glGetCurrentContextCalled = 0;
    }
    if (strcmp(name, "GLGetCurrentDisplayCalled") == 0) {
        glGetCurrentDisplayCalled = 0;
    }
    if (strcmp(name, "GLGetSyncivCalled") == 0) {
        glGetSyncivCalled = 0;
    }
    if (strcmp(name, "GLMakeCurrentCalled") == 0) {
        glMakeCurrentCalled = 0;
    }
    if (strcmp(name, "GLReleaseSharedBufferCalled") == 0) {
        glReleaseSharedBufferCalled = 0;
    }
    if (strcmp(name, "GLReleaseSharedRenderBufferCalled") == 0) {
        glReleaseSharedRenderBufferCalled = 0;
    }
    if (strcmp(name, "GLReleaseSharedTextureCalled") == 0) {
        glReleaseSharedTextureCalled = 0;
    }
    if (strcmp(name, "GLReleaseSyncCalled") == 0) {
        glReleaseSyncCalled = 0;
    }
    if (strcmp(name, "GLRetainSyncCalled") == 0) {
        glRetainSyncCalled = 0;
    }
    if (strcmp(name, "EGLCreateContextCalled") == 0) {
        eglCreateContextCalled = 0;
    }
    if (strcmp(name, "EGLDeleteContextCalled") == 0) {
        eglDeleteContextCalled = 0;
    }
    if (strcmp(name, "EGLShareListsCalled") == 0) {
        eglShareListsCalled = 0;
    }
    if (strcmp(name, "") == 0) {
        glAcquireSharedBufferCalled = 0;
        glAcquireSharedRenderBufferCalled = 0;
        glAcquireSharedTextureCalled = 0;
        glDeleteContextCalled = 0;
        glGetCurrentContextCalled = 0;
        glGetCurrentDisplayCalled = 0;
        glGetSyncivCalled = 0;
        glMakeCurrentCalled = 0;
        glReleaseSharedBufferCalled = 0;
        glReleaseSharedRenderBufferCalled = 0;
        glReleaseSharedTextureCalled = 0;
        glReleaseSyncCalled = 0;
        glRetainSyncCalled = 0;
        eglCreateContextCalled = 0;
        eglDeleteContextCalled = 0;
        eglShareListsCalled = 0;
    }
};
int getParam(const char *name) {
    if (strcmp(name, "GLAcquireSharedBufferCalled") == 0) {
        return glAcquireSharedBufferCalled;
    }
    if (strcmp(name, "GLAcquireSharedRenderBufferCalled") == 0) {
        return glAcquireSharedRenderBufferCalled;
    }
    if (strcmp(name, "GLAcquireSharedTextureCalled") == 0) {
        return glAcquireSharedTextureCalled;
    }
    if (strcmp(name, "GLDeleteContextCalled") == 0) {
        return glDeleteContextCalled;
    }
    if (strcmp(name, "GLGetCurrentContextCalled") == 0) {
        return glGetCurrentContextCalled;
    }
    if (strcmp(name, "GLGetCurrentDisplayCalled") == 0) {
        return glGetCurrentDisplayCalled;
    }
    if (strcmp(name, "GLGetSyncivCalled") == 0) {
        return glGetSyncivCalled;
    }
    if (strcmp(name, "GLMakeCurrentCalled") == 0) {
        return glMakeCurrentCalled;
    }
    if (strcmp(name, "GLReleaseSharedBufferCalled") == 0) {
        return glReleaseSharedBufferCalled;
    }
    if (strcmp(name, "GLReleaseSharedRenderBufferCalled") == 0) {
        return glReleaseSharedRenderBufferCalled;
    }
    if (strcmp(name, "GLReleaseSharedTextureCalled") == 0) {
        return glReleaseSharedTextureCalled;
    }
    if (strcmp(name, "GLReleaseSyncCalled") == 0) {
        return glReleaseSyncCalled;
    }
    if (strcmp(name, "GLRetainSyncCalled") == 0) {
        return glRetainSyncCalled;
    }
    if (strcmp(name, "EGLCreateContextCalled") == 0) {
        return eglCreateContextCalled;
    }
    if (strcmp(name, "EGLDeleteContextCalled") == 0) {
        return eglDeleteContextCalled;
    }
    if (strcmp(name, "EGLShareListsCalled") == 0) {
        return eglShareListsCalled;
    }
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

    std::string functionName(procname);
    if (functionName == "eglGetCurrentContext") {
        return reinterpret_cast<__eglMustCastToProperFunctionPointerType EGLAPIENTRY>(eglGetCurrentContext);
    } else if (functionName == "eglGetCurrentDisplay") {
        return reinterpret_cast<__eglMustCastToProperFunctionPointerType EGLAPIENTRY>(eglGetCurrentDisplay);
    } else if (functionName == "eglCreateContext") {
        return reinterpret_cast<__eglMustCastToProperFunctionPointerType EGLAPIENTRY>(eglCreateContext);
    } else if (functionName == "eglDestroyContext") {
        return reinterpret_cast<__eglMustCastToProperFunctionPointerType EGLAPIENTRY>(eglDestroyContext);
    } else if (functionName == "eglMakeCurrent") {
        return reinterpret_cast<__eglMustCastToProperFunctionPointerType EGLAPIENTRY>(eglMakeCurrent);
    } else if (functionName == "eglExportDMABUFImageMESA") {
        return reinterpret_cast<__eglMustCastToProperFunctionPointerType EGLAPIENTRY>(eglExportDMABUFImageMESA);
    }
    return nullptr;
}
}
