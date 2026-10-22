/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/api/api.h"

#include "cl_api_tests.h"

using namespace NEO;

namespace ULT {

TEST(ClGetExtensionFunctionAddressTests, GivenNonExistentExtensionWhenGettingExtensionFunctionThenNullIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("__some__function__");
    EXPECT_EQ(nullptr, retVal);
}

TEST(ClGetExtensionFunctionAddressTests, GivenExtensionFunctionNameWhenGettingExtensionFunctionAddressThenCorrectAddressIsReturned) {
    EXPECT_EQ(reinterpret_cast<void *>(clIcdGetPlatformIDsKHR), clGetExtensionFunctionAddress("clIcdGetPlatformIDsKHR"));
    EXPECT_EQ(reinterpret_cast<void *>(clCreateAcceleratorINTEL), clGetExtensionFunctionAddress("clCreateAcceleratorINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clGetAcceleratorInfoINTEL), clGetExtensionFunctionAddress("clGetAcceleratorInfoINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clRetainAcceleratorINTEL), clGetExtensionFunctionAddress("clRetainAcceleratorINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clReleaseAcceleratorINTEL), clGetExtensionFunctionAddress("clReleaseAcceleratorINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clCreatePerfCountersCommandQueueINTEL), clGetExtensionFunctionAddress("clCreatePerfCountersCommandQueueINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clSetPerformanceConfigurationINTEL), clGetExtensionFunctionAddress("clSetPerformanceConfigurationINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clCreateBufferWithPropertiesINTEL), clGetExtensionFunctionAddress("clCreateBufferWithPropertiesINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clCreateImageWithPropertiesINTEL), clGetExtensionFunctionAddress("clCreateImageWithPropertiesINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clAddCommentINTEL), clGetExtensionFunctionAddress("clAddCommentINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clCreateTracingHandleINTEL), clGetExtensionFunctionAddress("clCreateTracingHandleINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clSetTracingPointINTEL), clGetExtensionFunctionAddress("clSetTracingPointINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clDestroyTracingHandleINTEL), clGetExtensionFunctionAddress("clDestroyTracingHandleINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clEnableTracingINTEL), clGetExtensionFunctionAddress("clEnableTracingINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clDisableTracingINTEL), clGetExtensionFunctionAddress("clDisableTracingINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clGetTracingStateINTEL), clGetExtensionFunctionAddress("clGetTracingStateINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clHostMemAllocINTEL), clGetExtensionFunctionAddress("clHostMemAllocINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clDeviceMemAllocINTEL), clGetExtensionFunctionAddress("clDeviceMemAllocINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clSharedMemAllocINTEL), clGetExtensionFunctionAddress("clSharedMemAllocINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clMemFreeINTEL), clGetExtensionFunctionAddress("clMemFreeINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clMemBlockingFreeINTEL), clGetExtensionFunctionAddress("clMemBlockingFreeINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clGetMemAllocInfoINTEL), clGetExtensionFunctionAddress("clGetMemAllocInfoINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clSetKernelArgMemPointerINTEL), clGetExtensionFunctionAddress("clSetKernelArgMemPointerINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clEnqueueMemsetINTEL), clGetExtensionFunctionAddress("clEnqueueMemsetINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clEnqueueMemFillINTEL), clGetExtensionFunctionAddress("clEnqueueMemFillINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clEnqueueMemcpyINTEL), clGetExtensionFunctionAddress("clEnqueueMemcpyINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clEnqueueMigrateMemINTEL), clGetExtensionFunctionAddress("clEnqueueMigrateMemINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clEnqueueMemAdviseINTEL), clGetExtensionFunctionAddress("clEnqueueMemAdviseINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clGetDeviceGlobalVariablePointerINTEL), clGetExtensionFunctionAddress("clGetDeviceGlobalVariablePointerINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clGetDeviceFunctionPointerINTEL), clGetExtensionFunctionAddress("clGetDeviceFunctionPointerINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clGetKernelSuggestedLocalWorkSizeINTEL), clGetExtensionFunctionAddress("clGetKernelSuggestedLocalWorkSizeINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clGetKernelSuggestedLocalWorkSizeKHR), clGetExtensionFunctionAddress("clGetKernelSuggestedLocalWorkSizeKHR"));
    EXPECT_EQ(reinterpret_cast<void *>(clGetKernelSuggestedLocalWorkSize), clGetExtensionFunctionAddress("clGetKernelSuggestedLocalWorkSize"));
    EXPECT_EQ(reinterpret_cast<void *>(clGetKernelMaxConcurrentWorkGroupCountINTEL), clGetExtensionFunctionAddress("clGetKernelMaxConcurrentWorkGroupCountINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clEnqueueNDCountKernelINTEL), clGetExtensionFunctionAddress("clEnqueueNDCountKernelINTEL"));
    EXPECT_EQ(reinterpret_cast<void *>(clSetProgramSpecializationConstant), clGetExtensionFunctionAddress("clSetProgramSpecializationConstant"));
}
} // namespace ULT
