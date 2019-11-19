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

TEST_F(clGetExtensionFunctionAddressTests, GivenClCreateImageWithPropertiesINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto functionPointer = clGetExtensionFunctionAddress("clCreateImageWithPropertiesINTEL");
    EXPECT_EQ(functionPointer, reinterpret_cast<void *>(clCreateImageWithPropertiesINTEL));
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

TEST_F(clGetExtensionFunctionAddressTests, GivenClHostMemAllocINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clHostMemAllocINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clHostMemAllocINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClDeviceMemAllocINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clDeviceMemAllocINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clDeviceMemAllocINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClSharedMemAllocINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clSharedMemAllocINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clSharedMemAllocINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClMemFreeINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clMemFreeINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clMemFreeINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClGetMemAllocInfoINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clGetMemAllocInfoINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clGetMemAllocInfoINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClSetKernelArgMemPointerINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clSetKernelArgMemPointerINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clSetKernelArgMemPointerINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClEnqueueMemsetINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clEnqueueMemsetINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clEnqueueMemsetINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClEnqueueMemFillINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clEnqueueMemFillINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clEnqueueMemFillINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClEnqueueMemcpyINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clEnqueueMemcpyINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clEnqueueMemcpyINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClEnqueueMigrateMemINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clEnqueueMigrateMemINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clEnqueueMigrateMemINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClEnqueueMemAdviseINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clEnqueueMemAdviseINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clEnqueueMemAdviseINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClGetDeviceGlobalVariablePointerINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clGetDeviceGlobalVariablePointerINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clGetDeviceGlobalVariablePointerINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClGetDeviceFunctionPointerINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clGetDeviceFunctionPointerINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clGetDeviceFunctionPointerINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClGetExecutionInfoINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clGetExecutionInfoINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clGetExecutionInfoINTEL));
}

TEST_F(clGetExtensionFunctionAddressTests, GivenClEnqueueNDRangeKernelINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clEnqueueNDRangeKernelINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clEnqueueNDRangeKernelINTEL));
}
} // namespace ULT
