/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/hw_timestamps.h"
#include "shared/source/utilities/perf_counter.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_ostime.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/helpers/task_information.h"
#include "opencl/source/memory_manager/mem_obj_surface.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/mocks/mock_printf_handler.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/os_interface/mock_performance_counters.h"

#include "event_fixture.h"

#include <memory>
#include <type_traits>

using namespace NEO;

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
    MockCommandQueue cmdQ(&ctx, mockDevice.get(), 0, false);
    Event event(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, CompletionStamp::notReady, 0);

    EXPECT_FALSE(event.peekIsBlocked());
    EXPECT_EQ(CL_QUEUED, event.peekExecutionStatus());
    event.updateExecutionStatus();
    EXPECT_EQ(CL_QUEUED, event.peekExecutionStatus());
}

TEST(Event, givenEventThatStatusChangeWhenPeekIsCalledThenEventIsNotUpdated) {
    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext ctx;
    MockCommandQueue cmdQ(&ctx, mockDevice.get(), 0, false);

    struct mockEvent : public Event {
        using Event::Event;
        void updateExecutionStatus() override {
            callCount++;
        }
        uint32_t callCount = 0u;
    };

    mockEvent event(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, CompletionStamp::notReady, 0);
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
    event->updateTaskCount(8, 0);
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
    auto cmdQ = std::unique_ptr<MockCommandQueue>(new MockCommandQueue(ctx.get(), mockDevice.get(), 0, false));
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

TEST(Event, givenBcsCsrSetInEventWhenPeekingBcsTaskCountThenReturnCorrectTaskCount) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    REQUIRE_FULL_BLITTER_OR_SKIP(&hwInfo);

    auto device = ReleaseableObjectPtr<MockClDevice>{
        new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockAlignedMallocManagerDevice>(&hwInfo)}};
    MockContext context{device.get()};
    MockCommandQueue queue{context};
    queue.constructBcsEngine(false);
    queue.updateBcsTaskCount(queue.bcsEngines[0]->getEngineType(), 19);
    Event event{&queue, CL_COMMAND_READ_BUFFER, 0, 0};

    EXPECT_EQ(0u, event.peekBcsTaskCountFromCommandQueue());

    event.setupBcs(queue.bcsEngines[0]->getEngineType());
    EXPECT_EQ(19u, event.peekBcsTaskCountFromCommandQueue());
}

TEST(Event, givenCommandQueueWhenEventIsCreatedWithCommandQueueThenCommandQueueInternalRefCountIsIncremented) {
    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext ctx;
    MockCommandQueue cmdQ(&ctx, mockDevice.get(), 0, false);
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
    MockCommandQueue cmdQ(&ctx, nullptr, 0, false);
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
        MockCommandQueueWithFlushCheck(Context &context, ClDevice *device) : MockCommandQueue(&context, device, nullptr, false) {
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
        MockCommandQueueWithFlushCheck(Context &context, ClDevice *device) : MockCommandQueue(&context, device, nullptr, false) {
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
    std::unique_ptr<Event> event1(new Event(cmdQ1.get(), CL_COMMAND_NDRANGE_KERNEL, CompletionStamp::notReady, 0));
    cl_event eventWaitlist[] = {event1.get()};

    Event::waitForEvents(1, eventWaitlist);

    EXPECT_EQ(0u, cmdQ1->flushCounter);
}

TEST(Event, givenNotReadyEventOnWaitlistWhenCheckingUserEventDependeciesThenTrueIsReturned) {
    auto event1 = std::make_unique<Event>(nullptr, CL_COMMAND_NDRANGE_KERNEL, CompletionStamp::notReady, 0);
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
    Event event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, CompletionStamp::notReady, CompletionStamp::notReady);
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
    ASSERT_NE(nullptr, pEvent); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

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
    retVal = pEvent->getReference(); // NOLINT(clang-analyzer-cplusplus.NewDelete)
    EXPECT_EQ(1, retVal);

    delete pEvent;
}

TEST_F(EventTest, WhenGettingClEventContextThenCorrectValueIsReturned) {
    uint32_t tagEvent = 5;

    Event *pEvent = new Event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 3, tagEvent);
    ASSERT_NE(nullptr, pEvent); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

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
    Event event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 3, CompletionStamp::notReady);
    auto result = event.wait(false, false);
    EXPECT_EQ(WaitStatus::NotReady, result);
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
    auto temporary = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), false, size, device->getDeviceBitfield()}, ptr);
    temporary->updateTaskCount(3, commandQueue->getGpgpuCommandStreamReceiver().getOsContext().getContextId());
    commandQueue->getGpgpuCommandStreamReceiver().getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(temporary), TEMPORARY_ALLOCATION);
    Event event(commandQueue.get(), CL_COMMAND_NDRANGE_KERNEL, 3, 3);

    EXPECT_EQ(1u, hostPtrManager->getFragmentCount());

    event.updateExecutionStatus();

    EXPECT_EQ(0u, hostPtrManager->getFragmentCount());
}

