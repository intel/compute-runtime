/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/tracing/tracing_api.h"
#include "opencl/source/tracing/tracing_notify.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"

using namespace NEO;

namespace ULT {

struct IntelTracingTest : public ApiTests {
  public:
    IntelTracingTest() {}

    void SetUp() override {
        ApiTests::SetUp();
    }

    void TearDown() override {
        ApiTests::TearDown();
    }

  protected:
    static void callback(ClFunctionId fid, cl_callback_data *callbackData, void *userData) {
        ASSERT_NE(nullptr, userData);
        IntelTracingTest *base = (IntelTracingTest *)userData;
        base->vcallback(fid, callbackData, nullptr);
    }

    virtual void vcallback(ClFunctionId fid, cl_callback_data *callbackData, void *userData) {}

  protected:
    cl_tracing_handle handle = nullptr;
    cl_int status = CL_SUCCESS;
};

TEST_F(IntelTracingTest, GivenInvalidDeviceWhenCreatingTracingHandleThenInvalidValueErrorIsReturned) {
    status = clCreateTracingHandleINTEL(nullptr, callback, nullptr, &handle);
    EXPECT_EQ(static_cast<cl_tracing_handle>(nullptr), handle);
    EXPECT_EQ(CL_INVALID_VALUE, status);
}

TEST_F(IntelTracingTest, GivenInvalidCallbackExpectFailWhenCreatingTracingHandleThenInvalidValueErrorIsReturned) {
    status = clCreateTracingHandleINTEL(testedClDevice, nullptr, nullptr, &handle);
    EXPECT_EQ(static_cast<cl_tracing_handle>(nullptr), handle);
    EXPECT_EQ(CL_INVALID_VALUE, status);
}

TEST_F(IntelTracingTest, GivenInvalidHandlePointerWhenCreatingTracingHandleThenInvalidValueErrorIsReturned) {
    status = clCreateTracingHandleINTEL(testedClDevice, callback, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, status);
}

TEST_F(IntelTracingTest, GivenNullHandleWhenCallingTracingFunctionThenInvalidValueErrorIsReturned) {
    status = clSetTracingPointINTEL(nullptr, CL_FUNCTION_clBuildProgram, CL_TRUE);
    EXPECT_EQ(CL_INVALID_VALUE, status);

    status = clDestroyTracingHandleINTEL(nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, status);

    status = clEnableTracingINTEL(nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, status);

    status = clDisableTracingINTEL(nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, status);

    status = clSetTracingPointINTEL(nullptr, CL_FUNCTION_clBuildProgram, CL_FALSE);
    EXPECT_EQ(CL_INVALID_VALUE, status);

    cl_bool enabled = CL_FALSE;
    status = clGetTracingStateINTEL(nullptr, &enabled);
    EXPECT_EQ(CL_INVALID_VALUE, status);
}

TEST_F(IntelTracingTest, GivenInactiveHandleWhenCallingTracingFunctionThenInvalidValueErrorIsReturned) {
    status = clCreateTracingHandleINTEL(testedClDevice, callback, nullptr, &handle);
    EXPECT_EQ(CL_SUCCESS, status);

    status = clDisableTracingINTEL(handle);
    EXPECT_EQ(CL_INVALID_VALUE, status);

    status = clDestroyTracingHandleINTEL(handle);
    EXPECT_EQ(CL_SUCCESS, status);
}

TEST_F(IntelTracingTest, GivenTooManyHandlesWhenEnablingTracingFunctionThenOutOfResourcesErrorIsReturned) {
    cl_tracing_handle handle[HostSideTracing::tracingMaxHandleCount + 1] = {nullptr};

    for (uint32_t i = 0; i < HostSideTracing::tracingMaxHandleCount + 1; ++i) {
        status = clCreateTracingHandleINTEL(testedClDevice, callback, nullptr, &(handle[i]));
        EXPECT_EQ(CL_SUCCESS, status);
    }

    for (uint32_t i = 0; i < HostSideTracing::tracingMaxHandleCount; ++i) {
        status = clEnableTracingINTEL(handle[i]);
        EXPECT_EQ(CL_SUCCESS, status);
    }

    status = clEnableTracingINTEL(handle[HostSideTracing::tracingMaxHandleCount]);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, status);

    for (uint32_t i = 0; i < HostSideTracing::tracingMaxHandleCount; ++i) {
        status = clDisableTracingINTEL(handle[i]);
        EXPECT_EQ(CL_SUCCESS, status);
    }

    for (uint32_t i = 0; i < HostSideTracing::tracingMaxHandleCount + 1; ++i) {
        status = clDestroyTracingHandleINTEL(handle[i]);
        EXPECT_EQ(CL_SUCCESS, status);
    }
}

TEST_F(IntelTracingTest, GivenInactiveHandleWhenDisablingTracingThenInvalidValueIsReturned) {
    cl_tracing_handle handle[HostSideTracing::tracingMaxHandleCount + 1] = {nullptr};

    for (uint32_t i = 0; i < HostSideTracing::tracingMaxHandleCount + 1; ++i) {
        status = clCreateTracingHandleINTEL(testedClDevice, callback, nullptr, &(handle[i]));
        EXPECT_EQ(CL_SUCCESS, status);
    }

    for (uint32_t i = 0; i < HostSideTracing::tracingMaxHandleCount; ++i) {
        status = clEnableTracingINTEL(handle[i]);
        EXPECT_EQ(CL_SUCCESS, status);
    }

    status = clDisableTracingINTEL(handle[HostSideTracing::tracingMaxHandleCount]);
    EXPECT_EQ(CL_INVALID_VALUE, status);

    cl_bool enable = CL_TRUE;
    status = clGetTracingStateINTEL(handle[HostSideTracing::tracingMaxHandleCount], &enable);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_EQ(static_cast<cl_bool>(CL_FALSE), enable);

    for (uint32_t i = 0; i < HostSideTracing::tracingMaxHandleCount; ++i) {
        status = clDisableTracingINTEL(handle[i]);
        EXPECT_EQ(CL_SUCCESS, status);
    }

    for (uint32_t i = 0; i < HostSideTracing::tracingMaxHandleCount + 1; ++i) {
        status = clDestroyTracingHandleINTEL(handle[i]);
        EXPECT_EQ(CL_SUCCESS, status);
    }
}

