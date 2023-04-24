/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/context/context.h"
#include "opencl/source/mem_obj/pipe.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "cl_api_tests.h"

using namespace NEO;

struct ClGetPipeInfoTests : ApiTests {
    VariableBackup<bool> supportsPipesBackup{&defaultHwInfo->capabilityTable.supportsPipes, true};
};

namespace ULT {

TEST_F(ClGetPipeInfoTests, GivenValidPipeWithPacketSizeOneWhenGettingPipeInfoThenPacketSizeReturnedIsOne) {
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

TEST_F(ClGetPipeInfoTests, GivenValidPipeWithMaxPacketEqualTwentyWhenGettingPipeInfoThenMaxPacketReturnedIsTwenty) {
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

TEST_F(ClGetPipeInfoTests, GivenInvalidParamNameWhenGettingPipeInfoThenClInvalidValueErrorIsReturned) {
    auto pipe = clCreatePipe(pContext, CL_MEM_READ_WRITE, 1, 20, nullptr, &retVal);

    EXPECT_NE(nullptr, pipe);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_uint paramValue = 0;
    size_t paramValueRetSize = 0;

    retVal = clGetPipeInfo(pipe, CL_MEM_READ_WRITE, sizeof(paramValue), &paramValue, &paramValueRetSize);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    clReleaseMemObject(pipe);
}

TEST_F(ClGetPipeInfoTests, GivenInvalidParametersWhenGettingPipeInfoThenValueSizeRetIsNotUpdated) {
    auto pipe = clCreatePipe(pContext, CL_MEM_READ_WRITE, 1, 20, nullptr, &retVal);

    EXPECT_NE(nullptr, pipe);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_uint paramValue = 0;
    size_t paramValueRetSize = 0x1234;

    retVal = clGetPipeInfo(pipe, CL_MEM_READ_WRITE, sizeof(paramValue), &paramValue, &paramValueRetSize);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(0x1234u, paramValueRetSize);

    clReleaseMemObject(pipe);
}

TEST_F(ClGetPipeInfoTests, GivenInvalidMemoryObjectWhenGettingPipeInfoThenClInvalidMemObjectErrorIsReturned) {

    cl_uint paramValue = 0;
    size_t paramValueRetSize = 0;
    char fakeMemoryObj[sizeof(Pipe)];

    retVal = clGetPipeInfo((cl_mem)&fakeMemoryObj[0], CL_MEM_READ_WRITE, sizeof(paramValue), &paramValue, &paramValueRetSize);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(ClGetPipeInfoTests, GivenNullParamValueWhenGettingPipeInfoThenClSuccessIsReturned) {
    auto pipe = clCreatePipe(pContext, CL_MEM_READ_WRITE, 1, 20, nullptr, &retVal);

    EXPECT_NE(nullptr, pipe);
    EXPECT_EQ(CL_SUCCESS, retVal);
    size_t paramValueRetSize = 0;

    cl_uint paramValue = 0;

    retVal = clGetPipeInfo(pipe, CL_PIPE_MAX_PACKETS, sizeof(paramValue), nullptr, &paramValueRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseMemObject(pipe);
}

TEST_F(ClGetPipeInfoTests, GivenNullParamValueSizeRetWhenGettingPipeInfoThenClSuccessIsReturned) {
    auto pipe = clCreatePipe(pContext, CL_MEM_READ_WRITE, 1, 20, nullptr, &retVal);

    EXPECT_NE(nullptr, pipe);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_uint paramValue = 0;

    retVal = clGetPipeInfo(pipe, CL_PIPE_MAX_PACKETS, sizeof(paramValue), &paramValue, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseMemObject(pipe);
}

TEST_F(ClGetPipeInfoTests, GivenParamValueSizeRetTooSmallWhenGettingPipeInfoThenClInvalidValueErrorIsReturned) {
    auto pipe = clCreatePipe(pContext, CL_MEM_READ_WRITE, 1, 20, nullptr, &retVal);

    EXPECT_NE(nullptr, pipe);
    EXPECT_EQ(CL_SUCCESS, retVal);

    uint16_t paramValue = 0;
    size_t paramValueRetSize = 0;

    retVal = clGetPipeInfo(pipe, CL_PIPE_PACKET_SIZE, sizeof(paramValue), &paramValue, &paramValueRetSize);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    clReleaseMemObject(pipe);
}

TEST_F(ClGetPipeInfoTests, GivenBufferInsteadOfPipeWhenGettingPipeInfoThenClInvalidMemObjectErrorIsReturned) {
    auto buffer = clCreateBuffer(pContext, CL_MEM_READ_WRITE, 20, nullptr, &retVal);

    EXPECT_NE(nullptr, buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_uint paramValue = 0;
    size_t paramValueRetSize = 0;

    retVal = clGetPipeInfo(buffer, CL_PIPE_PACKET_SIZE, sizeof(paramValue), &paramValue, &paramValueRetSize);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);

    clReleaseMemObject(buffer);
}

TEST_F(ClGetPipeInfoTests, WhenQueryingPipePropertiesThenNothingIsCopied) {
    auto pipe = clCreatePipe(pContext, CL_MEM_READ_WRITE, 1, 20, nullptr, &retVal);

    EXPECT_NE(nullptr, pipe);
    EXPECT_EQ(CL_SUCCESS, retVal);

    size_t paramSize = 1u;

    retVal = clGetPipeInfo(pipe, CL_PIPE_PROPERTIES, 0, nullptr, &paramSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, paramSize);

    clReleaseMemObject(pipe);
}

} // namespace ULT
