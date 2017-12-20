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

#include "unit_tests/fixtures/scenario_test_fixture.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "runtime/event/event.h"
#include "runtime/helpers/options.h"

#include "gtest/gtest.h"
#include "test.h"

using namespace OCLRT;

typedef ScenarioTest BarrierScenarioTest;

HWTEST_F(BarrierScenarioTest, givenBlockedEnqueueBarrierOnOOQWhenUserEventIsUnblockedThenNextEnqueuesAreNotBlocked) {
    cl_command_queue clCommandQ = nullptr;
    cl_queue_properties properties[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pPlatform->getDevice(0), properties);
    clCommandQ = mockCmdQ;

    cl_kernel clKernel = kernel;
    size_t offset[] = {0, 0, 0};
    size_t gws[] = {1, 1, 1};

    cl_int retVal = CL_SUCCESS;
    cl_int success = CL_SUCCESS;

    UserEvent *userEvent = new UserEvent(context);
    cl_event eventBlocking = userEvent;

    retVal = clEnqueueBarrierWithWaitList(clCommandQ, 1, &eventBlocking, nullptr);
    EXPECT_EQ(success, retVal);

    EXPECT_EQ(Event::eventNotReady, mockCmdQ->taskLevel);
    EXPECT_NE(nullptr, mockCmdQ->virtualEvent);

    clSetUserEventStatus(eventBlocking, CL_COMPLETE);
    userEvent->release();

    EXPECT_NE(Event::eventNotReady, mockCmdQ->taskLevel);
    EXPECT_EQ(nullptr, mockCmdQ->virtualEvent);

    retVal = clEnqueueNDRangeKernel(clCommandQ, clKernel, 1, offset, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(success, retVal);

    retVal = clFinish(clCommandQ);
    EXPECT_EQ(success, retVal);

    mockCmdQ->release();
}
