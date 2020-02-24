/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/utilities/base_object_utils.h"
#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/helpers/dispatch_info_builder.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/helpers/unit_test_helper.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_builtins.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_csr.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "test.h"

using namespace NEO;

struct CommandQueueHwTest
    : public DeviceFixture,
      public ContextFixture,
      public CommandQueueHwFixture,
      ::testing::Test {

    using ContextFixture::SetUp;

    void SetUp() override {
        DeviceFixture::SetUp();
        cl_device_id device = pClDevice;
        ContextFixture::SetUp(1, &device);
        CommandQueueHwFixture::SetUp(pClDevice, 0);
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
        cl_device_id device = pClDevice;
        ContextFixture::SetUp(1, &device);
        OOQueueFixture::SetUp(pClDevice, 0);
    }

    void SetUp(ClDevice *pDevice, cl_command_queue_properties properties) override {
    }

    void TearDown() override {
        OOQueueFixture::TearDown();
        ContextFixture::TearDown();
        DeviceFixture::TearDown();
    }
};

HWTEST_F(CommandQueueHwTest, WhenEnqueuingBlockedMapUnmapOperationThenVirtualEventIsCreated) {

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

HWTEST_F(CommandQueueHwTest, WhenAddMapUnmapToWaitlistEventsThenDependenciesAreNotAddedIntoChild) {
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

HWTEST_F(CommandQueueHwTest, givenMapCommandWhenZeroStateCommandIsSubmittedThenTaskCountIsNotBeingWaited) {
    auto buffer = new MockBuffer;
    MockCommandQueueHw<FamilyType> mockCmdQueueHw(context, pClDevice, nullptr);

    MockEventBuilder eventBuilder;
    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    mockCmdQueueHw.enqueueBlockedMapUnmapOperation(nullptr,
                                                   0,
                                                   MAP,
                                                   buffer,
                                                   size, offset, false,
                                                   eventBuilder);

    EXPECT_NE(nullptr, mockCmdQueueHw.virtualEvent);
    mockCmdQueueHw.virtualEvent->setStatus(CL_COMPLETE);
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), mockCmdQueueHw.latestTaskCountWaited);

    buffer->decRefInternal();
}

HWTEST_F(CommandQueueHwTest, givenMapCommandWhenZeroStateCommandIsSubmittedOnNonZeroCopyBufferThenTaskCountIsBeingWaited) {
    auto buffer = new MockBuffer;
    buffer->isZeroCopy = false;
    MockCommandQueueHw<FamilyType> mockCmdQueueHw(context, pClDevice, nullptr);

    MockEventBuilder eventBuilder;
    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    mockCmdQueueHw.enqueueBlockedMapUnmapOperation(nullptr,
                                                   0,
                                                   MAP,
                                                   buffer,
                                                   size, offset, false,
                                                   eventBuilder);

    EXPECT_NE(nullptr, mockCmdQueueHw.virtualEvent);
    mockCmdQueueHw.virtualEvent->setStatus(CL_COMPLETE);
    EXPECT_EQ(1u, mockCmdQueueHw.latestTaskCountWaited);

    buffer->decRefInternal();
}

HWTEST_F(CommandQueueHwTest, GivenEventWhenEnqueuingBlockedMapUnmapOperationThenEventIsRetained) {
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
    delete returnEvent;
    buffer->decRefInternal();
}

