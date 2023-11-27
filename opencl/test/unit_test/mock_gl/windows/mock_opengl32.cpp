/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#define _GDI32_ // It causes that definitions of functions are not loaded from dll in file wingdi.h because they are in this file.

#include "opencl/test/unit_test/helpers/windows/mock_function.h"
#include "opencl/test/unit_test/mocks/gl/windows/mock_gl_sharing_windows.h"

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
int wglCreateContextCalled = 0;
int wglDeleteContextCalled = 0;
int wglShareListsCalled = 0;
CL_GL_BUFFER_INFO bufferInfoInput = {0};
CL_GL_BUFFER_INFO bufferInfoOutput = {0};
CL_GL_RESOURCE_INFO textureInfoInput = {0};
CL_GL_RESOURCE_INFO textureInfoOutput = {0};
NEO::GLMockReturnedValues glMockReturnedValues = {0};
GLboolean glSetSharedOCLContextStateReturnedValue = 1u;
bool glGetLuidFuncAvailable = true;
int glGetLuidCalled = 0;

const unsigned char *WINAPI glGetString(unsigned int name) {
    if (name == GL_VENDOR)
        return reinterpret_cast<const unsigned char *>(glString);
    if (name == GL_VERSION)
        return reinterpret_cast<const unsigned char *>(glVersion);
    return reinterpret_cast<const unsigned char *>("");
};
GLboolean WINAPI wglSetSharedOCLContextStateINTELMock(HDC hdcHandle, HGLRC contextHandle, unsigned char state,
                                                      void *pContextInfo) {
    ((NEO::ContextInfo *)pContextInfo)->contextHandle = 1;
    ((NEO::ContextInfo *)pContextInfo)->deviceHandle = 2;
    return glSetSharedOCLContextStateReturnedValue;
};
GLboolean WINAPI mockGLAcquireSharedBuffer(GLDisplay, GLContext, GLContext, GLvoid *pResourceInfo) {

    auto pBufferInfo = (CL_GL_BUFFER_INFO *)pResourceInfo;
    bufferInfoInput = *pBufferInfo;
    pBufferInfo->bufferSize = bufferInfoOutput.bufferSize;
    pBufferInfo->globalShareHandle = bufferInfoOutput.globalShareHandle;
    pBufferInfo->pGmmResInfo = bufferInfoOutput.pGmmResInfo;
    pBufferInfo->bufferOffset = bufferInfoOutput.bufferOffset;
    glAcquireSharedBufferCalled++;
    return (GLboolean)1;
};
GLboolean WINAPI mockGLReleaseSharedBuffer(GLDisplay, GLContext, GLContext, GLvoid *pResourceInfo) {
    bufferInfoInput = *static_cast<CL_GL_BUFFER_INFO *>(pResourceInfo);
    glReleaseSharedBufferCalled++;
    return (GLboolean)1;
};
GLboolean WINAPI mockGLAcquireSharedRenderBuffer(GLDisplay, GLContext, GLContext, GLvoid *pResourceInfo) {
    auto pTextureInfo = (CL_GL_RESOURCE_INFO *)pResourceInfo;
    textureInfoInput = *pTextureInfo;
    pTextureInfo->globalShareHandle = textureInfoOutput.globalShareHandle;
    pTextureInfo->pGmmResInfo = textureInfoOutput.pGmmResInfo;
    pTextureInfo->glInternalFormat = GL_RGBA8;
    glAcquireSharedRenderBufferCalled++;
    return (GLboolean)1;
};
GLboolean WINAPI mockGLReleaseSharedRenderBuffer(GLDisplay, GLContext, GLContext, GLvoid *pResourceInfo) {
    textureInfoInput = *static_cast<CL_GL_RESOURCE_INFO *>(pResourceInfo);
    glReleaseSharedRenderBufferCalled++;
    return (GLboolean)1;
};
GLboolean WINAPI mockGLAcquireSharedTexture(GLDisplay, GLContext, GLContext, GLvoid *pResourceInfo) {
    auto pTextureInfo = (CL_GL_RESOURCE_INFO *)pResourceInfo;
    textureInfoInput = *pTextureInfo;
    pTextureInfo->globalShareHandle = textureInfoOutput.globalShareHandle;
    pTextureInfo->globalShareHandleMCS = textureInfoOutput.globalShareHandleMCS;
    if (pTextureInfo->target == GL_TEXTURE_BUFFER) {
        // size and width for texture buffer are queried from textureInfo - not from gmm
        pTextureInfo->textureBufferSize = textureInfoOutput.textureBufferSize;
        pTextureInfo->textureBufferWidth = textureInfoOutput.textureBufferWidth;
    }
    pTextureInfo->pGmmResInfo = textureInfoOutput.pGmmResInfo;
    pTextureInfo->glInternalFormat = textureInfoOutput.glInternalFormat ? textureInfoOutput.glInternalFormat : GL_RGBA8;
    pTextureInfo->glHWFormat = textureInfoOutput.glHWFormat;
    pTextureInfo->textureBufferOffset = textureInfoOutput.textureBufferOffset;
    pTextureInfo->numberOfSamples = textureInfoOutput.numberOfSamples;
    pTextureInfo->isAuxEnabled = textureInfoOutput.isAuxEnabled;
    glAcquireSharedTextureCalled++;
    return (GLboolean)1;
};
GLboolean WINAPI mockGLReleaseSharedTexture(GLDisplay, GLContext, GLContext, GLvoid *pResourceInfo) {
    textureInfoInput = *static_cast<CL_GL_RESOURCE_INFO *>(pResourceInfo);
    glReleaseSharedTextureCalled++;
    return (GLboolean)1;
};
GLboolean WINAPI mockGlRetainSync(GLDisplay hdcHandle, GLContext contextHandle, GLContext backupContextHandle,
                                  GLvoid *pSyncInfo) {
    glRetainSyncCalled++;
    GL_CL_SYNC_INFO *syncInfo = (GL_CL_SYNC_INFO *)(pSyncInfo);
    syncInfo->pSync = (void *)0x123;
    return GL_TRUE;
};
GLboolean WINAPI mockGlReleaseSync(GLDisplay hdcHandle, GLContext contextHandle, GLContext backupContextHandle,
                                   GLvoid *pSync) {
    glReleaseSyncCalled++;
    return GL_TRUE;
};
void WINAPI mockGlGetSynciv(GLvoid *pSync, GLenum pname, GLint *value) {
    glGetSyncivCalled++;
    *value = glMockReturnedValues.syncivRetVal;
};
const unsigned char *_stdcall glGetStringiMock(unsigned int name, unsigned int index) { return reinterpret_cast<const unsigned char *>(arrayStringi[index]); };
GLDisplay WINAPI mockGLGetCurrentDisplay() {
    glGetCurrentDisplayCalled++;
    return glMockReturnedValues.currentDisplay;
};

