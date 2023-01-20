/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/context/context.h"
#include "opencl/source/helpers/base_object.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "cl_api_tests.h"

using namespace NEO;

struct clCreatePipeTests : api_tests {
    VariableBackup<bool> supportsPipesBackup{&defaultHwInfo->capabilityTable.supportsPipes, true};
};

namespace ClCreatePipeTests {

class ClCreatePipeWithParamTests : public ApiFixture<>, public testing::TestWithParam<uint64_t> {
    void SetUp() override {
        ApiFixture::setUp();
    }
    void TearDown() override {
        ApiFixture::tearDown();
    }
    VariableBackup<bool> supportsPipesBackup{&defaultHwInfo->capabilityTable.supportsPipes, true};
};

class ClCreatePipeWithParamNegativeTests : public ApiFixture<>, public testing::TestWithParam<uint64_t> {
    void SetUp() override {
        ApiFixture::setUp();
    }
    void TearDown() override {
        ApiFixture::tearDown();
    }
    VariableBackup<bool> supportsPipesBackup{&defaultHwInfo->capabilityTable.supportsPipes, true};
};

TEST_P(ClCreatePipeWithParamTests, GivenValidFlagsWhenCreatingPipeThenPipeIsCreatedAndSuccessIsReturned) {
    cl_mem_flags flags = GetParam();

    auto pipe = clCreatePipe(pContext, flags, 1, 20, nullptr, &retVal);
    EXPECT_NE(nullptr, pipe);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseMemObject(pipe);
}

TEST_P(ClCreatePipeWithParamNegativeTests, GivenInalidFlagsWhenCreatingPipeThenInvalidValueErrorIsReturned) {
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
    ClCreatePipeWithParamTests,
    testing::ValuesIn(validFlags));

INSTANTIATE_TEST_CASE_P(
    CreatePipeCheckFlagsNegative,
    ClCreatePipeWithParamNegativeTests,
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

TEST_F(clCreatePipeTests, GivenDeviceNotSupportingPipesWhenCreatingPipeThenInvalidOperationErrorIsReturned) {
    auto hardwareInfo = *defaultHwInfo;
    hardwareInfo.capabilityTable.supportsPipes = false;

    auto pClDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo, 0));
    MockContext mockContext{pClDevice.get(), false};

    auto pipe = clCreatePipe(&mockContext, 0, 1, 20, nullptr, &retVal);
    EXPECT_EQ(nullptr, pipe);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
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
    const ClDeviceInfo &devInfo = device->getDeviceInfo();
    if (devInfo.svmCapabilities == 0 || device->getHardwareInfo().capabilityTable.supportsPipes == false) {
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
