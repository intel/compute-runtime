/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/api/cl_api_tests.h"

using namespace NEO;

namespace ULT {

TEST(clSetContextDestructorCallbackTest, givenNullptrContextWhenSettingContextDestructorCallbackThenInvalidContextErrorIsReturned) {
    auto retVal = clSetContextDestructorCallback(nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

using clSetContextDestructorCallbackTests = api_tests;

TEST_F(clSetContextDestructorCallbackTests, givenPfnNotifyNullptrWhenSettingContextDestructorCallbackThenInvalidValueErrorIsReturned) {
    auto retVal = clSetContextDestructorCallback(pContext, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clSetContextDestructorCallbackTests, WhenSettingContextDestructorCallbackThenOutOfHostMemoryErrorIsReturned) {
    using NotifyFunctionType = void(CL_CALLBACK *)(cl_context, void *);
    NotifyFunctionType pfnNotify = reinterpret_cast<NotifyFunctionType>(0x1234);
    void *userData = reinterpret_cast<void *>(0x4321);
    auto retVal = clSetContextDestructorCallback(pContext, pfnNotify, userData);
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
}

} // namespace ULT
