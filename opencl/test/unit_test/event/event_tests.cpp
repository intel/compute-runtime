/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/event/perf_counter.h"
#include "opencl/source/helpers/task_information.h"
#include "opencl/source/memory_manager/mem_obj_surface.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_csr.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/os_interface/mock_performance_counters.h"
#include "test.h"

#include "event_fixture.h"

#include <memory>
#include <type_traits>

TEST(Event, GivenEventWhenCheckingTraitThenEventIsNotCopyable) {
    EXPECT_FALSE(std::is_move_constructible<Event>::value);
    EXPECT_FALSE(std::is_copy_constructible<Event>::value);
}

TEST(Event, GivenEventWhenCheckingTraitThenEventIsNotAssignable) {
    EXPECT_FALSE(std::is_move_assignable<Event>::value);
    EXPECT_FALSE(std::is_copy_assignable<Event>::value);
}

TEST(Event, WhenPeekIsCalledThenExecutionIsNotUpdated) {
    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext ctx;
    MockCommandQueue cmdQ(&ctx, mockDevice.get(), 0);
    Event event(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, CompletionStamp::levelNotReady, 0);

    EXPECT_FALSE(event.peekIsBlocked());
    EXPECT_EQ(CL_QUEUED, event.peekExecutionStatus());
    event.updateExecutionStatus();
    EXPECT_EQ(CL_QUEUED, event.peekExecutionStatus());
}

TEST(Event, givenEventThatStatusChangeWhenPeekIsCalledThenEventIsNotUpdated) {
    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext ctx;
    MockCommandQueue cmdQ(&ctx, mockDevice.get(), 0);

    struct mockEvent : public Event {
        using Event::Event;
        void updateExecutionStatus() override {
            callCount++;
        }
        uint32_t callCount = 0u;
    };

    mockEvent event(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, CompletionStamp::levelNotReady, 0);
    EXPECT_EQ(0u, event.callCount);
    event.peekExecutionStatus();
    EXPECT_EQ(0u, event.callCount);
    event.updateEventAndReturnCurrentStatus();
    EXPECT_EQ(1u, event.callCount);
    event.updateEventAndReturnCurrentStatus();
    EXPECT_EQ(2u, event.callCount);
}

TEST(Event, givenEventWithHigherTaskCountWhenLowerTaskCountIsBeingSetThenTaskCountRemainsUnmodifed) {
    Event *event = new Event(nullptr, CL_COMMAND_NDRANGE_KERNEL, 4, 10);

    EXPECT_EQ(10u, event->peekTaskCount());
    event->updateTaskCount(8);
    EXPECT_EQ(10u, event->peekTaskCount());
    delete event;
}

TEST(Event, WhenGettingTaskLevelThenCorrectTaskLevelIsReturned) {
    class TempEvent : public Event {
      public:
        TempEvent() : Event(nullptr, CL_COMMAND_NDRANGE_KERNEL, 5, 7){};

        uint32_t getTaskLevel() override {
            return Event::getTaskLevel();
        }
    };
    TempEvent event;
    // taskLevel and getTaskLevel() should give the same result
    EXPECT_EQ(5u, event.taskLevel);
    EXPECT_EQ(5u, event.getTaskLevel());
}

TEST(Event, WhenGettingTaskCountThenCorrectValueIsReturned) {
    Event event(nullptr, CL_COMMAND_NDRANGE_KERNEL, 5, 7);
    EXPECT_EQ(7u, event.getCompletionStamp());
}

TEST(Event, WhenGettingEventInfoThenCqIsReturned) {
    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto ctx = std::unique_ptr<Context>(new MockContext());
    auto cmdQ = std::unique_ptr<CommandQueue>(new MockCommandQueue(ctx.get(), mockDevice.get(), 0));
    Event *event = new Event(cmdQ.get(), CL_COMMAND_NDRANGE_KERNEL, 1, 5);
    cl_event clEvent = event;
    cl_command_queue cmdQResult = nullptr;
    size_t sizeReturned = 0;

    auto result = clGetEventInfo(clEvent, CL_EVENT_COMMAND_QUEUE, 0, nullptr, &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeof(cl_command_queue), sizeReturned);

    result = clGetEventInfo(clEvent, CL_EVENT_COMMAND_QUEUE, sizeof(cmdQResult), &cmdQResult, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(cmdQ.get(), cmdQResult);
    EXPECT_EQ(sizeReturned, sizeof(cmdQResult));
    delete event;
}

TEST(Event, givenCommandQueueWhenEventIsCreatedWithCommandQueueThenCommandQueueInternalRefCountIsIncremented) {
    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext ctx;
    MockCommandQueue cmdQ(&ctx, mockDevice.get(), 0);
    auto intitialRefCount = cmdQ.getRefInternalCount();

    Event *event = new Event(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, 4, 10);
    auto newRefCount = cmdQ.getRefInternalCount();

    EXPECT_EQ(intitialRefCount + 1, newRefCount);
    delete event;

    auto finalRefCount = cmdQ.getRefInternalCount();
    EXPECT_EQ(intitialRefCount, finalRefCount);
}

TEST(Event, givenCommandQueueWhenEventIsCreatedWithoutCommandQueueThenCommandQueueInternalRefCountIsNotModified) {
    MockContext ctx;
    MockCommandQueue cmdQ(&ctx, nullptr, 0);
    auto intitialRefCount = cmdQ.getRefInternalCount();

    Event *event = new Event(nullptr, CL_COMMAND_NDRANGE_KERNEL, 4, 10);
    auto newRefCount = cmdQ.getRefInternalCount();

    EXPECT_EQ(intitialRefCount, newRefCount);
    delete event;

    auto finalRefCount = cmdQ.getRefInternalCount();
    EXPECT_EQ(intitialRefCount, finalRefCount);
}

TEST(Event, WhenWaitingForEventsThenAllQueuesAreFlushed) {
    class MockCommandQueueWithFlushCheck : public MockCommandQueue {
      public:
        MockCommandQueueWithFlushCheck() = delete;
        MockCommandQueueWithFlushCheck(MockCommandQueueWithFlushCheck &) = delete;
        MockCommandQueueWithFlushCheck(Context &context, ClDevice *device) : MockCommandQueue(&context, device, nullptr) {
        }
        cl_int flush() override {
            flushCounter++;
            return CL_SUCCESS;
        }
        uint32_t flushCounter = 0;
    };

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext context;

    std::unique_ptr<MockCommandQueueWithFlushCheck> cmdQ1(new MockCommandQueueWithFlushCheck(context, device.get()));
    std::unique_ptr<Event> event1(new Event(cmdQ1.get(), CL_COMMAND_NDRANGE_KERNEL, 4, 10));

    std::unique_ptr<MockCommandQueueWithFlushCheck> cmdQ2(new MockCommandQueueWithFlushCheck(context, device.get()));
    std::unique_ptr<Event> event2(new Event(cmdQ2.get(), CL_COMMAND_NDRANGE_KERNEL, 5, 20));

    cl_event eventWaitlist[] = {event1.get(), event2.get()};

    Event::waitForEvents(2, eventWaitlist);

    EXPECT_EQ(1u, cmdQ1->flushCounter);
    EXPECT_EQ(1u, cmdQ2->flushCounter);
}

TEST(Event, GivenNotReadyEventWhenWaitingForEventsThenQueueIsNotFlushed) {
    class MockCommandQueueWithFlushCheck : public MockCommandQueue {
      public:
        MockCommandQueueWithFlushCheck() = delete;
        MockCommandQueueWithFlushCheck(MockCommandQueueWithFlushCheck &) = delete;
        MockCommandQueueWithFlushCheck(Context &context, ClDevice *device) : MockCommandQueue(&context, device, nullptr) {
        }
        cl_int flush() override {
            flushCounter++;
            return CL_SUCCESS;
        }
        uint32_t flushCounter = 0;
    };

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext context;

    std::unique_ptr<MockCommandQueueWithFlushCheck> cmdQ1(new MockCommandQueueWithFlushCheck(context, device.get()));
    std::unique_ptr<Event> event1(new Event(cmdQ1.get(), CL_COMMAND_NDRANGE_KERNEL, CompletionStamp::levelNotReady, 0));
    cl_event eventWaitlist[] = {event1.get()};

    Event::waitForEvents(1, eventWaitlist);

    EXPECT_EQ(0u, cmdQ1->flushCounter);
}

TEST(Event, givenNotReadyEventOnWaitlistWhenCheckingUserEventDependeciesThenTrueIsReturned) {
    auto event1 = std::make_unique<Event>(nullptr, CL_COMMAND_NDRANGE_KERNEL, CompletionStamp::levelNotReady, 0);
    cl_event eventWaitlist[] = {event1.get()};

    bool userEventDependencies = Event::checkUserEventDependencies(1, eventWaitlist);
    EXPECT_TRUE(userEventDependencies);
}

TEST(Event, givenReadyEventsOnWaitlistWhenCheckingUserEventDependeciesThenFalseIsReturned) {
    auto event1 = std::make_unique<Event>(nullptr, CL_COMMAND_NDRANGE_KERNEL, 5, 0);
    cl_event eventWaitlist[] = {event1.get()};

    bool userEventDependencies = Event::checkUserEventDependencies(1, eventWaitlist);
    EXPECT_FALSE(userEventDependencies);
}

TEST_F(EventTest, WhenGettingClEventCommandExecutionStatusThenCorrectSizeIsReturned) {
    Event event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 1, 5);
    cl_int eventStatus = -1;
    size_t sizeReturned = 0;

    auto result = clGetEventInfo(&event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(eventStatus), &eventStatus, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeReturned, sizeof(eventStatus));
}

