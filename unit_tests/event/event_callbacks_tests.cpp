/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/event/async_events_handler.h"
#include "runtime/event/user_event.h"
#include "runtime/platform/platform.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_event.h"

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
    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext context;
    MockCommandQueue queue(&context, device.get(), nullptr);
    MockEvent<Event> event(&queue, CL_COMMAND_MARKER, 0, 0);
    uint32_t nestLevel = 0;

    event.addCallback(CallbackData::callback, CL_COMPLETE, &nestLevel);
    event.setStatus(CL_COMPLETE);

    platform()->getAsyncEventsHandler()->closeThread();

    EXPECT_EQ(4u, nestLevel);
}
