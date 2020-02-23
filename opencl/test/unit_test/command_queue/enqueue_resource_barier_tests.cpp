/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"
#include "test.h"

using namespace NEO;

using ResourceBarrierTest = Test<CommandEnqueueFixture>;

HWTEST_F(ResourceBarrierTest, givenNullArgsAndHWCommandQueueWhenEnqueueResourceBarrierCalledThenCorrectStatusReturned) {
    auto retVal = pCmdQ->enqueueResourceBarrier(
        nullptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}