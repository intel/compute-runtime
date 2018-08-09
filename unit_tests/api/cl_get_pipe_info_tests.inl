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
#include "runtime/context/context.h"
#include "runtime/mem_obj/pipe.h"

using namespace OCLRT;

typedef api_tests clGetPipeInfoTests;

namespace ULT {

TEST_F(clGetPipeInfoTests, packetSize) {
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

TEST_F(clGetPipeInfoTests, maxPackets) {
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

TEST_F(clGetPipeInfoTests, invalidParamName) {
    auto pipe = clCreatePipe(pContext, CL_MEM_READ_WRITE, 1, 20, nullptr, &retVal);

    EXPECT_NE(nullptr, pipe);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_uint paramValue = 0;
    size_t paramValueRetSize = 0;

    retVal = clGetPipeInfo(pipe, CL_MEM_READ_WRITE, sizeof(paramValue), &paramValue, &paramValueRetSize);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    clReleaseMemObject(pipe);
}

TEST_F(clGetPipeInfoTests, invalidMemoryObject) {

    cl_uint paramValue = 0;
    size_t paramValueRetSize = 0;
    char fakeMemoryObj[sizeof(Pipe)];

    retVal = clGetPipeInfo((cl_mem)&fakeMemoryObj[0], CL_MEM_READ_WRITE, sizeof(paramValue), &paramValue, &paramValueRetSize);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(clGetPipeInfoTests, paramValueIsNull) {
    auto pipe = clCreatePipe(pContext, CL_MEM_READ_WRITE, 1, 20, nullptr, &retVal);

    EXPECT_NE(nullptr, pipe);
    EXPECT_EQ(CL_SUCCESS, retVal);
    size_t paramValueRetSize = 0;

    cl_uint paramValue = 0;

    retVal = clGetPipeInfo(pipe, CL_PIPE_MAX_PACKETS, sizeof(paramValue), nullptr, &paramValueRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseMemObject(pipe);
}

TEST_F(clGetPipeInfoTests, paramValueRetSizeIsNull) {
    auto pipe = clCreatePipe(pContext, CL_MEM_READ_WRITE, 1, 20, nullptr, &retVal);

    EXPECT_NE(nullptr, pipe);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_uint paramValue = 0;

    retVal = clGetPipeInfo(pipe, CL_PIPE_MAX_PACKETS, sizeof(paramValue), &paramValue, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseMemObject(pipe);
}

TEST_F(clGetPipeInfoTests, paramValueSizeTooSmall) {
    auto pipe = clCreatePipe(pContext, CL_MEM_READ_WRITE, 1, 20, nullptr, &retVal);

    EXPECT_NE(nullptr, pipe);
    EXPECT_EQ(CL_SUCCESS, retVal);

    uint16_t paramValue = 0;
    size_t paramValueRetSize = 0;

    retVal = clGetPipeInfo(pipe, CL_PIPE_PACKET_SIZE, sizeof(paramValue), &paramValue, &paramValueRetSize);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    clReleaseMemObject(pipe);
}

TEST_F(clGetPipeInfoTests, BufferInsteadOfPipe) {
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
