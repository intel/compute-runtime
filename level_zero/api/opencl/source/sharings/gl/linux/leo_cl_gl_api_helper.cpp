/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/sharings/gl/linux/leo_gl_sharing_linux.h"

namespace NEO {
namespace LEO {

std::unique_ptr<GLSharingFunctions> GLSharingFunctions::create() {
    return std::make_unique<GLSharingFunctionsLinux>();
}

} // namespace LEO
} // namespace NEO