TEST_F(EventTest, GivenTagCsLessThanTaskCountWhenGettingClEventCommandExecutionStatusThenClSubmittedIsReturned) {
    uint32_t tagHW = 4;
    uint32_t taskCount = 5;
    *pTagMemory = tagHW;

    Event event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 3, taskCount);
    cl_int eventStatus = -1;
    size_t sizeReturned = 0;

    auto result = clGetEventInfo(&event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(eventStatus), &eventStatus, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);

    // If tagCS < taskCount, we always return submitted (ie. no buffering!)
    EXPECT_EQ(CL_SUBMITTED, eventStatus);
}

TEST_F(EventTest, GivenTagCsEqualTaskCountWhenGettingClEventCommandExecutionStatusThenClCompleteIsReturned) {
    uint32_t tagHW = 5;
    uint32_t taskCount = 5;
    *pTagMemory = tagHW;

    Event event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 3, taskCount);
    cl_int eventStatus = -1;
    size_t sizeReturned = 0;

    auto result = clGetEventInfo(&event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(eventStatus), &eventStatus, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);

    // If tagCS == event.taskCount, the event is completed.
    EXPECT_EQ(CL_COMPLETE, eventStatus);
}

TEST_F(EventTest, GivenTagCsGreaterThanTaskCountWhenGettingClEventCommandExecutionStatusThenClCompleteIsReturned) {
    uint32_t tagHW = 6;
    uint32_t taskCount = 5;
    *pTagMemory = tagHW;

    Event event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 3, taskCount);
    cl_int eventStatus = -1;
    size_t sizeReturned = 0;

    auto result = clGetEventInfo(&event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(eventStatus), &eventStatus, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);

    EXPECT_EQ(CL_COMPLETE, eventStatus);
}

TEST_F(EventTest, WhenGettingClEventCommandExecutionStatusThenEventStatusIsReturned) {
    Event event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, CompletionStamp::levelNotReady, CompletionStamp::levelNotReady);
    cl_int eventStatus = -1;

    event.setStatus(-1);
    auto result = clGetEventInfo(&event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(eventStatus), &eventStatus, 0);
    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(-1, eventStatus);
}

TEST_F(EventTest, GivenNewEventWhenGettingClEventReferenceCountThenOneIsReturned) {
    uint32_t tagEvent = 5;

    Event event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 3, tagEvent);
    cl_uint refCount = 0;
    size_t sizeReturned = 0;

    auto result = clGetEventInfo(&event, CL_EVENT_REFERENCE_COUNT, sizeof(refCount), &refCount, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeof(refCount), sizeReturned);

    EXPECT_EQ(1u, refCount);
}

TEST_F(EventTest, GivenRetainedEventWhenGettingClEventReferenceCountThenTwoIsReturned) {
    uint32_t tagEvent = 5;

    Event event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 3, tagEvent);
    event.retain();

    cl_uint refCount = 0;
    size_t sizeReturned = 0;

    auto result = clGetEventInfo(&event, CL_EVENT_REFERENCE_COUNT, sizeof(refCount), &refCount, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeof(refCount), sizeReturned);

    EXPECT_EQ(2u, refCount);
    event.release();
}

TEST_F(EventTest, GivenRetainAndReleaseEventWhenGettingClEventReferenceCountThenOneIsReturned) {
    uint32_t tagEvent = 5;

    Event *pEvent = new Event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 3, tagEvent);
    ASSERT_NE(nullptr, pEvent);

    pEvent->retain();
    auto retVal = pEvent->getReference();
    EXPECT_EQ(2, retVal);

    cl_uint refCount = 0;
    size_t sizeReturned = 0;
    auto result = clGetEventInfo(pEvent, CL_EVENT_REFERENCE_COUNT, sizeof(refCount), &refCount, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeof(refCount), sizeReturned);
    EXPECT_EQ(2u, refCount);

    pEvent->release();
    retVal = pEvent->getReference();
    EXPECT_EQ(1, retVal);

    delete pEvent;
}

TEST_F(EventTest, WhenGettingClEventContextThenCorrectValueIsReturned) {
    uint32_t tagEvent = 5;

    Event *pEvent = new Event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 3, tagEvent);
    ASSERT_NE(nullptr, pEvent);

    cl_context context;
    size_t sizeReturned = 0;
    auto result = clGetEventInfo(pEvent, CL_EVENT_CONTEXT, sizeof(context), &context, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeof(context), sizeReturned);

    cl_context qCtx = (cl_context)&mockContext;
    EXPECT_EQ(qCtx, context);

    delete pEvent;
}

TEST_F(EventTest, GivenInvalidEventWhenGettingEventInfoThenInvalidValueErrorIsReturned) {
    uint32_t tagEvent = 5;

    Event event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 3, tagEvent);
    cl_int eventStatus = -1;

    auto result = clGetEventInfo(&event, -1, sizeof(eventStatus), &eventStatus, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, result);
}

TEST_F(EventTest, GivenNonBlockingEventWhenWaitingThenFalseIsReturned) {
    Event event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 3, CompletionStamp::levelNotReady);
    auto result = event.wait(false, false);
    EXPECT_FALSE(result);
}

struct UpdateEventTest : public ::testing::Test {

    void SetUp() override {
        executionEnvironment = platform()->peekExecutionEnvironment();
        memoryManager = new MockMemoryManager(*executionEnvironment);
        hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());
        executionEnvironment->memoryManager.reset(memoryManager);
        device.reset(new ClDevice{*Device::create<RootDevice>(executionEnvironment, 0u), platform()});
        context = std::make_unique<MockContext>(device.get());
        cl_int retVal = CL_OUT_OF_RESOURCES;
        commandQueue.reset(CommandQueue::create(context.get(), device.get(), nullptr, false, retVal));
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    ExecutionEnvironment *executionEnvironment;
    MockMemoryManager *memoryManager;
    MockHostPtrManager *hostPtrManager;
    std::unique_ptr<ClDevice> device;
    std::unique_ptr<Context> context;
    std::unique_ptr<CommandQueue> commandQueue;
};

TEST_F(UpdateEventTest, givenEventContainingCommandQueueWhenItsStatusIsUpdatedToCompletedThenTemporaryAllocationsAreDeleted) {
    void *ptr = (void *)0x1000;
    size_t size = 4096;
    auto temporary = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, size}, ptr);
    temporary->updateTaskCount(3, commandQueue->getGpgpuCommandStreamReceiver().getOsContext().getContextId());
    commandQueue->getGpgpuCommandStreamReceiver().getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(temporary), TEMPORARY_ALLOCATION);
    Event event(commandQueue.get(), CL_COMMAND_NDRANGE_KERNEL, 3, 3);

    EXPECT_EQ(1u, hostPtrManager->getFragmentCount());

    event.updateExecutionStatus();

    EXPECT_EQ(0u, hostPtrManager->getFragmentCount());
}

class SurfaceMock : public Surface {
  public:
    SurfaceMock() {
        resident = nonResident = 0;
    };
    ~SurfaceMock() override{};

    void makeResident(CommandStreamReceiver &csr) override {
        if (parent) {
            parent->resident++;
        } else {
            resident++;
        }
        if (this->graphicsAllocation) {
            csr.makeResident(*graphicsAllocation);
        }
    };
    Surface *duplicate() override {
        return new SurfaceMock(this);
    };

    SurfaceMock *parent = nullptr;
    std::atomic<uint32_t> resident;
    std::atomic<uint32_t> nonResident;

    GraphicsAllocation *graphicsAllocation = nullptr;

  protected:
    SurfaceMock(SurfaceMock *parent) : parent(parent){};
};

