/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/utilities/arrayref.h"

#include "opencl/source/event/event_builder.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/helpers/task_information.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_csr.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "gtest/gtest.h"

namespace NEO {

struct SmallEventBuilderEventMock : MockEvent<Event> {
    SmallEventBuilderEventMock(int param1, float param2)
        : MockEvent<Event>(nullptr, CL_COMMAND_NDRANGE_KERNEL, 0, 0), constructionParam1(param1), constructionParam2(param2) {
    }

    SmallEventBuilderEventMock()
        : SmallEventBuilderEventMock(1, 2.0f) {
    }

    void overrideMagic(cl_long newMagic) {
        this->magic = newMagic;
    }

    int constructionParam1 = 1;
    float constructionParam2 = 2.0f;
};

struct SmallEventBuilderMock : EventBuilder {
    void clear() {
        EventBuilder::clear();
    }
    void clearEvent() {
        event = nullptr;
    }

    ArrayRef<Event *> getParentEvents() {
        return this->parentEvents;
    }
};

TEST(EventBuilder, whenCreatingNewEventForwardsArgumentsToEventConstructor) {
    EventBuilder eventBuilder;
    EXPECT_EQ(nullptr, eventBuilder.getEvent());

    constexpr int constrParam1 = 7;
    constexpr float constrParam2 = 13.0f;
    eventBuilder.create<SmallEventBuilderEventMock>(constrParam1, constrParam2);
    Event *peekedEvent = eventBuilder.getEvent();
    ASSERT_NE(nullptr, peekedEvent);
    auto finalizedEvent = static_cast<SmallEventBuilderEventMock *>(eventBuilder.finalizeAndRelease());
    EXPECT_EQ(peekedEvent, peekedEvent);
    EXPECT_EQ(constrParam1, finalizedEvent->constructionParam1);
    EXPECT_EQ(constrParam2, finalizedEvent->constructionParam2);

    finalizedEvent->release();
}

TEST(EventBuilder, givenVirtualEventWithCommandThenFinalizeAddChild) {
    class MockCommandComputeKernel : public CommandComputeKernel {
      public:
        using CommandComputeKernel::eventsWaitlist;
        MockCommandComputeKernel(CommandQueue &commandQueue, std::unique_ptr<KernelOperation> &kernelOperation, std::vector<Surface *> &surfaces, Kernel *kernel)
            : CommandComputeKernel(commandQueue, kernelOperation, surfaces, false, false, false, nullptr, PreemptionMode::Disabled, kernel, 0) {}
    };

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    CommandQueue cmdQ(nullptr, device.get(), nullptr);
    MockKernelWithInternals kernel(*device);

    IndirectHeap *ih1 = nullptr, *ih2 = nullptr, *ih3 = nullptr;
    cmdQ.allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, 1, ih1);
    cmdQ.allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, 1, ih2);
    cmdQ.allocateHeapMemory(IndirectHeap::SURFACE_STATE, 1, ih3);
    auto cmdStream = new LinearStream(device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), 1, GraphicsAllocation::AllocationType::COMMAND_BUFFER}));

    std::vector<Surface *> surfaces;
    auto kernelOperation = std::make_unique<KernelOperation>(cmdStream, *device->getDefaultEngine().commandStreamReceiver->getInternalAllocationStorage());
    kernelOperation->setHeaps(ih1, ih2, ih3);

    std::unique_ptr<MockCommandComputeKernel> command = std::make_unique<MockCommandComputeKernel>(cmdQ, kernelOperation, surfaces, kernel);

    VirtualEvent virtualEvent;
    virtualEvent.setCommand(std::move(command));

    EventBuilder eventBuilder;
    EXPECT_EQ(nullptr, eventBuilder.getEvent());

    constexpr int constrParam1 = 7;
    constexpr float constrParam2 = 13.0f;
    eventBuilder.create<SmallEventBuilderEventMock>(constrParam1, constrParam2);
    Event *peekedEvent = eventBuilder.getEvent();
    ASSERT_NE(nullptr, peekedEvent);
    virtualEvent.taskLevel = CL_SUBMITTED;
    eventBuilder.addParentEvent(&virtualEvent);

    eventBuilder.finalize();

    peekedEvent->release();
}

