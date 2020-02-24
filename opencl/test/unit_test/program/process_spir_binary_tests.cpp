/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/helpers/string.h"

#include "opencl/test/unit_test/mocks/mock_program.h"

#include "gtest/gtest.h"

using namespace NEO;

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
    EXPECT_EQ(binarySize, program->irBinarySize);
}

TEST_F(ProcessSpirBinaryTests, SomeBinary) {
    char pBinary[] = "somebinary\0";
    size_t binarySize = strnlen_s(pBinary, 11);
    auto retVal = program->processSpirBinary(pBinary, binarySize, false);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, memcmp(pBinary, program->irBinary.get(), program->irBinarySize));
    EXPECT_EQ(binarySize, program->irBinarySize);

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