TEST_F(InternalsEventTest, GivenSubmitCommandFalseWhenSubmittingCommandsThenRefApiCountAndRefInternalGetHandledCorrectly) {
    MockCommandQueue cmdQ(mockContext, pClDevice, nullptr, false);
    MockEvent<Event> event(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);

    auto cmdStream = new LinearStream(pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), true, 4096, AllocationType::COMMAND_BUFFER, false, pDevice->getDeviceBitfield()}));
    IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    cmdQ.allocateHeapMemory(IndirectHeap::Type::DYNAMIC_STATE, 4096u, dsh);
    cmdQ.allocateHeapMemory(IndirectHeap::Type::INDIRECT_OBJECT, 4096u, ioh);
    cmdQ.allocateHeapMemory(IndirectHeap::Type::SURFACE_STATE, 4096u, ssh);

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

    auto graphicsAllocation = buffer.getGraphicsAllocation(pClDevice->getRootDeviceIndex());

    EXPECT_FALSE(graphicsAllocation->isResident(csr.getOsContext().getContextId()));
}

TEST_F(InternalsEventTest, GivenSubmitCommandTrueWhenSubmittingCommandsThenRefApiCountAndRefInternalGetHandledCorrectly) {
    MockCommandQueue cmdQ(mockContext, pClDevice, nullptr, false);
    MockEvent<Event> event(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);

    auto cmdStream = new LinearStream(pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), 4096, AllocationType::COMMAND_BUFFER, pDevice->getDeviceBitfield()}));
    IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    cmdQ.allocateHeapMemory(IndirectHeap::Type::DYNAMIC_STATE, 4096u, dsh);
    cmdQ.allocateHeapMemory(IndirectHeap::Type::INDIRECT_OBJECT, 4096u, ioh);
    cmdQ.allocateHeapMemory(IndirectHeap::Type::SURFACE_STATE, 4096u, ssh);

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
    MockCommandQueue mockCmdQueue(mockContext, pClDevice, nullptr, false);

    testing::internal::CaptureStdout();
    MockEvent<Event> event(&mockCmdQueue, CL_COMMAND_NDRANGE_KERNEL, 0, 0);

    auto cmdStream = new LinearStream(pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), 4096, AllocationType::COMMAND_BUFFER, pDevice->getDeviceBitfield()}));
    IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    mockCmdQueue.allocateHeapMemory(IndirectHeap::Type::DYNAMIC_STATE, 4096u, dsh);
    mockCmdQueue.allocateHeapMemory(IndirectHeap::Type::INDIRECT_OBJECT, 4096u, ioh);
    mockCmdQueue.allocateHeapMemory(IndirectHeap::Type::SURFACE_STATE, 4096u, ssh);

    auto blockedCommandsData = std::make_unique<KernelOperation>(cmdStream, *mockCmdQueue.getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    blockedCommandsData->setHeaps(dsh, ioh, ssh);

    std::string testString = "test";

    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto pKernel = mockKernelWithInternals.mockKernel;

    auto &kernelInfo = mockKernelWithInternals.kernelInfo;
    kernelInfo.kernelDescriptor.kernelAttributes.binaryFormat = DeviceBinaryFormat::Patchtokens;
    kernelInfo.setPrintfSurface(sizeof(uintptr_t), 0);
    kernelInfo.addToPrintfStringsMap(0, testString);

    uint64_t crossThread[10];
    pKernel->setCrossThreadData(&crossThread, sizeof(uint64_t) * 8);

    MockMultiDispatchInfo multiDispatchInfo(pClDevice, pKernel);
    std::unique_ptr<PrintfHandler> printfHandler(PrintfHandler::create(multiDispatchInfo, *pClDevice));
    printfHandler->prepareDispatch(multiDispatchInfo);
    auto surface = printfHandler->getSurface();

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
}

TEST_F(InternalsEventTest, givenGpuHangOnCmdQueueWaitFunctionAndBlockedKernelWithPrintfWhenSubmittedThenEventIsAbortedAndHangIsReported) {
    MockCommandQueue mockCmdQueue(mockContext, pClDevice, nullptr, false);
    mockCmdQueue.waitUntilCompleteReturnValue = WaitStatus::GpuHang;

    testing::internal::CaptureStdout();
    MockEvent<Event> event(&mockCmdQueue, CL_COMMAND_NDRANGE_KERNEL, 0, 0);

    auto cmdStream = new LinearStream(pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), 4096, AllocationType::COMMAND_BUFFER, pDevice->getDeviceBitfield()}));
    IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    mockCmdQueue.allocateHeapMemory(IndirectHeap::Type::DYNAMIC_STATE, 4096u, dsh);
    mockCmdQueue.allocateHeapMemory(IndirectHeap::Type::INDIRECT_OBJECT, 4096u, ioh);
    mockCmdQueue.allocateHeapMemory(IndirectHeap::Type::SURFACE_STATE, 4096u, ssh);

    auto blockedCommandsData = std::make_unique<KernelOperation>(cmdStream, *mockCmdQueue.getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    blockedCommandsData->setHeaps(dsh, ioh, ssh);

    std::string testString = "test";

    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto pKernel = mockKernelWithInternals.mockKernel;

    auto &kernelInfo = mockKernelWithInternals.kernelInfo;
    kernelInfo.kernelDescriptor.kernelAttributes.binaryFormat = DeviceBinaryFormat::Patchtokens;
    kernelInfo.setPrintfSurface(sizeof(uintptr_t), 0);
    kernelInfo.addToPrintfStringsMap(0, testString);

    uint64_t crossThread[10];
    pKernel->setCrossThreadData(&crossThread, sizeof(uint64_t) * 8);

    MockMultiDispatchInfo multiDispatchInfo(pClDevice, pKernel);
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
    EXPECT_EQ(Event::executionAbortedDueToGpuHang, event.peekExecutionStatus());

    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STREQ("test", output.c_str());
}

