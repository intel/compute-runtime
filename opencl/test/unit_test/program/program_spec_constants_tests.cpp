/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/compiler_interface/compiler_interface.inl"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/global_environment.h"
#include "opencl/test/unit_test/helpers/test_files.h"
#include "opencl/test/unit_test/mocks/mock_cif.h"
#include "opencl/test/unit_test/mocks/mock_compilers.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "gmock/gmock.h"

#include <memory>

using namespace NEO;

struct UpdateSpecConstantsTest : public ::testing::Test {
    void SetUp() override {
        mockProgram.reset(new MockProgram(executionEnvironment));

        mockProgram->specConstantsIds.reset(new MockCIFBuffer());
        mockProgram->specConstantsSizes.reset(new MockCIFBuffer());
        mockProgram->specConstantsValues.reset(new MockCIFBuffer());

        uint32_t id1 = 1u, id2 = 2u, id3 = 3u;
        mockProgram->specConstantsIds->PushBackRawCopy(id1);
        mockProgram->specConstantsIds->PushBackRawCopy(id2);
        mockProgram->specConstantsIds->PushBackRawCopy(id3);

        uint32_t size1 = sizeof(char), size2 = sizeof(uint16_t), size3 = sizeof(int);
        mockProgram->specConstantsSizes->PushBackRawCopy(size1);
        mockProgram->specConstantsSizes->PushBackRawCopy(size2);
        mockProgram->specConstantsSizes->PushBackRawCopy(size3);

        mockProgram->specConstantsValues->PushBackRawCopy(static_cast<uint64_t>(val1));
        mockProgram->specConstantsValues->PushBackRawCopy(static_cast<uint64_t>(val2));
        mockProgram->specConstantsValues->PushBackRawCopy(static_cast<uint64_t>(val3));

        values = mockProgram->specConstantsValues->GetMemoryWriteable<uint64_t>();

        EXPECT_EQ(val1, static_cast<char>(values[0]));
        EXPECT_EQ(val2, static_cast<uint16_t>(values[1]));
        EXPECT_EQ(val3, static_cast<int>(values[2]));
    }
    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<MockProgram> mockProgram;

    char val1 = 5;
    uint16_t val2 = 50;
    int val3 = 500;
    uint64_t *values;
};

TEST_F(UpdateSpecConstantsTest, givenNewSpecConstValueWhenUpdateSpecializationConstantThenProperValueIsCopiedAndUpdated) {
    int newSpecConstVal3 = 5000;

    auto ret = mockProgram->updateSpecializationConstant(3, sizeof(int), &newSpecConstVal3);

    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_EQ(val1, static_cast<char>(values[0]));
    EXPECT_EQ(val2, static_cast<uint16_t>(values[1]));
    EXPECT_EQ(newSpecConstVal3, static_cast<int>(values[2]));
    EXPECT_NE(val3, static_cast<int>(values[2]));

    newSpecConstVal3 = 50000;
    EXPECT_NE(newSpecConstVal3, static_cast<int>(values[2]));

    ret = mockProgram->updateSpecializationConstant(3, sizeof(int), &newSpecConstVal3);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_EQ(newSpecConstVal3, static_cast<int>(values[2]));
}

TEST_F(UpdateSpecConstantsTest, givenNewSpecConstValueWithUnproperSizeWhenUpdateSpecializationConstantThenErrorIsReturned) {
    int newSpecConstVal3 = 5000;

    auto ret = mockProgram->updateSpecializationConstant(3, 10 * sizeof(int), &newSpecConstVal3);

    EXPECT_EQ(CL_INVALID_VALUE, ret);
    EXPECT_EQ(val1, static_cast<char>(values[0]));
    EXPECT_EQ(val2, static_cast<uint16_t>(values[1]));
    EXPECT_EQ(val3, static_cast<int>(values[2]));
}

TEST_F(UpdateSpecConstantsTest, givenNewSpecConstValueWithUnproperIdAndSizeWhenUpdateSpecializationConstantThenErrorIsReturned) {
    int newSpecConstVal3 = 5000;

    auto ret = mockProgram->updateSpecializationConstant(4, sizeof(int), &newSpecConstVal3);

    EXPECT_EQ(CL_INVALID_SPEC_ID, ret);
    EXPECT_EQ(val1, static_cast<char>(values[0]));
    EXPECT_EQ(val2, static_cast<uint16_t>(values[1]));
    EXPECT_EQ(val3, static_cast<int>(values[2]));
}
