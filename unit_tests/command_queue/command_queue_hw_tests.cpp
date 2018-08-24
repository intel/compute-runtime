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

#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_queue/enqueue_kernel.h"
#include "runtime/command_queue/enqueue_marker.h"
#include "runtime/command_queue/enqueue_common.h"
#include "runtime/event/event.h"
#include "runtime/event/event_builder.h"
#include "runtime/helpers/queue_helpers.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/memory_manager/surface.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/gen_common/matchers.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/memory_management.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_gmm.h"
#include "unit_tests/mocks/mock_event.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "test.h"
#include "gmock/gmock.h"
#include <memory>

using namespace OCLRT;

struct CommandQueueHwTest
    : public DeviceFixture,
      public ContextFixture,
      public CommandQueueHwFixture,
      ::testing::Test {

    using ContextFixture::SetUp;

    void SetUp() override {
        DeviceFixture::SetUp();
        cl_device_id device = pDevice;
        ContextFixture::SetUp(1, &device);
        CommandQueueHwFixture::SetUp(pDevice, 0);
    }

    void TearDown() override {
        CommandQueueHwFixture::TearDown();
        ContextFixture::TearDown();
        DeviceFixture::TearDown();
    }

    cl_command_queue_properties properties;
    const HardwareInfo *pHwInfo = nullptr;
};

struct OOQueueHwTest : public DeviceFixture,
                       public ContextFixture,
                       public OOQueueFixture,
                       ::testing::Test {
    using ContextFixture::SetUp;

    OOQueueHwTest() {
    }

    void SetUp() override {
        DeviceFixture::SetUp();
        cl_device_id device = pDevice;
        ContextFixture::SetUp(1, &device);
        OOQueueFixture::SetUp(pDevice, 0);
    }

    void SetUp(Device *pDevice, cl_command_queue_properties properties) override {
    }

    void TearDown() override {
        OOQueueFixture::TearDown();
        ContextFixture::TearDown();
        DeviceFixture::TearDown();
    }
};

HWTEST_F(CommandQueueHwTest, enqueueBlockedMapUnmapOperationCreatesVirtualEvent) {

    CommandQueueHw<FamilyType> *pHwQ = reinterpret_cast<CommandQueueHw<FamilyType> *>(pCmdQ);

    MockBuffer buffer;
    pHwQ->virtualEvent = nullptr;

    MockEventBuilder eventBuilder;
    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    pHwQ->enqueueBlockedMapUnmapOperation(nullptr,
                                          0,
                                          MAP,
                                          &buffer,
                                          size, offset, false,
                                          eventBuilder);

    ASSERT_NE(nullptr, pHwQ->virtualEvent);
    pHwQ->virtualEvent->decRefInternal();
    pHwQ->virtualEvent = nullptr;
}

HWTEST_F(CommandQueueHwTest, givenBlockedMapBufferCallWhenMemObjectIsPassedToCommandThenItsRefCountIsBeingIncreased) {
    CommandQueueHw<FamilyType> *pHwQ = reinterpret_cast<CommandQueueHw<FamilyType> *>(pCmdQ);
    MockBuffer buffer;
    pHwQ->virtualEvent = nullptr;

    auto currentRefCount = buffer.getRefInternalCount();

    MockEventBuilder eventBuilder;
    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    pHwQ->enqueueBlockedMapUnmapOperation(nullptr,
                                          0,
                                          MAP,
                                          &buffer,
                                          size, offset, false,
                                          eventBuilder);

    EXPECT_EQ(currentRefCount + 1, buffer.getRefInternalCount());

    ASSERT_NE(nullptr, pHwQ->virtualEvent);
    pHwQ->virtualEvent->decRefInternal();
    pHwQ->virtualEvent = nullptr;
    EXPECT_EQ(currentRefCount, buffer.getRefInternalCount());
}

HWTEST_F(CommandQueueHwTest, givenNoReturnEventWhenCallingEnqueueBlockedMapUnmapOperationThenVirtualEventIncrementsCommandQueueInternalRefCount) {
    CommandQueueHw<FamilyType> *pHwQ = reinterpret_cast<CommandQueueHw<FamilyType> *>(pCmdQ);

    MockBuffer buffer;
    pHwQ->virtualEvent = nullptr;

    auto initialRefCountInternal = pHwQ->getRefInternalCount();

    MockEventBuilder eventBuilder;
    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    pHwQ->enqueueBlockedMapUnmapOperation(nullptr,
                                          0,
                                          MAP,
                                          &buffer,
                                          size, offset, false,
                                          eventBuilder);

    ASSERT_NE(nullptr, pHwQ->virtualEvent);

    auto refCountInternal = pHwQ->getRefInternalCount();
    EXPECT_EQ(initialRefCountInternal + 1, refCountInternal);

    pHwQ->virtualEvent->decRefInternal();
    pHwQ->virtualEvent = nullptr;
}

HWTEST_F(CommandQueueHwTest, addMapUnmapToWaitlistEventsDoesntAddDependenciesIntoChild) {
    auto buffer = new MockBuffer;
    CommandQueueHw<FamilyType> *pHwQ = reinterpret_cast<CommandQueueHw<FamilyType> *>(pCmdQ);
    auto returnEvent = new Event(pHwQ, CL_COMMAND_MAP_BUFFER, 0, 0);
    auto event = new Event(pHwQ, CL_COMMAND_MAP_BUFFER, 0, 0);
    const cl_event eventWaitList = event;

    pHwQ->virtualEvent = nullptr;

    MockEventBuilder eventBuilder(returnEvent);
    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    pHwQ->enqueueBlockedMapUnmapOperation(&eventWaitList,
                                          1,
                                          MAP,
                                          buffer,
                                          size, offset, false,
                                          eventBuilder);

    EXPECT_EQ(returnEvent, pHwQ->virtualEvent);

    ASSERT_EQ(nullptr, event->peekChildEvents());

    // Release API refcount (i.e. from workload's perspective)
    returnEvent->release();
    event->decRefInternal();
    buffer->decRefInternal();
}