TEST_F(InternalsEventTest, givenGpuHangOnPrintingEnqueueOutputAndBlockedKernelWithPrintfWhenSubmittedThenEventIsAbortedAndHangIsReported) {
    MockCommandQueue mockCmdQueue(mockContext, pClDevice, nullptr, false);

    testing::internal::CaptureStdout();
    MockEvent<Event> event(&mockCmdQueue, CL_COMMAND_NDRANGE_KERNEL, 0, 0);

    auto cmdStream = new LinearStream(pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), 4096, AllocationType::COMMAND_BUFFER, pDevice->getDeviceBitfield()}));
    IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    mockCmdQueue.allocateHeapMemory(IndirectHeap::Type::DYNAMIC_STATE, 4096u, dsh);
    mockCmdQueue.allocateHeapMemory(IndirectHeap::Type::INDIRECT_OBJECT, 4096u, ioh);
    mockCmdQueue.allocateHeapMemory(IndirectHeap::Type::SURFACE_STATE, 4096u, ssh);

    auto blockedCommandsData = std::make_unique<KernelOperation>(cmdStream, *mockCmdQueue.getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    blockedCommandsData->setHeaps(dsh, ioh, ssh);

    std::string testString = "test";

    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto pKernel = mockKernelWithInternals.mockKernel;

    auto &kernelInfo = mockKernelWithInternals.kernelInfo;
    kernelInfo.kernelDescriptor.kernelAttributes.binaryFormat = DeviceBinaryFormat::Patchtokens;
    kernelInfo.setPrintfSurface(sizeof(uintptr_t), 0);
    kernelInfo.addToPrintfStringsMap(0, testString);

    uint64_t crossThread[10];
    pKernel->setCrossThreadData(&crossThread, sizeof(uint64_t) * 8);

    MockMultiDispatchInfo multiDispatchInfo(pClDevice, pKernel);
    std::unique_ptr<MockPrintfHandler> printfHandler(new MockPrintfHandler(*pClDevice));
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
    EXPECT_EQ(Event::executionAbortedDueToGpuHang, event.peekExecutionStatus());

    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.empty());
}

TEST_F(InternalsEventTest, GivenMapOperationWhenSubmittingCommandsThenTaskLevelIsIncremented) {
    auto pCmdQ = makeReleaseable<MockCommandQueue>(mockContext, pClDevice, nullptr, false);
    MockEvent<Event> event(pCmdQ.get(), CL_COMMAND_NDRANGE_KERNEL, 0, 0);

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
    auto pCmdQ = makeReleaseable<MockCommandQueue>(mockContext, pClDevice, nullptr, false);
    MockEvent<Event> event(pCmdQ.get(), CL_COMMAND_NDRANGE_KERNEL, 0, 0);

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
    std::unique_ptr<MockCommandQueue> pCmdQ(new MockCommandQueue(mockContext, pClDevice, props, false));

    std::unique_ptr<MockEvent<Event>> event(new MockEvent<Event>(pCmdQ.get(), GetParam(), 0, 0));
    EXPECT_TRUE(event->isProfilingEnabled());
}

INSTANTIATE_TEST_CASE_P(InternalsEventProfilingTest,
                        InternalsEventProfilingTest,
                        ::testing::ValuesIn(commands));

TEST_F(InternalsEventTest, GivenProfilingWhenUserEventCreatedThenProfilingNotSet) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    std::unique_ptr<MockCommandQueue> pCmdQ(new MockCommandQueue(mockContext, pClDevice, props, false));

    std::unique_ptr<MockEvent<Event>> event(new MockEvent<Event>(pCmdQ.get(), CL_COMMAND_USER, 0, 0));
    EXPECT_FALSE(event->isProfilingEnabled());
}

TEST_F(InternalsEventTest, givenDeviceTimestampBaseNotEnabledWhenGetEventProfilingInfoThenCpuTimestampIsReturned) {
    pClDevice->setOSTime(new MockOSTimeWithConstTimestamp());
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    MockCommandQueue cmdQ(mockContext, pClDevice, props, false);
    MockEvent<Event> event(&cmdQ, CL_COMMAND_MARKER, 0, 0);

    event.setCommand(std::unique_ptr<Command>(new CommandWithoutKernel(cmdQ)));

    event.submitCommand(false);
    uint64_t submitTime = 0ULL;
    event.getEventProfilingInfo(CL_PROFILING_COMMAND_SUBMIT, sizeof(uint64_t), &submitTime, 0);

    EXPECT_EQ(submitTime, MockDeviceTimeWithConstTimestamp::CPU_TIME_IN_NS);
}

