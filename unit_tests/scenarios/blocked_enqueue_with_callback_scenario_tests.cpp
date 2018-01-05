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
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "runtime/helpers/options.h"
#include "runtime/platform/platform.h"
#include "runtime/event/async_events_handler.h"

#include "gtest/gtest.h"
#include "test.h"

using namespace OCLRT;

struct CallbackData {
    cl_kernel kernel;
    cl_command_queue queue;
    bool callbackCalled = false;
    UserEvent *signalCallbackDoneEvent = nullptr;
};

void CL_CALLBACK callback(cl_event event, cl_int status, void *data) {
    CallbackData *callbackData = (CallbackData *)data;
    size_t offset[] = {0, 0, 0};
    size_t gws[] = {1, 1, 1};

    clEnqueueNDRangeKernel(callbackData->queue, callbackData->kernel, 1, offset, gws, nullptr, 0, nullptr, nullptr);
    clFinish(callbackData->queue);

    callbackData->callbackCalled = true;

    if (callbackData->signalCallbackDoneEvent) {
        cl_event callbackEvent = callbackData->signalCallbackDoneEvent;
        clSetUserEventStatus(callbackEvent, CL_COMPLETE);
        // No need to reatin and release this synchronization event
        //clReleaseEvent(callbackEvent);
    }
}

TEST_F(ScenarioTest, givenAsyncHandlerDisabledAndUserEventBlockingEnqueueAndOutputEventWithCallbackWhenUserEventIsSetCompleteThanCallbackIsExecuted) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableAsyncEventsHandler.set(false);

    cl_command_queue clCommandQ = nullptr;
    cl_queue_properties properties = 0;
    cl_kernel clKernel = kernel;
    size_t offset[] = {0, 0, 0};
    size_t gws[] = {1, 1, 1};

    cl_int retVal = CL_SUCCESS;
    cl_int success = CL_SUCCESS;

    UserEvent *userEvent = new UserEvent(context);
    cl_event eventBlocking = userEvent;
    cl_event eventOut = nullptr;

    clCommandQ = clCreateCommandQueue(context, devices[0], properties, &retVal);

    retVal = clEnqueueNDRangeKernel(clCommandQ, clKernel, 1, offset, gws, nullptr, 1, &eventBlocking, &eventOut);
    EXPECT_EQ(success, retVal);
    ASSERT_NE(nullptr, eventOut);

    CallbackData data;
    data.kernel = clKernel;
    data.queue = clCommandQ;
    data.callbackCalled = false;

    clSetEventCallback(eventOut, CL_COMPLETE, callback, &data);
    EXPECT_FALSE(data.callbackCalled);

    clSetUserEventStatus(eventBlocking, CL_COMPLETE);
    userEvent->release();

    clWaitForEvents(1, &eventOut);
    EXPECT_TRUE(data.callbackCalled);

    clReleaseEvent(eventOut);
    clReleaseCommandQueue(clCommandQ);
}

TEST_F(ScenarioTest, givenAsyncHandlerEnabledAndUserEventBlockingEnqueueAndOutputEventWithCallbackWhenUserEventIsSetCompleteThanCallbackIsExecuted) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableAsyncEventsHandler.set(true);

    cl_command_queue clCommandQ = nullptr;
    cl_queue_properties properties = 0;
    cl_kernel clKernel = kernel;
    size_t offset[] = {0, 0, 0};
    size_t gws[] = {1, 1, 1};
    cl_int retVal = CL_SUCCESS;
    cl_int success = CL_SUCCESS;
    UserEvent *userEvent = new UserEvent(context);
    cl_event eventBlocking = userEvent;
    cl_event eventOut = nullptr;

    clCommandQ = clCreateCommandQueue(context, devices[0], properties, &retVal);
    retVal = clEnqueueNDRangeKernel(clCommandQ, clKernel, 1, offset, gws, nullptr, 1, &eventBlocking, &eventOut);

    EXPECT_EQ(success, retVal);
    ASSERT_NE(nullptr, eventOut);

    CallbackData data;
    data.kernel = clKernel;
    data.queue = clCommandQ;
    data.callbackCalled = false;
    data.signalCallbackDoneEvent = new UserEvent(context);
    cl_event callbackEvent = data.signalCallbackDoneEvent;

    // This retain should not be needed, runtime shouldn't delete event before callbacks are finished
    //clRetainEvent(callbackEvent);

    clSetEventCallback(eventOut, CL_COMPLETE, callback, &data);
    EXPECT_FALSE(data.callbackCalled);

    clSetUserEventStatus(eventBlocking, CL_COMPLETE);
    userEvent->release();

    clWaitForEvents(1, &eventOut);
    clWaitForEvents(1, &callbackEvent);

    EXPECT_TRUE(data.callbackCalled);

    data.signalCallbackDoneEvent->release();

    clReleaseEvent(eventOut);
    clReleaseCommandQueue(clCommandQ);

    platform()->getAsyncEventsHandler()->closeThread();
}