TEST(EventBuilder, givenVirtualEventWithSubmittedCommandAsParentThenFinalizeNotAddChild) {
    class MockVirtualEvent : public VirtualEvent {
      public:
        using VirtualEvent::eventWithoutCommand;
        using VirtualEvent::submittedCmd;
    };

    class MockCommandComputeKernel : public CommandComputeKernel {
      public:
        using CommandComputeKernel::eventsWaitlist;
        MockCommandComputeKernel(CommandQueue &commandQueue, std::unique_ptr<KernelOperation> &kernelOperation, std::vector<Surface *> &surfaces, Kernel *kernel)
            : CommandComputeKernel(commandQueue, kernelOperation, surfaces, false, false, false, nullptr, PreemptionMode::Disabled, kernel, 0) {}
    };

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    CommandQueue cmdQ(nullptr, device.get(), nullptr);
    MockKernelWithInternals kernel(*device);

    IndirectHeap *ih1 = nullptr, *ih2 = nullptr, *ih3 = nullptr;
    cmdQ.allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, 1, ih1);
    cmdQ.allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, 1, ih2);
    cmdQ.allocateHeapMemory(IndirectHeap::SURFACE_STATE, 1, ih3);
    auto cmdStream = new LinearStream(device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), 4096, GraphicsAllocation::AllocationType::COMMAND_BUFFER}));

    std::vector<Surface *> surfaces;
    auto kernelOperation = std::make_unique<KernelOperation>(cmdStream, *device->getDefaultEngine().commandStreamReceiver->getInternalAllocationStorage());
    kernelOperation->setHeaps(ih1, ih2, ih3);

    std::unique_ptr<MockCommandComputeKernel> command = std::make_unique<MockCommandComputeKernel>(cmdQ, kernelOperation, surfaces, kernel);

    MockVirtualEvent virtualEvent;
    virtualEvent.eventWithoutCommand = false;
    virtualEvent.submittedCmd.exchange(command.release());

    EventBuilder eventBuilder;
    EXPECT_EQ(nullptr, eventBuilder.getEvent());

    constexpr int constrParam1 = 7;
    constexpr float constrParam2 = 13.0f;
    eventBuilder.create<SmallEventBuilderEventMock>(constrParam1, constrParam2);
    Event *peekedEvent = eventBuilder.getEvent();
    ASSERT_NE(nullptr, peekedEvent);
    virtualEvent.taskLevel = CL_SUBMITTED;
    eventBuilder.addParentEvent(&virtualEvent);

    eventBuilder.finalize();

    peekedEvent->release();
}

TEST(EventBuilder, whenDestroyingEventBuilderImplicitFinalizeIscalled) {
    SmallEventBuilderEventMock *ev = nullptr;
    auto parentEvent = new UserEvent;
    {
        EventBuilder eventBuilder{};
        eventBuilder.create<SmallEventBuilderEventMock>();
        eventBuilder.addParentEvent(*parentEvent);
        ev = static_cast<SmallEventBuilderEventMock *>(eventBuilder.getEvent());
        ASSERT_NE(nullptr, ev);
        EXPECT_EQ(0U, ev->peekNumEventsBlockingThis());
    }
    // make sure that finalize was called on EventBuilder's d-tor and parent was added properly
    EXPECT_EQ(1U, ev->peekNumEventsBlockingThis());
    ev->release();
    parentEvent->release();
}

TEST(EventBuilder, whenFinalizeIsCalledTwiceOnEventBuilderThenSecondRequestIsDropped) {
    SmallEventBuilderEventMock *ev = nullptr;
    EventBuilder eventBuilder{};
    eventBuilder.create<SmallEventBuilderEventMock>();
    ev = static_cast<SmallEventBuilderEventMock *>(eventBuilder.getEvent());
    ASSERT_NE(nullptr, ev);
    eventBuilder.finalize();
    auto *falseParentEvent = new UserEvent();
    auto *falseChildEvent = new SmallEventBuilderEventMock;
    auto numParents = ev->peekNumEventsBlockingThis();
    auto numChildren = (ev->peekChildEvents() != nullptr) ? 1U + ev->peekChildEvents()->countSuccessors() : 0;
    eventBuilder.addParentEvent(*falseParentEvent);
    eventBuilder.finalize();
    // make sure that new parent was not added in second finalize
    EXPECT_EQ(numParents, ev->peekNumEventsBlockingThis());
    EXPECT_EQ(numChildren, (ev->peekChildEvents() != nullptr) ? 1U + ev->peekChildEvents()->countSuccessors() : 0);
    falseParentEvent->release();
    falseChildEvent->release();
    ev->release();
}

