/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "program.h"

namespace NEO {

bool Program::isValidSpirvBinary(
    const void *pBinary,
    size_t binarySize) {

    const uint32_t magicWord[2] = {0x03022307, 0x07230203};
    bool retVal = false;

    if (pBinary && (binarySize > sizeof(uint32_t))) {
        if ((memcmp(pBinary, &magicWord[0], sizeof(uint32_t)) == 0) ||
            (memcmp(pBinary, &magicWord[1], sizeof(uint32_t)) == 0)) {
            retVal = true;
        }
    }
    return retVal;
}

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
