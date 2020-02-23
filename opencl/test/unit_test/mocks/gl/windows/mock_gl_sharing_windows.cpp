/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/gl/windows/mock_gl_sharing_windows.h"

#include "config.h"

namespace NEO {
int EGLCreateContextCalled = 0;
int EGLChooseConfigCalled = 0;
int EGLDeleteContextCalled = 0;
int GlxChooseFBConfigCalled = 0;
int GlxQueryContextCalled = 0;
int GlxCreateNewContextCalled = 0;
int GlxDeleteContextCalled = 0;
int GlxIsDirectCalled = 0;
EGLBkpContextParams eglBkpContextParams = {0};
GLXBkpContextParams glxBkpContextParams = {0};

void GlSharingFunctionsMock::initMembers() {
    GLSharingFunctionsWindows::initGLFunctions();
    glDllHelper dllParam;
    dllParam.setGLSetSharedOCLContextStateReturnedValue(1u);
    dllParam.resetParam("");
    dllParam.loadTexture({0});
    dllParam.loadBuffer({0});
    EGLChooseConfigCalled = 0;
    EGLCreateContextCalled = 0;
    EGLDeleteContextCalled = 0;
    GlxChooseFBConfigCalled = 0;
    GlxQueryContextCalled = 0;
    GlxCreateNewContextCalled = 0;
    GlxDeleteContextCalled = 0;
    GlxIsDirectCalled = 0;
    memset(&eglBkpContextParams, 0, sizeof(EGLBkpContextParams));
    memset(&glxBkpContextParams, 0, sizeof(GLXBkpContextParams));
}

GlSharingFunctionsMock::GlSharingFunctionsMock() {
    initMembers();
}

MockGlSharing::MockGlSharing(GLType glhdcType, GLContext glhglrcHandle, GLContext glhglrcHandleBkpCtx, GLDisplay glhdcHandle) {
    sharingFunctions->setHandles(glhdcType, glhglrcHandle, glhglrcHandleBkpCtx, glhdcHandle);
}
} // namespace NEO