TEST_F(InternalsEventTest, GivenSubmitCommandFalseWhenSubmittingCommandsThenRefApiCountAndRefInternalGetHandledCorrectly) {
    CommandQueue cmdQ(mockContext, pClDevice, nullptr);
    MockEvent<Event> event(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);

    auto cmdStream = new LinearStream(pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), 4096, GraphicsAllocation::AllocationType::COMMAND_BUFFER}));
    IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    cmdQ.allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, 4096u, dsh);
    cmdQ.allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, 4096u, ioh);
    cmdQ.allocateHeapMemory(IndirectHeap::SURFACE_STATE, 4096u, ssh);

    auto blockedCommandsData = std::make_unique<KernelOperation>(cmdStream, *cmdQ.getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    blockedCommandsData->setHeaps(dsh, ioh, ssh);

    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto pKernel = mockKernelWithInternals.mockKernel;

    auto &csr = cmdQ.getGpgpuCommandStreamReceiver();
    std::vector<Surface *> v;
    MockBuffer buffer;
    buffer.retain();
    auto initialRefCount = buffer.getRefApiCount();
    auto initialInternalCount = buffer.getRefInternalCount();

    auto bufferSurf = new MemObjSurface(&buffer);

    EXPECT_EQ(initialInternalCount + 1, buffer.getRefInternalCount());
    EXPECT_EQ(initialRefCount, buffer.getRefApiCount());

    PreemptionMode preemptionMode = pDevice->getPreemptionMode();
    v.push_back(bufferSurf);
    auto cmd = new CommandComputeKernel(cmdQ, blockedCommandsData, v, false, false, false, nullptr, preemptionMode, pKernel, 1);
    event.setCommand(std::unique_ptr<Command>(cmd));

    auto taskLevelBefore = csr.peekTaskLevel();

    auto refCount = buffer.getRefApiCount();
    auto refInternal = buffer.getRefInternalCount();

    event.submitCommand(false);

    EXPECT_EQ(refCount, buffer.getRefApiCount());
    EXPECT_EQ(refInternal - 1, buffer.getRefInternalCount());

    auto taskLevelAfter = csr.peekTaskLevel();

    EXPECT_EQ(taskLevelBefore + 1, taskLevelAfter);

    auto graphicsAllocation = buffer.getGraphicsAllocation();

    EXPECT_FALSE(graphicsAllocation->isResident(csr.getOsContext().getContextId()));
}

TEST_F(InternalsEventTest, GivenSubmitCommandTrueWhenSubmittingCommandsThenRefApiCountAndRefInternalGetHandledCorrectly) {
    CommandQueue cmdQ(mockContext, pClDevice, nullptr);
    MockEvent<Event> event(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);

    auto cmdStream = new LinearStream(pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), 4096, GraphicsAllocation::AllocationType::COMMAND_BUFFER}));
    IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    cmdQ.allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, 4096u, dsh);
    cmdQ.allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, 4096u, ioh);
    cmdQ.allocateHeapMemory(IndirectHeap::SURFACE_STATE, 4096u, ssh);

    auto blockedCommandsData = std::make_unique<KernelOperation>(cmdStream, *cmdQ.getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    blockedCommandsData->setHeaps(dsh, ioh, ssh);

    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto pKernel = mockKernelWithInternals.mockKernel;

    auto &csr = cmdQ.getGpgpuCommandStreamReceiver();
    std::vector<Surface *> v;
    NullSurface *surface = new NullSurface;
    v.push_back(surface);
    PreemptionMode preemptionMode = pDevice->getPreemptionMode();
    auto cmd = new CommandComputeKernel(cmdQ, blockedCommandsData, v, false, false, false, nullptr, preemptionMode, pKernel, 1);
    event.setCommand(std::unique_ptr<Command>(cmd));

    auto taskLevelBefore = csr.peekTaskLevel();

    event.submitCommand(true);

    auto taskLevelAfter = csr.peekTaskLevel();

    EXPECT_EQ(taskLevelBefore, taskLevelAfter);
}

TEST_F(InternalsEventTest, givenBlockedKernelWithPrintfWhenSubmittedThenPrintOutput) {
    MockCommandQueue mockCmdQueue(mockContext, pClDevice, nullptr);

    testing::internal::CaptureStdout();
    MockEvent<Event> event(&mockCmdQueue, CL_COMMAND_NDRANGE_KERNEL, 0, 0);

    auto cmdStream = new LinearStream(pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), 4096, GraphicsAllocation::AllocationType::COMMAND_BUFFER}));
    IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    mockCmdQueue.allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, 4096u, dsh);
    mockCmdQueue.allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, 4096u, ioh);
    mockCmdQueue.allocateHeapMemory(IndirectHeap::SURFACE_STATE, 4096u, ssh);

    auto blockedCommandsData = std::make_unique<KernelOperation>(cmdStream, *mockCmdQueue.getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    blockedCommandsData->setHeaps(dsh, ioh, ssh);

    SPatchAllocateStatelessPrintfSurface *pPrintfSurface = new SPatchAllocateStatelessPrintfSurface();
    pPrintfSurface->DataParamOffset = 0;
    pPrintfSurface->DataParamSize = 8;

    std::string testString = "test";

    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto pKernel = mockKernelWithInternals.mockKernel;
    KernelInfo *kernelInfo = const_cast<KernelInfo *>(&pKernel->getKernelInfo());
    kernelInfo->patchInfo.pAllocateStatelessPrintfSurface = pPrintfSurface;
    kernelInfo->patchInfo.stringDataMap.insert(std::make_pair(0, testString));
    uint64_t crossThread[10];
    pKernel->setCrossThreadData(&crossThread, sizeof(uint64_t) * 8);

    MockMultiDispatchInfo multiDispatchInfo(pKernel);
    std::unique_ptr<PrintfHandler> printfHandler(PrintfHandler::create(multiDispatchInfo, *pClDevice));
    printfHandler.get()->prepareDispatch(multiDispatchInfo);
    auto surface = printfHandler.get()->getSurface();

    auto printfSurface = reinterpret_cast<uint32_t *>(surface->getUnderlyingBuffer());
    printfSurface[0] = 8;
    printfSurface[1] = 0;

    std::vector<Surface *> v;
    PreemptionMode preemptionMode = pDevice->getPreemptionMode();
    auto cmd = new CommandComputeKernel(mockCmdQueue, blockedCommandsData, v, false, false, false, std::move(printfHandler), preemptionMode, pKernel, 1);
    event.setCommand(std::unique_ptr<Command>(cmd));

    event.submitCommand(false);

    EXPECT_EQ(1u, mockCmdQueue.latestTaskCountWaited);

    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STREQ("test", output.c_str());
    EXPECT_FALSE(surface->isResident(pDevice->getDefaultEngine().osContext->getContextId()));

    delete pPrintfSurface;
}

TEST_F(InternalsEventTest, GivenMapOperationWhenSubmittingCommandsThenTaskLevelIsIncremented) {
    auto pCmdQ = make_releaseable<CommandQueue>(mockContext, pClDevice, nullptr);
    MockEvent<Event> event(nullptr, CL_COMMAND_NDRANGE_KERNEL, 0, 0);

    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();
    auto buffer = new MockBuffer;

    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    event.setCommand(std::unique_ptr<Command>(new CommandMapUnmap(MAP, *buffer, size, offset, false, *pCmdQ)));

    auto taskLevelBefore = csr.peekTaskLevel();

    event.submitCommand(false);

    auto taskLevelAfter = csr.peekTaskLevel();

    EXPECT_EQ(taskLevelBefore + 1, taskLevelAfter);
    buffer->decRefInternal();
}

TEST_F(InternalsEventTest, GivenMapOperationNonZeroCopyBufferWhenSubmittingCommandsThenTaskLevelIsIncremented) {
    auto pCmdQ = make_releaseable<CommandQueue>(mockContext, pClDevice, nullptr);
    MockEvent<Event> event(nullptr, CL_COMMAND_NDRANGE_KERNEL, 0, 0);

    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();
    auto buffer = new UnalignedBuffer;

    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    event.setCommand(std::unique_ptr<Command>(new CommandMapUnmap(MAP, *buffer, size, offset, false, *pCmdQ)));

    auto taskLevelBefore = csr.peekTaskLevel();

    event.submitCommand(false);

    auto taskLevelAfter = csr.peekTaskLevel();

    EXPECT_EQ(taskLevelBefore + 1, taskLevelAfter);
    buffer->decRefInternal();
}

