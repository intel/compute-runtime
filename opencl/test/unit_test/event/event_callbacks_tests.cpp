/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/event/async_events_handler.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "test.h"

#include <memory>

using namespace NEO;

struct CallbackData {
    static void CL_CALLBACK callback(cl_event event, cl_int status, void *userData) {
        uint32_t *nestLevel = (uint32_t *)userData;

        if (*nestLevel < 4) {
            (*nestLevel)++;
            clSetEventCallback(event, CL_COMPLETE, CallbackData::callback, userData);
        }
    }
};

TEST(EventCallbackTest, GivenUserEventWhenAddingCallbackThenNestedCallbacksCanBeCreated) {

    MockEvent<UserEvent> event(nullptr);
    uint32_t nestLevel = 0;

    event.addCallback(CallbackData::callback, CL_COMPLETE, &nestLevel);
    event.setStatus(CL_COMPLETE);
    EXPECT_EQ(4u, nestLevel);
}

TEST(EventCallbackTest, GivenEventWhenAddingCallbackThenNestedCallbacksCanBeCreated) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext context;
    MockCommandQueue queue(&context, device.get(), nullptr);
    MockEvent<Event> event(&queue, CL_COMMAND_MARKER, 0, 0);
    uint32_t nestLevel = 0;

    event.addCallback(CallbackData::callback, CL_COMPLETE, &nestLevel);
    event.setStatus(CL_COMPLETE);

    platform()->getAsyncEventsHandler()->closeThread();

    EXPECT_EQ(4u, nestLevel);
}