TEST_F(InternalsEventTest, givenDeviceTimestampBaseEnabledWhenGetEventProfilingInfoThenGpuTimestampIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableDeviceBasedTimestamps.set(true);

    pClDevice->setOSTime(new MockOSTimeWithConstTimestamp());
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    MockCommandQueue cmdQ(mockContext, pClDevice, props, false);
    MockEvent<Event> event(&cmdQ, CL_COMMAND_MARKER, 0, 0);
    event.queueTimeStamp.GPUTimeStamp = MockDeviceTimeWithConstTimestamp::GPU_TIMESTAMP;

    event.setCommand(std::unique_ptr<Command>(new CommandWithoutKernel(cmdQ)));
    event.submitCommand(false);
    uint64_t submitTime = 0ULL;
    event.getEventProfilingInfo(CL_PROFILING_COMMAND_SUBMIT, sizeof(uint64_t), &submitTime, 0);

    auto resolution = pClDevice->getDevice().getDeviceInfo().profilingTimerResolution;
    EXPECT_EQ(submitTime, static_cast<uint64_t>(MockDeviceTimeWithConstTimestamp::GPU_TIMESTAMP * resolution));
}

TEST_F(InternalsEventTest, givenDeviceTimestampBaseEnabledWhenCalculateStartTimestampThenCorrectTimeIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableDeviceBasedTimestamps.set(true);

    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    MockCommandQueue cmdQ(mockContext, pClDevice, props, false);
    MockEvent<Event> event(&cmdQ, CL_COMPLETE, 0, 0);

    HwTimeStamps timestamp{};
    timestamp.GlobalStartTS = 2;
    event.queueTimeStamp.GPUTimeStamp = 1;
    TagNode<HwTimeStamps> timestampNode{};
    timestampNode.tagForCpuAccess = &timestamp;
    event.timeStampNode = &timestampNode;

    uint64_t start;
    event.getEventProfilingInfo(CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, nullptr);

    auto resolution = pClDevice->getDevice().getDeviceInfo().profilingTimerResolution;
    EXPECT_EQ(start, static_cast<uint64_t>(timestamp.GlobalStartTS * resolution));

    event.timeStampNode = nullptr;
}

TEST_F(InternalsEventTest, givenDeviceTimestampBaseEnabledAndGlobalStartTSSmallerThanQueueTSWhenCalculateStartTimestampThenCorrectTimeIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableDeviceBasedTimestamps.set(true);

    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    MockCommandQueue cmdQ(mockContext, pClDevice, props, false);
    MockEvent<Event> event(&cmdQ, CL_COMPLETE, 0, 0);

    HwTimeStamps timestamp{};
    timestamp.GlobalStartTS = 1;
    event.queueTimeStamp.GPUTimeStamp = 2;
    TagNode<HwTimeStamps> timestampNode{};
    timestampNode.tagForCpuAccess = &timestamp;
    event.timeStampNode = &timestampNode;

    uint64_t start = 0u;
    event.getEventProfilingInfo(CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, nullptr);

    auto &hwHelper = HwHelper::get(pClDevice->getHardwareInfo().platform.eRenderCoreFamily);
    auto resolution = pClDevice->getDevice().getDeviceInfo().profilingTimerResolution;
    auto refStartTime = static_cast<uint64_t>(timestamp.GlobalStartTS * resolution + (1ULL << hwHelper.getGlobalTimeStampBits()) * resolution);
    EXPECT_EQ(start, refStartTime);

    event.timeStampNode = nullptr;
}