HWTEST_F(CommandQueueHwTest, givenMapCommandWhenZeroStateCommandIsSubmittedThenTaskCountIsBeingWaited) {
    auto buffer = new MockBuffer;
    CommandQueueHw<FamilyType> *pHwQ = reinterpret_cast<CommandQueueHw<FamilyType> *>(pCmdQ);

    MockEventBuilder eventBuilder;
    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    pHwQ->enqueueBlockedMapUnmapOperation(nullptr,
                                          0,
                                          MAP,
                                          buffer,
                                          size, offset, false,
                                          eventBuilder);

    EXPECT_NE(nullptr, pHwQ->virtualEvent);
    pHwQ->virtualEvent->setStatus(CL_COMPLETE);

    EXPECT_EQ(1u, pHwQ->latestTaskCountWaited);
    buffer->decRefInternal();
}

HWTEST_F(CommandQueueHwTest, enqueueBlockedMapUnmapOperationInjectedCommand) {
    CommandQueueHw<FamilyType> *pHwQ = reinterpret_cast<CommandQueueHw<FamilyType> *>(pCmdQ);
    Event *returnEvent = new Event(pHwQ, CL_COMMAND_MAP_BUFFER, 0, 0);
    auto buffer = new MockBuffer;
    pHwQ->virtualEvent = nullptr;

    MockEventBuilder eventBuilder(returnEvent);
    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    pHwQ->enqueueBlockedMapUnmapOperation(nullptr,
                                          0,
                                          MAP,
                                          buffer,
                                          size, offset, false,
                                          eventBuilder);
    eventBuilder.finalizeAndRelease();

    EXPECT_EQ(returnEvent, pHwQ->virtualEvent);
    EXPECT_NE(nullptr, returnEvent->peekCommand());
    // CommandQueue has retained this event, release it
    returnEvent->release();
    pHwQ->virtualEvent = nullptr;
    // now delete
    delete returnEvent;
    buffer->decRefInternal();
}

HWTEST_F(CommandQueueHwTest, enqueueBlockedMapUnmapOperationPreviousEventHasNotInjectedChild) {
    auto buffer = new MockBuffer;
    CommandQueueHw<FamilyType> *pHwQ = reinterpret_cast<CommandQueueHw<FamilyType> *>(pCmdQ);
    Event *returnEvent = new Event(pHwQ, CL_COMMAND_MAP_BUFFER, 0, 0);
    Event event(pHwQ, CL_COMMAND_MAP_BUFFER, 0, 0);

    pHwQ->virtualEvent = nullptr;

    pHwQ->virtualEvent = &event;
    //virtual event from regular event to stored in previousVirtualEvent
    pHwQ->virtualEvent->incRefInternal();

    MockEventBuilder eventBuilder(returnEvent);
    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    pHwQ->enqueueBlockedMapUnmapOperation(nullptr,
                                          0,
                                          MAP,
                                          buffer,
                                          size, offset, false,
                                          eventBuilder);

    EXPECT_EQ(returnEvent, pHwQ->virtualEvent);
    ASSERT_EQ(nullptr, event.peekChildEvents());

    returnEvent->release();
    buffer->decRefInternal();
}

HWTEST_F(CommandQueueHwTest, GivenNonEmptyQueueOnBlockingMapBufferWillWaitForPrecedingCommandsToComplete) {
    struct MockCmdQ : CommandQueueHw<FamilyType> {
        MockCmdQ(Context *context, Device *device)
            : CommandQueueHw<FamilyType>(context, device, 0) {
            finishWasCalled = false;
        }
        cl_int finish(bool dcFlush) override {
            finishWasCalled = true;
            return 0;
        }

        bool finishWasCalled;
    };

    MockCmdQ cmdQ(context, &pCmdQ->getDevice());

    auto b1 = clCreateBuffer(context, CL_MEM_READ_WRITE, 20, nullptr, nullptr);
    auto b2 = clCreateBuffer(context, CL_MEM_READ_WRITE, 20, nullptr, nullptr);

    auto gatingEvent = clCreateUserEvent(context, nullptr);
    void *ptr1 = clEnqueueMapBuffer(&cmdQ, b1, CL_FALSE, CL_MAP_READ, 0, 8, 1, &gatingEvent, nullptr, nullptr);
    clEnqueueUnmapMemObject(&cmdQ, b1, ptr1, 0, nullptr, nullptr);

    ASSERT_FALSE(cmdQ.finishWasCalled);

    void *ptr2 = clEnqueueMapBuffer(&cmdQ, b2, CL_TRUE, CL_MAP_READ, 0, 8, 0, nullptr, nullptr, nullptr);

    ASSERT_TRUE(cmdQ.finishWasCalled);

    clSetUserEventStatus(gatingEvent, CL_COMPLETE);

    clEnqueueUnmapMemObject(pCmdQ, b2, ptr2, 0, nullptr, nullptr);

    clReleaseMemObject(b1);
    clReleaseMemObject(b2);

    clReleaseEvent(gatingEvent);
}

HWTEST_F(CommandQueueHwTest, GivenEventsWaitlistOnBlockingMapBufferWillWaitForEvents) {
    struct MockEvent : UserEvent {
        MockEvent(Context *ctx, uint32_t updateCountBeforeCompleted)
            : UserEvent(ctx),
              updateCount(0), updateCountBeforeCompleted(updateCountBeforeCompleted) {
            this->updateTaskCount(0);
            this->taskLevel = 0;
        }

        void updateExecutionStatus() override {
            ++updateCount;
            if (updateCount == updateCountBeforeCompleted) {
                transitionExecutionStatus(CL_COMPLETE);
            }
            unblockEventsBlockedByThis(executionStatus);
        }

        uint32_t updateCount;
        uint32_t updateCountBeforeCompleted;
    };

    MockEvent *me = new MockEvent(context, 1024);
    auto b1 = clCreateBuffer(context, CL_MEM_READ_WRITE, 20, nullptr, nullptr);
    cl_event meAsClEv = me;
    void *ptr1 = clEnqueueMapBuffer(pCmdQ, b1, CL_TRUE, CL_MAP_READ, 0, 8, 1, &meAsClEv, nullptr, nullptr);
    ASSERT_TRUE(me->updateStatusAndCheckCompletion());
    ASSERT_LE(me->updateCountBeforeCompleted, me->updateCount);

    clEnqueueUnmapMemObject(pCmdQ, b1, ptr1, 0, nullptr, nullptr);
    clReleaseMemObject(b1);
    me->release();
}

