/*
 * Copyright (c) 2017, Intel Corporation
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

#include "runtime/helpers/string.h"
#include "runtime/program/program.h"
#include "gtest/gtest.h"

using namespace OCLRT;

class ProcessSpirBinaryTests : public Program,
                               public ::testing::Test {
};

TEST_F(ProcessSpirBinaryTests, NullBinary) {
    auto retVal = processSpirBinary(nullptr, 0, false);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(true, sourceCode.empty());
}

TEST_F(ProcessSpirBinaryTests, InvalidSizeBinary) {
    char pBinary[] = "somebinary\0";
    size_t binarySize = 1;
    auto retVal = processSpirBinary(pBinary, binarySize, false);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(binarySize, sourceCode.size());
}

TEST_F(ProcessSpirBinaryTests, SomeBinary) {
    char pBinary[] = "somebinary\0";
    size_t binarySize = strnlen_s(pBinary, 11);
    auto retVal = processSpirBinary(pBinary, binarySize, false);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, strcmp(pBinary, sourceCode.c_str()));
    EXPECT_EQ(binarySize, sourceCode.size());

    // Verify no built log is available
    auto pBuildLog = getBuildLog(pDevice);
    EXPECT_EQ(nullptr, pBuildLog);
}

TEST_F(ProcessSpirBinaryTests, SpirvBinary) {
    const uint32_t pBinary[2] = {0x03022307, 0x07230203};
    size_t binarySize = sizeof(pBinary);

    processSpirBinary(pBinary, binarySize, false);
    EXPECT_FALSE(this->isSpirV);

    processSpirBinary(pBinary, binarySize, true);
    EXPECT_TRUE(this->isSpirV);
}