uint32_t commands[] = {
    CL_COMMAND_NDRANGE_KERNEL,
    CL_COMMAND_TASK,
    CL_COMMAND_NATIVE_KERNEL,
    CL_COMMAND_READ_BUFFER,
    CL_COMMAND_WRITE_BUFFER,
    CL_COMMAND_COPY_BUFFER,
    CL_COMMAND_READ_IMAGE,
    CL_COMMAND_WRITE_IMAGE,
    CL_COMMAND_COPY_IMAGE,
    CL_COMMAND_COPY_IMAGE_TO_BUFFER,
    CL_COMMAND_COPY_BUFFER_TO_IMAGE,
    CL_COMMAND_MAP_BUFFER,
    CL_COMMAND_MAP_IMAGE,
    CL_COMMAND_UNMAP_MEM_OBJECT,
    CL_COMMAND_MARKER,
    CL_COMMAND_ACQUIRE_GL_OBJECTS,
    CL_COMMAND_RELEASE_GL_OBJECTS,
    CL_COMMAND_READ_BUFFER_RECT,
    CL_COMMAND_WRITE_BUFFER_RECT,
    CL_COMMAND_COPY_BUFFER_RECT,
    CL_COMMAND_BARRIER,
    CL_COMMAND_MIGRATE_MEM_OBJECTS,
    CL_COMMAND_FILL_BUFFER,
    CL_COMMAND_FILL_IMAGE,
    CL_COMMAND_SVM_FREE,
    CL_COMMAND_SVM_MEMCPY,
    CL_COMMAND_SVM_MEMFILL,
    CL_COMMAND_SVM_MAP,
    CL_COMMAND_SVM_UNMAP,
};

class InternalsEventProfilingTest : public InternalsEventTest,
                                    public ::testing::WithParamInterface<uint32_t> {
    void SetUp() override {
        InternalsEventTest::SetUp();
    }

    void TearDown() override {
        InternalsEventTest::TearDown();
    }
};

TEST_P(InternalsEventProfilingTest, GivenProfilingWhenEventCreatedThenProfilingSet) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    std::unique_ptr<CommandQueue> pCmdQ(new CommandQueue(mockContext, pClDevice, props));

    std::unique_ptr<MockEvent<Event>> event(new MockEvent<Event>(pCmdQ.get(), GetParam(), 0, 0));
    EXPECT_TRUE(event.get()->isProfilingEnabled());
}

INSTANTIATE_TEST_CASE_P(InternalsEventProfilingTest,
                        InternalsEventProfilingTest,
                        ::testing::ValuesIn(commands));

TEST_F(InternalsEventTest, GivenProfilingWhenUserEventCreatedThenProfilingNotSet) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    std::unique_ptr<CommandQueue> pCmdQ(new CommandQueue(mockContext, pClDevice, props));

    std::unique_ptr<MockEvent<Event>> event(new MockEvent<Event>(pCmdQ.get(), CL_COMMAND_USER, 0, 0));
    EXPECT_FALSE(event.get()->isProfilingEnabled());
}

TEST_F(InternalsEventTest, GivenProfilingWHENMapOperationTHENTimesSet) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    CommandQueue *pCmdQ = new CommandQueue(mockContext, pClDevice, props);

    MockEvent<Event> *event = new MockEvent<Event>(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);

    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();
    UnalignedBuffer buffer;

    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    event->setCommand(std::unique_ptr<Command>(new CommandMapUnmap(MAP, buffer, size, offset, false, *pCmdQ)));

    auto taskLevelBefore = csr.peekTaskLevel();

    event->submitCommand(false);
    uint64_t submitTime = 0ULL;
    event->getEventProfilingInfo(CL_PROFILING_COMMAND_SUBMIT, sizeof(uint64_t), &submitTime, 0);
    EXPECT_NE(0ULL, submitTime);
    auto taskLevelAfter = csr.peekTaskLevel();

    delete event;

    EXPECT_EQ(taskLevelBefore + 1, taskLevelAfter);
    delete pCmdQ;
}

TEST_F(InternalsEventTest, GivenUnMapOperationWhenSubmittingCommandsThenTaskLevelIsIncremented) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    auto pCmdQ = make_releaseable<CommandQueue>(mockContext, pClDevice, props);
    MockEvent<Event> event(nullptr, CL_COMMAND_NDRANGE_KERNEL, 0, 0);

    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();
    auto buffer = new UnalignedBuffer;

    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    event.setCommand(std::unique_ptr<Command>(new CommandMapUnmap(UNMAP, *buffer, size, offset, false, *pCmdQ)));

    auto taskLevelBefore = csr.peekTaskLevel();

    event.submitCommand(false);

    auto taskLevelAfter = csr.peekTaskLevel();

    EXPECT_EQ(taskLevelBefore + 1, taskLevelAfter);
    buffer->decRefInternal();
}

TEST_F(InternalsEventTest, givenBlockedMapCommandWhenSubmitIsCalledItReleasesMemObjectReference) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    auto pCmdQ = std::make_unique<CommandQueue>(mockContext, pClDevice, props);
    MockEvent<Event> event(nullptr, CL_COMMAND_NDRANGE_KERNEL, 0, 0);

    auto buffer = new UnalignedBuffer;

    auto currentBufferRefInternal = buffer->getRefInternalCount();

    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    event.setCommand(std::unique_ptr<Command>(new CommandMapUnmap(UNMAP, *buffer, size, offset, false, *pCmdQ)));
    EXPECT_EQ(currentBufferRefInternal + 1, buffer->getRefInternalCount());

    event.submitCommand(false);

    EXPECT_EQ(currentBufferRefInternal, buffer->getRefInternalCount());
    buffer->decRefInternal();
}
TEST_F(InternalsEventTest, GivenUnMapOperationNonZeroCopyBufferWhenSubmittingCommandsThenTaskLevelIsIncremented) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    auto pCmdQ = std::make_unique<CommandQueue>(mockContext, pClDevice, props);
    MockEvent<Event> event(nullptr, CL_COMMAND_NDRANGE_KERNEL, 0, 0);

    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();
    auto buffer = new UnalignedBuffer;

    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    event.setCommand(std::unique_ptr<Command>(new CommandMapUnmap(UNMAP, *buffer, size, offset, false, *pCmdQ)));

    auto taskLevelBefore = csr.peekTaskLevel();

    event.submitCommand(false);

    auto taskLevelAfter = csr.peekTaskLevel();

    EXPECT_EQ(taskLevelBefore + 1, taskLevelAfter);
    buffer->decRefInternal();
}

HWTEST_F(InternalsEventTest, givenCpuProfilingPathWhenEnqueuedMarkerThenDontUseTimeStampNode) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    CommandQueue *pCmdQ = new CommandQueue(mockContext, pClDevice, props);
    MockEvent<Event> *event = new MockEvent<Event>(pCmdQ, CL_COMMAND_MARKER, 0, 0);
    event->setCPUProfilingPath(true);

    event->setCommand(std::unique_ptr<Command>(new CommandWithoutKernel(*pCmdQ)));

    event->submitCommand(false);

    uint64_t submit, start, end;
    event->getEventProfilingInfo(CL_PROFILING_COMMAND_SUBMIT, sizeof(uint64_t), &submit, 0);
    event->getEventProfilingInfo(CL_PROFILING_COMMAND_START, sizeof(uint64_t), &start, 0);
    event->getEventProfilingInfo(CL_PROFILING_COMMAND_END, sizeof(uint64_t), &end, 0);

    EXPECT_LT(0u, submit);
    EXPECT_LT(submit, start);
    EXPECT_LT(start, end);
    delete event;
    delete pCmdQ;
}

struct InternalsEventWithPerfCountersTest
    : public InternalsEventTest,
      public PerformanceCountersFixture {
    void SetUp() override {
        PerformanceCountersFixture::SetUp();
        InternalsEventTest::SetUp();
        createPerfCounters();
        pDevice->setPerfCounters(performanceCountersBase.get());
    }

    void TearDown() override {
        performanceCountersBase.release();
        InternalsEventTest::TearDown();
        PerformanceCountersFixture::TearDown();
    }
};
HWTEST_F(InternalsEventWithPerfCountersTest, givenCpuProfilingPerfCountersPathWhenEnqueuedMarkerThenDontUseTimeStampNodePerfCounterNode) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    CommandQueue *pCmdQ = new CommandQueue(mockContext, pClDevice, props);
    bool ret = false;
    ret = pCmdQ->setPerfCountersEnabled();
    EXPECT_TRUE(ret);

    MockEvent<Event> *event = new MockEvent<Event>(pCmdQ, CL_COMMAND_MARKER, 0, 0);
    event->setCPUProfilingPath(true);

    event->setCommand(std::unique_ptr<Command>(new CommandWithoutKernel(*pCmdQ)));

    event->submitCommand(false);

    uint64_t submit, start, end;
    event->getEventProfilingInfo(CL_PROFILING_COMMAND_SUBMIT, sizeof(uint64_t), &submit, 0);
    event->getEventProfilingInfo(CL_PROFILING_COMMAND_START, sizeof(uint64_t), &start, 0);
    event->getEventProfilingInfo(CL_PROFILING_COMMAND_END, sizeof(uint64_t), &end, 0);

    EXPECT_LT(0u, submit);
    EXPECT_LT(submit, start);
    EXPECT_LT(start, end);
    delete event;
    delete pCmdQ;
}