HWTEST_F(CommandQueueHwTest, GivenNotCompleteUserEventPassedToEnqueueWhenEventIsUnblockedThenAllSurfacesForBlockedCommandsAreMadeResident) {
    int32_t executionStamp = 0;
    auto mockCSR = new MockCsr<FamilyType>(executionStamp, *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCSR);

    UserEvent userEvent(context);
    KernelInfo kernelInfo;
    MockKernelWithInternals mockKernelWithInternals(*pDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;
    auto mockProgram = mockKernelWithInternals.mockProgram;

    size_t offset = 0;
    size_t size = 1;

    GraphicsAllocation *constantSurface = mockCSR->getMemoryManager()->allocateGraphicsMemory(10);
    mockProgram->setConstantSurface(constantSurface);

    GraphicsAllocation *printfSurface = mockCSR->getMemoryManager()->allocateGraphicsMemory(10);
    GraphicsAllocation *privateSurface = mockCSR->getMemoryManager()->allocateGraphicsMemory(10);

    mockKernel->setPrivateSurface(privateSurface, 10);

    cl_event blockedEvent = &userEvent;
    pCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);

    userEvent.setStatus(CL_COMPLETE);

    EXPECT_TRUE(mockCSR->isMadeResident(constantSurface));
    EXPECT_TRUE(mockCSR->isMadeResident(privateSurface));

    mockKernel->setPrivateSurface(nullptr, 0);
    mockProgram->setConstantSurface(nullptr);

    mockCSR->getMemoryManager()->freeGraphicsMemory(privateSurface);
    mockCSR->getMemoryManager()->freeGraphicsMemory(printfSurface);
    mockCSR->getMemoryManager()->freeGraphicsMemory(constantSurface);
}

typedef CommandQueueHwTest BlockedCommandQueueTest;

HWTEST_F(BlockedCommandQueueTest, givenCommandQueueWhenBlockedCommandIsBeingSubmittedThenQueueHeapsAreNotUsed) {
    UserEvent userEvent(context);
    MockKernelWithInternals mockKernelWithInternals(*pDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;

    size_t offset = 0;
    size_t size = 1;

    cl_event blockedEvent = &userEvent;

    pCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);
    userEvent.setStatus(CL_COMPLETE);

    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 4096u);
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 4096u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 4096u);

    EXPECT_EQ(0u, ioh.getUsed());
    EXPECT_EQ(0u, dsh.getUsed());
    EXPECT_EQ(0u, ssh.getUsed());
}

HWTEST_F(BlockedCommandQueueTest, givenCommandQueueWithUsedHeapsWhenBlockedCommandIsBeingSubmittedThenQueueHeapsAreNotUsed) {
    UserEvent userEvent(context);
    MockKernelWithInternals mockKernelWithInternals(*pDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;

    size_t offset = 0;
    size_t size = 1;

    cl_event blockedEvent = &userEvent;

    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 4096u);
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 4096u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 4096u);

    auto spaceToUse = 4u;

    ioh.getSpace(spaceToUse);
    dsh.getSpace(spaceToUse);
    ssh.getSpace(spaceToUse);

    pCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);
    userEvent.setStatus(CL_COMPLETE);

    EXPECT_EQ(spaceToUse, ioh.getUsed());
    EXPECT_EQ(spaceToUse, dsh.getUsed());
    EXPECT_EQ(spaceToUse, ssh.getUsed());
}

HWTEST_F(BlockedCommandQueueTest, givenCommandQueueWhichHasSomeUnusedHeapsWhenBlockedCommandIsBeingSubmittedThenThoseHeapsAreBeingUsed) {
    UserEvent userEvent(context);
    MockKernelWithInternals mockKernelWithInternals(*pDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;

    size_t offset = 0;
    size_t size = 1;

    cl_event blockedEvent = &userEvent;

    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 4096u);
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 4096u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 4096u);

    auto iohBase = ioh.getCpuBase();
    auto dshBase = dsh.getCpuBase();
    auto sshBase = ssh.getCpuBase();

    pCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);
    userEvent.setStatus(CL_COMPLETE);

    EXPECT_EQ(iohBase, ioh.getCpuBase());
    EXPECT_EQ(dshBase, dsh.getCpuBase());
    EXPECT_EQ(sshBase, ssh.getCpuBase());
}

HWTEST_F(BlockedCommandQueueTest, givenEnqueueBlockedByUserEventWhenItIsEnqueuedThenKernelReferenceCountIsIncreased) {
    UserEvent userEvent(context);
    MockKernelWithInternals mockKernelWithInternals(*pDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;

    size_t offset = 0;
    size_t size = 1;

    cl_event blockedEvent = &userEvent;

    auto currentRefCount = mockKernel->getRefInternalCount();
    pCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);
    EXPECT_EQ(currentRefCount + 1, mockKernel->getRefInternalCount());
    userEvent.setStatus(CL_COMPLETE);
    EXPECT_EQ(currentRefCount, mockKernel->getRefInternalCount());
}

typedef CommandQueueHwTest CommandQueueHwRefCountTest;

