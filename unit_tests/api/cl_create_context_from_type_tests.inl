/*
* Copyright (c) 2017 - 2018, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#include "cl_api_tests.h"

using namespace OCLRT;

typedef api_tests clCreateContextFromTypeTests;

namespace ULT {
void CL_CALLBACK contextCallBack(const char *, const void *,
                                 size_t, void *) {
}

TEST_F(clCreateContextFromTypeTests, returnsSuccess) {
    auto context =
        clCreateContextFromType(nullptr, CL_DEVICE_TYPE_GPU, nullptr, nullptr, &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, context);
    EXPECT_NE(nullptr, context->dispatch.icdDispatch);
    EXPECT_NE(nullptr, context->dispatch.crtDispatch);

    retVal = clReleaseContext(context);
    ASSERT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateContextFromTypeTests, returnsFailOnCpuType) {
    auto context =
        clCreateContextFromType(nullptr, CL_DEVICE_TYPE_CPU, nullptr, nullptr, &retVal);
    ASSERT_EQ(nullptr, context);
    ASSERT_EQ(CL_DEVICE_NOT_FOUND, retVal);
}

TEST_F(clCreateContextFromTypeTests, returnsFailOnWrongData) {
    cl_int a;
    auto context =
        clCreateContextFromType(nullptr, CL_DEVICE_TYPE_CPU, nullptr, &a, &retVal);
    ASSERT_EQ(nullptr, context);
    ASSERT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clCreateContextFromTypeTests, nullUserData) {
    auto context = clCreateContextFromType(nullptr, CL_DEVICE_TYPE_GPU, contextCallBack, nullptr, &retVal);
    ASSERT_NE(nullptr, context);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context);
    ASSERT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateContextFromTypeTests, noRet) {
    auto context =
        clCreateContextFromType(nullptr, CL_DEVICE_TYPE_GPU, nullptr, nullptr, nullptr);

    ASSERT_NE(nullptr, context);
    EXPECT_NE(nullptr, context->dispatch.icdDispatch);
    EXPECT_NE(nullptr, context->dispatch.crtDispatch);

    retVal = clReleaseContext(context);
    ASSERT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateContextFromTypeTests, givenInvalidContextCreationPropertiesThenContextCreationFails) {
    cl_context_properties invalidProperties[3] = {CL_CONTEXT_PLATFORM, (cl_context_properties) nullptr, 0};
    auto context = clCreateContextFromType(invalidProperties, CL_DEVICE_TYPE_GPU, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
    EXPECT_EQ(nullptr, context);
}

} // namespace ULT
