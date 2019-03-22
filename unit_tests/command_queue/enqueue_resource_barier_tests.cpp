/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"
#include "unit_tests/command_queue/command_enqueue_fixture.h"

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