HWTEST_F(CommandQueueHwTest, GivenEventWhenEnqueuingBlockedMapUnmapOperationThenChildIsUnaffected) {
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
        MockCmdQ(Context *context, ClDevice *device)
            : CommandQueueHw<FamilyType>(context, device, 0, false) {
            finishWasCalled = false;
        }
        cl_int finish() override {
            finishWasCalled = true;
            return 0;
        }

        bool finishWasCalled;
    };

    MockCmdQ cmdQ(context, pCmdQ->getDevice().getSpecializedDevice<ClDevice>());

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
    auto mockCSR = new MockCsr<FamilyType>(executionStamp, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex());
    pDevice->resetCommandStreamReceiver(mockCSR);

    auto userEvent = make_releaseable<UserEvent>(context);
    KernelInfo kernelInfo;
    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;
    auto mockProgram = mockKernelWithInternals.mockProgram;

    size_t offset = 0;
    size_t size = 1;

    GraphicsAllocation *constantSurface = mockCSR->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    mockProgram->setConstantSurface(constantSurface);

    GraphicsAllocation *printfSurface = mockCSR->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    GraphicsAllocation *privateSurface = mockCSR->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    mockKernel->setPrivateSurface(privateSurface, 10);

    cl_event blockedEvent = userEvent.get();
    pCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);

    userEvent->setStatus(CL_COMPLETE);

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
    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;

    size_t offset = 0;
    size_t size = 1;

    cl_event blockedEvent = &userEvent;

    pCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);
    userEvent.setStatus(CL_COMPLETE);

    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 4096u);
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 4096u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 4096u);

    uint32_t defaultSshUse = UnitTestHelper<FamilyType>::getDefaultSshUsage();

    EXPECT_EQ(0u, ioh.getUsed());
    EXPECT_EQ(0u, dsh.getUsed());
    EXPECT_EQ(defaultSshUse, ssh.getUsed());

    pCmdQ->isQueueBlocked();
}

HWTEST_F(BlockedCommandQueueTest, givenCommandQueueWithUsedHeapsWhenBlockedCommandIsBeingSubmittedThenQueueHeapsAreNotUsed) {
    UserEvent userEvent(context);
    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
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

    uint32_t sshSpaceUse = spaceToUse + UnitTestHelper<FamilyType>::getDefaultSshUsage();

    EXPECT_EQ(spaceToUse, ioh.getUsed());
    EXPECT_EQ(spaceToUse, dsh.getUsed());
    EXPECT_EQ(sshSpaceUse, ssh.getUsed());

    pCmdQ->isQueueBlocked();
}

HWTEST_F(BlockedCommandQueueTest, givenCommandQueueWhichHasSomeUnusedHeapsWhenBlockedCommandIsBeingSubmittedThenThoseHeapsAreBeingUsed) {
    UserEvent userEvent(context);
    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
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

    pCmdQ->isQueueBlocked();
}

HWTEST_F(BlockedCommandQueueTest, givenEnqueueBlockedByUserEventWhenItIsEnqueuedThenKernelReferenceCountIsIncreased) {
    UserEvent userEvent(context);
    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;

    size_t offset = 0;
    size_t size = 1;

    cl_event blockedEvent = &userEvent;

    auto currentRefCount = mockKernel->getRefInternalCount();
    pCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);
    EXPECT_EQ(currentRefCount + 1, mockKernel->getRefInternalCount());
    userEvent.setStatus(CL_COMPLETE);
    pCmdQ->isQueueBlocked();
    EXPECT_EQ(currentRefCount, mockKernel->getRefInternalCount());
}

typedef CommandQueueHwTest CommandQueueHwRefCountTest;

HWTEST_F(CommandQueueHwRefCountTest, givenBlockedCmdQWhenNewBlockedEnqueueReplacesVirtualEventThenPreviousVirtualEventDecrementsCmdQRefCount) {
    cl_int retVal = 0;
    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pClDevice, 0);

    UserEvent userEvent(context);
    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
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
    EXPECT_EQ(2, mockCmdQ->getRefInternalCount());

    //this call will release the queue
    releaseQueue<CommandQueue>(mockCmdQ, retVal);
}

HWTEST_F(CommandQueueHwRefCountTest, givenBlockedCmdQWithOutputEventAsVirtualEventWhenNewBlockedEnqueueReplacesVirtualEventCreatedFromOutputEventThenPreviousVirtualEventDoesntDecrementRefCount) {
    cl_int retVal = 0;
    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pClDevice, 0);

    UserEvent userEvent(context);
    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
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

    EXPECT_EQ(3, mockCmdQ->getRefInternalCount());

    auto pEventOut = castToObject<Event>(eventOut);
    pEventOut->release();
    // releasing output event decrements refCount
    EXPECT_EQ(2, mockCmdQ->getRefInternalCount());
    mockCmdQ->isQueueBlocked();

    releaseQueue<CommandQueue>(mockCmdQ, retVal);
}