LUID WINAPI wglGetLuidMock(GLContext glContext) {
    glGetLuidCalled++;
    LUID luid{};
    if (reinterpret_cast<GLContext>(1) == glContext) {
        luid.HighPart = 0x1d2e;
        luid.LowPart = 0x3f4a;
    } else if (reinterpret_cast<GLContext>(2) == glContext) {
        luid.HighPart = 0x5d2e;
        luid.LowPart = 0x3f4a;
    }
    return luid;
};

PROC WINAPI wglGetProcAddress(LPCSTR name) {
    if (strcmp(name, "wglSetSharedOCLContextStateINTEL") == 0) {
        return reinterpret_cast<PROC>(*wglSetSharedOCLContextStateINTELMock);
    }
    if (strcmp(name, "wglAcquireSharedBufferINTEL") == 0) {
        return reinterpret_cast<PROC>(*mockGLAcquireSharedBuffer);
    }
    if (strcmp(name, "wglReleaseSharedBufferINTEL") == 0) {
        return reinterpret_cast<PROC>(*mockGLReleaseSharedBuffer);
    }
    if (strcmp(name, "wglAcquireSharedRenderBufferINTEL") == 0) {
        return reinterpret_cast<PROC>(*mockGLAcquireSharedRenderBuffer);
    }
    if (strcmp(name, "wglReleaseSharedRenderBufferINTEL") == 0) {
        return reinterpret_cast<PROC>(*mockGLReleaseSharedRenderBuffer);
    }
    if (strcmp(name, "wglAcquireSharedTextureINTEL") == 0) {
        return reinterpret_cast<PROC>(*mockGLAcquireSharedTexture);
    }
    if (strcmp(name, "wglReleaseSharedTextureINTEL") == 0) {
        return reinterpret_cast<PROC>(*mockGLReleaseSharedTexture);
    }
    if (strcmp(name, "wglRetainSyncINTEL") == 0) {
        return reinterpret_cast<PROC>(*mockGlRetainSync);
    }
    if (strcmp(name, "wglReleaseSyncINTEL") == 0) {
        return reinterpret_cast<PROC>(*mockGlReleaseSync);
    }
    if (strcmp(name, "wglGetSyncivINTEL") == 0) {
        return reinterpret_cast<PROC>(*mockGlGetSynciv);
    }
    if (strcmp(name, "glGetStringi") == 0) {
        return reinterpret_cast<PROC>(*glGetStringiMock);
    }
    if (strcmp(name, "wglGetLuidINTEL") == 0 && glGetLuidFuncAvailable) {
        return reinterpret_cast<PROC>(wglGetLuidMock);
    }
    return nullptr;
}
HGLRC WINAPI wglGetCurrentContext() {
    glGetCurrentContextCalled++;
    return glMockReturnedValues.currentContext;
};
HDC WINAPI wglGetCurrentDC() { return mockGLGetCurrentDisplay(); };
HGLRC WINAPI wglCreateContext(HDC arg1) {
    wglCreateContextCalled++;
    return (GLContext)0x101;
};
BOOL WINAPI wglDeleteContext(HGLRC arg1) {
    wglDeleteContextCalled++;
    glDeleteContextCalled++;
    return (GLboolean)1;
};
void WINAPI glGetIntegerv(GLenum pname, GLint *params) { return NEO::MockGLSharingFunctions::glGetIntegervTest(pname, params); };
BOOL WINAPI wglShareLists(HGLRC arg1, HGLRC arg2) {
    wglShareListsCalled++;
    return 1;
};

