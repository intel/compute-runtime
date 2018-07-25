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

#include "runtime/event/async_events_handler.h"
#include "runtime/event/user_event.h"
#include "runtime/platform/platform.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_event.h"

#include "test.h"

#include <memory>

using namespace OCLRT;

struct CallbackData {
    static void CL_CALLBACK callback(cl_event event, cl_int status, void *userData) {
        uint32_t *nestLevel = (uint32_t *)userData;

        if (*nestLevel < 4) {
            (*nestLevel)++;
            clSetEventCallback(event, CL_COMPLETE, CallbackData::callback, userData);
        }
    }
};

TEST(EventCallbackTest, NestedCallbacksAreCalledForUserEvent) {

    MockEvent<UserEvent> event(nullptr);
    uint32_t nestLevel = 0;

    event.addCallback(CallbackData::callback, CL_COMPLETE, &nestLevel);
    event.setStatus(CL_COMPLETE);
    EXPECT_EQ(4u, nestLevel);
}

TEST(EventCallbackTest, NestedCallbacksAreCalledForEvent) {
    constructPlatform();
    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext context;
    MockCommandQueue queue(&context, device.get(), nullptr);
    MockEvent<Event> event(&queue, CL_COMMAND_MARKER, 0, 0);
    uint32_t nestLevel = 0;

    event.addCallback(CallbackData::callback, CL_COMPLETE, &nestLevel);
    event.setStatus(CL_COMPLETE);

    platform()->getAsyncEventsHandler()->closeThread();

    EXPECT_EQ(4u, nestLevel);
    platformImpl.reset(nullptr);
}