HWTEST_F(InternalsEventWithPerfCountersTest, givenCpuProfilingPerfCountersPathWhenEnqueuedMarkerThenUseTimeStampNodePerfCounterNode) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    CommandQueue *pCmdQ = new CommandQueue(mockContext, pClDevice, props);
    pCmdQ->setPerfCountersEnabled();
    MockEvent<Event> *event = new MockEvent<Event>(pCmdQ, CL_COMMAND_MARKER, 0, 0);
    event->setCPUProfilingPath(true);
    HwPerfCounter *perfCounter = event->getHwPerfCounterNode()->tagForCpuAccess;
    ASSERT_NE(nullptr, perfCounter);
    HwTimeStamps *timeStamps = event->getHwTimeStampNode()->tagForCpuAccess;
    ASSERT_NE(nullptr, timeStamps);

    event->setCommand(std::unique_ptr<Command>(new CommandWithoutKernel(*pCmdQ)));

    event->submitCommand(false);

    uint64_t submit, start, end;
    event->getEventProfilingInfo(CL_PROFILING_COMMAND_SUBMIT, sizeof(uint64_t), &submit, 0);
    event->getEventProfilingInfo(CL_PROFILING_COMMAND_START, sizeof(uint64_t), &start, 0);
    event->getEventProfilingInfo(CL_PROFILING_COMMAND_END, sizeof(uint64_t), &end, 0);

    EXPECT_LT(0u, submit);
    EXPECT_LT(submit, start);
    EXPECT_LT(start, end);
    delete event;
    delete pCmdQ;
}

TEST_F(InternalsEventWithPerfCountersTest, GivenPerfCountersEnabledWhenEventIsCreatedThenProfilingEnabledAndPerfCountersEnabledAreTrue) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    CommandQueue *pCmdQ = new CommandQueue(mockContext, pClDevice, props);
    pCmdQ->setPerfCountersEnabled();
    Event *ev = new Event(pCmdQ, CL_COMMAND_COPY_BUFFER, 3, 0);
    EXPECT_TRUE(ev->isProfilingEnabled());
    EXPECT_TRUE(ev->isPerfCountersEnabled());
    delete ev;
    delete pCmdQ;
}

TEST(Event, WhenReleasingEventThenEventIsNull) {
    UserEvent *ue = new UserEvent();
    auto autoptr = ue->release();
    ASSERT_TRUE(autoptr.isUnused());
}

HWTEST_F(EventTest, givenVirtualEventWhenCommandSubmittedThenLockCsrOccurs) {
    class MockCommandComputeKernel : public CommandComputeKernel {
      public:
        using CommandComputeKernel::eventsWaitlist;
        MockCommandComputeKernel(CommandQueue &commandQueue, std::unique_ptr<KernelOperation> &kernelOperation, std::vector<Surface *> &surfaces, Kernel *kernel)
            : CommandComputeKernel(commandQueue, kernelOperation, surfaces, false, false, false, nullptr, PreemptionMode::Disabled, kernel, 0) {}
    };
    class MockEvent : public Event {
      public:
        using Event::submitCommand;
        MockEvent(CommandQueue *cmdQueue, cl_command_type cmdType,
                  uint32_t taskLevel, uint32_t taskCount) : Event(cmdQueue, cmdType,
                                                                  taskLevel, taskCount) {}
    };

    MockKernelWithInternals kernel(*pClDevice);

    IndirectHeap *ih1 = nullptr, *ih2 = nullptr, *ih3 = nullptr;
    pCmdQ->allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, 1, ih1);
    pCmdQ->allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, 1, ih2);
    pCmdQ->allocateHeapMemory(IndirectHeap::SURFACE_STATE, 1, ih3);
    auto cmdStream = new LinearStream(pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), 4096, GraphicsAllocation::AllocationType::COMMAND_BUFFER}));

    std::vector<Surface *> surfaces;
    auto kernelOperation = std::make_unique<KernelOperation>(cmdStream, *pDevice->getDefaultEngine().commandStreamReceiver->getInternalAllocationStorage());
    kernelOperation->setHeaps(ih1, ih2, ih3);

    std::unique_ptr<MockCommandComputeKernel> command = std::make_unique<MockCommandComputeKernel>(*pCmdQ, kernelOperation, surfaces, kernel);

    auto virtualEvent = make_releaseable<MockEvent>(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, CompletionStamp::levelNotReady, CompletionStamp::levelNotReady);

    virtualEvent->setCommand(std::move(command));

    virtualEvent->submitCommand(false);

    EXPECT_EQ(pDevice->getUltCommandStreamReceiver<FamilyType>().recursiveLockCounter, 2u);
}

HWTEST_F(EventTest, givenVirtualEventWhenSubmitCommandEventNotReadyAndEventWithoutCommandThenOneLockCsrNeeded) {
    class MockEvent : public Event {
      public:
        using Event::submitCommand;
        MockEvent(CommandQueue *cmdQueue, cl_command_type cmdType,
                  uint32_t taskLevel, uint32_t taskCount) : Event(cmdQueue, cmdType,
                                                                  taskLevel, taskCount) {}
    };

    auto virtualEvent = make_releaseable<MockEvent>(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, CompletionStamp::levelNotReady, CompletionStamp::levelNotReady);

    virtualEvent->submitCommand(false);

    EXPECT_EQ(pDevice->getUltCommandStreamReceiver<FamilyType>().recursiveLockCounter, 1u);
}

