/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_event.h"

#include "gtest/gtest.h"

#include <memory>
#include <thread>

using namespace NEO;

class SmallMockEvent : public Event {
  public:
    SmallMockEvent(Context *ctx) : Event(ctx, nullptr, CL_COMMAND_NDRANGE_KERNEL, 0, 0) {}
    void switchToSubmitted() {
        transitionExecutionStatus(CL_SUBMITTED);
    }
    void switchToComplete() {
        transitionExecutionStatus(CL_COMPLETE);
    }
};

TEST(EventTestMt, WhenWaitingForEventsThenDoNotReturnUntilAllStatusesSetToComplete) {

    for (uint32_t i = 0; i < 4; i++) {
        std::unique_ptr<UserEvent> userEvent = std::unique_ptr<UserEvent>(new UserEvent(nullptr));

        std::thread t([&]() {
            userEvent->setStatus(CL_COMPLETE);
        });

        t.join();
        cl_event clEvent = userEvent.get();
        Event::waitForEvents(1, &clEvent);
    }
}