BOOL WINAPI wglMakeCurrent(HDC arg1, HGLRC arg2) {
    glMakeCurrentCalled++;
    glMockReturnedValues.madeCurrentContext = arg2;
    if (glMockReturnedValues.forceMakeCurrentCallFail) {
        if (glMockReturnedValues.failsCounter < glMockReturnedValues.numberOfCallFails) {
            glMockReturnedValues.failsCounter++;
            return GL_FALSE;
        }
    }
    return (GLboolean)1;
};
PROC __stdcall mockLoader(const char *name) {
    if (strcmp(name, "realFunction") == 0) {
        return reinterpret_cast<PROC>(*realFunction);
    }
    return 0u;
};
void resetParam(const char *name) {
    if (strcmp(name, "glAcquireSharedBufferCalled") == 0) {
        glAcquireSharedBufferCalled = 0;
    }
    if (strcmp(name, "glAcquireSharedRenderBufferCalled") == 0) {
        glAcquireSharedRenderBufferCalled = 0;
    }
    if (strcmp(name, "glAcquireSharedTextureCalled") == 0) {
        glAcquireSharedTextureCalled = 0;
    }
    if (strcmp(name, "glDeleteContextCalled") == 0) {
        glDeleteContextCalled = 0;
    }
    if (strcmp(name, "glGetCurrentContextCalled") == 0) {
        glGetCurrentContextCalled = 0;
    }
    if (strcmp(name, "glGetCurrentDisplayCalled") == 0) {
        glGetCurrentDisplayCalled = 0;
    }
    if (strcmp(name, "glGetSyncivCalled") == 0) {
        glGetSyncivCalled = 0;
    }
    if (strcmp(name, "glMakeCurrentCalled") == 0) {
        glMakeCurrentCalled = 0;
    }
    if (strcmp(name, "glReleaseSharedBufferCalled") == 0) {
        glReleaseSharedBufferCalled = 0;
    }
    if (strcmp(name, "glReleaseSharedRenderBufferCalled") == 0) {
        glReleaseSharedRenderBufferCalled = 0;
    }
    if (strcmp(name, "glReleaseSharedTextureCalled") == 0) {
        glReleaseSharedTextureCalled = 0;
    }
    if (strcmp(name, "glReleaseSyncCalled") == 0) {
        glReleaseSyncCalled = 0;
    }
    if (strcmp(name, "glRetainSyncCalled") == 0) {
        glRetainSyncCalled = 0;
    }
    if (strcmp(name, "wglCreateContextCalled") == 0) {
        wglCreateContextCalled = 0;
    }
    if (strcmp(name, "wglDeleteContextCalled") == 0) {
        wglDeleteContextCalled = 0;
    }
    if (strcmp(name, "wglShareListsCalled") == 0) {
        wglShareListsCalled = 0;
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
        wglCreateContextCalled = 0;
        wglDeleteContextCalled = 0;
        wglShareListsCalled = 0;
        glGetLuidCalled = 0;
        glGetLuidFuncAvailable = true;
    }
    if (strcmp(name, "glGetLuidCalled") == 0) {
        glGetLuidCalled = 0;
    }
    if (strcmp(name, "glGetLuidFuncAvailable") == 0) {
        glGetLuidFuncAvailable = true;
    }
    if (strcmp(name, "glGetLuidFuncNotAvailable") == 0) {
        glGetLuidFuncAvailable = false;
    }
};
int getParam(const char *name) {
    if (strcmp(name, "glAcquireSharedBufferCalled") == 0) {
        return glAcquireSharedBufferCalled;
    }
    if (strcmp(name, "glAcquireSharedRenderBufferCalled") == 0) {
        return glAcquireSharedRenderBufferCalled;
    }
    if (strcmp(name, "glAcquireSharedTextureCalled") == 0) {
        return glAcquireSharedTextureCalled;
    }
    if (strcmp(name, "glDeleteContextCalled") == 0) {
        return glDeleteContextCalled;
    }
    if (strcmp(name, "glGetCurrentContextCalled") == 0) {
        return glGetCurrentContextCalled;
    }
    if (strcmp(name, "glGetCurrentDisplayCalled") == 0) {
        return glGetCurrentDisplayCalled;
    }
    if (strcmp(name, "glGetSyncivCalled") == 0) {
        return glGetSyncivCalled;
    }
    if (strcmp(name, "glMakeCurrentCalled") == 0) {
        return glMakeCurrentCalled;
    }
    if (strcmp(name, "glReleaseSharedBufferCalled") == 0) {
        return glReleaseSharedBufferCalled;
    }
    if (strcmp(name, "glReleaseSharedRenderBufferCalled") == 0) {
        return glReleaseSharedRenderBufferCalled;
    }
    if (strcmp(name, "glReleaseSharedTextureCalled") == 0) {
        return glReleaseSharedTextureCalled;
    }
    if (strcmp(name, "glReleaseSyncCalled") == 0) {
        return glReleaseSyncCalled;
    }
    if (strcmp(name, "glRetainSyncCalled") == 0) {
        return glRetainSyncCalled;
    }
    if (strcmp(name, "wglCreateContextCalled") == 0) {
        return wglCreateContextCalled;
    }
    if (strcmp(name, "wglDeleteContextCalled") == 0) {
        return wglDeleteContextCalled;
    }
    if (strcmp(name, "wglShareListsCalled") == 0) {
        return wglShareListsCalled;
    }
    if (strcmp(name, "glGetLuidCalled") == 0) {
        return glGetLuidCalled;
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
    memset(&glMockReturnedValues, 0, sizeof(NEO::GLMockReturnedValues));
};
void loadBuffer(CL_GL_BUFFER_INFO buff) { bufferInfoOutput = buff; };
void loadTexture(CL_GL_RESOURCE_INFO texture) { textureInfoOutput = texture; };
NEO::GLMockReturnedValues getGlMockReturnedValues() { return glMockReturnedValues; };
void setGlMockReturnedValues(NEO::GLMockReturnedValues value) { glMockReturnedValues = value; };
void setGetSyncivReturnValue(int val) { glMockReturnedValues.syncivRetVal = val; }
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
}
