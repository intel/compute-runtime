/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/queue_helpers.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_event.h"

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