HWTEST_F(CommandQueueHwRefCountTest, givenBlockedCmdQWhenNewBlockedEnqueueReplacesVirtualEventThenPreviousVirtualEventDecrementsCmdQRefCount) {
    cl_int retVal = 0;
    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pDevice, 0);

    UserEvent userEvent(context);
    MockKernelWithInternals mockKernelWithInternals(*pDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;

    size_t offset = 0;
    size_t size = 1;

    cl_event blockedEvent = &userEvent;

    EXPECT_EQ(1, mockCmdQ->getRefInternalCount());
    mockCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);

    // UserEvent on waitlist doesn't increments cmdQ refCount, virtualEvent increments refCount
    EXPECT_EQ(2, mockCmdQ->getRefInternalCount());

    mockCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);

    // new virtual event increments refCount
    EXPECT_EQ(3, mockCmdQ->getRefInternalCount());

    userEvent.setStatus(CL_COMPLETE);
    // UserEvent is set to complete and event tree is unblocked, queue has only 1 refference to itself after this operation
    EXPECT_EQ(1, mockCmdQ->getRefInternalCount());

    //this call will release the queue
    releaseQueue<CommandQueue>(mockCmdQ, retVal);
}

HWTEST_F(CommandQueueHwRefCountTest, givenBlockedCmdQWithOutputEventAsVirtualEventWhenNewBlockedEnqueueReplacesVirtualEventCreatedFromOutputEventThenPreviousVirtualEventDoesntDecrementRefCount) {
    cl_int retVal = 0;
    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pDevice, 0);

    UserEvent userEvent(context);
    MockKernelWithInternals mockKernelWithInternals(*pDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;

    size_t offset = 0;
    size_t size = 1;

    cl_event eventOut = nullptr;
    cl_event blockedEvent = &userEvent;

    EXPECT_EQ(1, mockCmdQ->getRefInternalCount());
    mockCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);

    // UserEvent on waitlist doesn't increment cmdQ refCount, virtualEvent increments refCount
    EXPECT_EQ(2, mockCmdQ->getRefInternalCount());

    mockCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, &eventOut);

    //output event increments
    EXPECT_EQ(3, mockCmdQ->getRefInternalCount());

    mockCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);

    // previous virtualEvent which was outputEvent DOES NOT decrement refCount,
    // new virtual event increments refCount
    EXPECT_EQ(4, mockCmdQ->getRefInternalCount());

    // unblocking deletes 2 virtualEvents
    userEvent.setStatus(CL_COMPLETE);

    EXPECT_EQ(2, mockCmdQ->getRefInternalCount());

    auto pEventOut = castToObject<Event>(eventOut);
    pEventOut->release();
    // releasing output event decrements refCount
    EXPECT_EQ(1, mockCmdQ->getRefInternalCount());

    releaseQueue<CommandQueue>(mockCmdQ, retVal);
}

HWTEST_F(CommandQueueHwRefCountTest, givenSeriesOfBlockedEnqueuesWhenEveryEventIsDeletedAndCmdQIsReleasedThenCmdQIsDeleted) {
    cl_int retVal = 0;
    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pDevice, 0);

    UserEvent *userEvent = new UserEvent(context);
    MockKernelWithInternals mockKernelWithInternals(*pDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;

    size_t offset = 0;
    size_t size = 1;

    cl_event eventOut = nullptr;
    cl_event blockedEvent = userEvent;

    EXPECT_EQ(1, mockCmdQ->getRefInternalCount());
    mockCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);

    // UserEvent on waitlist doesn't increment cmdQ refCount, virtualEvent increments refCount
    EXPECT_EQ(2, mockCmdQ->getRefInternalCount());

    mockCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, &eventOut);

    //output event increments refCount
    EXPECT_EQ(3, mockCmdQ->getRefInternalCount());

    mockCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);

    // previous virtualEvent which was outputEvent DOES NOT decrement refCount,
    // new virtual event increments refCount
    EXPECT_EQ(4, mockCmdQ->getRefInternalCount());

    // unblocking deletes 2 virtualEvents
    userEvent->setStatus(CL_COMPLETE);

    userEvent->release();
    // releasing UserEvent doesn't change the refCount
    EXPECT_EQ(2, mockCmdQ->getRefInternalCount());

    auto pEventOut = castToObject<Event>(eventOut);
    pEventOut->release();

    // releasing output event decrements refCount
    EXPECT_EQ(1, mockCmdQ->getRefInternalCount());

    releaseQueue<CommandQueue>(mockCmdQ, retVal);
}

HWTEST_F(CommandQueueHwRefCountTest, givenSeriesOfBlockedEnqueuesWhenCmdQIsReleasedBeforeOutputEventThenOutputEventDeletesCmdQ) {
    cl_int retVal = 0;
    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pDevice, 0);

    UserEvent *userEvent = new UserEvent(context);
    MockKernelWithInternals mockKernelWithInternals(*pDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;

    size_t offset = 0;
    size_t size = 1;

    cl_event eventOut = nullptr;
    cl_event blockedEvent = userEvent;

    EXPECT_EQ(1, mockCmdQ->getRefInternalCount());
    mockCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);

    // UserEvent on waitlist doesn't increment cmdQ refCount, virtualEvent increments refCount
    EXPECT_EQ(2, mockCmdQ->getRefInternalCount());

    mockCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, &eventOut);

    //output event increments refCount
    EXPECT_EQ(3, mockCmdQ->getRefInternalCount());

    mockCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);

    // previous virtualEvent which was outputEvent DOES NOT decrement refCount,
    // new virtual event increments refCount
    EXPECT_EQ(4, mockCmdQ->getRefInternalCount());

    userEvent->setStatus(CL_COMPLETE);

    userEvent->release();
    // releasing UserEvent doesn't change the queue refCount
    EXPECT_EQ(2, mockCmdQ->getRefInternalCount());

    releaseQueue<CommandQueue>(mockCmdQ, retVal);

    // releasing cmdQ decrements refCount
    EXPECT_EQ(1, mockCmdQ->getRefInternalCount());

    auto pEventOut = castToObject<Event>(eventOut);
    pEventOut->release();
}

