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

#include "runtime/helpers/string.h"
#include "runtime/program/program.h"

#include "unit_tests/mocks/mock_program.h"

#include "gtest/gtest.h"

using namespace OCLRT;

class ProcessSpirBinaryTests : public ::testing::Test {
  public:
    void SetUp() override {
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        program = std::make_unique<MockProgram>(*executionEnvironment);
    }

    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    std::unique_ptr<MockProgram> program;
};

TEST_F(ProcessSpirBinaryTests, NullBinary) {
    auto retVal = program->processSpirBinary(nullptr, 0, false);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(true, program->sourceCode.empty());
}

TEST_F(ProcessSpirBinaryTests, InvalidSizeBinary) {
    char pBinary[] = "somebinary\0";
    size_t binarySize = 1;
    auto retVal = program->processSpirBinary(pBinary, binarySize, false);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(binarySize, program->sourceCode.size());
}

TEST_F(ProcessSpirBinaryTests, SomeBinary) {
    char pBinary[] = "somebinary\0";
    size_t binarySize = strnlen_s(pBinary, 11);
    auto retVal = program->processSpirBinary(pBinary, binarySize, false);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, strcmp(pBinary, program->sourceCode.c_str()));
    EXPECT_EQ(binarySize, program->sourceCode.size());

    // Verify no built log is available
    auto pBuildLog = program->getBuildLog(program->getDevicePtr());
    EXPECT_EQ(nullptr, pBuildLog);
}

TEST_F(ProcessSpirBinaryTests, SpirvBinary) {
    const uint32_t pBinary[2] = {0x03022307, 0x07230203};
    size_t binarySize = sizeof(pBinary);

    program->processSpirBinary(pBinary, binarySize, false);
    EXPECT_FALSE(program->getIsSpirV());

    program->processSpirBinary(pBinary, binarySize, true);
    EXPECT_TRUE(program->getIsSpirV());
}
