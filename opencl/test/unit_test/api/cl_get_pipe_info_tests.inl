/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/context/context.h"
#include "opencl/source/mem_obj/pipe.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clGetPipeInfoTests;

namespace ULT {

TEST_F(clGetPipeInfoTests, GivenValidPipeWithPacketSizeOneWhenGettingPipeInfoThenPacketSizeReturnedIsOne) {
    auto pipe = clCreatePipe(pContext, CL_MEM_READ_WRITE, 1, 20, nullptr, &retVal);

    EXPECT_NE(nullptr, pipe);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_uint paramValue = 0;
    size_t paramValueRetSize = 0;

    retVal = clGetPipeInfo(pipe, CL_PIPE_PACKET_SIZE, sizeof(paramValue), &paramValue, &paramValueRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(paramValue, 1u);
    EXPECT_EQ(paramValueRetSize, sizeof(cl_uint));

    clReleaseMemObject(pipe);
}

TEST_F(clGetPipeInfoTests, GivenValidPipeWithMaxPacketEqualTwentyWhenGettingPipeInfoThenMaxPacketReturnedIsTwenty) {
    auto pipe = clCreatePipe(pContext, CL_MEM_READ_WRITE, 1, 20, nullptr, &retVal);

    EXPECT_NE(nullptr, pipe);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_uint paramValue = 0;
    size_t paramValueRetSize = 0;

    retVal = clGetPipeInfo(pipe, CL_PIPE_MAX_PACKETS, sizeof(paramValue), &paramValue, &paramValueRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(paramValue, 20u);
    EXPECT_EQ(paramValueRetSize, sizeof(cl_uint));

    clReleaseMemObject(pipe);
}

TEST_F(clGetPipeInfoTests, GivenInvalidParamNameWhenGettingPipeInfoThenClInvalidValueErrorIsReturned) {
    auto pipe = clCreatePipe(pContext, CL_MEM_READ_WRITE, 1, 20, nullptr, &retVal);

    EXPECT_NE(nullptr, pipe);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_uint paramValue = 0;
    size_t paramValueRetSize = 0;

    retVal = clGetPipeInfo(pipe, CL_MEM_READ_WRITE, sizeof(paramValue), &paramValue, &paramValueRetSize);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    clReleaseMemObject(pipe);
}

TEST_F(clGetPipeInfoTests, GivenInvalidMemoryObjectWhenGettingPipeInfoThenClInvalidMemObjectErrorIsReturned) {

    cl_uint paramValue = 0;
    size_t paramValueRetSize = 0;
    char fakeMemoryObj[sizeof(Pipe)];

    retVal = clGetPipeInfo((cl_mem)&fakeMemoryObj[0], CL_MEM_READ_WRITE, sizeof(paramValue), &paramValue, &paramValueRetSize);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(clGetPipeInfoTests, GivenNullParamValueWhenGettingPipeInfoThenClSuccessIsReturned) {
    auto pipe = clCreatePipe(pContext, CL_MEM_READ_WRITE, 1, 20, nullptr, &retVal);

    EXPECT_NE(nullptr, pipe);
    EXPECT_EQ(CL_SUCCESS, retVal);
    size_t paramValueRetSize = 0;

    cl_uint paramValue = 0;

    retVal = clGetPipeInfo(pipe, CL_PIPE_MAX_PACKETS, sizeof(paramValue), nullptr, &paramValueRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseMemObject(pipe);
}

TEST_F(clGetPipeInfoTests, GivenNullParamValueSizeRetWhenGettingPipeInfoThenClSuccessIsReturned) {
    auto pipe = clCreatePipe(pContext, CL_MEM_READ_WRITE, 1, 20, nullptr, &retVal);

    EXPECT_NE(nullptr, pipe);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_uint paramValue = 0;

    retVal = clGetPipeInfo(pipe, CL_PIPE_MAX_PACKETS, sizeof(paramValue), &paramValue, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseMemObject(pipe);
}

TEST_F(clGetPipeInfoTests, GivenParamValueSizeRetTooSmallWhenGettingPipeInfoThenClInvalidValueErrorIsReturned) {
    auto pipe = clCreatePipe(pContext, CL_MEM_READ_WRITE, 1, 20, nullptr, &retVal);

    EXPECT_NE(nullptr, pipe);
    EXPECT_EQ(CL_SUCCESS, retVal);

    uint16_t paramValue = 0;
    size_t paramValueRetSize = 0;

    retVal = clGetPipeInfo(pipe, CL_PIPE_PACKET_SIZE, sizeof(paramValue), &paramValue, &paramValueRetSize);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    clReleaseMemObject(pipe);
}

TEST_F(clGetPipeInfoTests, GivenBufferInsteadOfPipeWhenGettingPipeInfoThenClInvalidMemObjectErrorIsReturned) {
    auto buffer = clCreateBuffer(pContext, CL_MEM_READ_WRITE, 20, nullptr, &retVal);

    EXPECT_NE(nullptr, buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_uint paramValue = 0;
    size_t paramValueRetSize = 0;

    retVal = clGetPipeInfo(buffer, CL_PIPE_PACKET_SIZE, sizeof(paramValue), &paramValue, &paramValueRetSize);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);

    clReleaseMemObject(buffer);
}
} // namespace ULT
