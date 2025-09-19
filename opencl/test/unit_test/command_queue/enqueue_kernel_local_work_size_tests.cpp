/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"

#include "gtest/gtest.h"

using namespace NEO;

typedef HelloWorldTest<HelloWorldFixtureFactory> EnqueueKernelLocalWorkSize;

TEST_F(EnqueueKernelLocalWorkSize, GivenNullLwsInWhenEnqueuingKernelThenSuccessIsReturned) {
    size_t globalWorkOffset[3] = {0, 999, 9999};
    size_t globalWorkSize[3] = {1, 999, 9999};

    auto retVal = pCmdQ->enqueueKernel(
        pKernel,
        1,
        globalWorkOffset,
        globalWorkSize,
        nullptr,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

struct EnqueueKernelRequiredWorkSize : public HelloWorldKernelFixture,
                                       public CommandQueueHwFixture,
                                       public ClDeviceFixture,
                                       public ::testing::Test {
    void SetUp() override {
        ClDeviceFixture::setUp();
        CommandQueueHwFixture::setUp(pClDevice, 0);
        MockZebinWrapper<>::Descriptor desc{};
        auto productHelper = NEO::CompilerProductHelper::create(defaultHwInfo->platform.eProductFamily);
        desc.isStateless = productHelper->isForceToStatelessRequired();
        desc.userAttributes["reqd_work_group_size"] = "[ 8, 2, 2 ]";
        desc.execEnv["required_work_group_size"] = "[ 8, 2, 2 ]";
        HelloWorldKernelFixture::setUp(pClDevice, desc);
    }

    void TearDown() override {
        HelloWorldKernelFixture::tearDown();
        CommandQueueHwFixture::tearDown();
        ClDeviceFixture::tearDown();
    }
};

// Kernel specifies the optional reqd_work_group_size() attribute but it wasn't
// specified.  We'll permit the user to not specify the local work group size
// and pick up the correct values instead.
HWTEST_F(EnqueueKernelRequiredWorkSize, GivenUnspecifiedWorkGroupSizeWhenEnqueueingKernelThenLwsIsSetCorrectly) {
    size_t globalWorkSize[3] = {32, 32, 32};
    size_t *localWorkSize = nullptr;

    auto retVal = pCmdQ->enqueueKernel(
        pKernel,
        3,
        nullptr,
        globalWorkSize,
        localWorkSize,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    // IGC may inline local worksize here so tokens are optional
    if (pKernel->getKernelInfo().kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[0] != undefined<CrossThreadDataOffset>) {
        auto localWorkSizeVal = pKernel->getLocalWorkSizeValues();
        EXPECT_EQ(8u, *localWorkSizeVal[0]);
        EXPECT_EQ(2u, *localWorkSizeVal[1]);
        EXPECT_EQ(2u, *localWorkSizeVal[2]);
    }
}

// Fully specified
TEST_F(EnqueueKernelRequiredWorkSize, GivenRequiredWorkGroupSizeWhenEnqueueingKernelThenLwsIsSetCorrectly) {
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {32, 32, 32};
    size_t localWorkSize[3] = {8, 2, 2};

    auto retVal = pCmdQ->enqueueKernel(
        pKernel,
        3,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    // IGC may inline local worksize here so tokens are optional
    if (pKernel->getKernelInfo().kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[0] != undefined<CrossThreadDataOffset>) {
        auto localWorkSizeVal = pKernel->getLocalWorkSizeValues();
        EXPECT_EQ(8u, *localWorkSizeVal[0]);
        EXPECT_EQ(2u, *localWorkSizeVal[1]);
        EXPECT_EQ(2u, *localWorkSizeVal[2]);
    }
}

// Underspecified.  Won't permit.
HWTEST_F(EnqueueKernelRequiredWorkSize, givenKernelRequiringLocalWorkgroupSizeWhen1DimensionIsPassedThatIsCorrectThenNdRangeIsSuccessful) {
    size_t globalWorkOffset[1] = {0};
    size_t globalWorkSize[1] = {32};
    size_t localWorkSize[1] = {8};

    auto retVal = pCmdQ->enqueueKernel(
        pKernel,
        1,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

// Incorrectly specified
HWTEST_F(EnqueueKernelRequiredWorkSize, GivenInvalidRequiredWorkgroupSizeWhenEnqueuingKernelThenInvalidWorkGroupSizeErrorIsReturned) {
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {32, 32, 32};
    size_t localWorkSize[3] = {16, 8, 1};

    auto retVal = pCmdQ->enqueueKernel(
        pKernel,
        3,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_WORK_GROUP_SIZE, retVal);
}