TEST_F(InternalsEventTest, givenGpuHangWhenEventWaitReportsHangThenWaititingIsAbortedAndUnfinishedEventsHaveExecutionStatusEqualsToAbortedDueToGpuHang) {
    MockCommandQueue cmdQ(mockContext, pClDevice, nullptr, false);

    MockEvent<Event> passingEvent(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
    passingEvent.waitReturnValue = WaitStatus::Ready;

    MockEvent<Event> hangingEvent(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
    hangingEvent.waitReturnValue = WaitStatus::GpuHang;

    cl_event eventWaitlist[] = {&passingEvent, &hangingEvent};

    const auto result = Event::waitForEvents(2, eventWaitlist);
    EXPECT_EQ(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST, result);

    EXPECT_NE(Event::executionAbortedDueToGpuHang, passingEvent.peekExecutionStatus());
    EXPECT_EQ(Event::executionAbortedDueToGpuHang, hangingEvent.peekExecutionStatus());
}

TEST_F(InternalsEventTest, givenPassingEventWhenWaitingForEventsThenWaititingIsSuccessfulAndEventIsNotAborted) {
    MockCommandQueue cmdQ(mockContext, pClDevice, nullptr, false);

    MockEvent<Event> passingEvent(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
    passingEvent.waitReturnValue = WaitStatus::Ready;

    cl_event eventWaitlist[] = {&passingEvent};

    const auto result = Event::waitForEvents(1, eventWaitlist);
    EXPECT_EQ(CL_SUCCESS, result);

    EXPECT_NE(Event::executionAbortedDueToGpuHang, passingEvent.peekExecutionStatus());
}

TEST_F(InternalsEventTest, GivenProfilingWHENMapOperationTHENTimesSet) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    MockCommandQueue *pCmdQ = new MockCommandQueue(mockContext, pClDevice, props, false);

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
    auto pCmdQ = makeReleaseable<MockCommandQueue>(mockContext, pClDevice, props, false);
    MockEvent<Event> event(pCmdQ.get(), CL_COMMAND_NDRANGE_KERNEL, 0, 0);

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

TEST_F(InternalsEventTest, givenBlockedMapCommandWhenSubmitIsCalledThenItReleasesMemObjectReference) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    auto pCmdQ = std::make_unique<MockCommandQueue>(mockContext, pClDevice, props, false);
    MockEvent<Event> event(pCmdQ.get(), CL_COMMAND_NDRANGE_KERNEL, 0, 0);

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
    auto pCmdQ = std::make_unique<MockCommandQueue>(mockContext, pClDevice, props, false);
    MockEvent<Event> event(pCmdQ.get(), CL_COMMAND_NDRANGE_KERNEL, 0, 0);

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

class MockCommand : public Command {
  public:
    using Command::Command;

    CompletionStamp &submit(uint32_t taskLevel, bool terminated) override {
        return completionStamp;
    }
};

TEST_F(InternalsEventTest, GivenHangingCommandWhenSubmittingItThenTaskIsAborted) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    auto cmdQ = std::make_unique<MockCommandQueue>(mockContext, pClDevice, props, false);

    auto command = std::make_unique<MockCommand>(*cmdQ);
    command->completionStamp.taskCount = CompletionStamp::gpuHang;

    MockEvent<Event> event(cmdQ.get(), CL_COMMAND_NDRANGE_KERNEL, 0, 0);
    event.setCommand(std::move(command));
    event.submitCommand(false);

    EXPECT_EQ(Event::executionAbortedDueToGpuHang, event.peekExecutionStatus());
}

HWTEST_F(InternalsEventTest, givenCpuProfilingPathWhenEnqueuedMarkerThenDontUseTimeStampNode) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    MockCommandQueue *pCmdQ = new MockCommandQueue(mockContext, pClDevice, props, false);
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
        pDevice->setPerfCounters(MockPerformanceCounters::create());
    }

    void TearDown() override {
        InternalsEventTest::TearDown();
        PerformanceCountersFixture::TearDown();
    }
};
HWTEST_F(InternalsEventWithPerfCountersTest, givenCpuProfilingPerfCountersPathWhenEnqueuedMarkerThenDontUseTimeStampNodePerfCounterNode) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    MockCommandQueue *pCmdQ = new MockCommandQueue(mockContext, pClDevice, props, false);
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
    MockCommandQueue *pCmdQ = new MockCommandQueue(mockContext, pClDevice, props, false);
    pCmdQ->setPerfCountersEnabled();
    MockEvent<Event> *event = new MockEvent<Event>(pCmdQ, CL_COMMAND_MARKER, 0, 0);
    event->setCPUProfilingPath(true);
    HwPerfCounter *perfCounter = static_cast<TagNode<HwPerfCounter> *>(event->getHwPerfCounterNode())->tagForCpuAccess;
    ASSERT_NE(nullptr, perfCounter);

    auto hwTimeStampNode = static_cast<TagNode<HwTimeStamps> *>(event->getHwTimeStampNode());
    if (pCmdQ->getTimestampPacketContainer()) {
        EXPECT_EQ(nullptr, hwTimeStampNode);
    } else {
        ASSERT_NE(nullptr, hwTimeStampNode->tagForCpuAccess);
    }

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
    MockCommandQueue *pCmdQ = new MockCommandQueue(mockContext, pClDevice, props, false);
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
    pCmdQ->allocateHeapMemory(IndirectHeap::Type::DYNAMIC_STATE, 1, ih1);
    pCmdQ->allocateHeapMemory(IndirectHeap::Type::INDIRECT_OBJECT, 1, ih2);
    pCmdQ->allocateHeapMemory(IndirectHeap::Type::SURFACE_STATE, 1, ih3);
    auto cmdStream = new LinearStream(pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), 4096, AllocationType::COMMAND_BUFFER, pDevice->getDeviceBitfield()}));

    std::vector<Surface *> surfaces;
    auto kernelOperation = std::make_unique<KernelOperation>(cmdStream, *pDevice->getDefaultEngine().commandStreamReceiver->getInternalAllocationStorage());
    kernelOperation->setHeaps(ih1, ih2, ih3);

    std::unique_ptr<MockCommandComputeKernel> command = std::make_unique<MockCommandComputeKernel>(*pCmdQ, kernelOperation, surfaces, kernel);

    auto virtualEvent = makeReleaseable<MockEvent>(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, CompletionStamp::notReady, CompletionStamp::notReady);

    virtualEvent->setCommand(std::move(command));

    virtualEvent->submitCommand(false);

    uint32_t expectedLockCounter = pDevice->getDefaultEngine().commandStreamReceiver->getClearColorAllocation() ? 3u : 2u;

    EXPECT_EQ(expectedLockCounter, pDevice->getUltCommandStreamReceiver<FamilyType>().recursiveLockCounter);
}

HWTEST_F(EventTest, givenVirtualEventWhenSubmitCommandEventNotReadyAndEventWithoutCommandThenOneLockCsrNeeded) {
    class MockEvent : public Event {
      public:
        using Event::submitCommand;
        MockEvent(CommandQueue *cmdQueue, cl_command_type cmdType,
                  uint32_t taskLevel, uint32_t taskCount) : Event(cmdQueue, cmdType,
                                                                  taskLevel, taskCount) {}
    };

    auto virtualEvent = makeReleaseable<MockEvent>(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, CompletionStamp::notReady, CompletionStamp::notReady);

    virtualEvent->submitCommand(false);

    EXPECT_EQ(pDevice->getUltCommandStreamReceiver<FamilyType>().recursiveLockCounter, 1u);
}

