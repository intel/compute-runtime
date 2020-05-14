/*
 * Copyright (C) 2020 Intel Corporation
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

using clSetProgramReleaseCallbackTests = api_tests;

TEST_F(clSetProgramReleaseCallbackTests, givenPfnNotifyNullptrWhenSettingProgramReleaseCallbackThenInvalidValueErrorIsReturned) {
    auto retVal = clSetProgramReleaseCallback(pProgram, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clSetProgramReleaseCallbackTests, WhenSettingProgramReleaseCallbackThenInvalidOperationErrorIsReturned) {
    using NotifyFunctionType = void(CL_CALLBACK *)(cl_program, void *);
    NotifyFunctionType pfnNotify = reinterpret_cast<NotifyFunctionType>(0x1234);
    void *userData = reinterpret_cast<void *>(0x4321);
    auto retVal = clSetProgramReleaseCallback(pProgram, pfnNotify, userData);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

} // namespace ULT