HWTEST_F(CommandQueueHwTest, GivenEventThatIsNotCompletedWhenFinishIsCalledAndItGetsCompletedThenItStatusIsUpdatedAfterFinishCall) {
    cl_int ret;
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableAsyncEventsHandler.set(false);

    struct ClbFuncTempStruct {
        static void CL_CALLBACK ClbFuncT(cl_event e, cl_int execStatus, void *valueForUpdate) {
            *((cl_int *)valueForUpdate) = 1;
        }
    };
    auto Value = 0u;

    auto ev = new Event(this->pCmdQ, CL_COMMAND_COPY_BUFFER, 3, Event::eventNotReady + 1);
    clSetEventCallback(ev, CL_COMPLETE, ClbFuncTempStruct::ClbFuncT, &Value);

    auto &csr = this->pCmdQ->getDevice().getCommandStreamReceiver();
    EXPECT_GT(3u, csr.peekTaskCount());
    *csr.getTagAddress() = Event::eventNotReady + 1;
    ret = clFinish(this->pCmdQ);
    ASSERT_EQ(CL_SUCCESS, ret);

    ev->updateExecutionStatus();
    EXPECT_EQ(1u, Value);
    ev->decRefInternal();
}

void CloneMdi(MultiDispatchInfo &dst, const MultiDispatchInfo &src) {
    for (auto &srcDi : src) {
        dst.push(srcDi);
    }
}

struct MockBuilder : BuiltinDispatchInfoBuilder {
    MockBuilder(OCLRT::BuiltIns &builtins) : BuiltinDispatchInfoBuilder(builtins) {
    }
    bool buildDispatchInfos(MultiDispatchInfo &d, const BuiltinOpParams &conf) const override {
        wasBuildDispatchInfosWithBuiltinOpParamsCalled = true;
        return true;
    }
    bool buildDispatchInfos(MultiDispatchInfo &d, Kernel *kernel,
                            const uint32_t dim, const Vec3<size_t> &gws, const Vec3<size_t> &elws, const Vec3<size_t> &offset) const override {
        paramsReceived.kernel = kernel;
        paramsReceived.gws = gws;
        paramsReceived.elws = elws;
        paramsReceived.offset = offset;
        wasBuildDispatchInfosWithKernelParamsCalled = true;

        DispatchInfoBuilder<OCLRT::SplitDispatch::Dim::d3D, OCLRT::SplitDispatch::SplitMode::NoSplit> dib;
        dib.setKernel(paramsToUse.kernel);
        dib.setDispatchGeometry(dim, paramsToUse.gws, paramsToUse.elws, paramsToUse.offset);
        dib.bake(d);

        CloneMdi(paramsReceived.multiDispatchInfo, d);
        return true;
    }

    mutable bool wasBuildDispatchInfosWithBuiltinOpParamsCalled = false;
    mutable bool wasBuildDispatchInfosWithKernelParamsCalled = false;
    struct Params {
        MultiDispatchInfo multiDispatchInfo;
        Kernel *kernel = nullptr;
        Vec3<size_t> gws = Vec3<size_t>{0, 0, 0};
        Vec3<size_t> elws = Vec3<size_t>{0, 0, 0};
        Vec3<size_t> offset = Vec3<size_t>{0, 0, 0};
    };

    mutable Params paramsReceived;
    Params paramsToUse;
};

