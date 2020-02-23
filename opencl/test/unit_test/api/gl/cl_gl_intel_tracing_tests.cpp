/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/tracing/tracing_api.h"
#include "opencl/source/tracing/tracing_notify.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"

using namespace NEO;

namespace ULT {

struct IntelGlTracingTest : public api_tests {
  public:
    IntelGlTracingTest() {}

    void SetUp() override {
        api_tests::SetUp();

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

        api_tests::TearDown();
    }

  protected:
    static void callback(cl_function_id fid, cl_callback_data *callback_data, void *user_data) {
        ASSERT_NE(nullptr, user_data);
        IntelGlTracingTest *base = (IntelGlTracingTest *)user_data;
        base->vcallback(fid, callback_data, nullptr);
    }

    virtual void vcallback(cl_function_id fid, cl_callback_data *callback_data, void *user_data) {
        if (fid == functionId) {
            if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
                ++enterCount;
            } else if (callback_data->site == CL_CALLBACK_SITE_EXIT) {
                ++exitCount;
            }
        }
    }

    uint16_t callFunctions() {
        uint16_t count = 0;

        ++count;
        functionId = CL_FUNCTION_clCreateFromGLBuffer;
        clCreateFromGLBuffer(0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateFromGLRenderbuffer;
        clCreateFromGLRenderbuffer(0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateFromGLTexture;
        clCreateFromGLTexture(0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateFromGLTexture2D;
        clCreateFromGLTexture2D(0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clCreateFromGLTexture3D;
        clCreateFromGLTexture3D(0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueAcquireGLObjects;
        clEnqueueAcquireGLObjects(0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clEnqueueReleaseGLObjects;
        clEnqueueReleaseGLObjects(0, 0, 0, 0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetGLObjectInfo;
        clGetGLObjectInfo(0, 0, 0);

        ++count;
        functionId = CL_FUNCTION_clGetGLTextureInfo;
        clGetGLTextureInfo(0, 0, 0, 0, 0);

        return count;
    }

  protected:
    cl_tracing_handle handle = nullptr;
    cl_int status = CL_SUCCESS;

    uint16_t enterCount = 0;
    uint16_t exitCount = 0;
    cl_function_id functionId = CL_FUNCTION_COUNT;
};

TEST_F(IntelGlTracingTest, GivenAllFunctionsToTraceExpectPass) {
    uint16_t count = callFunctions();
    EXPECT_EQ(count, enterCount);
    EXPECT_EQ(count, exitCount);
}

TEST_F(IntelGlTracingTest, GivenNoFunctionsToTraceExpectPass) {
    for (uint32_t i = 0; i < CL_FUNCTION_COUNT; ++i) {
        status = clSetTracingPointINTEL(handle, static_cast<cl_function_id>(i), CL_FALSE);
        EXPECT_EQ(CL_SUCCESS, status);
    }

    callFunctions();

    EXPECT_EQ(0, enterCount);
    EXPECT_EQ(0, exitCount);
}

} // namespace ULT
