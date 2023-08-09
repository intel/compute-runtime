/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sharings/gl/gl_context_guard.h"
#include "opencl/source/sharings/gl/linux/gl_sharing_linux.h"

namespace NEO {
GLContextGuard::GLContextGuard(GLSharingFunctions &sharingFcns) : sharingFunctions(&sharingFcns) {
}

GLContextGuard::~GLContextGuard() {
}
} // namespace NEO
