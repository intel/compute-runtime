/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"

using namespace NEO;

typedef api_tests clCreateUserEventMtTests;

namespace ULT {

TEST_F(clCreateUserEventMtTests, GivenClCompleteEventWhenWaitingForEventThenWaitForEventsIsCompleted) {
    auto userEvent = clCreateUserEvent(
        pContext,
        &retVal);

    std::atomic<bool> threadStarted(false);
    std::atomic<bool> waitForEventsCompleted(false);
    int counter = 0;
    int deadline = 2000;
    std::thread t([&]() {
        threadStarted = true;
        clWaitForEvents(1, &userEvent);
        waitForEventsCompleted = true;
    });

    //wait for the thread to start
    while (!threadStarted)
        ;
    //now wait a while.
    while (!waitForEventsCompleted && counter++ < deadline)
        ;

    ASSERT_EQ(waitForEventsCompleted, false) << "WaitForEvents returned while user event is not signaled!";

    //set event to CL_COMPLETE
    retVal = clSetUserEventStatus(userEvent, CL_COMPLETE);
    t.join();

    ASSERT_EQ(waitForEventsCompleted, true);

    retVal = clReleaseEvent(userEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
} // namespace ULT
