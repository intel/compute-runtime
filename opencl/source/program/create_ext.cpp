/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/program/program.h"

namespace NEO {
cl_int Program::createFromILExt(Context *context, const void *il, size_t length) {
    return CL_INVALID_BINARY;
}
} // namespace NEO