HWTEST_F(CommandQueueHwRefCountTest, givenSeriesOfBlockedEnqueuesWhenEveryEventIsDeletedAndCmdQIsReleasedThenCmdQIsDeleted) {
    cl_int retVal = 0;
    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pClDevice, 0);

    UserEvent *userEvent = new UserEvent(context);
    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
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
    EXPECT_EQ(3, mockCmdQ->getRefInternalCount());

    auto pEventOut = castToObject<Event>(eventOut);
    pEventOut->release();

    // releasing output event decrements refCount
    EXPECT_EQ(2, mockCmdQ->getRefInternalCount());

    mockCmdQ->isQueueBlocked();

    EXPECT_EQ(1, mockCmdQ->getRefInternalCount());

    releaseQueue<CommandQueue>(mockCmdQ, retVal);
}

HWTEST_F(CommandQueueHwRefCountTest, givenSeriesOfBlockedEnqueuesWhenCmdQIsReleasedBeforeOutputEventThenOutputEventDeletesCmdQ) {
    cl_int retVal = 0;
    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pClDevice, 0);

    UserEvent *userEvent = new UserEvent(context);
    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
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
    EXPECT_EQ(3, mockCmdQ->getRefInternalCount());

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

    auto ev = new Event(this->pCmdQ, CL_COMMAND_COPY_BUFFER, 3, CompletionStamp::levelNotReady + 1);
    clSetEventCallback(ev, CL_COMPLETE, ClbFuncTempStruct::ClbFuncT, &Value);

    auto &csr = this->pCmdQ->getGpgpuCommandStreamReceiver();
    EXPECT_GT(3u, csr.peekTaskCount());
    *csr.getTagAddress() = CompletionStamp::levelNotReady + 1;
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
    MockBuilder(NEO::BuiltIns &builtins) : BuiltinDispatchInfoBuilder(builtins) {
    }
    bool buildDispatchInfos(MultiDispatchInfo &d, const BuiltinOpParams &conf) const override {
        wasBuildDispatchInfosWithBuiltinOpParamsCalled = true;
        paramsReceived.multiDispatchInfo.setBuiltinOpParams(conf);
        return true;
    }
    bool buildDispatchInfos(MultiDispatchInfo &d, Kernel *kernel,
                            const uint32_t dim, const Vec3<size_t> &gws, const Vec3<size_t> &elws, const Vec3<size_t> &offset) const override {
        paramsReceived.kernel = kernel;
        paramsReceived.gws = gws;
        paramsReceived.elws = elws;
        paramsReceived.offset = offset;
        wasBuildDispatchInfosWithKernelParamsCalled = true;

        DispatchInfoBuilder<NEO::SplitDispatch::Dim::d3D, NEO::SplitDispatch::SplitMode::NoSplit> dib;
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

struct BuiltinParamsCommandQueueHwTests : public CommandQueueHwTest {

    void SetUpImpl(EBuiltInOps::Type operation) {
        auto builtIns = new MockBuiltins();
        pCmdQ->getDevice().getExecutionEnvironment()->builtins.reset(builtIns);

        auto swapBuilder = builtIns->setBuiltinDispatchInfoBuilder(
            operation,
            *pContext,
            *pDevice,
            std::unique_ptr<NEO::BuiltinDispatchInfoBuilder>(new MockBuilder(*builtIns)));

        mockBuilder = static_cast<MockBuilder *>(&BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(
            operation,
            *pDevice));
    }

    MockBuilder *mockBuilder;
};

HWTEST_F(BuiltinParamsCommandQueueHwTests, givenEnqueueReadWriteBufferCallWhenBuiltinParamsArePassedThenCheckValuesCorectness) {

    SetUpImpl(EBuiltInOps::CopyBufferToBuffer);
    BufferDefaults::context = context;
    auto buffer = clUniquePtr(BufferHelper<>::create());

    char array[3 * MemoryConstants::cacheLineSize];
    char *ptr = &array[MemoryConstants::cacheLineSize];
    ptr = alignUp(ptr, MemoryConstants::cacheLineSize);
    ptr -= 1;

    cl_int status = pCmdQ->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, 0, ptr, nullptr, 0, 0, nullptr);
    EXPECT_EQ(CL_SUCCESS, status);

    void *alignedPtr = alignDown(ptr, 4);
    size_t ptrOffset = ptrDiff(ptr, alignedPtr);
    Vec3<size_t> offset = {0, 0, 0};

    auto builtinParams = mockBuilder->paramsReceived.multiDispatchInfo.peekBuiltinOpParams();

    EXPECT_EQ(alignedPtr, builtinParams.dstPtr);
    EXPECT_EQ(ptrOffset, builtinParams.dstOffset.x);
    EXPECT_EQ(offset, builtinParams.srcOffset);

    status = pCmdQ->enqueueWriteBuffer(buffer.get(), CL_FALSE, 0, 0, ptr, nullptr, 0, 0, nullptr);
    EXPECT_EQ(CL_SUCCESS, status);

    builtinParams = mockBuilder->paramsReceived.multiDispatchInfo.peekBuiltinOpParams();

    EXPECT_EQ(alignedPtr, builtinParams.srcPtr);
    EXPECT_EQ(ptrOffset, builtinParams.srcOffset.x);
    EXPECT_EQ(offset, builtinParams.dstOffset);
}

