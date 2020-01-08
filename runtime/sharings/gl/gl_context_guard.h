/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/os_interface/windows/gl/gl_sharing_os.h"
#include "runtime/sharings/gl/gl_sharing.h"

namespace NEO {
class GLContextGuard {
  public:
    GLContextGuard() = delete;
    GLContextGuard(GLSharingFunctions &sharingFcns);
    ~GLContextGuard();

  protected:
    GLSharingFunctions *sharingFunctions;

    GLContext currentContextHandle;
    GLDisplay currentDisplayHandle;
};
} // namespace NEO
