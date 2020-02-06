/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/device/device.h"
#include "core/unit_tests/utilities/base_object_utils.h"
#include "runtime/context/context.h"
#include "runtime/helpers/base_object.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clCreatePipeTests;

namespace ClCreatePipeTests {

class clCreatePipeWithParamTests : public ApiFixture, public testing::TestWithParam<uint64_t> {
    void SetUp() override {
        ApiFixture::SetUp();
    }
    void TearDown() override {
        ApiFixture::TearDown();
    }
};

class clCreatePipeWithParamNegativeTests : public ApiFixture, public testing::TestWithParam<uint64_t> {
    void SetUp() override {
        ApiFixture::SetUp();
    }
    void TearDown() override {
        ApiFixture::TearDown();
    }
};

TEST_P(clCreatePipeWithParamTests, GivenValidFlagsWhenCreatingPipeThenPipeIsCreatedAndSuccessIsReturned) {
    cl_mem_flags flags = GetParam();

    auto pipe = clCreatePipe(pContext, flags, 1, 20, nullptr, &retVal);
    EXPECT_NE(nullptr, pipe);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseMemObject(pipe);
}

TEST_P(clCreatePipeWithParamNegativeTests, GivenInalidFlagsWhenCreatingPipeThenInvalidValueErrorIsReturned) {
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

TEST_F(clCreatePipeTests, GivenValidFlagsAndNullReturnWhenCreatingPipeThenPipeIsCreated) {
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    auto pipe = clCreatePipe(pContext, flags, 1, 20, nullptr, nullptr);

    EXPECT_NE(nullptr, pipe);

    clReleaseMemObject(pipe);
}

TEST_F(clCreatePipeTests, GivenPipePacketSizeZeroWhenCreatingPipeThenInvalidPipeSizeErrorIsReturned) {
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    auto pipe = clCreatePipe(pContext, flags, 0, 20, nullptr, &retVal);

    EXPECT_EQ(nullptr, pipe);
    EXPECT_EQ(CL_INVALID_PIPE_SIZE, retVal);

    clReleaseMemObject(pipe);
}

TEST_F(clCreatePipeTests, GivenPipeMaxSizeZeroWhenCreatingPipeThenInvalidPipeSizeErrorIsReturned) {
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    auto pipe = clCreatePipe(pContext, flags, 1, 0, nullptr, &retVal);

    EXPECT_EQ(nullptr, pipe);
    EXPECT_EQ(CL_INVALID_PIPE_SIZE, retVal);

    clReleaseMemObject(pipe);
}

TEST_F(clCreatePipeTests, GivenPipePropertiesNotNullWhenCreatingPipeThenInvalidValueErrorIsReturned) {
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_pipe_properties properties = {0};
    auto pipe = clCreatePipe(pContext, flags, 1, 20, &properties, &retVal);

    EXPECT_EQ(nullptr, pipe);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    clReleaseMemObject(pipe);
}

TEST_F(clCreatePipeTests, GivenPipePacketSizeGreaterThanAllowedWhenCreatingPipeThenInvalidPipeSizeErrorIsReturned) {
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

TEST_F(clCreatePipeTests, GivenNullContextWhenCreatingPipeThenInvalidContextErrorIsReturned) {

    auto pipe = clCreatePipe(nullptr, 0, 1, 20, nullptr, &retVal);

    EXPECT_EQ(nullptr, pipe);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);

    clReleaseMemObject(pipe);
}

TEST(clCreatePipeTest, givenPlatformWithoutDevicesWhenClCreatePipeIsCalledThenDeviceIsTakenFromContext) {
    auto executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto device = std::make_unique<ClDevice>(*Device::create<RootDevice>(executionEnvironment, 0u), platform());
    const DeviceInfo &devInfo = device->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
    cl_device_id clDevice = device.get();
    cl_int retVal;
    auto context = ReleaseableObjectPtr<Context>(Context::create<Context>(nullptr, ClDeviceVector(&clDevice, 1), nullptr, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(0u, platform()->getNumDevices());
    cl_uint packetSize = context->getDevice(0)->getDeviceInfo().pipeMaxPacketSize;
    cl_mem_flags flags = CL_MEM_READ_WRITE;

    auto pipe = clCreatePipe(context.get(), flags, packetSize, 20, nullptr, &retVal);
    EXPECT_NE(nullptr, pipe);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseMemObject(pipe);
}
} // namespace ClCreatePipeTests
