/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/api/cl_api_tests.h"

using namespace NEO;

namespace ULT {

TEST(clSetProgramReleaseCallbackTest, givenNullptrProgramWhenSettingProgramReleaseCallbackThenInvalidProgramErrorIsReturned) {
    auto retVal = clSetProgramReleaseCallback(nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);
}

using clSetProgramReleaseCallbackTests = ApiTests;

TEST_F(clSetProgramReleaseCallbackTests, givenPfnNotifyNullptrWhenSettingProgramReleaseCallbackThenInvalidValueErrorIsReturned) {
    auto retVal = clSetProgramReleaseCallback(pProgram, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

void CL_CALLBACK callback(cl_program, void *){};

TEST_F(clSetProgramReleaseCallbackTests, WhenSettingProgramReleaseCallbackThenInvalidOperationErrorIsReturned) {
    auto retVal = clSetProgramReleaseCallback(pProgram, callback, nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);

    auto userData = reinterpret_cast<void *>(0x4321);
    retVal = clSetProgramReleaseCallback(pProgram, callback, userData);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

} // namespace ULT
