/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/utilities/base_object_utils.h"

#include "opencl/source/event/user_event.h"
#include "opencl/test/unit_test/fixtures/scenario_test_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "gtest/gtest.h"

using namespace NEO;

typedef ScenarioTest BarrierScenarioTest;

HWTEST_F(BarrierScenarioTest, givenBlockedEnqueueBarrierOnOOQWhenUserEventIsUnblockedThenNextEnqueuesAreNotBlocked) {
    cl_command_queue clCommandQ = nullptr;
    cl_queue_properties properties[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    auto mockCmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, pPlatform->getClDevice(0), properties));
    clCommandQ = mockCmdQ.get();

    cl_kernel clKernel = kernelInternals->mockMultiDeviceKernel;
    size_t offset[] = {0, 0, 0};
    size_t gws[] = {1, 1, 1};

    cl_int retVal = CL_SUCCESS;
    cl_int success = CL_SUCCESS;

    UserEvent *userEvent = new UserEvent(context);
    cl_event eventBlocking = userEvent;

    retVal = clEnqueueBarrierWithWaitList(clCommandQ, 1, &eventBlocking, nullptr);
    EXPECT_EQ(success, retVal);

    EXPECT_EQ(CompletionStamp::notReady, mockCmdQ->taskLevel);
    EXPECT_NE(nullptr, mockCmdQ->virtualEvent);

    clSetUserEventStatus(eventBlocking, CL_COMPLETE);
    userEvent->release();

    mockCmdQ->isQueueBlocked();
    EXPECT_NE(CompletionStamp::notReady, mockCmdQ->taskLevel);
    EXPECT_EQ(nullptr, mockCmdQ->virtualEvent);

    retVal = clEnqueueNDRangeKernel(clCommandQ, clKernel, 1, offset, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(success, retVal);

    retVal = clFinish(clCommandQ);
    EXPECT_EQ(success, retVal);
}