HWTEST_F(BuiltinParamsCommandQueueHwTests, givenEnqueueWriteImageCallWhenBuiltinParamsArePassedThenCheckValuesCorectness) {

    SetUpImpl(EBuiltInOps::CopyBufferToImage3d);

    std::unique_ptr<Image> dstImage(ImageHelper<ImageUseHostPtr<Image2dDefaults>>::create(context));

    auto imageDesc = dstImage->getImageDesc();
    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, imageDesc.image_height, 0};

    size_t rowPitch = dstImage->getHostPtrRowPitch();
    size_t slicePitch = dstImage->getHostPtrSlicePitch();

    char array[3 * MemoryConstants::cacheLineSize];
    char *ptr = &array[MemoryConstants::cacheLineSize];
    ptr = alignUp(ptr, MemoryConstants::cacheLineSize);
    ptr -= 1;

    void *alignedPtr = alignDown(ptr, 4);
    size_t ptrOffset = ptrDiff(ptr, alignedPtr);
    Vec3<size_t> offset = {0, 0, 0};

    cl_int status = pCmdQ->enqueueWriteImage(dstImage.get(),
                                             CL_FALSE,
                                             origin,
                                             region,
                                             rowPitch,
                                             slicePitch,
                                             ptr,
                                             nullptr,
                                             0,
                                             0,
                                             nullptr);
    EXPECT_EQ(CL_SUCCESS, status);

    auto builtinParams = mockBuilder->paramsReceived.multiDispatchInfo.peekBuiltinOpParams();
    EXPECT_EQ(alignedPtr, builtinParams.srcPtr);
    EXPECT_EQ(ptrOffset, builtinParams.srcOffset.x);
    EXPECT_EQ(offset, builtinParams.dstOffset);
}

