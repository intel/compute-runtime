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

#include "runtime/device/device.h"
#include "runtime/event/event.h"
#include "runtime/event/user_event.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/helpers/options.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_event.h"

#include "test.h"

using namespace OCLRT;

void CL_CALLBACK WriteInputBufferComplete(cl_event e, cl_int status, void *data) {
    ++(*(int *)data);
}

TEST(Event, GivenContextOnCreationAndDeletionUsesEventRegistry) {
    MockContext ctx;
    UserEvent *ue = MockEventBuilder::createAndFinalize<UserEvent>(&ctx);

    ASSERT_EQ(ue, ctx.getEventsRegistry().peekHead());
    ue->release();
    ASSERT_EQ(nullptr, ctx.getEventsRegistry().peekHead());
}

TEST(Event, WhenAddToRegistryIsCalledThenEvenAddsItselfToEventsRegistry) {
    MockContext ctx;
    UserEvent *ue = new UserEvent(&ctx);
    ASSERT_EQ(nullptr, ctx.getEventsRegistry().peekHead());
    ue->addToRegistry();
    ASSERT_EQ(ue, ctx.getEventsRegistry().peekHead());
    ue->release();
    ASSERT_EQ(nullptr, ctx.getEventsRegistry().peekHead());
}

TEST(InternalsEventTests, dontLockDeviceOnRegistryBroadcastUpdateAll) {
    class MyMockDevice : public Device {
      public:
        MyMockDevice() : Device(*platformDevices[0], false) { memoryManager = new OsAgnosticMemoryManager; }
        bool takeOwnership(bool waitUntilGet) const override {
            acquireCounter++;
            return Device::takeOwnership(waitUntilGet);
        }
        void releaseOwnership() const override {
            releaseCounter++;
            Device::releaseOwnership();
        }
        mutable uint32_t acquireCounter = 0;
        mutable uint32_t releaseCounter = 0;
    } myDevice;

    EventsRegistry registry;
    registry.setDevice(&myDevice);
    registry.broadcastUpdateAll();
    EXPECT_EQ(0u, myDevice.acquireCounter);
    EXPECT_EQ(0u, myDevice.releaseCounter);
}

TEST(EventsRegistry, broadcast) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableAsyncEventsHandler.set(false);
    auto device = Device::create<MockDevice>(nullptr);
    *device->getTagAddress() = 0u;
    MockContext ctx;
    auto cmdQ = new MockCommandQueue(&ctx, device, 0);

    class SmallMockEvent : public Event {
      public:
        SmallMockEvent(CommandQueue *cmdQueue) : Event(cmdQueue, CL_COMMAND_NDRANGE_KERNEL, 1, 1) {}
        void switchToComplete() {
            transitionExecutionStatus(CL_COMPLETE);
        }
    };

    int counter = 0;
    EventBuilder eventBuilder1;
    eventBuilder1.create<SmallMockEvent>(cmdQ);
    eventBuilder1.getEvent()->addCallback(WriteInputBufferComplete, CL_COMPLETE, &counter);
    auto event1 = static_cast<SmallMockEvent *>(eventBuilder1.finalizeAndRelease());

    EventBuilder eventBuilder2;
    eventBuilder2.create<SmallMockEvent>(cmdQ);
    eventBuilder2.getEvent()->addCallback(WriteInputBufferComplete, CL_COMPLETE, &counter);
    auto event2 = static_cast<SmallMockEvent *>(eventBuilder2.finalizeAndRelease());

    auto &reg = ctx.getEventsRegistry();

    event1->switchToComplete();
    event2->switchToComplete();
    ASSERT_EQ(0, counter);
    reg.broadcastUpdateAll();
    ASSERT_EQ(2, counter);

    // delete events prior to delete command queue, otherwise events will defer-delete already deleted cmdQ
    event1->release();
    event2->release();

    delete cmdQ;
    delete device;
}
