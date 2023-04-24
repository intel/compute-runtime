/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"

using namespace NEO;

using ClGetExtensionFunctionAddressTests = ApiTests;

namespace ULT {

TEST_F(ClGetExtensionFunctionAddressTests, GivenNonExistentExtensionWhenGettingExtensionFunctionThenNullIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("__some__function__");
    EXPECT_EQ(nullptr, retVal);
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClIcdGetPlatformIDsKHRWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clIcdGetPlatformIDsKHR");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clIcdGetPlatformIDsKHR));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClCreateAcceleratorINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clCreateAcceleratorINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clCreateAcceleratorINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClGetAcceleratorInfoINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clGetAcceleratorInfoINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clGetAcceleratorInfoINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClRetainAcceleratorINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clRetainAcceleratorINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clRetainAcceleratorINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClReleaseAcceleratorINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clReleaseAcceleratorINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clReleaseAcceleratorINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClCreatePerfCountersCommandQueueINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clCreatePerfCountersCommandQueueINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clCreatePerfCountersCommandQueueINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClSetPerformanceConfigurationINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clSetPerformanceConfigurationINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clSetPerformanceConfigurationINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClCreateBufferWithPropertiesINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto functionPointer = clGetExtensionFunctionAddress("clCreateBufferWithPropertiesINTEL");
    EXPECT_EQ(functionPointer, reinterpret_cast<void *>(clCreateBufferWithPropertiesINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClCreateImageWithPropertiesINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto functionPointer = clGetExtensionFunctionAddress("clCreateImageWithPropertiesINTEL");
    EXPECT_EQ(functionPointer, reinterpret_cast<void *>(clCreateImageWithPropertiesINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, givenClAddCommentToAubIntelAsInputWhenFunctionIsCalledThenProperPointerIsReturned) {
    auto functionPointer = clGetExtensionFunctionAddress("clAddCommentINTEL");
    EXPECT_EQ(functionPointer, reinterpret_cast<void *>(clAddCommentINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClCreateTracingHandleINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clCreateTracingHandleINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clCreateTracingHandleINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClSetTracingPointINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clSetTracingPointINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clSetTracingPointINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClDestroyTracingHandleINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clDestroyTracingHandleINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clDestroyTracingHandleINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClEnableTracingINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clEnableTracingINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clEnableTracingINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClDisableTracingINTELLWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clDisableTracingINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clDisableTracingINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClGetTracingStateINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clGetTracingStateINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clGetTracingStateINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClHostMemAllocINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clHostMemAllocINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clHostMemAllocINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClDeviceMemAllocINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clDeviceMemAllocINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clDeviceMemAllocINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClSharedMemAllocINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clSharedMemAllocINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clSharedMemAllocINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClMemFreeINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clMemFreeINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clMemFreeINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClMemBlockingFreeINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clMemBlockingFreeINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clMemBlockingFreeINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClGetMemAllocInfoINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clGetMemAllocInfoINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clGetMemAllocInfoINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClSetKernelArgMemPointerINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clSetKernelArgMemPointerINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clSetKernelArgMemPointerINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClEnqueueMemsetINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clEnqueueMemsetINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clEnqueueMemsetINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClEnqueueMemFillINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clEnqueueMemFillINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clEnqueueMemFillINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClEnqueueMemcpyINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clEnqueueMemcpyINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clEnqueueMemcpyINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClEnqueueMigrateMemINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clEnqueueMigrateMemINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clEnqueueMigrateMemINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClEnqueueMemAdviseINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clEnqueueMemAdviseINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clEnqueueMemAdviseINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClGetDeviceGlobalVariablePointerINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clGetDeviceGlobalVariablePointerINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clGetDeviceGlobalVariablePointerINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClGetDeviceFunctionPointerINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clGetDeviceFunctionPointerINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clGetDeviceFunctionPointerINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClGetKernelSuggestedLocalWorkSizeINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clGetKernelSuggestedLocalWorkSizeINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clGetKernelSuggestedLocalWorkSizeINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClGetKernelSuggestedLocalWorkSizeKHRWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clGetKernelSuggestedLocalWorkSizeKHR");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clGetKernelSuggestedLocalWorkSizeKHR));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClGetKernelMaxConcurrentWorkGroupCountINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clGetKernelMaxConcurrentWorkGroupCountINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clGetKernelMaxConcurrentWorkGroupCountINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenClEnqueueNDCountKernelINTELWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clEnqueueNDCountKernelINTEL");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clEnqueueNDCountKernelINTEL));
}

TEST_F(ClGetExtensionFunctionAddressTests, GivenCSlSetProgramSpecializationConstantWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clSetProgramSpecializationConstant");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clSetProgramSpecializationConstant));
}
} // namespace ULT
