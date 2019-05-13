/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clGetExtensionFunctionAddressTests;

namespace ULT {

TEST_F(clGetExtensionFunctionAddressTests, GivenNonExistentExtensionWhenGettingExtensionFunctionThenNullIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("__some__function__");
    EXPECT_EQ(nullptr, retVal);
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClIcdGetPlatformIDsKHRWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clIcdGetPlatformIDsKHR");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clIcdGetPlatformIDsKHR));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClCreateAcceleratorINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clCreateAcceleratorINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clCreateAcceleratorINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClGetAcceleratorInfoINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clGetAcceleratorInfoINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clGetAcceleratorInfoINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClRetainAcceleratorINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clRetainAcceleratorINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clRetainAcceleratorINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClReleaseAcceleratorINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clReleaseAcceleratorINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clReleaseAcceleratorINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClCreatePerfCountersCommandQueueINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clCreatePerfCountersCommandQueueINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clCreatePerfCountersCommandQueueINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClSetPerformanceConfigurationINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clSetPerformanceConfigurationINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clSetPerformanceConfigurationINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClCreateBufferWithPropertiesINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto functionPointer = clGetExtensionFunctionAddress("clCreateBufferWithPropertiesINTEL");
    EXPECT_EQ(functionPointer, reinterpret_cast<void *>(clCreateBufferWithPropertiesINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, givenClAddCommentToAubIntelAsInputWhenFunctionIsCalledThenProperPointerIsReturned) {
    auto functionPointer = clGetExtensionFunctionAddress("clAddCommentINTEL");
    EXPECT_EQ(functionPointer, reinterpret_cast<void *>(clAddCommentINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClCreateTracingHandleINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clCreateTracingHandleINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clCreateTracingHandleINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClSetTracingPointINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clSetTracingPointINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clSetTracingPointINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClDestroyTracingHandleINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clDestroyTracingHandleINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clDestroyTracingHandleINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClEnableTracingINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clEnableTracingINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clEnableTracingINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClDisableTracingINTELLWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clDisableTracingINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clDisableTracingINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClGetTracingStateINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clGetTracingStateINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clGetTracingStateINTEL));
}
} // namespace ULT
