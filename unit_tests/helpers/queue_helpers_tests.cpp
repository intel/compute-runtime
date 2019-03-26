/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/queue_helpers.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_event.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(ReleaseQueueTest, givenCommandQueueWithoutVirtualEventWhenReleaseQueueIsCalledThenCmdQInternalRefCountIsNotDecremented) {
    cl_int retVal = CL_SUCCESS;
    MockCommandQueue *cmdQ = new MockCommandQueue;
    EXPECT_EQ(1, cmdQ->getRefInternalCount());

    EXPECT_EQ(1, cmdQ->getRefInternalCount());

    cmdQ->incRefInternal();
    EXPECT_EQ(2, cmdQ->getRefInternalCount());

    releaseQueue<CommandQueue>(cmdQ, retVal);

    EXPECT_EQ(1, cmdQ->getRefInternalCount());
    cmdQ->decRefInternal();
}
