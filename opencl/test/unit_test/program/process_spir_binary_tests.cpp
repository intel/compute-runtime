/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/string.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "gtest/gtest.h"

using namespace NEO;

class ProcessSpirBinaryTests : public ::testing::Test {
  public:
    void SetUp() override {
        device = std::make_unique<MockClDevice>(new MockDevice());
        program = std::make_unique<MockProgram>(toClDeviceVector(*device));
    }

    std::unique_ptr<ClDevice> device;
    std::unique_ptr<MockProgram> program;
};

TEST_F(ProcessSpirBinaryTests, GivenNullBinaryWhenProcessingSpirBinaryThenSourceCodeIsEmpty) {
    auto retVal = program->processSpirBinary(nullptr, 0, false);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(true, program->sourceCode.empty());
}

TEST_F(ProcessSpirBinaryTests, GivenInvalidSizeBinaryWhenProcessingSpirBinaryThenIrBinarSizeIsSetToPassedValue) {
    char pBinary[] = "somebinary\0";
    size_t binarySize = 1;
    auto retVal = program->processSpirBinary(pBinary, binarySize, false);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(binarySize, program->irBinarySize);
}

TEST_F(ProcessSpirBinaryTests, WhenProcessingSpirBinaryThenIrBinaryIsSetCorrectly) {
    char pBinary[] = "somebinary\0";
    size_t binarySize = strnlen_s(pBinary, 11);
    auto retVal = program->processSpirBinary(pBinary, binarySize, false);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, memcmp(pBinary, program->irBinary.get(), program->irBinarySize));
    EXPECT_EQ(binarySize, program->irBinarySize);

    // Verify no built log is available
    std::string buildLog = program->getBuildLog(0);
    EXPECT_TRUE(buildLog.empty());
}

TEST_F(ProcessSpirBinaryTests, WhenProcessingSpirBinaryThenIsSpirvIsSetBasedonPassedValue) {
    const uint32_t pBinary[2] = {0x03022307, 0x07230203};
    size_t binarySize = sizeof(pBinary);

    program->processSpirBinary(pBinary, binarySize, false);
    EXPECT_FALSE(program->getIsSpirV());

    program->processSpirBinary(pBinary, binarySize, true);
    EXPECT_TRUE(program->getIsSpirV());
}