HWTEST_F(CommandQueueHwTest, givenCommandQueueThatIsBlockedAndUsesCpuCopyWhenEventIsReturnedItIsNotReady) {
    CommandQueueHw<FamilyType> *cmdQHw = static_cast<CommandQueueHw<FamilyType> *>(this->pCmdQ);
    MockBuffer buffer;
    cl_event returnEvent = nullptr;
    auto retVal = CL_SUCCESS;
    cmdQHw->taskLevel = Event::eventNotReady;
    size_t offset = 0;
    size_t size = 4096u;
    TransferProperties transferProperties(&buffer, CL_COMMAND_READ_BUFFER, 0, false, &offset, &size, nullptr);
    EventsRequest eventsRequest(0, nullptr, &returnEvent);
    cmdQHw->cpuDataTransferHandler(transferProperties, eventsRequest, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(Event::eventNotReady, castToObject<Event>(returnEvent)->peekTaskCount());
    clReleaseEvent(returnEvent);
}

HWTEST_F(CommandQueueHwTest, givenEventWithRecordedCommandWhenSubmitCommandIsCalledThenTaskCountIsNotUpdated) {
    struct mockEvent : public Event {
        using Event::Event;
        using Event::eventWithoutCommand;
        using Event::submitCommand;
    };

    mockEvent neoEvent(this->pCmdQ, CL_COMMAND_MAP_BUFFER, Event::eventNotReady, Event::eventNotReady);
    EXPECT_TRUE(neoEvent.eventWithoutCommand);
    neoEvent.eventWithoutCommand = false;

    neoEvent.submitCommand(false);
    EXPECT_EQ(Event::eventNotReady, neoEvent.peekTaskCount());
}

HWTEST_F(CommandQueueHwTest, GivenBuiltinKernelWhenBuiltinDispatchInfoBuilderIsProvidedThenThisBuilderIsUsedForCreatingDispatchInfo) {
    CommandQueueHw<FamilyType> *cmdQHw = static_cast<CommandQueueHw<FamilyType> *>(this->pCmdQ);

    MockKernelWithInternals mockKernelToUse(*pDevice);
    MockBuilder builder(*pDevice->getExecutionEnvironment()->getBuiltIns());
    builder.paramsToUse.gws.x = 11;
    builder.paramsToUse.elws.x = 13;
    builder.paramsToUse.offset.x = 17;
    builder.paramsToUse.kernel = mockKernelToUse.mockKernel;

    MockKernelWithInternals mockKernelToSend(*pDevice);
    mockKernelToSend.kernelInfo.builtinDispatchBuilder = &builder;
    NullSurface s;
    Surface *surfaces[] = {&s};
    size_t gws[3] = {3, 0, 0};
    size_t lws[3] = {5, 0, 0};
    size_t off[3] = {7, 0, 0};

    EXPECT_FALSE(builder.wasBuildDispatchInfosWithBuiltinOpParamsCalled);
    EXPECT_FALSE(builder.wasBuildDispatchInfosWithKernelParamsCalled);
    cmdQHw->template enqueueHandler<CL_COMMAND_NDRANGE_KERNEL>(surfaces, false, mockKernelToSend.mockKernel, 1, off, gws, lws, 0, nullptr, nullptr);
    EXPECT_FALSE(builder.wasBuildDispatchInfosWithBuiltinOpParamsCalled);
    EXPECT_TRUE(builder.wasBuildDispatchInfosWithKernelParamsCalled);

    EXPECT_EQ(Vec3<size_t>(gws[0], gws[1], gws[2]), builder.paramsReceived.gws);
    EXPECT_EQ(Vec3<size_t>(lws[0], lws[1], lws[2]), builder.paramsReceived.elws);
    EXPECT_EQ(Vec3<size_t>(off[0], off[1], off[2]), builder.paramsReceived.offset);
    EXPECT_EQ(mockKernelToSend.mockKernel, builder.paramsReceived.kernel);

    auto dispatchInfo = builder.paramsReceived.multiDispatchInfo.begin();
    EXPECT_EQ(1U, builder.paramsReceived.multiDispatchInfo.size());
    EXPECT_EQ(builder.paramsToUse.gws.x, dispatchInfo->getGWS().x);
    EXPECT_EQ(builder.paramsToUse.elws.x, dispatchInfo->getEnqueuedWorkgroupSize().x);
    EXPECT_EQ(builder.paramsToUse.offset.x, dispatchInfo->getOffset().x);
    EXPECT_EQ(builder.paramsToUse.kernel, dispatchInfo->getKernel());
}

HWTEST_F(CommandQueueHwTest, givenNonBlockedEnqueueWhenEventIsPassedThenUpdateItsFlushStamp) {
    CommandQueueHw<FamilyType> *cmdQHw = static_cast<CommandQueueHw<FamilyType> *>(this->pCmdQ);
    MockKernelWithInternals mockKernelWithInternals(*pDevice);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.flushStamp->setStamp(5);

    size_t offset = 0;
    size_t size = 1;

    cl_event event;
    auto retVal = cmdQHw->enqueueKernel(mockKernelWithInternals.mockKernel, 1, &offset, &size, nullptr, 0, nullptr, &event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto eventObj = castToObject<Event>(event);
    EXPECT_EQ(csr.flushStamp->peekStamp(), eventObj->flushStamp->peekStamp());
    EXPECT_EQ(csr.flushStamp->peekStamp(), pCmdQ->flushStamp->peekStamp());
    eventObj->release();
}

HWTEST_F(CommandQueueHwTest, givenBlockedEnqueueWhenEventIsPassedThenDontUpdateItsFlushStamp) {
    UserEvent userEvent;
    cl_event event, clUserEvent;
    CommandQueueHw<FamilyType> *cmdQHw = static_cast<CommandQueueHw<FamilyType> *>(this->pCmdQ);
    MockKernelWithInternals mockKernelWithInternals(*pDevice);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.flushStamp->setStamp(5);

    size_t offset = 0;
    size_t size = 1;

    clUserEvent = &userEvent;
    auto retVal = cmdQHw->enqueueKernel(mockKernelWithInternals.mockKernel, 1, &offset, &size, nullptr, 1, &clUserEvent, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(cmdQHw->isQueueBlocked());

    retVal = cmdQHw->enqueueKernel(mockKernelWithInternals.mockKernel, 1, &offset, &size, nullptr, 0, nullptr, &event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    FlushStamp expectedFlushStamp = 0;
    auto eventObj = castToObject<Event>(event);
    EXPECT_EQ(expectedFlushStamp, eventObj->flushStamp->peekStamp());
    EXPECT_EQ(expectedFlushStamp, pCmdQ->flushStamp->peekStamp());

    eventObj->release();
}

HWTEST_F(CommandQueueHwTest, givenBlockedInOrderCmdQueueAndAsynchronouslyCompletedEventWhenEnqueueCompletesVirtualEventThenUpdatedTaskLevelIsPassedToEnqueueAndFlushTask) {
    CommandQueueHw<FamilyType> *cmdQHw = static_cast<CommandQueueHw<FamilyType> *>(this->pCmdQ);

    int32_t executionStamp = 0;
    auto mockCSR = new MockCsr<FamilyType>(executionStamp, *pDevice->executionEnvironment);

    pDevice->resetCommandStreamReceiver(mockCSR);

    MockKernelWithInternals mockKernelWithInternals(*pDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;
    size_t offset = 0;
    size_t size = 1;

    class MockEventWithSetCompleteOnUpdate : public Event {
      public:
        MockEventWithSetCompleteOnUpdate(CommandQueue *cmdQueue, cl_command_type cmdType,
                                         uint32_t taskLevel, uint32_t taskCount) : Event(cmdQueue, cmdType, taskLevel, taskCount) {
        }
        void updateExecutionStatus() override {
            setStatus(CL_COMPLETE);
        }
    };

    auto event = new Event(cmdQHw, CL_COMMAND_NDRANGE_KERNEL, 10, 0);

    uint32_t virtualEventTaskLevel = 77;
    uint32_t virtualEventTaskCount = 80;
    auto virtualEvent = new MockEventWithSetCompleteOnUpdate(cmdQHw, CL_COMMAND_NDRANGE_KERNEL, virtualEventTaskLevel, virtualEventTaskCount);
    virtualEvent->setStatus(CL_SUBMITTED);

    cl_event blockedEvent = event;

    // Put Queue in blocked state by assigning virtualEvent
    event->addChild(*virtualEvent);
    virtualEvent->incRefInternal();
    cmdQHw->virtualEvent = virtualEvent;

    cmdQHw->taskLevel = 23;
    cmdQHw->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);
    //new virtual event is created on enqueue, bind it to the created virtual event
    EXPECT_NE(cmdQHw->virtualEvent, virtualEvent);

    event->setStatus(CL_SUBMITTED);

    virtualEvent->Event::updateExecutionStatus();
    EXPECT_FALSE(cmdQHw->isQueueBlocked());
    // +1 for next level after virtualEvent is unblocked
    // +1 as virtualEvent was a parent for event with actual command that is being submitted
    EXPECT_EQ(virtualEventTaskLevel + 2, cmdQHw->taskLevel);
    //command being submitted was dependant only on virtual event hence only +1
    EXPECT_EQ(virtualEventTaskLevel + 1, mockCSR->lastTaskLevelToFlushTask);
    virtualEvent->decRefInternal();
    event->decRefInternal();
}

HWTEST_F(OOQueueHwTest, givenBlockedOutOfOrderCmdQueueAndAsynchronouslyCompletedEventWhenEnqueueCompletesVirtualEventThenUpdatedTaskLevelIsPassedToEnqueueAndFlushTask) {
    CommandQueueHw<FamilyType> *cmdQHw = static_cast<CommandQueueHw<FamilyType> *>(this->pCmdQ);

    int32_t executionStamp = 0;
    auto mockCSR = new MockCsr<FamilyType>(executionStamp, *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCSR);

    MockKernelWithInternals mockKernelWithInternals(*pDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;
    size_t offset = 0;
    size_t size = 1;

    class MockEventWithSetCompleteOnUpdate : public Event {
      public:
        MockEventWithSetCompleteOnUpdate(CommandQueue *cmdQueue, cl_command_type cmdType,
                                         uint32_t taskLevel, uint32_t taskCount) : Event(cmdQueue, cmdType, taskLevel, taskCount) {
        }
        void updateExecutionStatus() override {
            setStatus(CL_COMPLETE);
        }
    };

    Event event(cmdQHw, CL_COMMAND_NDRANGE_KERNEL, 10, 0);

    uint32_t virtualEventTaskLevel = 77;
    uint32_t virtualEventTaskCount = 80;
    MockEventWithSetCompleteOnUpdate virtualEvent(cmdQHw, CL_COMMAND_NDRANGE_KERNEL, virtualEventTaskLevel, virtualEventTaskCount);
    virtualEvent.setStatus(CL_SUBMITTED);

    cl_event blockedEvent = &event;

    // Put Queue in blocked state by assigning virtualEvent
    virtualEvent.incRefInternal();
    event.addChild(virtualEvent);
    cmdQHw->virtualEvent = &virtualEvent;

    cmdQHw->taskLevel = 23;
    cmdQHw->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);
    //new virtual event is created on enqueue, bind it to the created virtual event
    EXPECT_NE(cmdQHw->virtualEvent, &virtualEvent);

    event.setStatus(CL_SUBMITTED);

    virtualEvent.Event::updateExecutionStatus();
    EXPECT_FALSE(cmdQHw->isQueueBlocked());

    //+1 due to dependency between virtual event & new virtual event
    //new virtual event is actually responsible for command delivery
    EXPECT_EQ(virtualEventTaskLevel + 1, cmdQHw->taskLevel);
    EXPECT_EQ(virtualEventTaskLevel + 1, mockCSR->lastTaskLevelToFlushTask);
}

HWTEST_F(CommandQueueHwTest, givenWalkerSplitEnqueueNDRangeWhenNoBlockedThenKernelMakeResidentCalledOnce) {
    KernelInfo kernelInfo;
    MockKernelWithInternals mockKernelWithInternals(*pDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;
    auto mockProgram = mockKernelWithInternals.mockProgram;
    mockProgram->setAllowNonUniform(true);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.storeMakeResidentAllocations = true;

    size_t offset = 0;
    size_t gws = 63;
    size_t lws = 16;

    cl_int status = pCmdQ->enqueueKernel(mockKernel, 1, &offset, &gws, &lws, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_EQ(1u, mockKernel->makeResidentCalls);

    std::map<GraphicsAllocation *, uint32_t>::iterator it = csr.makeResidentAllocations.begin();
    for (; it != csr.makeResidentAllocations.end(); it++) {
        EXPECT_EQ(1u, it->second);
    }
}

HWTEST_F(CommandQueueHwTest, givenWalkerSplitEnqueueNDRangeWhenBlockedThenKernelGetResidencyCalledOnce) {
    UserEvent userEvent(context);
    KernelInfo kernelInfo;
    MockKernelWithInternals mockKernelWithInternals(*pDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;
    auto mockProgram = mockKernelWithInternals.mockProgram;
    mockProgram->setAllowNonUniform(true);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.storeMakeResidentAllocations = true;

    size_t offset = 0;
    size_t gws = 63;
    size_t lws = 16;

    cl_event blockedEvent = &userEvent;

    cl_int status = pCmdQ->enqueueKernel(mockKernel, 1, &offset, &gws, &lws, 1, &blockedEvent, nullptr);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_EQ(1u, mockKernel->getResidencyCalls);

    userEvent.setStatus(CL_COMPLETE);

    std::map<GraphicsAllocation *, uint32_t>::iterator it = csr.makeResidentAllocations.begin();
    for (; it != csr.makeResidentAllocations.end(); it++) {
        EXPECT_EQ(1u, it->second);
    }
}

HWTEST_F(CommandQueueHwTest, givenKernelSplitEnqueueReadBufferWhenBlockedThenEnqueueSurfacesMakeResidentIsCalledOnce) {
    UserEvent userEvent(context);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.storeMakeResidentAllocations = true;

    BufferDefaults::context = context;
    std::unique_ptr<Buffer> buffer(BufferHelper<>::create());
    GraphicsAllocation *bufferAllocation = buffer->getGraphicsAllocation();
    char array[3 * MemoryConstants::cacheLineSize];
    char *ptr = &array[MemoryConstants::cacheLineSize];
    ptr = alignUp(ptr, MemoryConstants::cacheLineSize);
    ptr -= 1;

    cl_event blockedEvent = &userEvent;

    cl_int status = pCmdQ->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, MemoryConstants::cacheLineSize, ptr, 1, &blockedEvent, nullptr);
    EXPECT_EQ(CL_SUCCESS, status);

    userEvent.setStatus(CL_COMPLETE);

    std::map<GraphicsAllocation *, uint32_t>::iterator it = csr.makeResidentAllocations.begin();
    for (; it != csr.makeResidentAllocations.end(); it++) {
        uint32_t expected = 1u;
        //Buffer surface will be added three times (for each kernel from split and as a base range of enqueueReadBuffer call)
        if (it->first == bufferAllocation) {
            expected = 3u;
        }
        EXPECT_EQ(expected, it->second);
    }
}

HWTEST_F(CommandQueueHwTest, givenReadOnlyHostPointerWhenAllocationForHostSurfaceWithPtrCopyAllowedIsCreatedThenCopyAllocationIsCreatedAndMemoryCopied) {
    ::testing::NiceMock<GMockMemoryManager> *gmockMemoryManager = new ::testing::NiceMock<GMockMemoryManager>;
    ASSERT_NE(nullptr, gmockMemoryManager);

    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    ASSERT_NE(nullptr, device.get());
    device->injectMemoryManager(gmockMemoryManager);
    MockContext *mockContext = new MockContext(device.get());
    ASSERT_NE(nullptr, mockContext);

    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(mockContext, device.get(), 0);
    ASSERT_NE(nullptr, mockCmdQ);

    const char memory[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    size_t size = sizeof(memory);
    HostPtrSurface surface(const_cast<char *>(memory), size, true);

    if (mockCmdQ->isFullRangeSvm()) {
        EXPECT_CALL(*gmockMemoryManager, populateOsHandles(::testing::_))
            .Times(1)
            .WillOnce(::testing::Return(MemoryManager::AllocationStatus::InvalidHostPointer));
    } else {
        EXPECT_CALL(*gmockMemoryManager, allocateGraphicsMemoryForNonSvmHostPtr(::testing::_, ::testing::_))
            .Times(1)
            .WillOnce(::testing::Return(nullptr));
    }

    bool result = mockCmdQ->createAllocationForHostSurface(surface);
    EXPECT_TRUE(result);

    auto allocation = surface.getAllocation();
    ASSERT_NE(nullptr, allocation);

    EXPECT_NE(memory, allocation->getUnderlyingBuffer());
    EXPECT_THAT(allocation->getUnderlyingBuffer(), MemCompare(memory, size));

    gmockMemoryManager->cleanAllocationList(-1, TEMPORARY_ALLOCATION);
    mockCmdQ->release();
    mockContext->release();
}

HWTEST_F(CommandQueueHwTest, givenReadOnlyHostPointerWhenAllocationForHostSurfaceWithPtrCopyNotAllowedIsCreatedThenCopyAllocationIsNotCreated) {
    ::testing::NiceMock<GMockMemoryManager> *gmockMemoryManager = new ::testing::NiceMock<GMockMemoryManager>;
    ASSERT_NE(nullptr, gmockMemoryManager);

    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    ASSERT_NE(nullptr, device.get());
    device->injectMemoryManager(gmockMemoryManager);
    MockContext *mockContext = new MockContext(device.get());
    ASSERT_NE(nullptr, mockContext);

    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(mockContext, device.get(), 0);
    ASSERT_NE(nullptr, mockCmdQ);

    const char memory[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    size_t size = sizeof(memory);
    HostPtrSurface surface(const_cast<char *>(memory), size, false);

    if (mockCmdQ->isFullRangeSvm()) {
        EXPECT_CALL(*gmockMemoryManager, populateOsHandles(::testing::_))
            .Times(1)
            .WillOnce(::testing::Return(MemoryManager::AllocationStatus::InvalidHostPointer));
    } else {
        EXPECT_CALL(*gmockMemoryManager, allocateGraphicsMemoryForNonSvmHostPtr(::testing::_, ::testing::_))
            .Times(1)
            .WillOnce(::testing::Return(nullptr));
    }

    bool result = mockCmdQ->createAllocationForHostSurface(surface);
    EXPECT_FALSE(result);

    auto allocation = surface.getAllocation();
    EXPECT_EQ(nullptr, allocation);

    gmockMemoryManager->cleanAllocationList(-1, TEMPORARY_ALLOCATION);
    mockCmdQ->release();
    mockContext->release();
}

struct ReducedAddrSpaceCommandQueueHwTest : public CommandQueueHwTest {
    HardwareInfo hwInfoToModify;
    std::unique_ptr<MockDevice> device;
    MockContext *mockContext = nullptr;
    ::testing::NiceMock<GMockMemoryManager> *gmockMemoryManager = nullptr;

    void SetUp() override {
        CommandQueueHwTest::SetUp();
        hwInfoToModify = *platformDevices[0];
        hwInfoToModify.capabilityTable.gpuAddressSpace = MemoryConstants::max32BitAddress;
        device.reset(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfoToModify));
        ASSERT_NE(nullptr, device.get());
        gmockMemoryManager = new ::testing::NiceMock<GMockMemoryManager>;
        ASSERT_NE(nullptr, gmockMemoryManager);
        device->injectMemoryManager(gmockMemoryManager);
        mockContext = new MockContext(device.get());
        ASSERT_NE(nullptr, mockContext);
    }

    void TearDown() override {
        CommandQueueHwTest::TearDown();
        gmockMemoryManager->cleanAllocationList(-1, TEMPORARY_ALLOCATION);
        mockContext->release();
    }
};

HWTEST_F(ReducedAddrSpaceCommandQueueHwTest,
         givenReducedGpuAddressSpaceWhenAllocationForHostSurfaceIsCreatedThenAllocateGraphicsMemoryForNonSvmHostPtrIsCalled) {

    std::unique_ptr<MockCommandQueueHw<FamilyType>, std::function<void(MockCommandQueueHw<FamilyType> *)>> mockCmdQ(
        new MockCommandQueueHw<FamilyType>(mockContext, device.get(), 0), [](MockCommandQueueHw<FamilyType> *ptr) { ptr->release(); });

    char memory[8] = {};
    HostPtrSurface surface(const_cast<char *>(memory), sizeof(memory), false);

    EXPECT_CALL(*gmockMemoryManager, allocateGraphicsMemoryForNonSvmHostPtr(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(nullptr));

    bool result = mockCmdQ->createAllocationForHostSurface(surface);
    EXPECT_FALSE(result);
}
