/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/tracing/tracing_api.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"

using namespace NEO;

namespace ULT {

struct IntelGlTracingTest : public ApiTests {
  public:
    IntelGlTracingTest() {}

    void SetUp() override {
        ApiTests::SetUp();

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

        ApiTests::TearDown();
    }

  protected:
    static void callback(ClFunctionId fid, cl_callback_data *callbackData, void *userData) {
        ASSERT_NE(nullptr, userData);
        IntelGlTracingTest *base = reinterpret_cast<IntelGlTracingTest *>(userData);
        base->vcallback(fid, callbackData, nullptr);
    }

    virtual void vcallback(ClFunctionId fid, cl_callback_data *callbackData, void *userData) {
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
    ClFunctionId functionId = CL_FUNCTION_COUNT;
};

TEST_F(IntelGlTracingTest, GivenAllFunctionsWhenSettingTracingPointThenTracingOnAllFunctionsIsPerformed) {
    uint16_t count = callFunctions();
    EXPECT_EQ(count, enterCount);
    EXPECT_EQ(count, exitCount);
}

TEST_F(IntelGlTracingTest, GivenNoFunctionsWhenSettingTracingPointThenNoTracingIsPerformed) {
    for (uint32_t i = 0; i < CL_FUNCTION_COUNT; ++i) {
        status = clSetTracingPointINTEL(handle, static_cast<ClFunctionId>(i), CL_FALSE);
        EXPECT_EQ(CL_SUCCESS, status);
    }

    callFunctions();

    EXPECT_EQ(0, enterCount);
    EXPECT_EQ(0, exitCount);
}

} // namespace ULT