TEST_F(IntelTracingTest, GivenValidParamsWhenCallingTracingFunctionsThenSuccessIsReturned) {
    status = clCreateTracingHandleINTEL(testedClDevice, callback, this, &handle);
    EXPECT_EQ(CL_SUCCESS, status);

    status = clSetTracingPointINTEL(handle, CL_FUNCTION_clBuildProgram, CL_TRUE);
    EXPECT_EQ(CL_SUCCESS, status);

    cl_bool enabled = CL_FALSE;
    status = clGetTracingStateINTEL(handle, &enabled);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_EQ(static_cast<cl_bool>(CL_FALSE), enabled);

    status = clEnableTracingINTEL(handle);
    EXPECT_EQ(CL_SUCCESS, status);

    status = clGetTracingStateINTEL(handle, &enabled);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), enabled);

    status = clDisableTracingINTEL(handle);
    EXPECT_EQ(CL_SUCCESS, status);

    status = clGetTracingStateINTEL(handle, &enabled);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_EQ(static_cast<cl_bool>(CL_FALSE), enabled);

    status = clSetTracingPointINTEL(handle, CL_FUNCTION_clBuildProgram, CL_FALSE);
    EXPECT_EQ(CL_SUCCESS, status);

    status = clDestroyTracingHandleINTEL(handle);
    EXPECT_EQ(CL_SUCCESS, status);
}

TEST_F(IntelTracingTest, GivenTwoHandlesWhenCallingTracingFunctionsThenSuccessIsReturned) {
    cl_tracing_handle handle1 = nullptr;
    status = clCreateTracingHandleINTEL(testedClDevice, callback, this, &handle1);
    EXPECT_EQ(CL_SUCCESS, status);
    cl_tracing_handle handle2 = nullptr;
    status = clCreateTracingHandleINTEL(testedClDevice, callback, this, &handle2);
    EXPECT_EQ(CL_SUCCESS, status);

    status = clSetTracingPointINTEL(handle1, CL_FUNCTION_clBuildProgram, CL_TRUE);
    EXPECT_EQ(CL_SUCCESS, status);
    status = clSetTracingPointINTEL(handle2, CL_FUNCTION_clBuildProgram, CL_TRUE);
    EXPECT_EQ(CL_SUCCESS, status);

    status = clEnableTracingINTEL(handle1);
    EXPECT_EQ(CL_SUCCESS, status);
    status = clEnableTracingINTEL(handle2);
    EXPECT_EQ(CL_SUCCESS, status);

    cl_bool enabled = CL_FALSE;
    status = clGetTracingStateINTEL(handle1, &enabled);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), enabled);
    status = clGetTracingStateINTEL(handle2, &enabled);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), enabled);

    status = clDisableTracingINTEL(handle1);
    EXPECT_EQ(CL_SUCCESS, status);
    status = clDisableTracingINTEL(handle2);
    EXPECT_EQ(CL_SUCCESS, status);

    status = clDestroyTracingHandleINTEL(handle1);
    EXPECT_EQ(CL_SUCCESS, status);
    status = clDestroyTracingHandleINTEL(handle2);
    EXPECT_EQ(CL_SUCCESS, status);
}

struct IntelAllTracingTest : public IntelTracingTest {
  public:
    IntelAllTracingTest() {}

    void SetUp() override {
        IntelTracingTest::SetUp();

        status = clCreateTracingHandleINTEL(testedClDevice, callback, this, &handle);
        ASSERT_NE(nullptr, handle);
        ASSERT_EQ(CL_SUCCESS, status);

        for (uint32_t i = 0; i < CL_FUNCTION_COUNT; ++i) {
            status = clSetTracingPointINTEL(handle, static_cast<ClFunctionId>(i), CL_TRUE);
            ASSERT_EQ(CL_SUCCESS, status);
        }

        status = clEnableTracingINTEL(handle);
        ASSERT_EQ(CL_SUCCESS, status);
    }

    void TearDown() override {
        status = clDisableTracingINTEL(handle);
        ASSERT_EQ(CL_SUCCESS, status);

        status = clDestroyTracingHandleINTEL(handle);
        ASSERT_EQ(CL_SUCCESS, status);

        IntelTracingTest::TearDown();
    }

  protected:
    void vcallback(ClFunctionId fid, cl_callback_data *callbackData, void *userData) override {
        if (fid == functionId) {
            if (callbackData->site == CL_CALLBACK_SITE_ENTER) {
                ++enterCount;
            } else if (callbackData->site == CL_CALLBACK_SITE_EXIT) {
                ++exitCount;
            }
        }
    }

