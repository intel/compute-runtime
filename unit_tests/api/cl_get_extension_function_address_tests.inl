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

} // namespace ULT
