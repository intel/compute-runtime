/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/sharings/gl/gl_context_guard.h"
#include "level_zero/api/opencl/source/sharings/gl/windows/gl_sharing_windows.h"

namespace NEO {
namespace LEO {

GLContextGuard::GLContextGuard(GLSharingFunctions &sharingFcns) : sharingFunctions(&sharingFcns) {
    auto &sharing = *static_cast<GLSharingFunctionsWindows *>(sharingFunctions);

    currentContextHandle = sharing.getCurrentContext();
    currentDisplayHandle = sharing.getCurrentDisplay();
    auto ctxToMakeCurrent = sharing.getContextHandle();

    if (currentContextHandle == 0) {
        ctxToMakeCurrent = sharing.getBackupContextHandle();
    }

    if (currentContextHandle != sharing.getContextHandle() && currentContextHandle != sharing.getBackupContextHandle()) {
        if (sharing.makeCurrent(ctxToMakeCurrent) == GL_FALSE) {
            while (sharing.makeCurrent(sharing.getBackupContextHandle()) == GL_FALSE) {
                ;
            }
        }
    }
}

GLContextGuard::~GLContextGuard() {
    auto &sharing = *static_cast<GLSharingFunctionsWindows *>(sharingFunctions);
    if (currentContextHandle != sharing.getContextHandle()) {
        sharing.makeCurrent(currentContextHandle, currentDisplayHandle);
    }
}

} // namespace LEO
} // namespace NEO
