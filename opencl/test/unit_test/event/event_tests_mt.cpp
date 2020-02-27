/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/test/unit_test/mocks/mock_event.h"

#include "event_fixture.h"

#include <memory>

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

    for (uint32_t i = 0; i < 100; i++) {
        std::unique_ptr<UserEvent> userEvent = std::unique_ptr<UserEvent>(new UserEvent(nullptr));

        std::thread t([&]() {
            userEvent->setStatus(CL_COMPLETE);
        });

        t.join();
        cl_event clEvent = userEvent.get();
        Event::waitForEvents(1, &clEvent);
    }
}