HWTEST_F(InternalsEventTest, GivenBufferWithoutZeroCopyOnCommandMapOrUnmapFlushesPreviousTasksBeforeMappingOrUnmapping) {
    struct MockNonZeroCopyBuff : UnalignedBuffer {
        MockNonZeroCopyBuff(int32_t &executionStamp)
            : executionStamp(executionStamp), dataTransferedStamp(-1) {
            hostPtr = &dataTransferedStamp;
            memoryStorage = &executionStamp;
            size = sizeof(executionStamp);
            hostPtrMinSize = size;
        }
        void setIsZeroCopy() {
            isZeroCopy = false;
        }

        void swapCopyDirection() {
            std::swap(hostPtr, memoryStorage);
        }

        int32_t &executionStamp;
        int32_t dataTransferedStamp;
    };

    int32_t executionStamp = 0;
    auto csr = new MockCsr<FamilyType>(executionStamp, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex());
    pDevice->resetCommandStreamReceiver(csr);

    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    auto pCmdQ = make_releaseable<CommandQueue>(mockContext, pClDevice, props);

    MockNonZeroCopyBuff buffer(executionStamp);

    MemObjSizeArray size = {{4, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    auto commandMap = std::unique_ptr<Command>(new CommandMapUnmap(MAP, buffer, size, offset, false, *pCmdQ));
    EXPECT_EQ(0, executionStamp);
    EXPECT_EQ(-1, csr->flushTaskStamp);
    EXPECT_EQ(-1, buffer.dataTransferedStamp);

    auto latestSentFlushTaskCount = csr->peekLatestSentTaskCount();

    commandMap->submit(0, false);
    EXPECT_EQ(1, executionStamp);
    EXPECT_EQ(0, csr->flushTaskStamp);
    EXPECT_EQ(1, buffer.dataTransferedStamp);
    auto latestSentFlushTaskCountAfterSubmit = csr->peekLatestSentTaskCount();
    EXPECT_GT(latestSentFlushTaskCountAfterSubmit, latestSentFlushTaskCount);

    executionStamp = 0;
    csr->flushTaskStamp = -1;
    buffer.dataTransferedStamp = -1;
    buffer.swapCopyDirection();

    auto commandUnMap = std::unique_ptr<Command>(new CommandMapUnmap(UNMAP, buffer, size, offset, false, *pCmdQ));
    EXPECT_EQ(0, executionStamp);
    EXPECT_EQ(-1, csr->flushTaskStamp);
    EXPECT_EQ(-1, buffer.dataTransferedStamp);
    commandUnMap->submit(0, false);
    EXPECT_EQ(1, executionStamp);
    EXPECT_EQ(0, csr->flushTaskStamp);
    EXPECT_EQ(1, buffer.dataTransferedStamp);
    EXPECT_EQ(nullptr, commandUnMap->getCommandStream());
}

TEST(EventCallback, WhenOverridingStatusThenEventUsesNewStatus) {
    struct ClbFuncTempStruct {
        static void CL_CALLBACK ClbFuncT(cl_event e, cl_int status, void *retStatus) {
            *((cl_int *)retStatus) = status;
        }
    };

    cl_int retStatus = 7;
    Event::Callback clb(nullptr, ClbFuncTempStruct::ClbFuncT, CL_COMPLETE, &retStatus);
    EXPECT_EQ(CL_COMPLETE, clb.getCallbackExecutionStatusTarget());
    clb.execute();
    EXPECT_EQ(CL_COMPLETE, retStatus);

    retStatus = 7;
    clb.overrideCallbackExecutionStatusTarget(-1);
    EXPECT_EQ(-1, clb.getCallbackExecutionStatusTarget());
    clb.execute();
    EXPECT_EQ(-1, retStatus);
}

TEST_F(EventTest, WhenSettingTimeStampThenCorrectValuesAreSet) {
    MyEvent ev(this->pCmdQ, CL_COMMAND_COPY_BUFFER, 3, 0);
    TimeStampData inTimeStamp = {1ULL, 2ULL};
    ev.setSubmitTimeStamp(&inTimeStamp);
    TimeStampData outtimeStamp = {0, 0};
    outtimeStamp = ev.getSubmitTimeStamp();
    EXPECT_EQ(1ULL, outtimeStamp.GPUTimeStamp);
    EXPECT_EQ(2ULL, outtimeStamp.CPUTimeinNS);
    inTimeStamp.GPUTimeStamp = 3;
    inTimeStamp.CPUTimeinNS = 4;
    ev.setQueueTimeStamp(&inTimeStamp);
    outtimeStamp = ev.getQueueTimeStamp();
    EXPECT_EQ(3ULL, outtimeStamp.GPUTimeStamp);
    EXPECT_EQ(4ULL, outtimeStamp.CPUTimeinNS);
}

TEST_F(EventTest, WhenSettingCpuTimeStampThenCorrectTimeIsSet) {
    MyEvent ev(this->pCmdQ, CL_COMMAND_COPY_BUFFER, 3, 0);

    ev.setProfilingEnabled(true);
    ev.setQueueTimeStamp();
    TimeStampData outtimeStamp = {0, 0};
    outtimeStamp = ev.getQueueTimeStamp();
    EXPECT_NE(0ULL, outtimeStamp.CPUTimeinNS);
    EXPECT_EQ(0ULL, outtimeStamp.GPUTimeStamp);

    ev.setSubmitTimeStamp();
    outtimeStamp = ev.getSubmitTimeStamp();
    EXPECT_NE(0ULL, outtimeStamp.CPUTimeinNS);
    EXPECT_EQ(0ULL, outtimeStamp.GPUTimeStamp);

    ev.setStartTimeStamp();
    uint64_t outCPUtimeStamp = ev.getStartTimeStamp();
    EXPECT_NE(0ULL, outCPUtimeStamp);

    ev.setEndTimeStamp();
    outCPUtimeStamp = ev.getEndTimeStamp();
    EXPECT_NE(0ULL, outCPUtimeStamp);

    outCPUtimeStamp = ev.getCompleteTimeStamp();
    EXPECT_NE(0ULL, outCPUtimeStamp);
}

TEST_F(EventTest, GivenNoQueueWhenSettingCpuTimeStampThenTimesIsNotSet) {
    MyEvent ev(nullptr, CL_COMMAND_COPY_BUFFER, 3, 0);

    ev.setQueueTimeStamp();
    TimeStampData outtimeStamp = {0, 0};
    outtimeStamp = ev.getQueueTimeStamp();
    EXPECT_EQ(0ULL, outtimeStamp.CPUTimeinNS);
    EXPECT_EQ(0ULL, outtimeStamp.GPUTimeStamp);

    ev.setSubmitTimeStamp();
    outtimeStamp = ev.getSubmitTimeStamp();
    EXPECT_EQ(0ULL, outtimeStamp.CPUTimeinNS);
    EXPECT_EQ(0ULL, outtimeStamp.GPUTimeStamp);

    ev.setStartTimeStamp();
    uint64_t outCPUtimeStamp = ev.getStartTimeStamp();
    EXPECT_EQ(0ULL, outCPUtimeStamp);

    ev.setEndTimeStamp();
    outCPUtimeStamp = ev.getEndTimeStamp();
    EXPECT_EQ(0ULL, outCPUtimeStamp);

    outCPUtimeStamp = ev.getCompleteTimeStamp();
    EXPECT_EQ(0ULL, outCPUtimeStamp);
}

TEST_F(EventTest, WhenGettingHwTimeStampsThenValidPointerIsReturned) {
    std::unique_ptr<Event> event(new Event(this->pCmdQ, CL_COMMAND_COPY_BUFFER, 0, 0));
    ASSERT_NE(nullptr, event);

    HwTimeStamps *timeStamps = event->getHwTimeStampNode()->tagForCpuAccess;
    ASSERT_NE(nullptr, timeStamps);

    //this should not cause any heap corruptions
    ASSERT_EQ(0ULL, timeStamps->GlobalStartTS);
    ASSERT_EQ(0ULL, timeStamps->ContextStartTS);
    ASSERT_EQ(0ULL, timeStamps->GlobalEndTS);
    ASSERT_EQ(0ULL, timeStamps->ContextEndTS);
    ASSERT_EQ(0ULL, timeStamps->GlobalCompleteTS);
    ASSERT_EQ(0ULL, timeStamps->ContextCompleteTS);

    EXPECT_TRUE(timeStamps->isCompleted());

    HwTimeStamps *timeStamps2 = event->getHwTimeStampNode()->tagForCpuAccess;
    ASSERT_EQ(timeStamps, timeStamps2);
}

TEST_F(EventTest, WhenGetHwTimeStampsAllocationThenValidPointerIsReturned) {
    std::unique_ptr<Event> event(new Event(this->pCmdQ, CL_COMMAND_COPY_BUFFER, 0, 0));
    ASSERT_NE(nullptr, event);

    GraphicsAllocation *allocation = event->getHwTimeStampNode()->getBaseGraphicsAllocation();
    ASSERT_NE(nullptr, allocation);

    void *memoryStorage = allocation->getUnderlyingBuffer();
    size_t memoryStorageSize = allocation->getUnderlyingBufferSize();

    EXPECT_NE(nullptr, memoryStorage);
    EXPECT_GT(memoryStorageSize, 0u);
}

TEST_F(EventTest, WhenEventIsCreatedThenHwTimeStampsMemoryIsPlacedInGraphicsAllocation) {
    std::unique_ptr<Event> event(new Event(this->pCmdQ, CL_COMMAND_COPY_BUFFER, 0, 0));
    ASSERT_NE(nullptr, event);

    HwTimeStamps *timeStamps = event->getHwTimeStampNode()->tagForCpuAccess;
    ASSERT_NE(nullptr, timeStamps);

    GraphicsAllocation *allocation = event->getHwTimeStampNode()->getBaseGraphicsAllocation();
    ASSERT_NE(nullptr, allocation);

    void *memoryStorage = allocation->getUnderlyingBuffer();
    size_t graphicsAllocationSize = allocation->getUnderlyingBufferSize();

    EXPECT_GE(timeStamps, memoryStorage);
    EXPECT_LE(timeStamps + 1, ptrOffset(memoryStorage, graphicsAllocationSize));
}

TEST_F(EventTest, GivenNullQueueWhenEventIsCreatedThenProfilingAndPerfCountersAreDisabled) {
    Event ev(nullptr, CL_COMMAND_COPY_BUFFER, 3, 0);
    EXPECT_FALSE(ev.isProfilingEnabled());
    EXPECT_FALSE(ev.isPerfCountersEnabled());
}

TEST_F(EventTest, GivenProfilingDisabledWhenEventIsCreatedThenPerfCountersAreDisabled) {
    Event ev(pCmdQ, CL_COMMAND_COPY_BUFFER, 3, 0);
    EXPECT_FALSE(ev.isProfilingEnabled());
    EXPECT_FALSE(ev.isPerfCountersEnabled());
}

TEST_F(InternalsEventTest, GivenOnlyProfilingEnabledWhenEventIsCreatedThenPerfCountersAreDisabled) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    CommandQueue *pCmdQ = new CommandQueue(mockContext, pClDevice, props);

    Event *ev = new Event(pCmdQ, CL_COMMAND_COPY_BUFFER, 3, 0);
    EXPECT_TRUE(ev->isProfilingEnabled());
    EXPECT_FALSE(ev->isPerfCountersEnabled());

    delete ev;
    delete pCmdQ;
}

TEST_F(EventTest, GivenClSubmittedWhenpeekIsSubmittedThenTrueIsReturned) {
    Event ev(this->pCmdQ, CL_COMMAND_COPY_BUFFER, 3, 0);
    int32_t executionStatusSnapshot = CL_SUBMITTED;
    bool executionStatus = ev.peekIsSubmitted(executionStatusSnapshot);
    EXPECT_EQ(true, executionStatus);
}

TEST_F(EventTest, GivenCompletedEventWhenQueryingExecutionStatusAfterFlushThenCsrIsNotFlushed) {
    cl_int ret;
    Event ev(this->pCmdQ, CL_COMMAND_COPY_BUFFER, 3, 3);
    auto &csr = this->pCmdQ->getGpgpuCommandStreamReceiver();
    *csr.getTagAddress() = 3;
    auto previousTaskLevel = csr.peekTaskLevel();
    EXPECT_GT(3u, previousTaskLevel);
    ret = clFlush(this->pCmdQ);
    ASSERT_EQ(CL_SUCCESS, ret);
    cl_int execState;
    ret = clGetEventInfo(&ev, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(execState), &execState, nullptr);
    ASSERT_EQ(CL_SUCCESS, ret);
    EXPECT_EQ(previousTaskLevel, csr.peekTaskLevel());
}

HWTEST_F(EventTest, GivenEventCreatedOnMapBufferWithoutCommandWhenSubmittingCommandThenTaskCountIsNotUpdated) {
    MockEvent<Event> ev(this->pCmdQ, CL_COMMAND_MAP_BUFFER, CompletionStamp::levelNotReady, CompletionStamp::levelNotReady);

    EXPECT_EQ(CompletionStamp::levelNotReady, ev.peekTaskCount());
    ev.submitCommand(false);
    EXPECT_EQ(0u, ev.peekTaskCount());
}

HWTEST_F(EventTest, GivenEventCreatedOnMapImageWithoutCommandWhenSubmittingCommandThenTaskCountIsNotUpdated) {
    MockEvent<Event> ev(this->pCmdQ, CL_COMMAND_MAP_IMAGE, CompletionStamp::levelNotReady, CompletionStamp::levelNotReady);

    EXPECT_EQ(CompletionStamp::levelNotReady, ev.peekTaskCount());
    ev.submitCommand(false);
    EXPECT_EQ(0u, ev.peekTaskCount());
}

TEST_F(EventTest, givenCmdQueueWithoutProfilingWhenIsCpuProfilingIsCalledThenFalseIsReturned) {
    MockEvent<Event> ev(this->pCmdQ, CL_COMMAND_MAP_IMAGE, CompletionStamp::levelNotReady, CompletionStamp::levelNotReady);
    bool cpuProfiling = ev.isCPUProfilingPath() != 0;
    EXPECT_FALSE(cpuProfiling);
}

TEST_F(EventTest, givenOutEventWhenBlockingEnqueueHandledOnCpuThenUpdateTaskCountAndFlushStampFromCmdQ) {
    std::unique_ptr<Image> image(ImageHelper<Image1dDefaults>::create(&mockContext));
    EXPECT_TRUE(image->mappingOnCpuAllowed());

    pCmdQ->flushStamp->setStamp(10);
    pCmdQ->taskCount = 11;

    size_t origin[3] = {0, 0, 0};
    size_t region[3] = {1, 1, 1};

    cl_int retVal;
    cl_event clEvent;
    pCmdQ->enqueueMapImage(image.get(), CL_TRUE, CL_MAP_READ, origin, region, nullptr, nullptr, 0, nullptr, &clEvent, retVal);

    auto eventObj = castToObject<Event>(clEvent);
    EXPECT_EQ(pCmdQ->taskCount, eventObj->peekTaskCount());
    EXPECT_EQ(pCmdQ->flushStamp->peekStamp(), eventObj->flushStamp->peekStamp());
    eventObj->release();
}

TEST_F(EventTest, givenCmdQueueWithProfilingWhenIsCpuProfilingIsCalledThenTrueIsReturned) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    std::unique_ptr<CommandQueue> pCmdQ(new CommandQueue(&mockContext, pClDevice, props));

    MockEvent<Event> ev(pCmdQ.get(), CL_COMMAND_MAP_IMAGE, CompletionStamp::levelNotReady, CompletionStamp::levelNotReady);
    bool cpuProfiling = ev.isCPUProfilingPath() != 0;
    EXPECT_TRUE(cpuProfiling);
}