    uint16_t callFunctions() {
        uint16_t count = 0;

        ++count;
        functionId = CL_FUNCTION_clBuildProgram;
        clBuildProgram(0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCloneKernel;
        clCloneKernel(0, 0);

        ++count;
        functionId = CL_FUNCTION_clCompileProgram;
        clCompileProgram(0, 0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateBuffer;
        clCreateBuffer(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateCommandQueue;
        clCreateCommandQueue(0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateCommandQueueWithProperties;
        clCreateCommandQueueWithProperties(0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateContext;
        clCreateContext(0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateContextFromType;
        clCreateContextFromType(0, 0, 0, 0, 0);

        ++count;
        cl_image_desc imageDesc = {0};
        functionId = CL_FUNCTION_clCreateImage;
        clCreateImage(0, 0, 0, &imageDesc, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateImage2D;
        clCreateImage2D(0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateImage3D;
        clCreateImage3D(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateKernel;
        clCreateKernel(0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateKernelsInProgram;
        clCreateKernelsInProgram(0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreatePipe;
        clCreatePipe(0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateProgramWithBinary;
        const size_t length = 32;
        unsigned char binary[length] = {0};
        clCreateProgramWithBinary(0, 0, &(testedClDevice), &length, reinterpret_cast<const unsigned char **>(&binary), 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateProgramWithBuiltInKernels;
        clCreateProgramWithBuiltInKernels(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateProgramWithIL;
        clCreateProgramWithIL(0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateProgramWithSource;
        clCreateProgramWithSource(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateSampler;
        clCreateSampler(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateSamplerWithProperties;
        clCreateSamplerWithProperties(0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateSubBuffer;
        clCreateSubBuffer(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateSubDevices;
        clCreateSubDevices(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateUserEvent;
        clCreateUserEvent(0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueBarrier;
        clEnqueueBarrier(0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueBarrierWithWaitList;
        clEnqueueBarrierWithWaitList(0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueCopyBuffer;
        clEnqueueCopyBuffer(0, 0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueCopyBufferRect;
        clEnqueueCopyBufferRect(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueCopyBufferToImage;
        clEnqueueCopyBufferToImage(0, 0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueCopyImage;
        clEnqueueCopyImage(0, 0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueCopyImageToBuffer;
        clEnqueueCopyImageToBuffer(0, 0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueFillBuffer;
        clEnqueueFillBuffer(0, 0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueFillImage;
        clEnqueueFillImage(0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueMapBuffer;
        clEnqueueMapBuffer(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueMapImage;
        clEnqueueMapImage(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueMarker;
        clEnqueueMarker(0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueMarkerWithWaitList;
        clEnqueueMarkerWithWaitList(0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueMigrateMemObjects;
        clEnqueueMigrateMemObjects(0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueNDRangeKernel;
        clEnqueueNDRangeKernel(0, 0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueNativeKernel;
        clEnqueueNativeKernel(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueReadBuffer;
        clEnqueueReadBuffer(0, 0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueReadBufferRect;
        clEnqueueReadBufferRect(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueReadImage;
        clEnqueueReadImage(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueSVMFree;
        clEnqueueSVMFree(0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueSVMMap;
        clEnqueueSVMMap(0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueSVMMemFill;
        clEnqueueSVMMemFill(0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueSVMMemcpy;
        clEnqueueSVMMemcpy(0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueSVMMigrateMem;
        clEnqueueSVMMigrateMem(0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueSVMUnmap;
        clEnqueueSVMUnmap(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueTask;
        clEnqueueTask(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueUnmapMemObject;
        clEnqueueUnmapMemObject(0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueWaitForEvents;
        clEnqueueWaitForEvents(0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueWriteBuffer;
        clEnqueueWriteBuffer(0, 0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueWriteBufferRect;
        clEnqueueWriteBufferRect(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueWriteImage;
        clEnqueueWriteImage(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clFinish;
        clFinish(0);

        ++count;
        functionId = CL_FUNCTION_clFlush;
        clFlush(0);

        ++count;
        functionId = CL_FUNCTION_clGetCommandQueueInfo;
        clGetCommandQueueInfo(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetContextInfo;
        clGetContextInfo(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetDeviceAndHostTimer;
        clGetDeviceAndHostTimer(0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetDeviceIDs;
        clGetDeviceIDs(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetDeviceInfo;
        clGetDeviceInfo(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetEventInfo;
        clGetEventInfo(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetEventProfilingInfo;
        clGetEventProfilingInfo(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetExtensionFunctionAddress;
        clGetExtensionFunctionAddress("test");

        ++count;
        functionId = CL_FUNCTION_clGetExtensionFunctionAddressForPlatform;
        clGetExtensionFunctionAddressForPlatform(0, "test");

        ++count;
        functionId = CL_FUNCTION_clGetHostTimer;
        clGetHostTimer(0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetImageInfo;
        clGetImageInfo(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetKernelArgInfo;
        clGetKernelArgInfo(0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetKernelInfo;
        clGetKernelInfo(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetKernelSubGroupInfo;
        clGetKernelSubGroupInfo(0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetKernelWorkGroupInfo;
        clGetKernelWorkGroupInfo(0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetMemObjectInfo;
        clGetMemObjectInfo(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetPipeInfo;
        clGetPipeInfo(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetPlatformIDs;
        clGetPlatformIDs(0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetPlatformInfo;
        clGetPlatformInfo(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetProgramBuildInfo;
        clGetProgramBuildInfo(0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetProgramInfo;
        clGetProgramInfo(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetSamplerInfo;
        clGetSamplerInfo(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetSupportedImageFormats;
        clGetSupportedImageFormats(0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clLinkProgram;
        clLinkProgram(0, 0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clReleaseCommandQueue;
        clReleaseCommandQueue(0);

        ++count;
        functionId = CL_FUNCTION_clReleaseContext;
        clReleaseContext(0);

        ++count;
        functionId = CL_FUNCTION_clReleaseDevice;
        clReleaseDevice(0);

        ++count;
        functionId = CL_FUNCTION_clReleaseEvent;
        clReleaseEvent(0);

        ++count;
        functionId = CL_FUNCTION_clReleaseKernel;
        clReleaseKernel(0);

        ++count;
        functionId = CL_FUNCTION_clReleaseMemObject;
        clReleaseMemObject(0);

        ++count;
        functionId = CL_FUNCTION_clReleaseProgram;
        clReleaseProgram(0);

        ++count;
        functionId = CL_FUNCTION_clReleaseSampler;
        clReleaseSampler(0);

        ++count;
        functionId = CL_FUNCTION_clRetainCommandQueue;
        clRetainCommandQueue(0);

        ++count;
        functionId = CL_FUNCTION_clRetainContext;
        clRetainContext(0);

        ++count;
        functionId = CL_FUNCTION_clRetainDevice;
        clRetainDevice(0);

        ++count;
        functionId = CL_FUNCTION_clRetainEvent;
        clRetainEvent(0);

        ++count;
        functionId = CL_FUNCTION_clRetainKernel;
        clRetainKernel(0);

        ++count;
        functionId = CL_FUNCTION_clRetainMemObject;
        clRetainMemObject(0);

        ++count;
        functionId = CL_FUNCTION_clRetainProgram;
        clRetainProgram(0);

        ++count;
        functionId = CL_FUNCTION_clRetainSampler;
        clRetainSampler(0);

        ++count;
        functionId = CL_FUNCTION_clSVMAlloc;
        clSVMAlloc(0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clSVMFree;
        clSVMFree(0, 0);

        ++count;
        functionId = CL_FUNCTION_clSetCommandQueueProperty;
        clSetCommandQueueProperty(0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clSetDefaultDeviceCommandQueue;
        clSetDefaultDeviceCommandQueue(0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clSetEventCallback;
        clSetEventCallback(0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clSetKernelArg;
        clSetKernelArg(0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clSetKernelArgSVMPointer;
        clSetKernelArgSVMPointer(0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clSetKernelExecInfo;
        clSetKernelExecInfo(0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clSetMemObjectDestructorCallback;
        clSetMemObjectDestructorCallback(0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clSetUserEventStatus;
        clSetUserEventStatus(0, 0);

        ++count;
        functionId = CL_FUNCTION_clUnloadCompiler;
        clUnloadCompiler();

        ++count;
        functionId = CL_FUNCTION_clUnloadPlatformCompiler;
        clUnloadPlatformCompiler(0);

        ++count;
        functionId = CL_FUNCTION_clWaitForEvents;
        clWaitForEvents(0, 0);

        ++count;
        functionId = CL_FUNCTION_clMemFreeINTEL;
        clMemFreeINTEL(0, 0);

        ++count;
        functionId = CL_FUNCTION_clIcdGetPlatformIDsKHR;
        clIcdGetPlatformIDsKHR(0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateBufferWithProperties;
        clCreateBufferWithProperties(0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateBufferWithPropertiesINTEL;
        clCreateBufferWithPropertiesINTEL(0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateImageWithProperties;
        clCreateImageWithProperties(0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateImageWithPropertiesINTEL;
        clCreateImageWithPropertiesINTEL(0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetImageParamsINTEL;
        clGetImageParamsINTEL(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreatePerfCountersCommandQueueINTEL;
        clCreatePerfCountersCommandQueueINTEL(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clHostMemAllocINTEL;
        clHostMemAllocINTEL(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clDeviceMemAllocINTEL;
        clDeviceMemAllocINTEL(0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clSharedMemAllocINTEL;
        clSharedMemAllocINTEL(0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clMemBlockingFreeINTEL;
        clMemBlockingFreeINTEL(0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetMemAllocInfoINTEL;
        clGetMemAllocInfoINTEL(0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clSetKernelArgMemPointerINTEL;
        clSetKernelArgMemPointerINTEL(0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueMemsetINTEL;
        clEnqueueMemsetINTEL(0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueMemFillINTEL;
        clEnqueueMemFillINTEL(0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueMemcpyINTEL;
        clEnqueueMemcpyINTEL(0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueMigrateMemINTEL;
        clEnqueueMigrateMemINTEL(0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueMemAdviseINTEL;
        clEnqueueMemAdviseINTEL(0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateCommandQueueWithPropertiesKHR;
        clCreateCommandQueueWithPropertiesKHR(0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateAcceleratorINTEL;
        clCreateAcceleratorINTEL(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clRetainAcceleratorINTEL;
        clRetainAcceleratorINTEL(0);

        ++count;
        functionId = CL_FUNCTION_clGetAcceleratorInfoINTEL;
        clGetAcceleratorInfoINTEL(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clReleaseAcceleratorINTEL;
        clReleaseAcceleratorINTEL(0);

        ++count;
        functionId = CL_FUNCTION_clCreateProgramWithILKHR;
        clCreateProgramWithILKHR(0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetKernelSuggestedLocalWorkSizeKHR;
        clGetKernelSuggestedLocalWorkSizeKHR(0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetKernelSubGroupInfoKHR;
        clGetKernelSubGroupInfoKHR(0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueVerifyMemoryINTEL;
        clEnqueueVerifyMemoryINTEL(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clAddCommentINTEL;
        clAddCommentINTEL(0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetDeviceGlobalVariablePointerINTEL;
        clGetDeviceGlobalVariablePointerINTEL(0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetDeviceFunctionPointerINTEL;
        clGetDeviceFunctionPointerINTEL(0, 0, "test", 0);

        ++count;
        functionId = CL_FUNCTION_clSetProgramReleaseCallback;
        clSetProgramReleaseCallback(0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clSetProgramSpecializationConstant;
        clSetProgramSpecializationConstant(0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetKernelSuggestedLocalWorkSizeINTEL;
        clGetKernelSuggestedLocalWorkSizeINTEL(0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetKernelMaxConcurrentWorkGroupCountINTEL;
        clGetKernelMaxConcurrentWorkGroupCountINTEL(0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueNDCountKernelINTEL;
        clEnqueueNDCountKernelINTEL(0, 0, 0, 0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clSetContextDestructorCallback;
        clSetContextDestructorCallback(0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueExternalMemObjectsKHR;
        clEnqueueExternalMemObjectsKHR(0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueAcquireExternalMemObjectsKHR;
        clEnqueueAcquireExternalMemObjectsKHR(0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueReleaseExternalMemObjectsKHR;
        clEnqueueReleaseExternalMemObjectsKHR(0, 0, 0, 0, 0, 0);

        return count;
    }

  protected:
    uint16_t enterCount = 0;
    uint16_t exitCount = 0;
    ClFunctionId functionId = CL_FUNCTION_COUNT;
};

TEST_F(IntelAllTracingTest, GivenValidFunctionWhenTracingThenTracingIsPerformed) {
    uint16_t count = callFunctions();
    EXPECT_EQ(count, enterCount);
    EXPECT_EQ(count, exitCount);
}

TEST_F(IntelAllTracingTest, GivenNoFunctionsWhenTracingThenTracingIsNotPerformed) {
    for (uint32_t i = 0; i < CL_FUNCTION_COUNT; ++i) {
        status = clSetTracingPointINTEL(handle, static_cast<ClFunctionId>(i), CL_FALSE);
        EXPECT_EQ(CL_SUCCESS, status);
    }

    callFunctions();

    EXPECT_EQ(0, enterCount);
    EXPECT_EQ(0, exitCount);
}

struct IntelAllTracingWithMaxHandlesTest : public IntelAllTracingTest {
  public:
    IntelAllTracingWithMaxHandlesTest() {}

    void SetUp() override {
        IntelTracingTest::SetUp();

        for (size_t i = 0; i < HostSideTracing::tracingMaxHandleCount; ++i) {
            status = clCreateTracingHandleINTEL(testedClDevice, callback, this, handleList + i);
            ASSERT_NE(nullptr, handleList[i]);
            ASSERT_EQ(CL_SUCCESS, status);
        }

        for (size_t i = 0; i < HostSideTracing::tracingMaxHandleCount; ++i) {
            for (uint32_t j = 0; j < CL_FUNCTION_COUNT; ++j) {
                status = clSetTracingPointINTEL(handleList[i], static_cast<ClFunctionId>(j), CL_TRUE);
                ASSERT_EQ(CL_SUCCESS, status);
            }
        }

        for (size_t i = 0; i < HostSideTracing::tracingMaxHandleCount; ++i) {
            status = clEnableTracingINTEL(handleList[i]);
            ASSERT_EQ(CL_SUCCESS, status);
        }
    }

    void TearDown() override {
        for (size_t i = 0; i < HostSideTracing::tracingMaxHandleCount; ++i) {
            status = clDisableTracingINTEL(handleList[i]);
            ASSERT_EQ(CL_SUCCESS, status);

            status = clDestroyTracingHandleINTEL(handleList[i]);
            ASSERT_EQ(CL_SUCCESS, status);
        }

        IntelTracingTest::TearDown();
    }

  protected:
    cl_tracing_handle handleList[HostSideTracing::tracingMaxHandleCount] = {nullptr};
};

TEST_F(IntelAllTracingWithMaxHandlesTest, GivenAllFunctionsWithMaxHandlesWhenTracingThenTracingIsPerformed) {
    uint16_t count = callFunctions();
    EXPECT_EQ(count * HostSideTracing::tracingMaxHandleCount, enterCount);
    EXPECT_EQ(count * HostSideTracing::tracingMaxHandleCount, exitCount);
}

struct IntelClGetDeviceInfoTracingCollectTest : public IntelAllTracingTest {
  public:
    IntelClGetDeviceInfoTracingCollectTest() {}

  protected:
    void call(cl_device_id target) {
        device = target;
        status = clGetDeviceInfo(device, CL_DEVICE_VENDOR, 0, nullptr, &paramValueSizeRet);
    }

    void vcallback(ClFunctionId fid, cl_callback_data *callbackData, void *userData) override {
        EXPECT_EQ(CL_FUNCTION_clGetDeviceInfo, fid);
        EXPECT_NE(nullptr, callbackData);

        if (callbackData->site == CL_CALLBACK_SITE_ENTER) {
            correlationId = callbackData->correlationId;
            EXPECT_NE(nullptr, callbackData->correlationData);
            callbackData->correlationData[0] = 777ull;
        } else {
            EXPECT_EQ(correlationId, callbackData->correlationId);
            EXPECT_NE(nullptr, callbackData->correlationData);
            EXPECT_EQ(777ull, callbackData->correlationData[0]);
        }

        EXPECT_NE(nullptr, callbackData->functionName);
        EXPECT_STREQ("clGetDeviceInfo", callbackData->functionName);
        EXPECT_NE(nullptr, callbackData->functionParams);
        if (callbackData->site == CL_CALLBACK_SITE_ENTER) {
            EXPECT_EQ(nullptr, callbackData->functionReturnValue);
        } else {
            EXPECT_NE(nullptr, callbackData->functionReturnValue);
        }

        cl_params_clGetDeviceInfo *params = (cl_params_clGetDeviceInfo *)callbackData->functionParams;
        EXPECT_NE(nullptr, *params->device);
        EXPECT_EQ(static_cast<cl_device_info>(CL_DEVICE_VENDOR), *params->paramName);
        EXPECT_EQ(0u, *params->paramValueSize);
        EXPECT_EQ(nullptr, *params->paramValue);

        if (callbackData->site == CL_CALLBACK_SITE_EXIT) {
            cl_int *retVal = (cl_int *)callbackData->functionReturnValue;
            EXPECT_EQ(CL_SUCCESS, *retVal);
            EXPECT_LT(0u, **params->paramValueSizeRet);
        }

        if (callbackData->site == CL_CALLBACK_SITE_ENTER) {
            ++enterCount;
        } else if (callbackData->site == CL_CALLBACK_SITE_EXIT) {
            ++exitCount;
        }
    }

  protected:
    cl_device_id device = nullptr;
    size_t paramValueSizeRet = 0;
    uint64_t correlationId = 0;
};

TEST_F(IntelClGetDeviceInfoTracingCollectTest, GivenGeneralCollectionWhenTracingThenTracingIsPerformed) {
    call(testedClDevice);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_LT(0u, paramValueSizeRet);

    EXPECT_EQ(1u, enterCount);
    EXPECT_EQ(1u, exitCount);
}

struct IntelClGetDeviceInfoTracingChangeParamsTest : public IntelAllTracingTest {
  public:
    IntelClGetDeviceInfoTracingChangeParamsTest() {}

  protected:
    void call(cl_device_id target) {
        device = target;
        status = clGetDeviceInfo(device, CL_DEVICE_VENDOR, 0, nullptr, &paramValueSizeRet);
    }

    void vcallback(ClFunctionId fid, cl_callback_data *callbackData, void *userData) override {
        if (callbackData->site == CL_CALLBACK_SITE_ENTER) {
            cl_params_clGetDeviceInfo *params = (cl_params_clGetDeviceInfo *)callbackData->functionParams;
            *params->paramValueSize = paramValueSize;
            *params->paramValue = paramValue;
        }
    }

  protected:
    cl_device_id device = nullptr;
    size_t paramValueSizeRet = 0;
    static const size_t paramValueSize = 256;
    char paramValue[paramValueSize] = {'\0'};
};

TEST_F(IntelClGetDeviceInfoTracingChangeParamsTest, GivenTracingCallbackWithParamsChangeWhenApiFunctionIsCalledThenParamsAreChanged) {
    paramValue[0] = '\0';
    call(testedClDevice);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_LT(0u, paramValueSizeRet);
    EXPECT_STRNE("", paramValue);
}

struct IntelClGetDeviceInfoTracingChangeRetValTest : public IntelAllTracingTest {
  public:
    IntelClGetDeviceInfoTracingChangeRetValTest() {}

  protected:
    void call(cl_device_id target) {
        device = target;
        status = clGetDeviceInfo(device, CL_DEVICE_VENDOR, 0, nullptr, &paramValueSizeRet);
    }

    void vcallback(ClFunctionId fid, cl_callback_data *callbackData, void *userData) override {
        if (callbackData->site == CL_CALLBACK_SITE_EXIT) {
            cl_int *retVal = reinterpret_cast<cl_int *>(callbackData->functionReturnValue);
            *retVal = CL_INVALID_VALUE;
        }
    }

  protected:
    cl_device_id device = nullptr;
    size_t paramValueSizeRet = 0;
};

TEST_F(IntelClGetDeviceInfoTracingChangeRetValTest, GivenTracingCallbackWithRetValChangeToInvalidValueWhenApiFunctionIsCalledThenInvalidValueErrorIsReturned) {
    call(testedClDevice);
    EXPECT_EQ(CL_INVALID_VALUE, status);
    EXPECT_LT(0u, paramValueSizeRet);
}

struct IntelClGetDeviceInfoTwoHandlesTracingCollectTest : public IntelClGetDeviceInfoTracingCollectTest {
  public:
    IntelClGetDeviceInfoTwoHandlesTracingCollectTest() {}

    void SetUp() override {
        IntelClGetDeviceInfoTracingCollectTest::SetUp();

        status = clCreateTracingHandleINTEL(testedClDevice, callback, this, &secondHandle);
        ASSERT_NE(nullptr, secondHandle);
        ASSERT_EQ(CL_SUCCESS, status);

        status = clSetTracingPointINTEL(secondHandle, CL_FUNCTION_clGetDeviceInfo, CL_TRUE);
        ASSERT_EQ(CL_SUCCESS, status);

        status = clEnableTracingINTEL(secondHandle);
        ASSERT_EQ(CL_SUCCESS, status);
    }

    void TearDown() override {
        status = clDisableTracingINTEL(secondHandle);
        ASSERT_EQ(CL_SUCCESS, status);

        status = clDestroyTracingHandleINTEL(secondHandle);
        ASSERT_EQ(CL_SUCCESS, status);

        IntelClGetDeviceInfoTracingCollectTest::TearDown();
    }

  protected:
    cl_tracing_handle secondHandle = nullptr;
};

TEST_F(IntelClGetDeviceInfoTwoHandlesTracingCollectTest, GivenTwoHandlesWhenTracingThenTracingIsPerformed) {
    call(testedClDevice);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_LT(0u, paramValueSizeRet);

    EXPECT_EQ(2u, enterCount);
    EXPECT_EQ(2u, exitCount);
}

struct IntelClCreateContextFromTypeTracingTest : public IntelTracingTest, PlatformFixture {
  public:
    void SetUp() override {
        PlatformFixture::setUp();
        IntelTracingTest::setUp();

        status = clCreateTracingHandleINTEL(devices[0], callback, this, &handle);
        ASSERT_NE(nullptr, handle);
        ASSERT_EQ(CL_SUCCESS, status);

        status = clSetTracingPointINTEL(handle, CL_FUNCTION_clCreateContextFromType, CL_TRUE);
        ASSERT_EQ(CL_SUCCESS, status);

        status = clEnableTracingINTEL(handle);
        ASSERT_EQ(CL_SUCCESS, status);
    }
    void TearDown() override {
        status = clDisableTracingINTEL(handle);
        ASSERT_EQ(CL_SUCCESS, status);

        status = clDestroyTracingHandleINTEL(handle);
        ASSERT_EQ(CL_SUCCESS, status);
        IntelTracingTest::tearDown();
        PlatformFixture::tearDown();
    }

  protected:
    void call() {
        context = clCreateContextFromType(nullptr, CL_DEVICE_TYPE_GPU, nullptr, nullptr, nullptr);
    }

    void vcallback(ClFunctionId fid, cl_callback_data *callbackData, void *userData) override {
        ASSERT_EQ(CL_FUNCTION_clCreateContextFromType, fid);
        if (callbackData->site == CL_CALLBACK_SITE_ENTER) {
            ++enterCount;
        } else if (callbackData->site == CL_CALLBACK_SITE_EXIT) {
            obtainedContextCallback = *reinterpret_cast<cl_context *>(callbackData->functionReturnValue);
            ++exitCount;
        }
    }

    cl_context context = nullptr;
    cl_context obtainedContextCallback = nullptr;
    uint16_t enterCount = 0;
    uint16_t exitCount = 0;
};

TEST_F(IntelClCreateContextFromTypeTracingTest, givenCreateContextFromTypeCallTracingWhenInvokingCallbackThenPointersFromCallAndCallbackPointToTheSameAddress) {
    call();
    EXPECT_EQ(1u, enterCount);
    EXPECT_EQ(1u, exitCount);

    EXPECT_NE(nullptr, context);
    EXPECT_NE(nullptr, obtainedContextCallback);
    EXPECT_EQ(context, obtainedContextCallback);
    clReleaseContext(context);
}

struct IntelClLinkProgramTracingTest : public IntelTracingTest, PlatformFixture {
  public:
    void SetUp() override {
        PlatformFixture::setUp();
        IntelTracingTest::setUp();

        status = clCreateTracingHandleINTEL(devices[0], callback, this, &handle);
        ASSERT_NE(nullptr, handle);
        ASSERT_EQ(CL_SUCCESS, status);

        status = clSetTracingPointINTEL(handle, CL_FUNCTION_clLinkProgram, CL_TRUE);
        ASSERT_EQ(CL_SUCCESS, status);

        status = clEnableTracingINTEL(handle);
        ASSERT_EQ(CL_SUCCESS, status);

        pProgram->irBinary = makeCopy(ir, sizeof(ir));
        pProgram->irBinarySize = sizeof(ir);
    }
    void TearDown() override {
        status = clDisableTracingINTEL(handle);
        ASSERT_EQ(CL_SUCCESS, status);

        status = clDestroyTracingHandleINTEL(handle);
        ASSERT_EQ(CL_SUCCESS, status);
        IntelTracingTest::tearDown();
        PlatformFixture::tearDown();
    }

  protected:
    void call() {
        cl_program inputPrograms = pProgram;
        programReturned = clLinkProgram(pContext, 1, &testedClDevice, nullptr, 1, &inputPrograms, nullptr, nullptr, &retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    void vcallback(ClFunctionId fid, cl_callback_data *callbackData, void *userData) override {
        ASSERT_EQ(CL_FUNCTION_clLinkProgram, fid);
        if (callbackData->site == CL_CALLBACK_SITE_ENTER) {
            ++enterCount;
        } else if (callbackData->site == CL_CALLBACK_SITE_EXIT) {
            obtainedProgramCallback = *reinterpret_cast<cl_program *>(callbackData->functionReturnValue);
            ++exitCount;
        }
    }

    uint8_t ir[10] = {'m', 'o', 'c', 'k', 'i', 'r', 'd', 'a', 't', 'a'};
    cl_program programReturned = nullptr;
    cl_program obtainedProgramCallback = nullptr;
    uint16_t enterCount = 0;
    uint16_t exitCount = 0;
};

TEST_F(IntelClLinkProgramTracingTest, givenLinkProgramCallTracingWhenInvokingCallbackThenPointersFromCallAndCallbackPointToTheSameAddress) {
    USE_REAL_FILE_SYSTEM();

    call();
    EXPECT_EQ(1u, enterCount);
    EXPECT_EQ(1u, exitCount);

    EXPECT_NE(nullptr, programReturned);
    EXPECT_NE(nullptr, obtainedProgramCallback);
    EXPECT_EQ(programReturned, obtainedProgramCallback);
    clReleaseProgram(programReturned);
}

struct IntelClCloneKernelTracingTest : public IntelTracingTest, PlatformFixture {
  public:
    void SetUp() override {
        PlatformFixture::setUp();
        IntelTracingTest::setUp();

        status = clCreateTracingHandleINTEL(devices[0], callback, this, &handle);
        ASSERT_NE(nullptr, handle);
        ASSERT_EQ(CL_SUCCESS, status);

        status = clSetTracingPointINTEL(handle, CL_FUNCTION_clCloneKernel, CL_TRUE);
        ASSERT_EQ(CL_SUCCESS, status);

        status = clEnableTracingINTEL(handle);
        ASSERT_EQ(CL_SUCCESS, status);

        const auto &gfxHelper = pDevice->getGfxCoreHelper();
        const_cast<KernelDescriptor &>(pKernel->getKernelInfo().kernelDescriptor).kernelAttributes.simdSize = gfxHelper.getMinimalSIMDSize();
    }
    void TearDown() override {
        status = clDisableTracingINTEL(handle);
        ASSERT_EQ(CL_SUCCESS, status);

        status = clDestroyTracingHandleINTEL(handle);
        ASSERT_EQ(CL_SUCCESS, status);
        IntelTracingTest::tearDown();
        PlatformFixture::tearDown();
    }

  protected:
    void call() {
        cl_kernel sourceKernel = pMultiDeviceKernel;
        clonedKernel = clCloneKernel(sourceKernel, &retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    void vcallback(ClFunctionId fid, cl_callback_data *callbackData, void *userData) override {
        ASSERT_EQ(CL_FUNCTION_clCloneKernel, fid);
        if (callbackData->site == CL_CALLBACK_SITE_ENTER) {
            ++enterCount;
        } else if (callbackData->site == CL_CALLBACK_SITE_EXIT) {
            obtainedClonedKernelCallback = *reinterpret_cast<cl_kernel *>(callbackData->functionReturnValue);
            ++exitCount;
        }
    }

    cl_kernel clonedKernel = nullptr;
    cl_kernel obtainedClonedKernelCallback = nullptr;
    uint16_t enterCount = 0;
    uint16_t exitCount = 0;
};

TEST_F(IntelClCloneKernelTracingTest, givenCloneKernelCallTracingWhenInvokingCallbackThenPointersFromCallAndCallbackPointToTheSameAddress) {
    call();
    EXPECT_EQ(1u, enterCount);
    EXPECT_EQ(1u, exitCount);

    EXPECT_NE(nullptr, clonedKernel);
    EXPECT_NE(nullptr, obtainedClonedKernelCallback);
    EXPECT_EQ(clonedKernel, obtainedClonedKernelCallback);
    clReleaseKernel(clonedKernel);
}

struct IntelClCreateSubDevicesTracingTest : public IntelTracingTest {
  public:
    void SetUp() override {
        IntelTracingTest::setUp();

        debugManager.flags.CreateMultipleSubDevices.set(deviceCount);
        device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

        status = clCreateTracingHandleINTEL(testedClDevice, callback, this, &handle);
        ASSERT_NE(nullptr, handle);
        ASSERT_EQ(CL_SUCCESS, status);

        status = clSetTracingPointINTEL(handle, CL_FUNCTION_clCreateSubDevices, CL_TRUE);
        ASSERT_EQ(CL_SUCCESS, status);

        status = clEnableTracingINTEL(handle);
        ASSERT_EQ(CL_SUCCESS, status);
    }
    void TearDown() override {
        status = clDisableTracingINTEL(handle);
        ASSERT_EQ(CL_SUCCESS, status);

        status = clDestroyTracingHandleINTEL(handle);
        ASSERT_EQ(CL_SUCCESS, status);
        IntelTracingTest::tearDown();
    }

    void call() {
        auto retVal = clCreateSubDevices(device.get(), properties, deviceCount, outDevices, numDevicesRet);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    void vcallback(ClFunctionId fid, cl_callback_data *callbackData, void *userData) override {
        ASSERT_EQ(CL_FUNCTION_clCreateSubDevices, fid);
        if (callbackData->site == CL_CALLBACK_SITE_ENTER) {
            ++enterCount;
        } else if (callbackData->site == CL_CALLBACK_SITE_EXIT) {
            auto funcParams = reinterpret_cast<const cl_params_clCreateSubDevices *>(callbackData->functionParams); // check at tracing exit, once api call has been executed
            EXPECT_EQ(static_cast<cl_device_id>(device.get()), *(funcParams->inDevice));
            EXPECT_EQ(2u, *(funcParams->numDevices));
            EXPECT_EQ(numDevicesRet[0], *(funcParams->numDevicesRet[0]));

            auto outDevicesCallback = *(funcParams->outDevices);
            EXPECT_EQ(static_cast<cl_device_id>(device->getSubDevice(0)), outDevicesCallback[0]);
            EXPECT_EQ(static_cast<cl_device_id>(device->getSubDevice(1)), outDevicesCallback[1]);

            auto propertiesCallback = *(funcParams->properties);
            EXPECT_EQ(CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN, propertiesCallback[0]);
            EXPECT_EQ(CL_DEVICE_AFFINITY_DOMAIN_NUMA, propertiesCallback[1]);
            EXPECT_EQ(0, propertiesCallback[2]);

            EXPECT_STREQ("clCreateSubDevices", callbackData->functionName);
            EXPECT_EQ(CL_SUCCESS, *reinterpret_cast<cl_int *>(callbackData->functionReturnValue));
            ++exitCount;
        }
    }

  protected:
    DebugManagerStateRestore restorer;
    std::unique_ptr<MockClDevice> device;
    cl_device_partition_property properties[3] = {CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN, CL_DEVICE_AFFINITY_DOMAIN_NUMA, 0};
    cl_uint deviceCount = 2u;
    cl_device_id outDevices[2] = {0, 0};
    cl_uint numDevicesRet[1] = {0u};

    uint16_t enterCount = 0u;
    uint16_t exitCount = 0u;
};

TEST_F(IntelClCreateSubDevicesTracingTest, givenCreateSubDevicesCallTracingEnabledWhenInvokingThisCallThenCallIsSuccessfullyTraced) {
    call();
    EXPECT_EQ(1u, enterCount);
    EXPECT_EQ(1u, exitCount);
}

} // namespace ULT
