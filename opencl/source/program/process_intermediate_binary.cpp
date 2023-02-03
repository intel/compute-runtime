/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/compiler_interface/linker.h"
#include "shared/source/helpers/string.h"

#include "program.h"

namespace NEO {
cl_int Program::processSpirBinary(
    const void *pBinary,
    size_t binarySize,
    bool isSpirV) {
    for (const auto &device : clDevices) {
        deviceBuildInfos[device].programBinaryType = CL_PROGRAM_BINARY_TYPE_INTERMEDIATE;
    }

    this->irBinary = makeCopy(pBinary, binarySize);
    this->irBinarySize = binarySize;

    setBuildStatus(CL_BUILD_NONE);

    this->isSpirV = isSpirV;

    return CL_SUCCESS;
}
} // namespace NEO