TEST(EventCallback, GivenEventWithCallbacksOnPeekHasCallbacksReturnsTrue) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableAsyncEventsHandler.set(false);
    struct ClbFuncTempStruct {
        static void CL_CALLBACK ClbFuncT(cl_event, cl_int, void *) {
        }
    };

    struct SmallMockEvent : Event {
        SmallMockEvent()
            : Event(nullptr, CL_COMMAND_COPY_BUFFER, 0, 0) {
            this->parentCount = 1; // block event
        }
    };

    {
        SmallMockEvent ev;
        EXPECT_FALSE(ev.peekHasCallbacks());
    }

    {
        SmallMockEvent ev;
        ev.addCallback(ClbFuncTempStruct::ClbFuncT, CL_SUBMITTED, nullptr);
        EXPECT_TRUE(ev.peekHasCallbacks());
        ev.decRefInternal();
    }

    {
        SmallMockEvent ev;
        ev.addCallback(ClbFuncTempStruct::ClbFuncT, CL_RUNNING, nullptr);
        EXPECT_TRUE(ev.peekHasCallbacks());
        ev.decRefInternal();
    }

    {
        SmallMockEvent ev;
        ev.addCallback(ClbFuncTempStruct::ClbFuncT, CL_COMPLETE, nullptr);
        EXPECT_TRUE(ev.peekHasCallbacks());
        ev.decRefInternal();
    }

    {
        SmallMockEvent ev;
        ev.addCallback(ClbFuncTempStruct::ClbFuncT, CL_SUBMITTED, nullptr);
        ev.addCallback(ClbFuncTempStruct::ClbFuncT, CL_COMPLETE, nullptr);
        EXPECT_TRUE(ev.peekHasCallbacks());
        ev.decRefInternal();
        ev.decRefInternal();
    }

    {
        SmallMockEvent ev;
        ev.addCallback(ClbFuncTempStruct::ClbFuncT, CL_RUNNING, nullptr);
        ev.addCallback(ClbFuncTempStruct::ClbFuncT, CL_COMPLETE, nullptr);
        EXPECT_TRUE(ev.peekHasCallbacks());
        ev.decRefInternal();
        ev.decRefInternal();
    }

    {
        SmallMockEvent ev;
        ev.addCallback(ClbFuncTempStruct::ClbFuncT, CL_SUBMITTED, nullptr);
        ev.addCallback(ClbFuncTempStruct::ClbFuncT, CL_RUNNING, nullptr);
        ev.addCallback(ClbFuncTempStruct::ClbFuncT, CL_COMPLETE, nullptr);
        EXPECT_TRUE(ev.peekHasCallbacks());
        ev.decRefInternal();
        ev.decRefInternal();
        ev.decRefInternal();
    }
}

TEST_F(EventTest, GivenNotCompletedEventWhenAddingChildThenNumEventsBlockingThisIsGreaterThanZero) {
    VirtualEvent virtualEvent(pCmdQ, &mockContext);
    {
        Event event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
        event.addChild(virtualEvent);
        EXPECT_NE(0U, virtualEvent.peekNumEventsBlockingThis());
    }
}

TEST(Event, whenCreatingRegularEventsThenExternalSynchronizationIsNotRequired) {
    Event *event = new Event(nullptr, 0, 0, 0);
    EXPECT_FALSE(event->isExternallySynchronized());
    event->release();

    UserEvent *userEvent = new UserEvent();
    EXPECT_FALSE(userEvent->isExternallySynchronized());
    userEvent->release();

    VirtualEvent *virtualEvent = new VirtualEvent();
    EXPECT_FALSE(virtualEvent->isExternallySynchronized());
    virtualEvent->release();
}

HWTEST_F(EventTest, givenEventWithNotReadyTaskLevelWhenUnblockedThenGetTaskLevelFromCsrIfGreaterThanParent) {
    uint32_t initialTaskLevel = 10;
    Event parentEventWithGreaterTaskLevel(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, initialTaskLevel + 5, 0);
    Event parentEventWithLowerTaskLevel(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, initialTaskLevel - 5, 0);

    Event childEvent0(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, CompletionStamp::levelNotReady, CompletionStamp::levelNotReady);
    Event childEvent1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, CompletionStamp::levelNotReady, CompletionStamp::levelNotReady);

    auto &csr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> &>(pCmdQ->getGpgpuCommandStreamReceiver());
    csr.taskLevel = initialTaskLevel;

    parentEventWithGreaterTaskLevel.addChild(childEvent0);
    parentEventWithLowerTaskLevel.addChild(childEvent1);

    parentEventWithGreaterTaskLevel.setStatus(CL_COMPLETE);
    parentEventWithLowerTaskLevel.setStatus(CL_COMPLETE);

    EXPECT_EQ(parentEventWithGreaterTaskLevel.getTaskLevel() + 1, childEvent0.getTaskLevel());
    EXPECT_EQ(csr.taskLevel, childEvent1.getTaskLevel());
}