HWTEST_F(BuiltinParamsCommandQueueHwTests, givenEnqueueReadImageCallWhenBuiltinParamsArePassedThenCheckValuesCorectness) {

    SetUpImpl(EBuiltInOps::CopyImage3dToBuffer);

    std::unique_ptr<Image> dstImage(ImageHelper<ImageUseHostPtr<Image2dDefaults>>::create(context));

    auto imageDesc = dstImage->getImageDesc();
    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, imageDesc.image_height, 0};

    size_t rowPitch = dstImage->getHostPtrRowPitch();
    size_t slicePitch = dstImage->getHostPtrSlicePitch();

    char array[3 * MemoryConstants::cacheLineSize];
    char *ptr = &array[MemoryConstants::cacheLineSize];
    ptr = alignUp(ptr, MemoryConstants::cacheLineSize);
    ptr -= 1;

    void *alignedPtr = alignDown(ptr, 4);
    size_t ptrOffset = ptrDiff(ptr, alignedPtr);
    Vec3<size_t> offset = {0, 0, 0};

    cl_int status = pCmdQ->enqueueReadImage(dstImage.get(),
                                            CL_FALSE,
                                            origin,
                                            region,
                                            rowPitch,
                                            slicePitch,
                                            ptr,
                                            nullptr,
                                            0,
                                            0,
                                            nullptr);
    EXPECT_EQ(CL_SUCCESS, status);

    auto builtinParams = mockBuilder->paramsReceived.multiDispatchInfo.peekBuiltinOpParams();
    EXPECT_EQ(alignedPtr, builtinParams.dstPtr);
    EXPECT_EQ(ptrOffset, builtinParams.dstOffset.x);
    EXPECT_EQ(offset, builtinParams.srcOffset);
}