TEST(EventBuilder, whenFinalizeAndReleaseIsCalledThenEventBuilderReleasesReferenceToEvent) {
    EventBuilder eventBuilder;
    eventBuilder.create<SmallEventBuilderEventMock>();
    auto ev = static_cast<SmallEventBuilderEventMock *>(eventBuilder.finalizeAndRelease());
    ASSERT_NE(nullptr, ev);
    ASSERT_EQ(nullptr, eventBuilder.getEvent());
    ASSERT_EQ(nullptr, eventBuilder.finalizeAndRelease());
    ev->release();
}

TEST(EventBuilder, whenClearIsCalledThenAllEventsAndReferencesAreDropped) {
    auto parentEvent = new UserEvent();
    SmallEventBuilderMock eventBuilder;
    eventBuilder.addParentEvent(*parentEvent);
    eventBuilder.clear();

    EXPECT_EQ(0U, eventBuilder.getParentEvents().size());
    EXPECT_EQ(nullptr, eventBuilder.getEvent());

    parentEvent->release();
}

TEST(EventBuilder, whenCParentEventsGetAddedThenTheirReferenceCountGetsIncreasedUntilFinalizeIsCalled) {
    UserEvent evParent1;
    UserEvent evParent2;

    EXPECT_EQ(1, evParent1.getRefInternalCount());
    EXPECT_EQ(1, evParent2.getRefInternalCount());

    EventBuilder eventBuilder;
    eventBuilder.create<SmallEventBuilderEventMock>();
    eventBuilder.addParentEvent(evParent1);
    EXPECT_EQ(2, evParent1.getRefInternalCount());
    eventBuilder.addParentEvent(evParent2);
    EXPECT_EQ(2, evParent2.getRefInternalCount());
    auto createdEvent = static_cast<SmallEventBuilderEventMock *>(eventBuilder.finalizeAndRelease());
    EXPECT_EQ(2U, createdEvent->peekNumEventsBlockingThis());
    createdEvent->release();
    evParent1.setStatus(CL_COMPLETE);
    evParent2.setStatus(CL_COMPLETE);

    EXPECT_EQ(1, evParent1.getRefInternalCount());
    EXPECT_EQ(1, evParent2.getRefInternalCount());
}

TEST(EventBuilder, whenFinalizeIsCalledWithEmptyEventsListsThenParentAndChildListsAreEmpty) {
    EventBuilder eventBuilder;
    eventBuilder.create<MockEvent<Event>>(nullptr, CL_COMMAND_MARKER, 0, 0);
    Event *event = eventBuilder.finalizeAndRelease();
    EXPECT_EQ(0U, event->peekNumEventsBlockingThis());
    EXPECT_EQ(nullptr, event->peekChildEvents());
    event->release();
}

TEST(EventBuilder, whenFinalizeIsCalledAndBuildersEventsListAreNotEmptyThenEventsListsAreAddedToEvent) {
    MockEvent<UserEvent> *parentEvent = new MockEvent<UserEvent>();

    EventBuilder eventBuilder;
    eventBuilder.create<MockEvent<Event>>(nullptr, CL_COMMAND_MARKER, 0, 0);
    eventBuilder.addParentEvent(*parentEvent);
    Event *event = eventBuilder.finalizeAndRelease();

    EXPECT_EQ(1U, event->peekNumEventsBlockingThis());
    ASSERT_NE(nullptr, parentEvent->peekChildEvents());
    EXPECT_EQ(event, parentEvent->peekChildEvents()->ref);
    parentEvent->setStatus(CL_COMPLETE);
    EXPECT_EQ(0U, event->peekNumEventsBlockingThis());
    EXPECT_EQ(nullptr, parentEvent->peekChildEvents());

    event->release();
    parentEvent->release();
}

