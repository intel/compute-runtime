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

#include "event_fixture.h"
#include "unit_tests/mocks/mock_event.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
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

TEST(EventTestMt, waitForEventsDoesNotReturnUntilSetStatusCompletes) {

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
