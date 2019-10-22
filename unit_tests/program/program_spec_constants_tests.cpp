/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/compiler_interface/compiler_interface.h"
#include "core/compiler_interface/compiler_interface.inl"
#include "core/helpers/file_io.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/helpers/hw_info.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/global_environment.h"
#include "unit_tests/helpers/test_files.h"
#include "unit_tests/mocks/mock_cif.h"
#include "unit_tests/mocks/mock_compilers.h"
#include "unit_tests/mocks/mock_program.h"

#include "gmock/gmock.h"

#include <memory>

using namespace NEO;

struct UpdateSpecConstantsTest : public ::testing::Test {
    void SetUp() override {
        mockProgram.reset(new MockProgram(executionEnvironment));

        mockProgram->specConstantsIds.reset(new MockCIFBuffer());
        mockProgram->specConstantsSizes.reset(new MockCIFBuffer());
        mockProgram->specConstantsValues.reset(new MockCIFBuffer());

        mockProgram->specConstantsIds->PushBackRawCopy(1);
        mockProgram->specConstantsIds->PushBackRawCopy(2);
        mockProgram->specConstantsIds->PushBackRawCopy(3);

        mockProgram->specConstantsSizes->PushBackRawCopy(sizeof(char));
        mockProgram->specConstantsSizes->PushBackRawCopy(sizeof(uint16_t));
        mockProgram->specConstantsSizes->PushBackRawCopy(sizeof(int));

        mockProgram->specConstantsValues->PushBackRawCopy(&val1);
        mockProgram->specConstantsValues->PushBackRawCopy(&val2);
        mockProgram->specConstantsValues->PushBackRawCopy(&val3);

        values = mockProgram->specConstantsValues->GetMemory<const void *>();

        EXPECT_EQ(val1, *reinterpret_cast<const char *>(values[0]));
        EXPECT_EQ(val2, *reinterpret_cast<const uint16_t *>(values[1]));
        EXPECT_EQ(val3, *reinterpret_cast<const int *>(values[2]));
    }
    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<MockProgram> mockProgram;

    char val1 = 5;
    uint16_t val2 = 50;
    int val3 = 500;
    const void *const *values;
};

TEST_F(UpdateSpecConstantsTest, givenNewSpecConstValueWhenUpdateSpecializationConstantThenProperValueIsUpdated) {
    int newSpecConstVal3 = 5000;

    auto ret = mockProgram->updateSpecializationConstant(3, sizeof(int), &newSpecConstVal3);

    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_EQ(val1, *reinterpret_cast<const char *>(values[0]));
    EXPECT_EQ(val2, *reinterpret_cast<const uint16_t *>(values[1]));
    EXPECT_EQ(newSpecConstVal3, *reinterpret_cast<const int *>(values[2]));
}

TEST_F(UpdateSpecConstantsTest, givenNewSpecConstValueWithUnproperSizeWhenUpdateSpecializationConstantThenErrorIsReturned) {
    int newSpecConstVal3 = 5000;

    auto ret = mockProgram->updateSpecializationConstant(3, 10 * sizeof(int), &newSpecConstVal3);

    EXPECT_EQ(CL_INVALID_VALUE, ret);
    EXPECT_EQ(val1, *reinterpret_cast<const char *>(values[0]));
    EXPECT_EQ(val2, *reinterpret_cast<const uint16_t *>(values[1]));
    EXPECT_EQ(val3, *reinterpret_cast<const int *>(values[2]));
}

TEST_F(UpdateSpecConstantsTest, givenNewSpecConstValueWithUnproperIdAndSizeWhenUpdateSpecializationConstantThenErrorIsReturned) {
    int newSpecConstVal3 = 5000;

    auto ret = mockProgram->updateSpecializationConstant(4, sizeof(int), &newSpecConstVal3);

    EXPECT_EQ(CL_INVALID_SPEC_ID, ret);
    EXPECT_EQ(val1, *reinterpret_cast<const char *>(values[0]));
    EXPECT_EQ(val2, *reinterpret_cast<const uint16_t *>(values[1]));
    EXPECT_EQ(val3, *reinterpret_cast<const int *>(values[2]));
}
