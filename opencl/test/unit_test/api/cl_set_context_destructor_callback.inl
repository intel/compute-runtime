/*
 * Copyright (C) 2020-2023 Intel Corporation
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

using clSetContextDestructorCallbackTests = ApiTests;

TEST_F(clSetContextDestructorCallbackTests, givenPfnNotifyNullptrWhenSettingContextDestructorCallbackThenInvalidValueErrorIsReturned) {
    auto retVal = clSetContextDestructorCallback(pContext, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

void CL_CALLBACK callback(cl_context, void *){};

TEST_F(clSetContextDestructorCallbackTests, WhenSettingContextDestructorCallbackThenSucccessIsReturned) {
    auto retVal = clSetContextDestructorCallback(pContext, callback, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto userData = reinterpret_cast<void *>(0x4321);
    retVal = clSetContextDestructorCallback(pContext, callback, userData);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

} // namespace ULT