TEST(EventBuilder, whenFinalizeIsCalledAndParentsListContainsManyEventsFromWhichOnlyFirstOnesAreCompletedThenEventIsNotCompleted) {
    MockEvent<UserEvent> *userEventNotCompleted = new MockEvent<UserEvent>();
    MockEvent<UserEvent> *userEventCompleted = new MockEvent<UserEvent>();
    userEventCompleted->setStatus(CL_COMPLETE);

    EventBuilder eventBuilder;
    eventBuilder.create<MockEvent<Event>>(nullptr, CL_COMMAND_MARKER, 0, 0);
    eventBuilder.addParentEvent(*userEventCompleted);
    eventBuilder.addParentEvent(*userEventNotCompleted);
    Event *event = eventBuilder.finalizeAndRelease();
    EXPECT_FALSE(event->updateStatusAndCheckCompletion());
    EXPECT_EQ(1U, event->peekNumEventsBlockingThis());
    ASSERT_EQ(nullptr, userEventCompleted->peekChildEvents());
    ASSERT_NE(nullptr, userEventNotCompleted->peekChildEvents());
    EXPECT_EQ(event, userEventNotCompleted->peekChildEvents()->ref);
    userEventNotCompleted->setStatus(CL_COMPLETE);
    EXPECT_EQ(0U, event->peekNumEventsBlockingThis());
    EXPECT_EQ(nullptr, userEventNotCompleted->peekChildEvents());
    event->release();
    userEventCompleted->release();
    userEventNotCompleted->release();
}

TEST(EventBuilder, whenAddingNullptrAsNewParentEventThenItIsIgnored) {
    SmallEventBuilderMock eventBuilder;
    EXPECT_EQ(0U, eventBuilder.getParentEvents().size());
    eventBuilder.addParentEvent(nullptr);
    EXPECT_EQ(0U, eventBuilder.getParentEvents().size());
}

TEST(EventBuilder, whenAddingValidEventAsNewParentEventThenItIsProperlyAddedToParentsList) {
    auto event = new SmallEventBuilderEventMock;
    SmallEventBuilderMock eventBuilder;
    eventBuilder.create<MockEvent<Event>>(nullptr, CL_COMMAND_MARKER, 0, 0);
    EXPECT_EQ(0U, eventBuilder.getParentEvents().size());
    eventBuilder.addParentEvent(event);
    EXPECT_EQ(1U, eventBuilder.getParentEvents().size());
    event->release();
    eventBuilder.finalize();
    eventBuilder.getEvent()->release();
}

TEST(EventBuilder, whenAddingMultipleEventsAsNewParentsThenOnlyValidOnesAreInsertedIntoParentsList) {
    auto event = new SmallEventBuilderEventMock;
    auto invalidEvent = new SmallEventBuilderEventMock;
    invalidEvent->overrideMagic(0);
    cl_event eventsList[] = {nullptr, event, invalidEvent};
    SmallEventBuilderMock eventBuilder;
    eventBuilder.create<MockEvent<Event>>(nullptr, CL_COMMAND_MARKER, 0, 0);
    EXPECT_EQ(0U, eventBuilder.getParentEvents().size());
    eventBuilder.addParentEvents(ArrayRef<cl_event>(eventsList));
    ASSERT_EQ(1U, eventBuilder.getParentEvents().size());
    EXPECT_EQ(event, *eventBuilder.getParentEvents().begin());
    invalidEvent->release();
    event->release();
    eventBuilder.finalize();
    eventBuilder.getEvent()->release();
}

TEST(EventBuilder, parentListDoesNotHaveDuplicates) {
    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext mockContext;
    MockCommandQueue mockCommandQueue(&mockContext, mockDevice.get(), nullptr);

    SmallEventBuilderMock *eventBuilder = new SmallEventBuilderMock;
    eventBuilder->create<MockEvent<Event>>(&mockCommandQueue, CL_COMMAND_MARKER, 0, 0);
    Event *event = eventBuilder->getEvent();

    Event *parentEvent = new Event(&mockCommandQueue, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
    Event *parentEvent2 = new Event(&mockCommandQueue, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
    Event *parentEvent3 = new Event(&mockCommandQueue, CL_COMMAND_NDRANGE_KERNEL, 0, 0);

    eventBuilder->addParentEvent(parentEvent);
    eventBuilder->addParentEvent(parentEvent2);
    eventBuilder->addParentEvent(parentEvent3);
    // add duplicate
    eventBuilder->addParentEvent(parentEvent);

    auto parents = eventBuilder->getParentEvents();
    size_t numberOfParents = parents.size();
    EXPECT_EQ(3u, numberOfParents);

    event->release();
    parentEvent->release();
    parentEvent2->release();
    parentEvent3->release();
    eventBuilder->clear();
    eventBuilder->clearEvent();

    delete eventBuilder;
}

} // namespace NEO
