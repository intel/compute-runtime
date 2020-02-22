/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "program.h"

namespace NEO {
cl_int Program::processSpirBinary(
    const void *pBinary,
    size_t binarySize,
    bool isSpirV) {
    programBinaryType = CL_PROGRAM_BINARY_TYPE_INTERMEDIATE;

    this->irBinary = makeCopy(pBinary, binarySize);
    this->irBinarySize = binarySize;

    buildStatus = CL_BUILD_NONE;
    this->isSpirV = isSpirV;

    return CL_SUCCESS;
}
} // namespace NEO