HWTEST_F(BuiltinParamsCommandQueueHwTests, givenEnqueueReadWriteBufferRectCallWhenBuiltinParamsArePassedThenCheckValuesCorectness) {

    SetUpImpl(EBuiltInOps::CopyBufferRect);

    BufferDefaults::context = context;
    auto buffer = clUniquePtr(BufferHelper<>::create());

    size_t bufferOrigin[3] = {0, 0, 0};
    size_t hostOrigin[3] = {0, 0, 0};
    size_t region[3] = {0, 0, 0};

    char array[3 * MemoryConstants::cacheLineSize];
    char *ptr = &array[MemoryConstants::cacheLineSize];
    ptr = alignUp(ptr, MemoryConstants::cacheLineSize);
    ptr -= 1;

    cl_int status = pCmdQ->enqueueReadBufferRect(buffer.get(), CL_FALSE, bufferOrigin, hostOrigin, region, 0, 0, 0, 0, ptr, 0, 0, nullptr);

    void *alignedPtr = alignDown(ptr, 4);
    size_t ptrOffset = ptrDiff(ptr, alignedPtr);
    Vec3<size_t> offset = {0, 0, 0};
    auto builtinParams = mockBuilder->paramsReceived.multiDispatchInfo.peekBuiltinOpParams();

    EXPECT_EQ(alignedPtr, builtinParams.dstPtr);
    EXPECT_EQ(ptrOffset, builtinParams.dstOffset.x);
    EXPECT_EQ(offset, builtinParams.srcOffset);

    status = pCmdQ->enqueueWriteBufferRect(buffer.get(), CL_FALSE, bufferOrigin, hostOrigin, region, 0, 0, 0, 0, ptr, 0, 0, nullptr);
    EXPECT_EQ(CL_SUCCESS, status);

    builtinParams = mockBuilder->paramsReceived.multiDispatchInfo.peekBuiltinOpParams();
    EXPECT_EQ(alignedPtr, builtinParams.srcPtr);
    EXPECT_EQ(offset, builtinParams.dstOffset);
    EXPECT_EQ(ptrOffset, builtinParams.srcOffset.x);
}
HWTEST_F(CommandQueueHwTest, givenCommandQueueThatIsBlockedAndUsesCpuCopyWhenEventIsReturnedItIsNotReady) {
    CommandQueueHw<FamilyType> *cmdQHw = static_cast<CommandQueueHw<FamilyType> *>(this->pCmdQ);
    MockBuffer buffer;
    cl_event returnEvent = nullptr;
    auto retVal = CL_SUCCESS;
    cmdQHw->taskLevel = CompletionStamp::levelNotReady;
    size_t offset = 0;
    size_t size = 4096u;
    TransferProperties transferProperties(&buffer, CL_COMMAND_READ_BUFFER, 0, false, &offset, &size, nullptr, false);
    EventsRequest eventsRequest(0, nullptr, &returnEvent);
    cmdQHw->cpuDataTransferHandler(transferProperties, eventsRequest, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(CompletionStamp::levelNotReady, castToObject<Event>(returnEvent)->peekTaskCount());
    clReleaseEvent(returnEvent);
}

HWTEST_F(CommandQueueHwTest, givenEventWithRecordedCommandWhenSubmitCommandIsCalledThenTaskCountMustBeUpdatedFromOtherThread) {
    std::atomic_bool go{false};

    struct mockEvent : public Event {
        using Event::Event;
        using Event::eventWithoutCommand;
        using Event::submitCommand;
        void synchronizeTaskCount() override {
            *atomicFence = true;
            Event::synchronizeTaskCount();
        }
        uint32_t synchronizeCallCount = 0u;
        std::atomic_bool *atomicFence = nullptr;
    };

    mockEvent neoEvent(this->pCmdQ, CL_COMMAND_MAP_BUFFER, CompletionStamp::levelNotReady, CompletionStamp::levelNotReady);
    neoEvent.atomicFence = &go;
    EXPECT_TRUE(neoEvent.eventWithoutCommand);
    neoEvent.eventWithoutCommand = false;

    EXPECT_EQ(CompletionStamp::levelNotReady, neoEvent.peekTaskCount());

    std::thread t([&]() {
        while (!go)
            ;
        neoEvent.updateTaskCount(77u);
    });

    neoEvent.submitCommand(false);

    EXPECT_EQ(77u, neoEvent.peekTaskCount());
    t.join();
}

HWTEST_F(CommandQueueHwTest, GivenBuiltinKernelWhenBuiltinDispatchInfoBuilderIsProvidedThenThisBuilderIsUsedForCreatingDispatchInfo) {
    CommandQueueHw<FamilyType> *cmdQHw = static_cast<CommandQueueHw<FamilyType> *>(this->pCmdQ);

    MockKernelWithInternals mockKernelToUse(*pClDevice);
    MockBuilder builder(*pDevice->getExecutionEnvironment()->getBuiltIns());
    builder.paramsToUse.gws.x = 11;
    builder.paramsToUse.elws.x = 13;
    builder.paramsToUse.offset.x = 17;
    builder.paramsToUse.kernel = mockKernelToUse.mockKernel;

    MockKernelWithInternals mockKernelToSend(*pClDevice);
    mockKernelToSend.kernelInfo.builtinDispatchBuilder = &builder;
    NullSurface s;
    Surface *surfaces[] = {&s};
    size_t gws[3] = {3, 0, 0};
    size_t lws[3] = {5, 0, 0};
    size_t off[3] = {7, 0, 0};

    EXPECT_FALSE(builder.wasBuildDispatchInfosWithBuiltinOpParamsCalled);
    EXPECT_FALSE(builder.wasBuildDispatchInfosWithKernelParamsCalled);
    cmdQHw->template enqueueHandler<CL_COMMAND_NDRANGE_KERNEL>(surfaces, false, mockKernelToSend.mockKernel, 1, off, gws, lws, lws, 0, nullptr, nullptr);
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
    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
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
    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
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
    auto mockCSR = new MockCsr<FamilyType>(executionStamp, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex());

    pDevice->resetCommandStreamReceiver(mockCSR);

    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;
    size_t offset = 0;
    size_t size = 1;

    auto event = new Event(cmdQHw, CL_COMMAND_NDRANGE_KERNEL, 10, 0);

    uint32_t virtualEventTaskLevel = 77;
    uint32_t virtualEventTaskCount = 80;
    auto virtualEvent = new Event(cmdQHw, CL_COMMAND_NDRANGE_KERNEL, virtualEventTaskLevel, virtualEventTaskCount);

    cl_event blockedEvent = event;

    // Put Queue in blocked state by assigning virtualEvent
    event->addChild(*virtualEvent);
    virtualEvent->incRefInternal();
    cmdQHw->virtualEvent = virtualEvent;

    *mockCSR->getTagAddress() = 0u;
    cmdQHw->taskLevel = 23;
    cmdQHw->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);
    //new virtual event is created on enqueue, bind it to the created virtual event
    EXPECT_NE(cmdQHw->virtualEvent, virtualEvent);

    EXPECT_EQ(virtualEvent->peekExecutionStatus(), CL_QUEUED);
    event->setStatus(CL_SUBMITTED);
    EXPECT_EQ(virtualEvent->peekExecutionStatus(), CL_SUBMITTED);

    EXPECT_FALSE(cmdQHw->isQueueBlocked());
    // +1 for next level after virtualEvent is unblocked
    // +1 as virtualEvent was a parent for event with actual command that is being submitted
    EXPECT_EQ(virtualEventTaskLevel + 2, cmdQHw->taskLevel);
    //command being submitted was dependant only on virtual event hence only +1
    EXPECT_EQ(virtualEventTaskLevel + 1, mockCSR->lastTaskLevelToFlushTask);
    *mockCSR->getTagAddress() = initialHardwareTag;
    virtualEvent->decRefInternal();
    event->decRefInternal();
}

