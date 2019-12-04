/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/tracing/tracing_api.h"
#include "runtime/tracing/tracing_notify.h"
#include "unit_tests/api/cl_api_tests.h"

using namespace NEO;

namespace ULT {

struct IntelTracingTest : public api_tests {
  public:
    IntelTracingTest() {}

    void SetUp() override {
        api_tests::SetUp();
    }

    void TearDown() override {
        api_tests::TearDown();
    }

  protected:
    static void callback(cl_function_id fid, cl_callback_data *callbackData, void *userData) {
        ASSERT_NE(nullptr, userData);
        IntelTracingTest *base = (IntelTracingTest *)userData;
        base->vcallback(fid, callbackData, nullptr);
    }

    virtual void vcallback(cl_function_id fid, cl_callback_data *callbackData, void *userData) {}

  protected:
    cl_tracing_handle handle = nullptr;
    cl_int status = CL_SUCCESS;
};

TEST_F(IntelTracingTest, GivenInvalidDeviceExpectFail) {
    status = clCreateTracingHandleINTEL(nullptr, callback, nullptr, &handle);
    EXPECT_EQ(static_cast<cl_tracing_handle>(nullptr), handle);
    EXPECT_EQ(CL_INVALID_VALUE, status);
}

TEST_F(IntelTracingTest, GivenInvalidCallbackExpectFail) {
    status = clCreateTracingHandleINTEL(devices[0], nullptr, nullptr, &handle);
    EXPECT_EQ(static_cast<cl_tracing_handle>(nullptr), handle);
    EXPECT_EQ(CL_INVALID_VALUE, status);
}

TEST_F(IntelTracingTest, GivenInvalidHandlePointerExpectFail) {
    status = clCreateTracingHandleINTEL(devices[0], callback, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, status);
}

TEST_F(IntelTracingTest, GivenInvalidHandleExpectFail) {
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

TEST_F(IntelTracingTest, GivenInactiveHandleExpectFail) {
    status = clCreateTracingHandleINTEL(devices[0], callback, nullptr, &handle);
    EXPECT_EQ(CL_SUCCESS, status);

    status = clDisableTracingINTEL(handle);
    EXPECT_EQ(CL_INVALID_VALUE, status);

    status = clDestroyTracingHandleINTEL(handle);
    EXPECT_EQ(CL_SUCCESS, status);
}

TEST_F(IntelTracingTest, GivenTooManyHandlesExpectFail) {
    cl_tracing_handle handle[HostSideTracing::TRACING_MAX_HANDLE_COUNT + 1] = {nullptr};

    for (uint32_t i = 0; i < HostSideTracing::TRACING_MAX_HANDLE_COUNT + 1; ++i) {
        status = clCreateTracingHandleINTEL(devices[0], callback, nullptr, &(handle[i]));
        EXPECT_EQ(CL_SUCCESS, status);
    }

    for (uint32_t i = 0; i < HostSideTracing::TRACING_MAX_HANDLE_COUNT; ++i) {
        status = clEnableTracingINTEL(handle[i]);
        EXPECT_EQ(CL_SUCCESS, status);
    }

    status = clEnableTracingINTEL(handle[HostSideTracing::TRACING_MAX_HANDLE_COUNT]);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, status);

    for (uint32_t i = 0; i < HostSideTracing::TRACING_MAX_HANDLE_COUNT; ++i) {
        status = clDisableTracingINTEL(handle[i]);
        EXPECT_EQ(CL_SUCCESS, status);
    }

    for (uint32_t i = 0; i < HostSideTracing::TRACING_MAX_HANDLE_COUNT + 1; ++i) {
        status = clDestroyTracingHandleINTEL(handle[i]);
        EXPECT_EQ(CL_SUCCESS, status);
    }
}

TEST_F(IntelTracingTest, EnableTracingExpectPass) {
    status = clCreateTracingHandleINTEL(devices[0], callback, this, &handle);
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

TEST_F(IntelTracingTest, EnableTwoHandlesExpectPass) {
    cl_tracing_handle handle1 = nullptr;
    status = clCreateTracingHandleINTEL(devices[0], callback, this, &handle1);
    EXPECT_EQ(CL_SUCCESS, status);
    cl_tracing_handle handle2 = nullptr;
    status = clCreateTracingHandleINTEL(devices[0], callback, this, &handle2);
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

        status = clCreateTracingHandleINTEL(devices[0], callback, this, &handle);
        ASSERT_NE(nullptr, handle);
        ASSERT_EQ(CL_SUCCESS, status);

        for (uint32_t i = 0; i < CL_FUNCTION_COUNT; ++i) {
            status = clSetTracingPointINTEL(handle, static_cast<cl_function_id>(i), CL_TRUE);
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
    void vcallback(cl_function_id fid, cl_callback_data *callbackData, void *userData) override {
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
        clCreateProgramWithBinary(0, 0, &(devices[0]), &length, reinterpret_cast<const unsigned char **>(&binary), 0, 0);

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

        return count;
    }

  protected:
    uint16_t enterCount = 0;
    uint16_t exitCount = 0;
    cl_function_id functionId = CL_FUNCTION_COUNT;
};

TEST_F(IntelAllTracingTest, GivenAllFunctionsToTraceExpectPass) {
    uint16_t count = callFunctions();
    EXPECT_EQ(count, enterCount);
    EXPECT_EQ(count, exitCount);
}

TEST_F(IntelAllTracingTest, GivenNoFunctionsToTraceExpectPass) {
    for (uint32_t i = 0; i < CL_FUNCTION_COUNT; ++i) {
        status = clSetTracingPointINTEL(handle, static_cast<cl_function_id>(i), CL_FALSE);
        EXPECT_EQ(CL_SUCCESS, status);
    }

    callFunctions();

    EXPECT_EQ(0, enterCount);
    EXPECT_EQ(0, exitCount);
}

struct IntelClGetDeviceInfoTracingCollectTest : public IntelAllTracingTest {
  public:
    IntelClGetDeviceInfoTracingCollectTest() {}

  protected:
    void call(cl_device_id target) {
        device = target;
        status = clGetDeviceInfo(device, CL_DEVICE_VENDOR, 0, nullptr, &paramValueSizeRet);
    }

    void vcallback(cl_function_id fid, cl_callback_data *callbackData, void *userData) override {
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

TEST_F(IntelClGetDeviceInfoTracingCollectTest, GeneralTracingCollectionExpectPass) {
    call(devices[0]);
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

    void vcallback(cl_function_id fid, cl_callback_data *callbackData, void *userData) override {
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
    char paramValue[paramValueSize];
};

TEST_F(IntelClGetDeviceInfoTracingChangeParamsTest, GeneralTracingWithParamsChangeExpectPass) {
    paramValue[0] = '\0';
    call(devices[0]);
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

    void vcallback(cl_function_id fid, cl_callback_data *callbackData, void *userData) override {
        if (callbackData->site == CL_CALLBACK_SITE_EXIT) {
            cl_int *retVal = reinterpret_cast<cl_int *>(callbackData->functionReturnValue);
            *retVal = CL_INVALID_VALUE;
        }
    }

  protected:
    cl_device_id device = nullptr;
    size_t paramValueSizeRet = 0;
};

TEST_F(IntelClGetDeviceInfoTracingChangeRetValTest, GeneralTracingWithRetValChangeExpectPass) {
    call(devices[0]);
    EXPECT_EQ(CL_INVALID_VALUE, status);
    EXPECT_LT(0u, paramValueSizeRet);
}

struct IntelClGetDeviceInfoTwoHandlesTracingCollectTest : public IntelClGetDeviceInfoTracingCollectTest {
  public:
    IntelClGetDeviceInfoTwoHandlesTracingCollectTest() {}

    void SetUp() override {
        IntelClGetDeviceInfoTracingCollectTest::SetUp();

        status = clCreateTracingHandleINTEL(devices[0], callback, this, &secondHandle);
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

TEST_F(IntelClGetDeviceInfoTwoHandlesTracingCollectTest, GeneralTracingCollectionWithTwoHandlesExpectPass) {
    call(devices[0]);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_LT(0u, paramValueSizeRet);

    EXPECT_EQ(2u, enterCount);
    EXPECT_EQ(2u, exitCount);
}

} // namespace ULT
