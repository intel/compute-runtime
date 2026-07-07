/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/sharings/gl/leo_gl_context_guard.h"
#include "level_zero/api/opencl/source/sharings/gl/linux/leo_gl_sharing_linux.h"

namespace NEO {
namespace LEO {

GLContextGuard::GLContextGuard(GLSharingFunctions &sharingFcns) : sharingFunctions(&sharingFcns) {
}

GLContextGuard::~GLContextGuard() {
}

} // namespace LEO
} // namespace NEO