HWTEST_F(CommandQueueHwTest, givenBlockedOutOfOrderQueueWhenUserEventIsSubmittedThenNDREventIsSubmittedAsWell) {
    CommandQueueHw<FamilyType> *cmdQHw = static_cast<CommandQueueHw<FamilyType> *>(this->pCmdQ);
    auto &mockCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;
    size_t offset = 0;
    size_t size = 1;

    cl_event userEvent = clCreateUserEvent(this->pContext, nullptr);
    cl_event blockedEvent = nullptr;

    *mockCsr.getTagAddress() = 0u;
    cmdQHw->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &userEvent, &blockedEvent);

    auto neoEvent = castToObject<Event>(blockedEvent);
    EXPECT_EQ(neoEvent->peekExecutionStatus(), CL_QUEUED);

    neoEvent->updateExecutionStatus();

    EXPECT_EQ(neoEvent->peekExecutionStatus(), CL_QUEUED);
    EXPECT_EQ(neoEvent->peekTaskCount(), CompletionStamp::levelNotReady);

    clSetUserEventStatus(userEvent, 0u);

    EXPECT_EQ(neoEvent->peekExecutionStatus(), CL_SUBMITTED);
    EXPECT_EQ(neoEvent->peekTaskCount(), 1u);

    *mockCsr.getTagAddress() = initialHardwareTag;
    clReleaseEvent(blockedEvent);
    clReleaseEvent(userEvent);
}

HWTEST_F(OOQueueHwTest, givenBlockedOutOfOrderCmdQueueAndAsynchronouslyCompletedEventWhenEnqueueCompletesVirtualEventThenUpdatedTaskLevelIsPassedToEnqueueAndFlushTask) {
    CommandQueueHw<FamilyType> *cmdQHw = static_cast<CommandQueueHw<FamilyType> *>(this->pCmdQ);

    int32_t executionStamp = 0;
    auto mockCSR = new MockCsr<FamilyType>(executionStamp, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex());
    pDevice->resetCommandStreamReceiver(mockCSR);

    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
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
    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
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
}

HWTEST_F(CommandQueueHwTest, givenWalkerSplitEnqueueNDRangeWhenBlockedThenKernelGetResidencyCalledOnce) {
    UserEvent userEvent(context);
    KernelInfo kernelInfo;
    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
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
    pCmdQ->isQueueBlocked();
}

HWTEST_F(CommandQueueHwTest, givenKernelSplitEnqueueReadBufferWhenBlockedThenEnqueueSurfacesMakeResidentIsCalledOnce) {
    UserEvent userEvent(context);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.storeMakeResidentAllocations = true;
    csr.timestampPacketWriteEnabled = false;

    BufferDefaults::context = context;
    auto buffer = clUniquePtr(BufferHelper<>::create());
    GraphicsAllocation *bufferAllocation = buffer->getGraphicsAllocation();
    char array[3 * MemoryConstants::cacheLineSize];
    char *ptr = &array[MemoryConstants::cacheLineSize];
    ptr = alignUp(ptr, MemoryConstants::cacheLineSize);
    ptr -= 1;

    cl_event blockedEvent = &userEvent;

    cl_int status = pCmdQ->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, MemoryConstants::cacheLineSize, ptr, nullptr, 1, &blockedEvent, nullptr);
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

    pCmdQ->isQueueBlocked();
}

