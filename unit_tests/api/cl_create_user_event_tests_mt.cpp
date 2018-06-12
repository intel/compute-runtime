/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "cl_api_tests.h"
#include "runtime/context/context.h"

using namespace OCLRT;

typedef api_tests clCreateUserEventMtTests;

namespace ULT {

TEST_F(clCreateUserEventMtTests, WaitForUserEventThatIsCLSubmittedStalls) {
    auto userEvent = clCreateUserEvent(
        pContext,
        &retVal);

    std::atomic<bool> ThreadStarted(false);
    std::atomic<bool> WaitForEventsCompleted(false);
    int counter = 0;
    int Deadline = 2000;
    std::thread t([&]() {
        ThreadStarted = true;
        clWaitForEvents(1, &userEvent);
        WaitForEventsCompleted = true;
    });

    //wait for the thread to start
    while (!ThreadStarted)
        ;
    //now wait a while.
    while (!WaitForEventsCompleted && counter++ < Deadline)
        ;

    ASSERT_EQ(WaitForEventsCompleted, false) << "WaitForEvents returned while user event is not signaled!";

    //set event to CL_COMPLETE
    retVal = clSetUserEventStatus(userEvent, CL_COMPLETE);
    t.join();

    ASSERT_EQ(WaitForEventsCompleted, true);

    retVal = clReleaseEvent(userEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
} // namespace ULT
