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

using GlobalWorkOffset = HelloWorldTest<HelloWorldFixtureFactory>;

TEST_F(GlobalWorkOffset, GivenNullGlobalWorkOffsetWhenEnqueuingKernelThenSuccessIsReturned) {
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};

    auto retVal = pCmdQ->enqueueKernel(
        pKernel,
        1,
        nullptr,
        globalWorkSize,
        localWorkSize,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}