HWTEST_F(CommandQueueHwTest, givenDefaultHwCommandQueueThenCacheFlushAfterWalkerIsNotNeeded) {
    EXPECT_FALSE(pCmdQ->getRequiresCacheFlushAfterWalker());
}

HWTEST_F(CommandQueueHwTest, givenSizeWhenForceStatelessIsCalledThenCorrectValueIsReturned) {

    if (is32bit) {
        GTEST_SKIP();
    }

    struct MockCommandQueueHw : public CommandQueueHw<FamilyType> {
        using CommandQueueHw<FamilyType>::forceStateless;
    };

    MockCommandQueueHw *pCmdQHw = reinterpret_cast<MockCommandQueueHw *>(pCmdQ);
    uint64_t bigSize = 4ull * MemoryConstants::gigaByte;
    EXPECT_TRUE(pCmdQHw->forceStateless(static_cast<size_t>(bigSize)));

    uint64_t smallSize = bigSize - 1;
    EXPECT_FALSE(pCmdQHw->forceStateless(static_cast<size_t>(smallSize)));
}

class MockCommandStreamReceiverWithFailingFlushBatchedSubmission : public MockCommandStreamReceiver {
  public:
    using MockCommandStreamReceiver::MockCommandStreamReceiver;
    bool flushBatchedSubmissions() override {
        return false;
    }
};

template <typename GfxFamily>
struct MockCommandQueueHwWithOverwrittenCsr : public CommandQueueHw<GfxFamily> {
    using CommandQueueHw<GfxFamily>::CommandQueueHw;
    MockCommandStreamReceiverWithFailingFlushBatchedSubmission *csr;
    CommandStreamReceiver &getGpgpuCommandStreamReceiver() const override { return *csr; }
};

HWTEST_F(CommandQueueHwTest, givenFlushWhenFlushBatchedSubmissionsFailsThenErrorIsRetured) {

    MockCommandQueueHwWithOverwrittenCsr<FamilyType> cmdQueue(context, device, nullptr, false);
    MockCommandStreamReceiverWithFailingFlushBatchedSubmission csr(*pDevice->executionEnvironment, 0);
    cmdQueue.csr = &csr;
    cl_int errorCode = cmdQueue.flush();
    EXPECT_EQ(CL_OUT_OF_RESOURCES, errorCode);
}

HWTEST_F(CommandQueueHwTest, givenFinishWhenFlushBatchedSubmissionsFailsThenErrorIsRetured) {
    MockCommandQueueHwWithOverwrittenCsr<FamilyType> cmdQueue(context, device, nullptr, false);
    MockCommandStreamReceiverWithFailingFlushBatchedSubmission csr(*pDevice->executionEnvironment, 0);
    cmdQueue.csr = &csr;
    cl_int errorCode = cmdQueue.finish();
    EXPECT_EQ(CL_OUT_OF_RESOURCES, errorCode);
}

HWTEST_F(CommandQueueHwTest, givenEmptyDispatchGlobalsArgsWhenEnqueueInitDispatchGlobalsCalledThenErrorIsReturned) {
    EXPECT_EQ(CL_INVALID_VALUE, pCmdQ->enqueueInitDispatchGlobals(nullptr, 0, nullptr, nullptr));
}

HWTEST_F(CommandQueueHwTest, WhenForcePerDssBackedBufferProgrammingSetThenDispatchFlagsAreSetAccordingly) {
    DebugManagerStateRestore restore;
    DebugManager.flags.ForcePerDssBackedBufferProgramming = true;

    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    size_t offset = 0;
    size_t gws = 64;
    size_t lws = 16;

    cl_int status = pCmdQ->enqueueKernel(mockKernel, 1, &offset, &gws, &lws, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_TRUE(csr.recordedDispatchFlags.usePerDssBackedBuffer);
}
