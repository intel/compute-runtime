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
#include "runtime/device/device.h"
#include "runtime/helpers/base_object.h"

using namespace OCLRT;

typedef api_tests clCreatePipeTests;

namespace ClCreatePipeTests {

class clCreatePipeWithParamTests : public api_fixture, public testing::TestWithParam<uint64_t> {
    void SetUp() override {
        api_fixture::SetUp();
    }
    void TearDown() override {
        api_fixture::TearDown();
    }
};

class clCreatePipeWithParamNegativeTests : public api_fixture, public testing::TestWithParam<uint64_t> {
    void SetUp() override {
        api_fixture::SetUp();
    }
    void TearDown() override {
        api_fixture::TearDown();
    }
};

TEST_P(clCreatePipeWithParamTests, validFlags) {
    cl_mem_flags flags = GetParam();

    auto pipe = clCreatePipe(pContext, flags, 1, 20, nullptr, &retVal);
    EXPECT_NE(nullptr, pipe);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseMemObject(pipe);
}

TEST_P(clCreatePipeWithParamNegativeTests, invalidFlags) {
    cl_mem_flags flags = GetParam();

    auto pipe = clCreatePipe(pContext, flags, 1, 20, nullptr, &retVal);
    EXPECT_EQ(nullptr, pipe);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    clReleaseMemObject(pipe);
}

static cl_mem_flags validFlags[] = {
    0,
    CL_MEM_READ_WRITE,
    CL_MEM_HOST_NO_ACCESS,
    CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS,
};

static cl_mem_flags invalidFlags[] = {
    CL_MEM_READ_WRITE | CL_MEM_HOST_READ_ONLY,
    CL_MEM_WRITE_ONLY,
    CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY,
    CL_MEM_HOST_READ_ONLY,
    CL_MEM_HOST_WRITE_ONLY,
    CL_MEM_USE_HOST_PTR,
    CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE,
    CL_MEM_COPY_HOST_PTR,
    CL_MEM_ALLOC_HOST_PTR,
    CL_MEM_COPY_HOST_PTR | CL_MEM_ALLOC_HOST_PTR,
    CL_MEM_READ_ONLY | CL_MEM_WRITE_ONLY,
    CL_MEM_READ_WRITE | CL_MEM_READ_ONLY,
    CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY,
    CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_NO_ACCESS,
    CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_WRITE_ONLY,
    CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_NO_ACCESS,
    CL_MEM_USE_HOST_PTR | CL_MEM_COPY_HOST_PTR,
    CL_MEM_USE_HOST_PTR | CL_MEM_ALLOC_HOST_PTR,
};

INSTANTIATE_TEST_CASE_P(
    CreatePipeCheckFlags,
    clCreatePipeWithParamTests,
    testing::ValuesIn(validFlags));

INSTANTIATE_TEST_CASE_P(
    CreatePipeCheckFlagsNegative,
    clCreatePipeWithParamNegativeTests,
    testing::ValuesIn(invalidFlags));

TEST_F(clCreatePipeTests, retValIsNull) {
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    auto pipe = clCreatePipe(pContext, flags, 1, 20, nullptr, nullptr);

    EXPECT_NE(nullptr, pipe);

    clReleaseMemObject(pipe);
}

TEST_F(clCreatePipeTests, PipePacketSizeIsZero) {
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    auto pipe = clCreatePipe(pContext, flags, 0, 20, nullptr, &retVal);

    EXPECT_EQ(nullptr, pipe);
    EXPECT_EQ(CL_INVALID_PIPE_SIZE, retVal);

    clReleaseMemObject(pipe);
}

TEST_F(clCreatePipeTests, PipeMaxPacketIsZero) {
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    auto pipe = clCreatePipe(pContext, flags, 1, 0, nullptr, &retVal);

    EXPECT_EQ(nullptr, pipe);
    EXPECT_EQ(CL_INVALID_PIPE_SIZE, retVal);

    clReleaseMemObject(pipe);
}

TEST_F(clCreatePipeTests, PipePropertiesAreNotNull) {
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_pipe_properties properties = {0};
    auto pipe = clCreatePipe(pContext, flags, 1, 20, &properties, &retVal);

    EXPECT_EQ(nullptr, pipe);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    clReleaseMemObject(pipe);
}

TEST_F(clCreatePipeTests, PipePacketSizeGreaterThanAllowed) {
    cl_uint packetSize = pContext->getDevice(0)->getDeviceInfo().pipeMaxPacketSize;
    cl_mem_flags flags = CL_MEM_READ_WRITE;

    auto pipe = clCreatePipe(pContext, flags, packetSize, 20, nullptr, &retVal);
    EXPECT_NE(nullptr, pipe);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseMemObject(pipe);

    packetSize += 1;
    pipe = clCreatePipe(pContext, flags, packetSize, 20, nullptr, &retVal);

    EXPECT_EQ(nullptr, pipe);
    EXPECT_EQ(CL_INVALID_PIPE_SIZE, retVal);

    clReleaseMemObject(pipe);
}

TEST_F(clCreatePipeTests, PipeInvalidContext) {

    auto pipe = clCreatePipe(nullptr, 0, 1, 20, nullptr, &retVal);

    EXPECT_EQ(nullptr, pipe);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);

    delete pipe;
}
} // namespace ClCreatePipeTests
