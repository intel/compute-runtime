/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "config.h"
#include "unit_tests/mocks/gl/mock_gl_sharing.h"

namespace OCLRT {
int GLSetSharedOCLContextStateCalled = 0;
int GLAcquireSharedBufferCalled = 0;
int GLAcquireSharedRenderBufferCalled = 0;
int GLAcquireSharedTextureCalled = 0;
int GLReleaseSharedBufferCalled = 0;
int GLReleaseSharedRenderBufferCalled = 0;
int GLReleaseSharedTextureCalled = 0;
int GLGetCurrentContextCalled = 0;
int GLGetCurrentDisplayCalled = 0;
int GLMakeCurrentCalled = 0;
int GLDeleteContextCalled = 0;
int WGLCreateContextCalled = 0;
int WGLShareListsCalled = 0;
int WGLDeleteContextCalled = 0;
int EGLCreateContextCalled = 0;
int EGLChooseConfigCalled = 0;
int EGLDeleteContextCalled = 0;
int GlxChooseFBConfigCalled = 0;
int GlxQueryContextCalled = 0;
int GlxCreateNewContextCalled = 0;
int GlxDeleteContextCalled = 0;
int GlxIsDirectCalled = 0;
int GLRetainSyncCalled = 0;
int GLReleaseSyncCalled = 0;
int GLGetSyncivCalled = 0;

CL_GL_BUFFER_INFO bufferInfoInput = {0};
CL_GL_BUFFER_INFO bufferInfoOutput = {0};
CL_GL_RESOURCE_INFO textureInfoInput = {0};
CL_GL_RESOURCE_INFO textureInfoOutput = {0};
EGLBkpContextParams eglBkpContextParams = {0};
GLXBkpContextParams glxBkpContextParams = {0};
GLMockReturnedValues glMockReturnedValues = {0};

void GlSharingFunctionsMock::initMembers() {
    GLSetSharedOCLContextState = mockGLSetSharedOCLContextState;
    GLAcquireSharedBuffer = mockGLAcquireSharedBuffer;
    GLAcquireSharedRenderBuffer = mockGLAcquireSharedRenderBuffer;
    GLAcquireSharedTexture = mockGLAcquireSharedTexture;
    GLReleaseSharedBuffer = mockGLReleaseSharedBuffer;
    GLReleaseSharedRenderBuffer = mockGLReleaseSharedRenderBuffer;
    GLReleaseSharedTexture = mockGLReleaseSharedTexture;
    GLGetCurrentContext = mockGLGetCurrentContext;
    GLGetCurrentDisplay = mockGLGetCurrentDisplay;
    this->wglMakeCurrent = mockWGLMakeCurrent;
    GLReleaseSync = mockGlReleaseSync;
    GLRetainSync = mockGlRetainSync;
    GLGetSynciv = mockGlGetSynciv;
    pfnWglCreateContext = mockWGlCreateContext;
    pfnWglShareLists = mockWGlShareLists;
    pfnWglDeleteContext = mockWGLDeleteContext;

    GLSetSharedOCLContextStateCalled = 0;
    GLAcquireSharedBufferCalled = 0;
    GLAcquireSharedRenderBufferCalled = 0;
    GLAcquireSharedTextureCalled = 0;
    GLReleaseSharedBufferCalled = 0;
    GLReleaseSharedRenderBufferCalled = 0;
    GLReleaseSharedTextureCalled = 0;
    GLGetCurrentContextCalled = 0;
    GLGetCurrentDisplayCalled = 0;
    GLMakeCurrentCalled = 0;
    GLDeleteContextCalled = 0;
    WGLCreateContextCalled = 0;
    WGLShareListsCalled = 0;
    WGLDeleteContextCalled = 0;
    EGLChooseConfigCalled = 0;
    EGLCreateContextCalled = 0;
    EGLDeleteContextCalled = 0;
    GlxChooseFBConfigCalled = 0;
    GlxQueryContextCalled = 0;
    GlxCreateNewContextCalled = 0;
    GlxDeleteContextCalled = 0;
    GlxIsDirectCalled = 0;
    GLRetainSyncCalled = 0;
    GLReleaseSyncCalled = 0;
    GLGetSyncivCalled = 0;
    memset(&bufferInfoInput, 0, sizeof(CL_GL_BUFFER_INFO));
    memset(&bufferInfoOutput, 0, sizeof(CL_GL_BUFFER_INFO));
    memset(&textureInfoInput, 0, sizeof(CL_GL_RESOURCE_INFO));
    memset(&textureInfoOutput, 0, sizeof(CL_GL_RESOURCE_INFO));
    memset(&eglBkpContextParams, 0, sizeof(EGLBkpContextParams));
    memset(&glxBkpContextParams, 0, sizeof(GLXBkpContextParams));
    memset(&glMockReturnedValues, 0, sizeof(GLMockReturnedValues));
}

GlSharingFunctionsMock::GlSharingFunctionsMock() {
    initMembers();
}

MockGlSharing::MockGlSharing(GLType glhdcType, GLContext glhglrcHandle, GLContext glhglrcHandleBkpCtx, GLDisplay glhdcHandle) {
    m_sharingFunctions.setHandles(glhdcType, glhglrcHandle, glhglrcHandleBkpCtx, glhdcHandle);
}
} // namespace OCLRT