TEST_F(EventTest, GivenCompletedEventWhenAddingChildThenNumEventsBlockingThisIsZero) {
    VirtualEvent virtualEvent(pCmdQ, &mockContext);
    {
        Event event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
        event.setStatus(CL_COMPLETE);
        event.addChild(virtualEvent);
        EXPECT_EQ(0U, virtualEvent.peekNumEventsBlockingThis());
    }
}

HWTEST_F(EventTest, givenQuickKmdSleepRequestWhenWaitIsCalledThenPassRequestToWaitingFunction) {
    struct MyCsr : public UltCommandStreamReceiver<FamilyType> {
        MyCsr(const ExecutionEnvironment &executionEnvironment) : UltCommandStreamReceiver<FamilyType>(const_cast<ExecutionEnvironment &>(executionEnvironment), 0) {}
        MOCK_METHOD3(waitForCompletionWithTimeout, bool(bool enableTimeout, int64_t timeoutMs, uint32_t taskCountToWait));
    };
    HardwareInfo localHwInfo = pDevice->getHardwareInfo();
    localHwInfo.capabilityTable.kmdNotifyProperties.enableKmdNotify = true;
    localHwInfo.capabilityTable.kmdNotifyProperties.enableQuickKmdSleep = true;
    localHwInfo.capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds = 1;
    localHwInfo.capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds = 2;

    pDevice->executionEnvironment->setHwInfo(&localHwInfo);

    auto csr = new ::testing::NiceMock<MyCsr>(*pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(csr);

    Event event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
    event.updateCompletionStamp(1u, 1u, 1u);

    EXPECT_CALL(*csr, waitForCompletionWithTimeout(::testing::_,
                                                   localHwInfo.capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(true));

    event.wait(true, true);
}

HWTEST_F(EventTest, givenNonQuickKmdSleepRequestWhenWaitIsCalledThenPassRequestToWaitingFunction) {
    struct MyCsr : public UltCommandStreamReceiver<FamilyType> {
        MyCsr(const ExecutionEnvironment &executionEnvironment) : UltCommandStreamReceiver<FamilyType>(const_cast<ExecutionEnvironment &>(executionEnvironment), 0) {}
        MOCK_METHOD3(waitForCompletionWithTimeout, bool(bool enableTimeout, int64_t timeoutMs, uint32_t taskCountToWait));
    };
    HardwareInfo localHwInfo = pDevice->getHardwareInfo();
    localHwInfo.capabilityTable.kmdNotifyProperties.enableKmdNotify = true;
    localHwInfo.capabilityTable.kmdNotifyProperties.enableQuickKmdSleep = true;
    localHwInfo.capabilityTable.kmdNotifyProperties.enableQuickKmdSleepForSporadicWaits = false;
    localHwInfo.capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds = 1;
    localHwInfo.capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds = 2;

    pDevice->executionEnvironment->setHwInfo(&localHwInfo);

    auto csr = new ::testing::NiceMock<MyCsr>(*pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(csr);

    Event event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
    event.updateCompletionStamp(1u, 1u, 1u);

    EXPECT_CALL(*csr, waitForCompletionWithTimeout(::testing::_,
                                                   localHwInfo.capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(true));

    event.wait(true, false);
}

HWTEST_F(InternalsEventTest, givenCommandWhenSubmitCalledThenUpdateFlushStamp) {
    auto pCmdQ = std::unique_ptr<CommandQueue>(new CommandQueue(mockContext, pClDevice, 0));
    MockEvent<Event> *event = new MockEvent<Event>(pCmdQ.get(), CL_COMMAND_MARKER, 0, 0);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.flushStamp->setStamp(5);

    FlushStamp expectedFlushStamp = 0;
    EXPECT_EQ(expectedFlushStamp, event->flushStamp->peekStamp());
    event->setCommand(std::unique_ptr<Command>(new CommandWithoutKernel(*pCmdQ)));
    event->submitCommand(false);
    EXPECT_EQ(csr.flushStamp->peekStamp(), event->flushStamp->peekStamp());
    delete event;
}

HWTEST_F(InternalsEventTest, givenAbortedCommandWhenSubmitCalledThenDontUpdateFlushStamp) {
    auto pCmdQ = std::unique_ptr<CommandQueue>(new CommandQueue(mockContext, pClDevice, 0));
    MockEvent<Event> *event = new MockEvent<Event>(pCmdQ.get(), CL_COMMAND_MARKER, 0, 0);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.flushStamp->setStamp(5);

    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto pKernel = mockKernelWithInternals.mockKernel;

    auto cmdStream = new LinearStream(pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), 4096, GraphicsAllocation::AllocationType::COMMAND_BUFFER}));
    IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    pCmdQ->allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, 4096u, dsh);
    pCmdQ->allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, 4096u, ioh);
    pCmdQ->allocateHeapMemory(IndirectHeap::SURFACE_STATE, 4096u, ssh);
    auto blockedCommandsData = std::make_unique<KernelOperation>(cmdStream, *pCmdQ->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    blockedCommandsData->setHeaps(dsh, ioh, ssh);
    PreemptionMode preemptionMode = pDevice->getPreemptionMode();
    std::vector<Surface *> v;
    auto cmd = new CommandComputeKernel(*pCmdQ, blockedCommandsData, v, false, false, false, nullptr, preemptionMode, pKernel, 1);
    event->setCommand(std::unique_ptr<Command>(cmd));

    FlushStamp expectedFlushStamp = 0;

    EXPECT_EQ(expectedFlushStamp, event->flushStamp->peekStamp());
    event->submitCommand(true);

    EXPECT_EQ(expectedFlushStamp, event->flushStamp->peekStamp());
    delete event;
}

TEST(EventLockerTests, givenEventWhenEventLockerIsUsedThenOwnershipIsAutomaticallyReleased) {
    Event ev(nullptr, CL_COMMAND_COPY_BUFFER, 3, 0);
    {
        TakeOwnershipWrapper<Event> locker(ev);
        EXPECT_TRUE(ev.hasOwnership());
    }
    EXPECT_FALSE(ev.hasOwnership());
}

TEST(EventLockerTests, givenEventWhenEventLockerIsUsedAndUnlockedThenOwnershipIsReleased) {
    Event ev(nullptr, CL_COMMAND_COPY_BUFFER, 3, 0);
    {
        TakeOwnershipWrapper<Event> locker(ev);
        locker.unlock();
        EXPECT_FALSE(ev.hasOwnership());
    }
    EXPECT_FALSE(ev.hasOwnership());
}

TEST(EventLockerTests, givenEventWhenEventLockerIsUsedAndlockedThenOwnershipIsAcquiredAgain) {
    Event ev(nullptr, CL_COMMAND_COPY_BUFFER, 3, 0);
    {
        TakeOwnershipWrapper<Event> locker(ev);
        locker.unlock();
        locker.lock();
        EXPECT_TRUE(ev.hasOwnership());
    }
    EXPECT_FALSE(ev.hasOwnership());
}
TEST(EventLockerTests, givenEventWhenEventLockerIsLockedTwiceThenOwnershipIsReleaseAfterLeavingTheScope) {
    Event ev(nullptr, CL_COMMAND_COPY_BUFFER, 3, 0);
    {
        TakeOwnershipWrapper<Event> locker(ev);
        locker.lock();
        EXPECT_TRUE(ev.hasOwnership());
    }
    EXPECT_FALSE(ev.hasOwnership());
}

TEST(EventsDebug, givenEventWhenTrackingOfParentsIsOnThenTrackParents) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.TrackParentEvents.set(true);
    Event event(nullptr, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
    Event event2(nullptr, CL_COMMAND_NDRANGE_KERNEL, 0, 0);

    auto &parentEvents = event.getParentEvents();
    auto &parentEvents2 = event2.getParentEvents();

    EXPECT_EQ(0u, parentEvents.size());
    EXPECT_EQ(0u, parentEvents2.size());

    event.addChild(event2);
    EXPECT_EQ(0u, parentEvents.size());
    EXPECT_EQ(1u, parentEvents2.size());
    EXPECT_EQ(&event, parentEvents2.at(0));
    event.setStatus(CL_COMPLETE);
}

TEST(EventsDebug, givenEventWhenTrackingOfParentsIsOffThenDoNotTrackParents) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.TrackParentEvents.set(false);
    Event event(nullptr, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
    Event event2(nullptr, CL_COMMAND_NDRANGE_KERNEL, 0, 0);

    auto &parentEvents = event.getParentEvents();
    auto &parentEvents2 = event2.getParentEvents();

    EXPECT_EQ(0u, parentEvents.size());
    EXPECT_EQ(0u, parentEvents2.size());

    event.addChild(event2);
    EXPECT_EQ(0u, parentEvents.size());
    EXPECT_EQ(0u, parentEvents2.size());
    event.setStatus(CL_COMPLETE);
}
