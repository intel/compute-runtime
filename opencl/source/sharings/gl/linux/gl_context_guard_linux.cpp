/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sharings/gl/gl_context_guard.h"
#include "opencl/source/sharings/gl/linux/gl_sharing_linux.h"

namespace NEO {
GLContextGuard::GLContextGuard(GLSharingFunctions &sharingFcns) : sharingFunctions(&sharingFcns) {
    auto &sharing = *static_cast<GLSharingFunctionsLinux *>(sharingFunctions);

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
    auto &sharing = *static_cast<GLSharingFunctionsLinux *>(sharingFunctions);
    if (currentContextHandle != sharing.getContextHandle()) {
        sharing.makeCurrent(currentContextHandle, currentDisplayHandle);
    }
}
} // namespace NEO
