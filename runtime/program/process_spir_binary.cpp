/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "program.h"

namespace OCLRT {

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

    std::string binaryString(static_cast<const char *>(pBinary), binarySize);
    sourceCode.swap(binaryString);

    buildStatus = CL_BUILD_NONE;
    this->isSpirV = isSpirV;

    return CL_SUCCESS;
}
} // namespace OCLRT
