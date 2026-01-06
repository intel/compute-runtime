/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/gl/linux/mock_gl_sharing_linux.h"

namespace NEO {
void GlSharingFunctionsMock::initMembers() {
    GLSharingFunctionsLinux::initGLFunctions();
    GlDllHelper dllParam;
    dllParam.setGLSetSharedOCLContextStateReturnedValue(1u);
    dllParam.resetParam("");
    dllParam.loadTexture({0});
    dllParam.loadBuffer({0});
}

GlSharingFunctionsMock::GlSharingFunctionsMock() {
    initMembers();
}

MockGlSharing::MockGlSharing(GLType glhdcType, GLContext glhglrcHandle, GLContext glhglrcHandleBkpCtx, GLDisplay glhdcHandle) {
    sharingFunctions->setHandles(glhdcType, glhglrcHandle, glhglrcHandleBkpCtx, glhdcHandle);
}
} // namespace NEO
