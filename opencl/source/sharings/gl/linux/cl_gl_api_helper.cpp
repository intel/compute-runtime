/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sharings/gl/linux/gl_sharing_linux.h"

namespace NEO {
std::unique_ptr<GLSharingFunctions> GLSharingFunctions::create() {
    return std::make_unique<GLSharingFunctionsLinux>();
}

} // namespace NEO
