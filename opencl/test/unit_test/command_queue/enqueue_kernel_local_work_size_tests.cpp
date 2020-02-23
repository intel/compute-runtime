/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

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

struct EnqueueKernelRequiredWorkSize : public HelloWorldTest<HelloWorldFixtureFactory> {
    typedef HelloWorldTest<HelloWorldFixtureFactory> Parent;

    void SetUp() override {
        Parent::kernelFilename = "required_work_group";
        Parent::kernelName = "CopyBuffer";
        Parent::SetUp();
    }

    void TearDown() override {
        Parent::TearDown();
    }
};

// Kernel specifies the optional reqd_work_group_size() attribute but it wasn't
// specified.  We'll permit the user to not specify the local work group size
// and pick up the correct values instead.
TEST_F(EnqueueKernelRequiredWorkSize, GivenUnspecifiedWorkGroupSizeWhenEnqeueingKernelThenLwsIsSetCorrectly) {
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

    EXPECT_EQ(*pKernel->localWorkSizeX, 8u);
    EXPECT_EQ(*pKernel->localWorkSizeY, 4u);
    EXPECT_EQ(*pKernel->localWorkSizeZ, 4u);

    EXPECT_EQ(*pKernel->enqueuedLocalWorkSizeX, 8u);
    EXPECT_EQ(*pKernel->enqueuedLocalWorkSizeY, 4u);
    EXPECT_EQ(*pKernel->enqueuedLocalWorkSizeZ, 4u);
}

// Fully specified
TEST_F(EnqueueKernelRequiredWorkSize, GivenRequiredWorkGroupSizeWhenEnqeueingKernelThenLwsIsSetCorrectly) {
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {32, 32, 32};
    size_t localWorkSize[3] = {8, 4, 4};

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

    EXPECT_EQ(*pKernel->enqueuedLocalWorkSizeX, 8u);
    EXPECT_EQ(*pKernel->enqueuedLocalWorkSizeY, 4u);
    EXPECT_EQ(*pKernel->enqueuedLocalWorkSizeZ, 4u);

    EXPECT_EQ(*pKernel->localWorkSizeX, 8u);
    EXPECT_EQ(*pKernel->localWorkSizeY, 4u);
    EXPECT_EQ(*pKernel->localWorkSizeZ, 4u);
}

// Underspecified.  Won't permit.
TEST_F(EnqueueKernelRequiredWorkSize, givenKernelRequiringLocalWorkgroupSizeWhen1DimensionIsPassedThatIsCorrectThenNdRangeIsSuccesful) {
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
TEST_F(EnqueueKernelRequiredWorkSize, GivenInvalidRequiredWorkgroupSizeWhenEnqueuingKernelThenInvalidWorkGroupSizeErrorIsReturned) {
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