HWTEST_F(InternalsEventTest, GivenBufferWithoutZeroCopyWhenMappingOrUnmappingThenFlushPreviousTasksBeforeMappingOrUnmapping) {
    struct MockNonZeroCopyBuff : UnalignedBuffer {
        MockNonZeroCopyBuff(int32_t &executionStamp)
            : executionStamp(executionStamp) {
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
        int32_t dataTransferedStamp = -1;
    };

    int32_t executionStamp = 0;
    auto csr = new MockCsr<FamilyType>(executionStamp, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(csr);

    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    auto pCmdQ = makeReleaseable<MockCommandQueue>(mockContext, pClDevice, props, false);

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
        static void CL_CALLBACK clbFuncT(cl_event e, cl_int status, void *retStatus) {
            *((cl_int *)retStatus) = status;
        }
    };

    cl_int retStatus = 7;
    Event::Callback clb(nullptr, ClbFuncTempStruct::clbFuncT, CL_COMPLETE, &retStatus);
    EXPECT_EQ(CL_COMPLETE, clb.getCallbackExecutionStatusTarget());
    clb.execute();
    EXPECT_EQ(CL_COMPLETE, retStatus);

    retStatus = 7;
    clb.overrideCallbackExecutionStatusTarget(-1);
    EXPECT_EQ(-1, clb.getCallbackExecutionStatusTarget());
    clb.execute();
    EXPECT_EQ(-1, retStatus);
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

HWTEST_F(EventTest, WhenGettingHwTimeStampsThenValidPointerIsReturned) {
    pDevice->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = false;

    auto myCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(pCmdQ->getContextPtr(), pClDevice, nullptr);

    std::unique_ptr<Event> event(new Event(myCmdQ.get(), CL_COMMAND_COPY_BUFFER, 0, 0));
    ASSERT_NE(nullptr, event);

    HwTimeStamps *timeStamps = static_cast<TagNode<HwTimeStamps> *>(event->getHwTimeStampNode())->tagForCpuAccess;
    ASSERT_NE(nullptr, timeStamps);

    // this should not cause any heap corruptions
    ASSERT_EQ(0ULL, timeStamps->GlobalStartTS);
    ASSERT_EQ(0ULL, timeStamps->ContextStartTS);
    ASSERT_EQ(0ULL, timeStamps->GlobalEndTS);
    ASSERT_EQ(0ULL, timeStamps->ContextEndTS);
    ASSERT_EQ(0ULL, timeStamps->GlobalCompleteTS);
    ASSERT_EQ(0ULL, timeStamps->ContextCompleteTS);

    HwTimeStamps *timeStamps2 = static_cast<TagNode<HwTimeStamps> *>(event->getHwTimeStampNode())->tagForCpuAccess;
    ASSERT_EQ(timeStamps, timeStamps2);
}

HWTEST_F(EventTest, WhenGetHwTimeStampsAllocationThenValidPointerIsReturned) {
    pDevice->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = false;

    auto myCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(pCmdQ->getContextPtr(), pClDevice, nullptr);

    std::unique_ptr<Event> event(new Event(myCmdQ.get(), CL_COMMAND_COPY_BUFFER, 0, 0));
    ASSERT_NE(nullptr, event);

    GraphicsAllocation *allocation = event->getHwTimeStampNode()->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation();
    ASSERT_NE(nullptr, allocation);

    void *memoryStorage = allocation->getUnderlyingBuffer();
    size_t memoryStorageSize = allocation->getUnderlyingBufferSize();

    EXPECT_NE(nullptr, memoryStorage);
    EXPECT_GT(memoryStorageSize, 0u);
}

HWTEST_F(EventTest, WhenEventIsCreatedThenHwTimeStampsMemoryIsPlacedInGraphicsAllocation) {
    pDevice->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = false;

    auto myCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(pCmdQ->getContextPtr(), pClDevice, nullptr);

    std::unique_ptr<Event> event(new Event(myCmdQ.get(), CL_COMMAND_COPY_BUFFER, 0, 0));
    ASSERT_NE(nullptr, event);

    HwTimeStamps *timeStamps = static_cast<TagNode<HwTimeStamps> *>(event->getHwTimeStampNode())->tagForCpuAccess;
    ASSERT_NE(nullptr, timeStamps);

    GraphicsAllocation *allocation = event->getHwTimeStampNode()->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation();
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
    MockCommandQueue *pCmdQ = new MockCommandQueue(mockContext, pClDevice, props, false);

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
    MockEvent<Event> ev(this->pCmdQ, CL_COMMAND_MAP_BUFFER, CompletionStamp::notReady, CompletionStamp::notReady);

    EXPECT_EQ(CompletionStamp::notReady, ev.peekTaskCount());
    ev.submitCommand(false);
    EXPECT_EQ(0u, ev.peekTaskCount());
}

HWTEST_F(EventTest, GivenEventCreatedOnMapImageWithoutCommandWhenSubmittingCommandThenTaskCountIsNotUpdated) {
    MockEvent<Event> ev(this->pCmdQ, CL_COMMAND_MAP_IMAGE, CompletionStamp::notReady, CompletionStamp::notReady);

    EXPECT_EQ(CompletionStamp::notReady, ev.peekTaskCount());
    ev.submitCommand(false);
    EXPECT_EQ(0u, ev.peekTaskCount());
}

TEST_F(EventTest, givenCmdQueueWithoutProfilingWhenIsCpuProfilingIsCalledThenFalseIsReturned) {
    MockEvent<Event> ev(this->pCmdQ, CL_COMMAND_MAP_IMAGE, CompletionStamp::notReady, CompletionStamp::notReady);
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
    std::unique_ptr<MockCommandQueue> pCmdQ(new MockCommandQueue(&mockContext, pClDevice, props, false));

    MockEvent<Event> ev(pCmdQ.get(), CL_COMMAND_MAP_IMAGE, CompletionStamp::notReady, CompletionStamp::notReady);
    bool cpuProfiling = ev.isCPUProfilingPath() != 0;
    EXPECT_TRUE(cpuProfiling);
}

TEST(EventCallback, GivenEventWithCallbacksOnWhenPeekingHasCallbacksThenReturnTrue) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableAsyncEventsHandler.set(false);
    struct ClbFuncTempStruct {
        static void CL_CALLBACK clbFuncT(cl_event, cl_int, void *) {
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
        ev.addCallback(ClbFuncTempStruct::clbFuncT, CL_SUBMITTED, nullptr);
        EXPECT_TRUE(ev.peekHasCallbacks());
        ev.decRefInternal();
    }

    {
        SmallMockEvent ev;
        ev.addCallback(ClbFuncTempStruct::clbFuncT, CL_RUNNING, nullptr);
        EXPECT_TRUE(ev.peekHasCallbacks());
        ev.decRefInternal();
    }

    {
        SmallMockEvent ev;
        ev.addCallback(ClbFuncTempStruct::clbFuncT, CL_COMPLETE, nullptr);
        EXPECT_TRUE(ev.peekHasCallbacks());
        ev.decRefInternal();
    }

    {
        SmallMockEvent ev;
        ev.addCallback(ClbFuncTempStruct::clbFuncT, CL_SUBMITTED, nullptr);
        ev.addCallback(ClbFuncTempStruct::clbFuncT, CL_COMPLETE, nullptr);
        EXPECT_TRUE(ev.peekHasCallbacks());
        ev.decRefInternal();
        ev.decRefInternal();
    }

    {
        SmallMockEvent ev;
        ev.addCallback(ClbFuncTempStruct::clbFuncT, CL_RUNNING, nullptr);
        ev.addCallback(ClbFuncTempStruct::clbFuncT, CL_COMPLETE, nullptr);
        EXPECT_TRUE(ev.peekHasCallbacks());
        ev.decRefInternal();
        ev.decRefInternal();
    }

    {
        SmallMockEvent ev;
        ev.addCallback(ClbFuncTempStruct::clbFuncT, CL_SUBMITTED, nullptr);
        ev.addCallback(ClbFuncTempStruct::clbFuncT, CL_RUNNING, nullptr);
        ev.addCallback(ClbFuncTempStruct::clbFuncT, CL_COMPLETE, nullptr);
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

    Event childEvent0(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, CompletionStamp::notReady, CompletionStamp::notReady);
    Event childEvent1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, CompletionStamp::notReady, CompletionStamp::notReady);

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

template <typename GfxFamily>
struct TestEventCsr : public UltCommandStreamReceiver<GfxFamily> {
    TestEventCsr(const ExecutionEnvironment &executionEnvironment, const DeviceBitfield deviceBitfield)
        : UltCommandStreamReceiver<GfxFamily>(const_cast<ExecutionEnvironment &>(executionEnvironment), 0, deviceBitfield) {}

    WaitStatus waitForCompletionWithTimeout(const WaitParams &params, uint32_t taskCountToWait) override {
        waitForCompletionWithTimeoutCalled++;
        waitForCompletionWithTimeoutParamsPassed.push_back({params.enableTimeout, params.waitTimeout, taskCountToWait});
        return waitForCompletionWithTimeoutResult;
    }

    struct WaitForCompletionWithTimeoutParams {
        bool enableTimeout = false;
        int64_t timeoutMs{};
        uint32_t taskCountToWait{};
    };

    uint32_t waitForCompletionWithTimeoutCalled = 0u;
    WaitStatus waitForCompletionWithTimeoutResult = WaitStatus::Ready;
    StackVec<WaitForCompletionWithTimeoutParams, 1> waitForCompletionWithTimeoutParamsPassed{};
};

HWTEST_F(EventTest, givenQuickKmdSleepRequestWhenWaitIsCalledThenPassRequestToWaitingFunction) {
    HardwareInfo localHwInfo = pDevice->getHardwareInfo();
    localHwInfo.capabilityTable.kmdNotifyProperties.enableKmdNotify = true;
    localHwInfo.capabilityTable.kmdNotifyProperties.enableQuickKmdSleep = true;
    localHwInfo.capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds = 1;
    localHwInfo.capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds = 2;

    pDevice->executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->setHwInfo(&localHwInfo);

    auto csr = new TestEventCsr<FamilyType>(*pDevice->executionEnvironment, pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(csr);

    Event event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
    event.updateCompletionStamp(1u, 0, 1u, 1u);

    const auto result = event.wait(true, true);
    EXPECT_EQ(WaitStatus::Ready, result);

    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(localHwInfo.capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
}

HWTEST_F(EventTest, givenNonQuickKmdSleepRequestWhenWaitIsCalledThenPassRequestToWaitingFunction) {
    HardwareInfo localHwInfo = pDevice->getHardwareInfo();
    localHwInfo.capabilityTable.kmdNotifyProperties.enableKmdNotify = true;
    localHwInfo.capabilityTable.kmdNotifyProperties.enableQuickKmdSleep = true;
    localHwInfo.capabilityTable.kmdNotifyProperties.enableQuickKmdSleepForSporadicWaits = false;
    localHwInfo.capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds = 1;
    localHwInfo.capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds = 2;

    pDevice->executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->setHwInfo(&localHwInfo);

    auto csr = new TestEventCsr<FamilyType>(*pDevice->executionEnvironment, pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(csr);

    Event event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
    event.updateCompletionStamp(1u, 0, 1u, 1u);

    const auto result = event.wait(true, false);
    EXPECT_EQ(WaitStatus::Ready, result);

    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(localHwInfo.capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds, csr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
}

HWTEST_F(EventTest, givenGpuHangWhenWaitIsCalledThenPassRequestToWaitingFunctionAndReturnGpuHang) {
    auto csr = new TestEventCsr<FamilyType>(*pDevice->executionEnvironment, pDevice->getDeviceBitfield());
    csr->waitForCompletionWithTimeoutResult = WaitStatus::GpuHang;
    pDevice->resetCommandStreamReceiver(csr);

    Event event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);

    const auto waitStatus = event.wait(true, false);
    EXPECT_EQ(WaitStatus::GpuHang, waitStatus);
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
}

HWTEST_F(InternalsEventTest, givenCommandWhenSubmitCalledThenUpdateFlushStamp) {
    auto pCmdQ = std::unique_ptr<MockCommandQueue>(new MockCommandQueue(mockContext, pClDevice, 0, false));
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
    auto pCmdQ = std::unique_ptr<MockCommandQueue>(new MockCommandQueue(mockContext, pClDevice, 0, false));
    MockEvent<Event> *event = new MockEvent<Event>(pCmdQ.get(), CL_COMMAND_MARKER, 0, 0);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.flushStamp->setStamp(5);

    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto pKernel = mockKernelWithInternals.mockKernel;

    auto cmdStream = new LinearStream(pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), 4096, AllocationType::COMMAND_BUFFER, pDevice->getDeviceBitfield()}));
    IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    pCmdQ->allocateHeapMemory(IndirectHeap::Type::DYNAMIC_STATE, 4096u, dsh);
    pCmdQ->allocateHeapMemory(IndirectHeap::Type::INDIRECT_OBJECT, 4096u, ioh);
    pCmdQ->allocateHeapMemory(IndirectHeap::Type::SURFACE_STATE, 4096u, ssh);
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

TEST(EventTimestampTest, givenTimestampPacketWritesDisabledAndQueueHasTimestampPacketContainerThenCreateTheContainerForEvent) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.EnableTimestampPacket.set(0);

    MockContext context{};
    MockCommandQueue queue{&context, context.getDevice(0), nullptr, false};
    ASSERT_FALSE(queue.getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled());
    ASSERT_EQ(nullptr, queue.timestampPacketContainer.get());
    queue.timestampPacketContainer = std::make_unique<TimestampPacketContainer>();

    MockEvent<Event> event{&queue, CL_COMMAND_MARKER, 0, 0};
    EXPECT_NE(nullptr, event.timestampPacketContainer);
}

TEST(EventTimestampTest, givenEnableTimestampWaitWhenCheckIsTimestampWaitEnabledThenReturnProperValue) {
    DebugManagerStateRestore restorer;
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useWaitForTimestamps = true;
    MockContext context{};
    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockCommandQueue cmdQ(&context, mockDevice.get(), 0, false);

    MockEvent<Event> event{&cmdQ, CL_COMMAND_MARKER, 0, 0};

    {
        DebugManager.flags.EnableTimestampWaitForEvents.set(-1);
        const auto &hwInfo = mockDevice->getHardwareInfo();
        const auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
        EXPECT_EQ(event.isWaitForTimestampsEnabled(), hwHelper.isTimestampWaitSupportedForEvents(hwInfo));
    }

    {
        DebugManager.flags.EnableTimestampWaitForEvents.set(0);
        EXPECT_FALSE(event.isWaitForTimestampsEnabled());
    }

    {
        DebugManager.flags.EnableTimestampWaitForEvents.set(1);
        EXPECT_EQ(event.isWaitForTimestampsEnabled(), cmdQ.getGpgpuCommandStreamReceiver().isUpdateTagFromWaitEnabled());
    }

    {
        DebugManager.flags.EnableTimestampWaitForEvents.set(2);
        EXPECT_EQ(event.isWaitForTimestampsEnabled(), cmdQ.getGpgpuCommandStreamReceiver().isDirectSubmissionEnabled());
    }

    {
        DebugManager.flags.EnableTimestampWaitForEvents.set(3);
        EXPECT_EQ(event.isWaitForTimestampsEnabled(), cmdQ.getGpgpuCommandStreamReceiver().isAnyDirectSubmissionEnabled());
    }

    {
        DebugManager.flags.EnableTimestampWaitForEvents.set(4);
        EXPECT_TRUE(event.isWaitForTimestampsEnabled());
    }
}
