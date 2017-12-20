/*
 * Copyright (c) 2017, Intel Corporation
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

#include "runtime/command_queue/command_queue.h"
#include "gtest/gtest.h"
#include "unit_tests/fixtures/hello_world_fixture.h"

using namespace OCLRT;

typedef HelloWorldTest<HelloWorldFixtureFactory> EnqueueKernelLocalWorkSize;

TEST_F(EnqueueKernelLocalWorkSize, nullPointer) {
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
TEST_F(EnqueueKernelRequiredWorkSize, unspecifiedWorkGroupSize) {
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

    EXPECT_EQ(*pKernel->localWorkSizeX, 16u);
    EXPECT_EQ(*pKernel->localWorkSizeY, 8u);
    EXPECT_EQ(*pKernel->localWorkSizeZ, 4u);

    EXPECT_EQ(*pKernel->enqueuedLocalWorkSizeX, 16u);
    EXPECT_EQ(*pKernel->enqueuedLocalWorkSizeY, 8u);
    EXPECT_EQ(*pKernel->enqueuedLocalWorkSizeZ, 4u);
}

// Fully specified
TEST_F(EnqueueKernelRequiredWorkSize, matchingRequiredWorkGroupSize) {
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {32, 32, 32};
    size_t localWorkSize[3] = {16, 8, 4};

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

    EXPECT_EQ(*pKernel->enqueuedLocalWorkSizeX, 16u);
    EXPECT_EQ(*pKernel->enqueuedLocalWorkSizeY, 8u);
    EXPECT_EQ(*pKernel->enqueuedLocalWorkSizeZ, 4u);

    EXPECT_EQ(*pKernel->localWorkSizeX, 16u);
    EXPECT_EQ(*pKernel->localWorkSizeY, 8u);
    EXPECT_EQ(*pKernel->localWorkSizeZ, 4u);
}

// Underspecified.  Won't permit.
TEST_F(EnqueueKernelRequiredWorkSize, givenKernelRequiringLocalWorkgroupSizeWhen1DimensionIsPassedThatIsCorrectThenNdRangeIsSuccesful) {
    size_t globalWorkOffset[1] = {0};
    size_t globalWorkSize[1] = {32};
    size_t localWorkSize[1] = {16};

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
TEST_F(EnqueueKernelRequiredWorkSize, notMatchingRequiredLocalWorkGroupSize) {
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
