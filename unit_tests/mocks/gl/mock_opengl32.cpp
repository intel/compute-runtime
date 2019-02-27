/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#define _GDI32_ //It causes that definitions of functions are not loaded from dll in file wingdi.h because they are in this file.

#include "unit_tests/helpers/windows/mock_function.h"
#include "unit_tests/mocks/gl/mock_gl_sharing.h"

#include "GL/gl.h"

#include <string.h>

extern "C" {
const char *glString = "Intel";
const char *glVersion = "4.0";
const char *arrayStringi[2]{"GL_OES_framebuffer_object", "GL_EXT_framebuffer_object"};
int GLAcquireSharedBufferCalled = 0;
int GLAcquireSharedRenderBufferCalled = 0;
int GLAcquireSharedTextureCalled = 0;
int GLDeleteContextCalled = 0;
int GLGetCurrentContextCalled = 0;
int GLGetCurrentDisplayCalled = 0;
int GLGetSyncivCalled = 0;
int GLMakeCurrentCalled = 0;
int GLReleaseSharedBufferCalled = 0;
int GLReleaseSharedRenderBufferCalled = 0;
int GLReleaseSharedTextureCalled = 0;
int GLReleaseSyncCalled = 0;
int GLRetainSyncCalled = 0;
int WGLCreateContextCalled = 0;
int WGLDeleteContextCalled = 0;
int WGLShareListsCalled = 0;
CL_GL_BUFFER_INFO bufferInfoInput = {0};
CL_GL_BUFFER_INFO bufferInfoOutput = {0};
CL_GL_RESOURCE_INFO textureInfoInput = {0};
CL_GL_RESOURCE_INFO textureInfoOutput = {0};
OCLRT::GLMockReturnedValues glMockReturnedValues = {0};
GLboolean GLSetSharedOCLContextStateReturnedValue = 1u;

const unsigned char *WINAPI glGetString(unsigned int name) {
    if (name == GL_VENDOR)
        return reinterpret_cast<const unsigned char *>(glString);
    if (name == GL_VERSION)
        return reinterpret_cast<const unsigned char *>(glVersion);
    return reinterpret_cast<const unsigned char *>("");
};
GLboolean WINAPI wglSetSharedOCLContextStateINTELMock(HDC HDCHandle, HGLRC ContextHandle, unsigned char State,
                                                      void *pContextInfo) {
    ((OCLRT::ContextInfo *)pContextInfo)->ContextHandle = 1;
    ((OCLRT::ContextInfo *)pContextInfo)->DeviceHandle = 2;
    return GLSetSharedOCLContextStateReturnedValue;
};
GLboolean WINAPI mockGLAcquireSharedBuffer(GLDisplay, GLContext, GLContext, GLvoid *pResourceInfo) {

    auto pBufferInfo = (CL_GL_BUFFER_INFO *)pResourceInfo;
    bufferInfoInput = *pBufferInfo;
    pBufferInfo->bufferSize = bufferInfoOutput.bufferSize;
    pBufferInfo->globalShareHandle = bufferInfoOutput.globalShareHandle;
    pBufferInfo->pGmmResInfo = bufferInfoOutput.pGmmResInfo;
    pBufferInfo->bufferOffset = bufferInfoOutput.bufferOffset;
    GLAcquireSharedBufferCalled++;
    return (GLboolean)1;
};
GLboolean WINAPI mockGLReleaseSharedBuffer(GLDisplay, GLContext, GLContext, GLvoid *pResourceInfo) {
    bufferInfoInput = *static_cast<CL_GL_BUFFER_INFO *>(pResourceInfo);
    GLReleaseSharedBufferCalled++;
    return (GLboolean)1;
};
GLboolean WINAPI mockGLAcquireSharedRenderBuffer(GLDisplay, GLContext, GLContext, GLvoid *pResourceInfo) {
    auto pTextureInfo = (CL_GL_RESOURCE_INFO *)pResourceInfo;
    textureInfoInput = *pTextureInfo;
    pTextureInfo->globalShareHandle = textureInfoOutput.globalShareHandle;
    pTextureInfo->pGmmResInfo = textureInfoOutput.pGmmResInfo;
    pTextureInfo->glInternalFormat = GL_RGBA8;
    GLAcquireSharedRenderBufferCalled++;
    return (GLboolean)1;
};
GLboolean WINAPI mockGLReleaseSharedRenderBuffer(GLDisplay, GLContext, GLContext, GLvoid *pResourceInfo) {
    textureInfoInput = *static_cast<CL_GL_RESOURCE_INFO *>(pResourceInfo);
    GLReleaseSharedRenderBufferCalled++;
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
    GLAcquireSharedTextureCalled++;
    return (GLboolean)1;
};
GLboolean WINAPI mockGLReleaseSharedTexture(GLDisplay, GLContext, GLContext, GLvoid *pResourceInfo) {
    textureInfoInput = *static_cast<CL_GL_RESOURCE_INFO *>(pResourceInfo);
    GLReleaseSharedTextureCalled++;
    return (GLboolean)1;
};
GLboolean WINAPI mockGlRetainSync(GLDisplay HDCHandle, GLContext ContextHandle, GLContext BackupContextHandle,
                                  GLvoid *pSyncInfo) {
    GLRetainSyncCalled++;
    GL_CL_SYNC_INFO *syncInfo = (GL_CL_SYNC_INFO *)(pSyncInfo);
    syncInfo->pSync = (void *)0x123;
    return GL_TRUE;
};
GLboolean WINAPI mockGlReleaseSync(GLDisplay HDCHandle, GLContext ContextHandle, GLContext BackupContextHandle,
                                   GLvoid *pSync) {
    GLReleaseSyncCalled++;
    return GL_TRUE;
};
void WINAPI mockGlGetSynciv(GLvoid *pSync, GLenum pname, GLint *value) {
    GLGetSyncivCalled++;
    *value = glMockReturnedValues.syncivRetVal;
};
const unsigned char *_stdcall glGetStringiMock(unsigned int name, unsigned int index) { return reinterpret_cast<const unsigned char *>(arrayStringi[index]); };
GLDisplay WINAPI mockGLGetCurrentDisplay() {
    GLGetCurrentDisplayCalled++;
    return glMockReturnedValues.currentDisplay;
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
    return nullptr;
}
HGLRC WINAPI wglGetCurrentContext() {
    GLGetCurrentContextCalled++;
    return glMockReturnedValues.currentContext;
};
HDC WINAPI wglGetCurrentDC() { return mockGLGetCurrentDisplay(); };
HGLRC WINAPI wglCreateContext(HDC Arg1) {
    WGLCreateContextCalled++;
    return (GLContext)0x101;
};
BOOL WINAPI wglDeleteContext(HGLRC Arg1) {
    WGLDeleteContextCalled++;
    GLDeleteContextCalled++;
    return (GLboolean)1;
};
void WINAPI glGetIntegerv(GLenum pname, GLint *params) { return OCLRT::MockGLSharingFunctions::glGetIntegervTest(pname, params); };
BOOL WINAPI wglShareLists(HGLRC arg1, HGLRC arg2) {
    WGLShareListsCalled++;
    return 1;
};

BOOL WINAPI wglMakeCurrent(HDC arg1, HGLRC arg2) {
    GLMakeCurrentCalled++;
    glMockReturnedValues.madeCurrentContext = arg2;
    if (glMockReturnedValues.forceMakeCurrentCallFail) {
        if (glMockReturnedValues.failsCounter < glMockReturnedValues.numberOfCallFails) {
            glMockReturnedValues.failsCounter++;
            return GL_FALSE;
        }
    }
    return (GLboolean)1;
};
void *WINAPI mockLoader(const char *name) {
    if (strcmp(name, "realFunction") == 0) {
        return *realFunction;
    }
    return nullptr;
};
void resetParam(const char *name) {
    if (strcmp(name, "GLAcquireSharedBufferCalled") == 0) {
        GLAcquireSharedBufferCalled = 0;
    }
    if (strcmp(name, "GLAcquireSharedRenderBufferCalled") == 0) {
        GLAcquireSharedRenderBufferCalled = 0;
    }
    if (strcmp(name, "GLAcquireSharedTextureCalled") == 0) {
        GLAcquireSharedTextureCalled = 0;
    }
    if (strcmp(name, "GLDeleteContextCalled") == 0) {
        GLDeleteContextCalled = 0;
    }
    if (strcmp(name, "GLGetCurrentContextCalled") == 0) {
        GLGetCurrentContextCalled = 0;
    }
    if (strcmp(name, "GLGetCurrentDisplayCalled") == 0) {
        GLGetCurrentDisplayCalled = 0;
    }
    if (strcmp(name, "GLGetSyncivCalled") == 0) {
        GLGetSyncivCalled = 0;
    }
    if (strcmp(name, "GLMakeCurrentCalled") == 0) {
        GLMakeCurrentCalled = 0;
    }
    if (strcmp(name, "GLReleaseSharedBufferCalled") == 0) {
        GLReleaseSharedBufferCalled = 0;
    }
    if (strcmp(name, "GLReleaseSharedRenderBufferCalled") == 0) {
        GLReleaseSharedRenderBufferCalled = 0;
    }
    if (strcmp(name, "GLReleaseSharedTextureCalled") == 0) {
        GLReleaseSharedTextureCalled = 0;
    }
    if (strcmp(name, "GLReleaseSyncCalled") == 0) {
        GLReleaseSyncCalled = 0;
    }
    if (strcmp(name, "GLRetainSyncCalled") == 0) {
        GLRetainSyncCalled = 0;
    }
    if (strcmp(name, "WGLCreateContextCalled") == 0) {
        WGLCreateContextCalled = 0;
    }
    if (strcmp(name, "WGLDeleteContextCalled") == 0) {
        WGLDeleteContextCalled = 0;
    }
    if (strcmp(name, "WGLShareListsCalled") == 0) {
        WGLShareListsCalled = 0;
    }
    if (strcmp(name, "") == 0) {
        GLAcquireSharedBufferCalled = 0;
        GLAcquireSharedRenderBufferCalled = 0;
        GLAcquireSharedTextureCalled = 0;
        GLDeleteContextCalled = 0;
        GLGetCurrentContextCalled = 0;
        GLGetCurrentDisplayCalled = 0;
        GLGetSyncivCalled = 0;
        GLMakeCurrentCalled = 0;
        GLReleaseSharedBufferCalled = 0;
        GLReleaseSharedRenderBufferCalled = 0;
        GLReleaseSharedTextureCalled = 0;
        GLReleaseSyncCalled = 0;
        GLRetainSyncCalled = 0;
        WGLCreateContextCalled = 0;
        WGLDeleteContextCalled = 0;
        WGLShareListsCalled = 0;
    }
};
int getParam(const char *name) {
    if (strcmp(name, "GLAcquireSharedBufferCalled") == 0) {
        return GLAcquireSharedBufferCalled;
    }
    if (strcmp(name, "GLAcquireSharedRenderBufferCalled") == 0) {
        return GLAcquireSharedRenderBufferCalled;
    }
    if (strcmp(name, "GLAcquireSharedTextureCalled") == 0) {
        return GLAcquireSharedTextureCalled;
    }
    if (strcmp(name, "GLDeleteContextCalled") == 0) {
        return GLDeleteContextCalled;
    }
    if (strcmp(name, "GLGetCurrentContextCalled") == 0) {
        return GLGetCurrentContextCalled;
    }
    if (strcmp(name, "GLGetCurrentDisplayCalled") == 0) {
        return GLGetCurrentDisplayCalled;
    }
    if (strcmp(name, "GLGetSyncivCalled") == 0) {
        return GLGetSyncivCalled;
    }
    if (strcmp(name, "GLMakeCurrentCalled") == 0) {
        return GLMakeCurrentCalled;
    }
    if (strcmp(name, "GLReleaseSharedBufferCalled") == 0) {
        return GLReleaseSharedBufferCalled;
    }
    if (strcmp(name, "GLReleaseSharedRenderBufferCalled") == 0) {
        return GLReleaseSharedRenderBufferCalled;
    }
    if (strcmp(name, "GLReleaseSharedTextureCalled") == 0) {
        return GLReleaseSharedTextureCalled;
    }
    if (strcmp(name, "GLReleaseSyncCalled") == 0) {
        return GLReleaseSyncCalled;
    }
    if (strcmp(name, "GLRetainSyncCalled") == 0) {
        return GLRetainSyncCalled;
    }
    if (strcmp(name, "WGLCreateContextCalled") == 0) {
        return WGLCreateContextCalled;
    }
    if (strcmp(name, "WGLDeleteContextCalled") == 0) {
        return WGLDeleteContextCalled;
    }
    if (strcmp(name, "WGLShareListsCalled") == 0) {
        return WGLShareListsCalled;
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
    memset(&glMockReturnedValues, 0, sizeof(GLMockReturnedValues));
};
void loadBuffer(CL_GL_BUFFER_INFO buff) { bufferInfoOutput = buff; };
void loadTexture(CL_GL_RESOURCE_INFO texture) { textureInfoOutput = texture; };
OCLRT::GLMockReturnedValues getGlMockReturnedValues() { return glMockReturnedValues; };
void setGlMockReturnedValues(OCLRT::GLMockReturnedValues value) { glMockReturnedValues = value; };
void setGetSyncivReturnValue(int val) { glMockReturnedValues.syncivRetVal = val; }
void glSetString(const char *name, unsigned int var) {
    if (var == GL_VENDOR) {
        glString = name;
    } else if (var == GL_VERSION) {
        glVersion = name;
    }
};
void glSetStringi(const char *name, unsigned int index) { arrayStringi[index] = name; };
void setGLSetSharedOCLContextStateReturnedValue(GLboolean value) { GLSetSharedOCLContextStateReturnedValue = static_cast<GLboolean>(value); };
GLboolean getGLSetSharedOCLContextStateReturnedValue() { return GLSetSharedOCLContextStateReturnedValue; };
}
