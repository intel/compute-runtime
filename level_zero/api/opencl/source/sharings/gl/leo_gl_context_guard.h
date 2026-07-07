/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/api/opencl/source/sharings/gl/leo_gl_sharing.h"

#include "leo_gl_types.h"

namespace NEO {
namespace LEO {

class GLContextGuard {
  public:
    GLContextGuard() = delete;
    GLContextGuard(GLSharingFunctions &sharingFcns);
    ~GLContextGuard();

  protected:
    GLSharingFunctions *sharingFunctions = nullptr;

    GLContext currentContextHandle{};
    GLDisplay currentDisplayHandle{};
};

} // namespace LEO
} // namespace NEO
