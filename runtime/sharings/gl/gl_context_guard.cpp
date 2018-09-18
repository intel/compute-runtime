/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/sharings/gl/gl_sharing.h"

namespace OCLRT {
GLContextGuard::GLContextGuard(GLSharingFunctions &sharingFcns) : sharingFunctions(&sharingFcns) {
    currentContextHandle = sharingFcns.getCurrentContext();
    currentDisplayHandle = sharingFcns.getCurrentDisplay();
    auto ctxToMakeCurrent = sharingFcns.getContextHandle();

    if (currentContextHandle == 0) {
        ctxToMakeCurrent = sharingFcns.getBackupContextHandle();
    }

    if (currentContextHandle != sharingFcns.getContextHandle() && currentContextHandle != sharingFcns.getBackupContextHandle()) {
        if (sharingFcns.makeCurrent(ctxToMakeCurrent) == GL_FALSE) {
            while (sharingFcns.makeCurrent(sharingFcns.getBackupContextHandle()) == GL_FALSE) {
                ;
            }
        }
    }
}

GLContextGuard::~GLContextGuard() {
    if (currentContextHandle != sharingFunctions->getContextHandle()) {
        sharingFunctions->makeCurrent(currentContextHandle, currentDisplayHandle);
    }
}
} // namespace OCLRT
