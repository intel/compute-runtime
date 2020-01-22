/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clGetExtensionFunctionAddressForPlatformTests;

namespace ULT {

TEST_F(clGetExtensionFunctionAddressForPlatformTests, GivenNullPlatformWhenGettingExtensionFunctionThenNullIsReturned) {
    auto retVal = clGetExtensionFunctionAddressForPlatform(nullptr, "clCreateAcceleratorINTEL");
    EXPECT_EQ(retVal, nullptr);
}

TEST_F(clGetExtensionFunctionAddressForPlatformTests, GivenNonExistentExtensionWhenGettingExtensionFunctionThenNullIsReturned) {
    auto retVal = clGetExtensionFunctionAddressForPlatform(pPlatform, "__some__function__");
    EXPECT_EQ(retVal, nullptr);
}

TEST_F(clGetExtensionFunctionAddressForPlatformTests, GivenClCreateAcceleratorINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddressForPlatform(pPlatform, "clCreateAcceleratorINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clCreateAcceleratorINTEL));
}

TEST_F(clGetExtensionFunctionAddressForPlatformTests, GivenClGetAcceleratorInfoINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddressForPlatform(pPlatform, "clGetAcceleratorInfoINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clGetAcceleratorInfoINTEL));
}

TEST_F(clGetExtensionFunctionAddressForPlatformTests, GivenClRetainAcceleratorINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddressForPlatform(pPlatform, "clRetainAcceleratorINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clRetainAcceleratorINTEL));
}

TEST_F(clGetExtensionFunctionAddressForPlatformTests, GivenClReleaseAcceleratorINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddressForPlatform(pPlatform, "clReleaseAcceleratorINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clReleaseAcceleratorINTEL));
}

TEST_F(clGetExtensionFunctionAddressForPlatformTests, GivenClCreateProgramWithILKHRWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddressForPlatform(pPlatform, "clCreateProgramWithILKHR");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clCreateProgramWithILKHR));
}

TEST_F(clGetExtensionFunctionAddressForPlatformTests, GivenClCreateTracingHandleINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddressForPlatform(pPlatform, "clCreateTracingHandleINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clCreateTracingHandleINTEL));
}

TEST_F(clGetExtensionFunctionAddressForPlatformTests, GivenClSetTracingPointINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddressForPlatform(pPlatform, "clSetTracingPointINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clSetTracingPointINTEL));
}

TEST_F(clGetExtensionFunctionAddressForPlatformTests, GivenClDestroyTracingHandleINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddressForPlatform(pPlatform, "clDestroyTracingHandleINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clDestroyTracingHandleINTEL));
}

TEST_F(clGetExtensionFunctionAddressForPlatformTests, GivenClEnableTracingINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddressForPlatform(pPlatform, "clEnableTracingINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clEnableTracingINTEL));
}

TEST_F(clGetExtensionFunctionAddressForPlatformTests, GivenClDisableTracingINTELLWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddressForPlatform(pPlatform, "clDisableTracingINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clDisableTracingINTEL));
}

TEST_F(clGetExtensionFunctionAddressForPlatformTests, GivenClGetTracingStateINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddressForPlatform(pPlatform, "clGetTracingStateINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clGetTracingStateINTEL));
}

TEST_F(clGetExtensionFunctionAddressForPlatformTests, GivenClGetKernelSuggestedLocalWorkSizeINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddressForPlatform(pPlatform, "clGetKernelSuggestedLocalWorkSizeINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clGetKernelSuggestedLocalWorkSizeINTEL));
}

TEST_F(clGetExtensionFunctionAddressForPlatformTests, GivenClGetKernelMaxConcurrentWorkGroupCountINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddressForPlatform(pPlatform, "clGetKernelMaxConcurrentWorkGroupCountINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clGetKernelMaxConcurrentWorkGroupCountINTEL));
}

TEST_F(clGetExtensionFunctionAddressForPlatformTests, GivenClEnqueueNDCountKernelINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddressForPlatform(pPlatform, "clEnqueueNDCountKernelINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clEnqueueNDCountKernelINTEL));
}
} // namespace ULT
