/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "event_fixture.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/event/perf_counter.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/task_information.h"
#include "runtime/memory_manager/internal_allocation_storage.h"
#include "runtime/memory_manager/surface.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/utilities/tag_allocator.h"
#include "test.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_event.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_mdi.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/os_interface/mock_performance_counters.h"
#include <memory>
#include <type_traits>

TEST(Event, NonCopyable) {
    EXPECT_FALSE(std::is_move_constructible<Event>::value);
    EXPECT_FALSE(std::is_copy_constructible<Event>::value);
}

TEST(Event, NonAssignable) {
    EXPECT_FALSE(std::is_move_assignable<Event>::value);
    EXPECT_FALSE(std::is_copy_assignable<Event>::value);
}

TEST(Event, dontUpdateExecutionStatusOnNotReadyEvent) {
    std::unique_ptr<MockDevice> mockDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext ctx;
    MockCommandQueue cmdQ(&ctx, mockDevice.get(), 0);
    Event event(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, Event::eventNotReady, 0);

    EXPECT_FALSE(event.peekIsBlocked());
    EXPECT_EQ(CL_QUEUED, event.peekExecutionStatus());
    event.updateExecutionStatus();
    EXPECT_EQ(CL_QUEUED, event.peekExecutionStatus());
}

TEST(Event, givenEventThatStatusChangeWhenPeekIsCalledThenEventIsNotUpdated) {
    std::unique_ptr<MockDevice> mockDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext ctx;
    MockCommandQueue cmdQ(&ctx, mockDevice.get(), 0);

    struct mockEvent : public Event {
        using Event::Event;
        void updateExecutionStatus() override {
            callCount++;
        }
        uint32_t callCount = 0u;
    };

    mockEvent event(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, Event::eventNotReady, 0);
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

TEST(Event_, testGetTaskLevel) {
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

TEST(Event_, testGetTaskCount) {
    Event event(nullptr, CL_COMMAND_NDRANGE_KERNEL, 5, 7);
    EXPECT_EQ(7u, event.getCompletionStamp());
}

TEST(Event_, testGetEventInfoReturnsTheCQ) {
    std::unique_ptr<MockDevice> mockDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
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
    std::unique_ptr<MockDevice> mockDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
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

TEST(Event, currentCmdQVirtualEventSetToFalseInCtor) {
    Event *event = new Event(nullptr, CL_COMMAND_NDRANGE_KERNEL, 4, 10);

    EXPECT_FALSE(event->isCurrentCmdQVirtualEvent());
    delete event;
}

TEST(Event, setCurrentCmdQVirtualEven) {
    Event *event = new Event(nullptr, CL_COMMAND_NDRANGE_KERNEL, 4, 10);
    event->setCurrentCmdQVirtualEvent(true);

    EXPECT_TRUE(event->isCurrentCmdQVirtualEvent());
    delete event;
}

TEST(Event, waitForEventsFlushesAllQueues) {
    class MockCommandQueueWithFlushCheck : public MockCommandQueue {
      public:
        MockCommandQueueWithFlushCheck() = delete;
        MockCommandQueueWithFlushCheck(MockCommandQueueWithFlushCheck &) = delete;
        MockCommandQueueWithFlushCheck(Context &context, Device *device) : MockCommandQueue(&context, device, nullptr) {
        }
        cl_int flush() override {
            flushCounter++;
            return CL_SUCCESS;
        }
        uint32_t flushCounter = 0;
    };

    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
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

TEST(Event, waitForEventsWithNotReadyEventDoesNotFlushQueue) {
    class MockCommandQueueWithFlushCheck : public MockCommandQueue {
      public:
        MockCommandQueueWithFlushCheck() = delete;
        MockCommandQueueWithFlushCheck(MockCommandQueueWithFlushCheck &) = delete;
        MockCommandQueueWithFlushCheck(Context &context, Device *device) : MockCommandQueue(&context, device, nullptr) {
        }
        cl_int flush() override {
            flushCounter++;
            return CL_SUCCESS;
        }
        uint32_t flushCounter = 0;
    };

    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext context;

    std::unique_ptr<MockCommandQueueWithFlushCheck> cmdQ1(new MockCommandQueueWithFlushCheck(context, device.get()));
    std::unique_ptr<Event> event1(new Event(cmdQ1.get(), CL_COMMAND_NDRANGE_KERNEL, Event::eventNotReady, 0));
    cl_event eventWaitlist[] = {event1.get()};

    Event::waitForEvents(1, eventWaitlist);

    EXPECT_EQ(0u, cmdQ1->flushCounter);
}

TEST_F(EventTest, GetEventInfo_CL_EVENT_COMMAND_EXECUTION_STATUS_sizeReturned) {
    Event event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 1, 5);
    cl_int eventStatus = -1;
    size_t sizeReturned = 0;

    auto result = clGetEventInfo(&event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(eventStatus), &eventStatus, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeReturned, sizeof(eventStatus));
}

TEST_F(EventTest, GetEventInfo_CL_EVENT_COMMAND_EXECUTION_STATUS_returns_CL_SUBMITTED_HW_LT_Event) {
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

TEST_F(EventTest, GetEventInfo_CL_EVENT_COMMAND_EXECUTION_STATUS_returns_CL_COMPLETE_HW_EQ_Event) {
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

TEST_F(EventTest, GetEventInfo_CL_EVENT_COMMAND_EXECUTION_STATUS_returns_CL_SUBMITTED_HW_GT_Event) {
    uint32_t tagHW = 6;
    uint32_t taskCount = 5;
    *pTagMemory = tagHW;

    Event event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 3, taskCount);
    cl_int eventStatus = -1;
    size_t sizeReturned = 0;

    auto result = clGetEventInfo(&event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(eventStatus), &eventStatus, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);

    // If tagCS > taskCount, the event is not completed.
    EXPECT_EQ(CL_COMPLETE, eventStatus);
}

TEST_F(EventTest, GetEventInfo_CL_EVENT_COMMAND_EXECUTION_STATUS_returnsSetStatus) {
    Event event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, Event::eventNotReady, Event::eventNotReady);
    cl_int eventStatus = -1;

    event.setStatus(-1);
    auto result = clGetEventInfo(&event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(eventStatus), &eventStatus, 0);
    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(-1, eventStatus);
}

TEST_F(EventTest, GetEventInfo_CL_EVENT_REFERENCE_COUNT_new_Event) {
    uint32_t tagEvent = 5;

    Event event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 3, tagEvent);
    cl_uint refCount = 0;
    size_t sizeReturned = 0;

    auto result = clGetEventInfo(&event, CL_EVENT_REFERENCE_COUNT, sizeof(refCount), &refCount, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeof(refCount), sizeReturned);

    EXPECT_EQ(1u, refCount);
}

TEST_F(EventTest, GetEventInfo_CL_EVENT_REFERENCE_COUNT_Retain_Event) {
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

TEST_F(EventTest, GetEventInfo_CL_EVENT_REFERENCE_COUNT_Retain_Release_Event) {
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

TEST_F(EventTest, GetEventInfo_CL_EVENT_CONTEXT) {
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

TEST_F(EventTest, GetEventInfo_InvalidParam) {
    uint32_t tagEvent = 5;

    Event event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 3, tagEvent);
    cl_int eventStatus = -1;

    auto result = clGetEventInfo(&event, -1, sizeof(eventStatus), &eventStatus, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, result);
}

TEST_F(EventTest, Event_Wait_NonBlocking) {
    Event event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 3, Event::eventNotReady);
    auto result = event.wait(false, false);
    EXPECT_FALSE(result);
}

struct UpdateEventTest : public ::testing::Test {

    void SetUp() override {
        executionEnvironment = new ExecutionEnvironment;
        memoryManager = new MockMemoryManager(*executionEnvironment);
        hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());
        executionEnvironment->memoryManager.reset(memoryManager);
        device.reset(Device::create<Device>(*platformDevices, executionEnvironment, 0u));
        context = std::make_unique<MockContext>(device.get());
        cl_int retVal = CL_OUT_OF_RESOURCES;
        commandQueue.reset(CommandQueue::create(context.get(), device.get(), nullptr, retVal));
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    ExecutionEnvironment *executionEnvironment;
    MockMemoryManager *memoryManager;
    MockHostPtrManager *hostPtrManager;
    std::unique_ptr<Device> device;
    std::unique_ptr<Context> context;
    std::unique_ptr<CommandQueue> commandQueue;
};

TEST_F(UpdateEventTest, givenEventContainingCommandQueueWhenItsStatusIsUpdatedToCompletedThenTemporaryAllocationsAreDeleted) {
    void *ptr = (void *)0x1000;
    size_t size = 4096;
    auto temporary = memoryManager->allocateGraphicsMemory(MockAllocationProperties{false, size}, ptr);
    temporary->updateTaskCount(3, commandQueue->getCommandStreamReceiver().getOsContext().getContextId());
    commandQueue->getCommandStreamReceiver().getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(temporary), TEMPORARY_ALLOCATION);
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

TEST_F(InternalsEventTest, processBlockedCommandsKernelOperation) {
    CommandQueue cmdQ(mockContext, pDevice, nullptr);
    MockEvent<Event> event(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);

    auto cmdStream = new LinearStream(alignedMalloc(4096, 4096), 4096);
    IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    cmdQ.allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, 4096u, dsh);
    cmdQ.allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, 4096u, ioh);
    cmdQ.allocateHeapMemory(IndirectHeap::SURFACE_STATE, 4096u, ssh);
    using UniqueIH = std::unique_ptr<IndirectHeap>;
    auto blockedCommandsData = new KernelOperation(std::unique_ptr<LinearStream>(cmdStream), UniqueIH(dsh),
                                                   UniqueIH(ioh), UniqueIH(ssh),
                                                   *cmdQ.getCommandStreamReceiver().getInternalAllocationStorage());

    MockKernelWithInternals mockKernelWithInternals(*pDevice);
    auto pKernel = mockKernelWithInternals.mockKernel;

    auto &csr = cmdQ.getCommandStreamReceiver();
    std::vector<Surface *> v;
    SurfaceMock *surface = new SurfaceMock;
    surface->graphicsAllocation = new MockGraphicsAllocation((void *)0x1234, 100u);
    PreemptionMode preemptionMode = pDevice->getPreemptionMode();
    v.push_back(surface);
    auto cmd = new CommandComputeKernel(cmdQ, std::unique_ptr<KernelOperation>(blockedCommandsData), v, false, false, false, nullptr, preemptionMode, pKernel, 1);
    event.setCommand(std::unique_ptr<Command>(cmd));

    auto taskLevelBefore = csr.peekTaskLevel();

    event.submitCommand(false);

    auto taskLevelAfter = csr.peekTaskLevel();

    EXPECT_EQ(taskLevelBefore + 1, taskLevelAfter);

    EXPECT_EQ(surface->resident, 1u);
    EXPECT_FALSE(surface->graphicsAllocation->isResident(csr.getOsContext().getContextId()));
    delete surface->graphicsAllocation;
}

TEST_F(InternalsEventTest, processBlockedCommandsAbortKernelOperation) {
    CommandQueue cmdQ(mockContext, pDevice, nullptr);
    MockEvent<Event> event(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);

    auto cmdStream = new LinearStream(alignedMalloc(4096, 4096), 4096);
    IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    cmdQ.allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, 4096u, dsh);
    cmdQ.allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, 4096u, ioh);
    cmdQ.allocateHeapMemory(IndirectHeap::SURFACE_STATE, 4096u, ssh);
    using UniqueIH = std::unique_ptr<IndirectHeap>;
    auto blockedCommandsData = new KernelOperation(std::unique_ptr<LinearStream>(cmdStream), UniqueIH(dsh),
                                                   UniqueIH(ioh), UniqueIH(ssh),
                                                   *cmdQ.getCommandStreamReceiver().getInternalAllocationStorage());

    MockKernelWithInternals mockKernelWithInternals(*pDevice);
    auto pKernel = mockKernelWithInternals.mockKernel;

    auto &csr = cmdQ.getCommandStreamReceiver();
    std::vector<Surface *> v;
    NullSurface *surface = new NullSurface;
    v.push_back(surface);
    PreemptionMode preemptionMode = pDevice->getPreemptionMode();
    auto cmd = new CommandComputeKernel(cmdQ, std::unique_ptr<KernelOperation>(blockedCommandsData), v, false, false, false, nullptr, preemptionMode, pKernel, 1);
    event.setCommand(std::unique_ptr<Command>(cmd));

    auto taskLevelBefore = csr.peekTaskLevel();

    event.submitCommand(true);

    auto taskLevelAfter = csr.peekTaskLevel();

    EXPECT_EQ(taskLevelBefore, taskLevelAfter);
}

TEST_F(InternalsEventTest, givenBlockedKernelWithPrintfWhenSubmittedThenPrintOutput) {
    testing::internal::CaptureStdout();
    CommandQueue cmdQ(mockContext, pDevice, nullptr);
    MockEvent<Event> event(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);

    auto cmdStream = new LinearStream(alignedMalloc(4096, 4096), 4096);
    IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    cmdQ.allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, 4096u, dsh);
    cmdQ.allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, 4096u, ioh);
    cmdQ.allocateHeapMemory(IndirectHeap::SURFACE_STATE, 4096u, ssh);
    using UniqueIH = std::unique_ptr<IndirectHeap>;
    auto blockedCommandsData = new KernelOperation(std::unique_ptr<LinearStream>(cmdStream), UniqueIH(dsh),
                                                   UniqueIH(ioh), UniqueIH(ssh),
                                                   *cmdQ.getCommandStreamReceiver().getInternalAllocationStorage());

    SPatchAllocateStatelessPrintfSurface *pPrintfSurface = new SPatchAllocateStatelessPrintfSurface();
    pPrintfSurface->DataParamOffset = 0;
    pPrintfSurface->DataParamSize = 8;

    char *testString = new char[sizeof("test")];

    strcpy_s(testString, sizeof("test"), "test");

    PrintfStringInfo printfStringInfo;
    printfStringInfo.SizeInBytes = sizeof("test");
    printfStringInfo.pStringData = testString;

    MockKernelWithInternals mockKernelWithInternals(*pDevice);
    auto pKernel = mockKernelWithInternals.mockKernel;
    KernelInfo *kernelInfo = const_cast<KernelInfo *>(&pKernel->getKernelInfo());
    kernelInfo->patchInfo.pAllocateStatelessPrintfSurface = pPrintfSurface;
    kernelInfo->patchInfo.stringDataMap.insert(std::make_pair(0, printfStringInfo));
    uint64_t crossThread[10];
    pKernel->setCrossThreadData(&crossThread, sizeof(uint64_t) * 8);

    MockMultiDispatchInfo multiDispatchInfo(pKernel);
    std::unique_ptr<PrintfHandler> printfHandler(PrintfHandler::create(multiDispatchInfo, *pDevice));
    printfHandler.get()->prepareDispatch(multiDispatchInfo);
    auto surface = printfHandler.get()->getSurface();

    auto printfSurface = reinterpret_cast<uint32_t *>(surface->getUnderlyingBuffer());
    printfSurface[0] = 8;
    printfSurface[1] = 0;

    std::vector<Surface *> v;
    PreemptionMode preemptionMode = pDevice->getPreemptionMode();
    auto cmd = new CommandComputeKernel(cmdQ, std::unique_ptr<KernelOperation>(blockedCommandsData), v, false, false, false, std::move(printfHandler), preemptionMode, pKernel, 1);
    event.setCommand(std::unique_ptr<Command>(cmd));

    event.submitCommand(false);

    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STREQ("test", output.c_str());
    EXPECT_FALSE(surface->isResident(pDevice->getDefaultEngine().osContext->getContextId()));

    delete pPrintfSurface;
}

TEST_F(InternalsEventTest, processBlockedCommandsMapOperation) {
    MockEvent<Event> event(nullptr, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
    CommandQueue *pCmdQ = new CommandQueue(mockContext, pDevice, 0);

    auto &csr = pCmdQ->getCommandStreamReceiver();
    auto buffer = new MockBuffer;

    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    event.setCommand(std::unique_ptr<Command>(new CommandMapUnmap(MAP, *buffer, size, offset, false, csr, *pCmdQ)));

    auto taskLevelBefore = csr.peekTaskLevel();

    event.submitCommand(false);

    auto taskLevelAfter = csr.peekTaskLevel();

    EXPECT_EQ(taskLevelBefore + 1, taskLevelAfter);
    buffer->decRefInternal();
    delete pCmdQ;
}

TEST_F(InternalsEventTest, processBlockedCommandsMapOperationNonZeroCopyBuffer) {
    MockEvent<Event> event(nullptr, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
    CommandQueue *pCmdQ = new CommandQueue(mockContext, pDevice, 0);

    auto &csr = pCmdQ->getCommandStreamReceiver();
    auto buffer = new UnalignedBuffer;

    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    event.setCommand(std::unique_ptr<Command>(new CommandMapUnmap(MAP, *buffer, size, offset, false, csr, *pCmdQ)));

    auto taskLevelBefore = csr.peekTaskLevel();

    event.submitCommand(false);

    auto taskLevelAfter = csr.peekTaskLevel();

    EXPECT_EQ(taskLevelBefore + 1, taskLevelAfter);
    buffer->decRefInternal();
    delete pCmdQ;
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
    std::unique_ptr<CommandQueue> pCmdQ(new CommandQueue(mockContext, pDevice, props));

    std::unique_ptr<MockEvent<Event>> event(new MockEvent<Event>(pCmdQ.get(), GetParam(), 0, 0));
    EXPECT_TRUE(event.get()->isProfilingEnabled());
}

INSTANTIATE_TEST_CASE_P(InternalsEventProfilingTest,
                        InternalsEventProfilingTest,
                        ::testing::ValuesIn(commands));

TEST_F(InternalsEventTest, GivenProfilingWhenUserEventCreatedThenProfilingNotSet) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    std::unique_ptr<CommandQueue> pCmdQ(new CommandQueue(mockContext, pDevice, props));

    std::unique_ptr<MockEvent<Event>> event(new MockEvent<Event>(pCmdQ.get(), CL_COMMAND_USER, 0, 0));
    EXPECT_FALSE(event.get()->isProfilingEnabled());
}

TEST_F(InternalsEventTest, GIVENProfilingWHENMapOperationTHENTimesSet) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    CommandQueue *pCmdQ = new CommandQueue(mockContext, pDevice, props);

    MockEvent<Event> *event = new MockEvent<Event>(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);

    auto &csr = pCmdQ->getCommandStreamReceiver();
    UnalignedBuffer buffer;

    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    event->setCommand(std::unique_ptr<Command>(new CommandMapUnmap(MAP, buffer, size, offset, false, csr, *pCmdQ)));

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

TEST_F(InternalsEventTest, processBlockedCommandsUnMapOperation) {
    MockEvent<Event> event(nullptr, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue *pCmdQ = new CommandQueue(mockContext, pDevice, props);

    auto &csr = pCmdQ->getCommandStreamReceiver();
    auto buffer = new UnalignedBuffer;

    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    event.setCommand(std::unique_ptr<Command>(new CommandMapUnmap(UNMAP, *buffer, size, offset, false, csr, *pCmdQ)));

    auto taskLevelBefore = csr.peekTaskLevel();

    event.submitCommand(false);

    auto taskLevelAfter = csr.peekTaskLevel();

    EXPECT_EQ(taskLevelBefore + 1, taskLevelAfter);
    buffer->decRefInternal();
    delete pCmdQ;
}

TEST_F(InternalsEventTest, processBlockedCommandsUnMapOperationNonZeroCopyBuffer) {
    MockEvent<Event> event(nullptr, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue *pCmdQ = new CommandQueue(mockContext, pDevice, props);

    auto &csr = pCmdQ->getCommandStreamReceiver();
    auto buffer = new UnalignedBuffer;

    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    event.setCommand(std::unique_ptr<Command>(new CommandMapUnmap(UNMAP, *buffer, size, offset, false, csr, *pCmdQ)));

    auto taskLevelBefore = csr.peekTaskLevel();

    event.submitCommand(false);

    auto taskLevelAfter = csr.peekTaskLevel();

    EXPECT_EQ(taskLevelBefore + 1, taskLevelAfter);
    buffer->decRefInternal();
    delete pCmdQ;
}

HWTEST_F(InternalsEventTest, givenCpuProfilingPathWhenEnqueuedMarkerThenDontUseTimeStampNode) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    CommandQueue *pCmdQ = new CommandQueue(mockContext, pDevice, props);
    MockEvent<Event> *event = new MockEvent<Event>(pCmdQ, CL_COMMAND_MARKER, 0, 0);
    event->setCPUProfilingPath(true);

    auto &csr = pCmdQ->getCommandStreamReceiver();

    event->setCommand(std::unique_ptr<Command>(new CommandMarker(*pCmdQ, csr, CL_COMMAND_MARKER, 4096u)));

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
        performanceCountersBase->initialize(platformDevices[0]);
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
    CommandQueue *pCmdQ = new CommandQueue(mockContext, pDevice, props);
    bool ret = false;
    ret = pCmdQ->setPerfCountersEnabled(true, 1);
    EXPECT_TRUE(ret);
    ret = pCmdQ->setPerfCountersEnabled(true, 1);
    EXPECT_TRUE(ret);
    MockEvent<Event> *event = new MockEvent<Event>(pCmdQ, CL_COMMAND_MARKER, 0, 0);
    event->setCPUProfilingPath(true);

    auto &csr = pCmdQ->getCommandStreamReceiver();

    event->setCommand(std::unique_ptr<Command>(new CommandMarker(*pCmdQ, csr, CL_COMMAND_MARKER, 4096u)));

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
    CommandQueue *pCmdQ = new CommandQueue(mockContext, pDevice, props);
    pCmdQ->setPerfCountersEnabled(true, 1);
    MockEvent<Event> *event = new MockEvent<Event>(pCmdQ, CL_COMMAND_MARKER, 0, 0);
    event->setCPUProfilingPath(true);
    HwPerfCounter *perfCounter = event->getHwPerfCounterNode()->tag;
    ASSERT_NE(nullptr, perfCounter);
    HwTimeStamps *timeStamps = event->getHwTimeStampNode()->tag;
    ASSERT_NE(nullptr, timeStamps);
    auto &csr = pCmdQ->getCommandStreamReceiver();

    event->setCommand(std::unique_ptr<Command>(new CommandMarker(*pCmdQ, csr, CL_COMMAND_MARKER, 4096u)));

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

TEST_F(InternalsEventWithPerfCountersTest, IsPerfCounter_Enabled) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    CommandQueue *pCmdQ = new CommandQueue(mockContext, pDevice, props);
    pCmdQ->setPerfCountersEnabled(true, 2);
    Event *ev = new Event(pCmdQ, CL_COMMAND_COPY_BUFFER, 3, 0);
    EXPECT_TRUE(ev->isProfilingEnabled());
    EXPECT_TRUE(ev->isPerfCountersEnabled());
    delete ev;
    delete pCmdQ;
}

TEST(Event, GivenNoContextOnDeletionDeletesSelf) {
    UserEvent *ue = new UserEvent();
    auto autoptr = ue->release();
    ASSERT_TRUE(autoptr.isUnused());
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
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue *pCmdQ = new CommandQueue(mockContext, pDevice, props);
    MockNonZeroCopyBuff buffer(executionStamp);
    MockCsr<FamilyType> csr(executionStamp, *pDevice->executionEnvironment);
    csr.setTagAllocation(pDevice->getDefaultEngine().commandStreamReceiver->getTagAllocation());
    csr.setOsContext(*pDevice->getDefaultEngine().osContext);

    MemObjSizeArray size = {{4, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    auto commandMap = std::unique_ptr<Command>(new CommandMapUnmap(MAP, buffer, size, offset, false, csr, *pCmdQ));
    EXPECT_EQ(0, executionStamp);
    EXPECT_EQ(-1, csr.flushTaskStamp);
    EXPECT_EQ(-1, buffer.dataTransferedStamp);

    auto latestSentFlushTaskCount = csr.peekLatestSentTaskCount();

    commandMap->submit(0, false);
    EXPECT_EQ(1, executionStamp);
    EXPECT_EQ(0, csr.flushTaskStamp);
    EXPECT_EQ(1, buffer.dataTransferedStamp);
    auto latestSentFlushTaskCountAfterSubmit = csr.peekLatestSentTaskCount();
    EXPECT_GT(latestSentFlushTaskCountAfterSubmit, latestSentFlushTaskCount);

    executionStamp = 0;
    csr.flushTaskStamp = -1;
    buffer.dataTransferedStamp = -1;
    buffer.swapCopyDirection();

    auto commandUnMap = std::unique_ptr<Command>(new CommandMapUnmap(UNMAP, buffer, size, offset, false, csr, *pCmdQ));
    EXPECT_EQ(0, executionStamp);
    EXPECT_EQ(-1, csr.flushTaskStamp);
    EXPECT_EQ(-1, buffer.dataTransferedStamp);
    commandUnMap->submit(0, false);
    EXPECT_EQ(1, executionStamp);
    EXPECT_EQ(0, csr.flushTaskStamp);
    EXPECT_EQ(1, buffer.dataTransferedStamp);
    EXPECT_EQ(nullptr, commandUnMap->getCommandStream());

    pCmdQ->getCommandStreamReceiver().setTagAllocation(nullptr);
    delete pCmdQ;
}

TEST(EventCallback, CallbackAfterStatusOverrideUsesNewStatus) {
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

TEST_F(EventTest, WhensetTimeStampThenCorrectValues) {
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

TEST_F(EventTest, WhensetCPUTimeStampThenCorrectTimes) {
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

TEST_F(EventTest, GIVENNoQueueWhensetCPUTimeStampThenTimesNotSet) {
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

TEST_F(EventTest, getHwTimeStampsReturnsValidPointer) {
    std::unique_ptr<Event> event(new Event(this->pCmdQ, CL_COMMAND_COPY_BUFFER, 0, 0));
    ASSERT_NE(nullptr, event);

    HwTimeStamps *timeStamps = event->getHwTimeStampNode()->tag;
    ASSERT_NE(nullptr, timeStamps);

    //this should not cause any heap corruptions
    ASSERT_EQ(0ULL, timeStamps->GlobalStartTS);
    ASSERT_EQ(0ULL, timeStamps->ContextStartTS);
    ASSERT_EQ(0ULL, timeStamps->GlobalEndTS);
    ASSERT_EQ(0ULL, timeStamps->ContextEndTS);
    ASSERT_EQ(0ULL, timeStamps->GlobalCompleteTS);
    ASSERT_EQ(0ULL, timeStamps->ContextCompleteTS);

    EXPECT_TRUE(timeStamps->canBeReleased());

    HwTimeStamps *timeStamps2 = event->getHwTimeStampNode()->tag;
    ASSERT_EQ(timeStamps, timeStamps2);
}

TEST_F(EventTest, getHwTimeStampsAllocationReturnsValidPointer) {
    std::unique_ptr<Event> event(new Event(this->pCmdQ, CL_COMMAND_COPY_BUFFER, 0, 0));
    ASSERT_NE(nullptr, event);

    GraphicsAllocation *allocation = event->getHwTimeStampNode()->getGraphicsAllocation();
    ASSERT_NE(nullptr, allocation);

    void *memoryStorage = allocation->getUnderlyingBuffer();
    size_t memoryStorageSize = allocation->getUnderlyingBufferSize();

    EXPECT_NE(nullptr, memoryStorage);
    EXPECT_GT(memoryStorageSize, 0u);
}

TEST_F(EventTest, hwTimeStampsMemoryIsPlacedInGraphicsAllocation) {
    std::unique_ptr<Event> event(new Event(this->pCmdQ, CL_COMMAND_COPY_BUFFER, 0, 0));
    ASSERT_NE(nullptr, event);

    HwTimeStamps *timeStamps = event->getHwTimeStampNode()->tag;
    ASSERT_NE(nullptr, timeStamps);

    GraphicsAllocation *allocation = event->getHwTimeStampNode()->getGraphicsAllocation();
    ASSERT_NE(nullptr, allocation);

    void *memoryStorage = allocation->getUnderlyingBuffer();
    size_t graphicsAllocationSize = allocation->getUnderlyingBufferSize();

    uintptr_t timeStampAddress = reinterpret_cast<uintptr_t>(timeStamps);
    uintptr_t graphicsAllocationStart = reinterpret_cast<uintptr_t>(memoryStorage);

    if (!((timeStampAddress >= graphicsAllocationStart) &&
          ((timeStampAddress + sizeof(HwTimeStamps)) <= (graphicsAllocationStart + graphicsAllocationSize)))) {
        EXPECT_TRUE(false);
    }
}

TEST_F(EventTest, getHwPerfCounterReturnsValidPointer) {
    std::unique_ptr<Event> event(new Event(this->pCmdQ, CL_COMMAND_COPY_BUFFER, 0, 0));
    ASSERT_NE(nullptr, event);

    HwPerfCounter *perfCounter = event->getHwPerfCounterNode()->tag;
    ASSERT_NE(nullptr, perfCounter);

    ASSERT_EQ(0ULL, perfCounter->HWTimeStamp.GlobalStartTS);
    ASSERT_EQ(0ULL, perfCounter->HWTimeStamp.ContextStartTS);
    ASSERT_EQ(0ULL, perfCounter->HWTimeStamp.GlobalEndTS);
    ASSERT_EQ(0ULL, perfCounter->HWTimeStamp.ContextEndTS);
    ASSERT_EQ(0ULL, perfCounter->HWTimeStamp.GlobalCompleteTS);
    ASSERT_EQ(0ULL, perfCounter->HWTimeStamp.ContextCompleteTS);

    EXPECT_TRUE(perfCounter->canBeReleased());

    HwPerfCounter *perfCounter2 = event->getHwPerfCounterNode()->tag;
    ASSERT_EQ(perfCounter, perfCounter2);
}

TEST_F(EventTest, getHwPerfCounterAllocationReturnsValidPointer) {
    std::unique_ptr<Event> event(new Event(this->pCmdQ, CL_COMMAND_COPY_BUFFER, 0, 0));
    ASSERT_NE(nullptr, event);

    GraphicsAllocation *allocation = event->getHwPerfCounterNode()->getGraphicsAllocation();
    ASSERT_NE(nullptr, allocation);

    void *memoryStorage = allocation->getUnderlyingBuffer();
    size_t memoryStorageSize = allocation->getUnderlyingBufferSize();

    EXPECT_NE(nullptr, memoryStorage);
    EXPECT_GT(memoryStorageSize, 0u);
}

TEST_F(EventTest, hwPerfCounterMemoryIsPlacedInGraphicsAllocation) {
    std::unique_ptr<Event> event(new Event(this->pCmdQ, CL_COMMAND_COPY_BUFFER, 0, 0));
    ASSERT_NE(nullptr, event);

    HwPerfCounter *perfCounter = event->getHwPerfCounterNode()->tag;
    ASSERT_NE(nullptr, perfCounter);

    GraphicsAllocation *allocation = event->getHwPerfCounterNode()->getGraphicsAllocation();
    ASSERT_NE(nullptr, allocation);

    void *memoryStorage = allocation->getUnderlyingBuffer();
    size_t graphicsAllocationSize = allocation->getUnderlyingBufferSize();

    uintptr_t perfCounterAddress = reinterpret_cast<uintptr_t>(perfCounter);
    uintptr_t graphicsAllocationStart = reinterpret_cast<uintptr_t>(memoryStorage);

    if (!((perfCounterAddress >= graphicsAllocationStart) &&
          ((perfCounterAddress + sizeof(HwPerfCounter)) <= (graphicsAllocationStart + graphicsAllocationSize)))) {
        EXPECT_TRUE(false);
    }
}

TEST_F(EventTest, IsPerfCounter_DisabledByNullQueue) {
    Event ev(nullptr, CL_COMMAND_COPY_BUFFER, 3, 0);
    EXPECT_FALSE(ev.isProfilingEnabled());
    EXPECT_FALSE(ev.isPerfCountersEnabled());
}

TEST_F(EventTest, IsPerfCounter_DisabledByNoProfiling) {
    Event ev(pCmdQ, CL_COMMAND_COPY_BUFFER, 3, 0);
    EXPECT_FALSE(ev.isProfilingEnabled());
    EXPECT_FALSE(ev.isPerfCountersEnabled());
}

TEST_F(InternalsEventTest, IsPerfCounter_DisabledByNoPerfCounter) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    CommandQueue *pCmdQ = new CommandQueue(mockContext, pDevice, props);

    Event *ev = new Event(pCmdQ, CL_COMMAND_COPY_BUFFER, 3, 0);
    EXPECT_TRUE(ev->isProfilingEnabled());
    EXPECT_FALSE(ev->isPerfCountersEnabled());

    delete ev;
    delete pCmdQ;
}

TEST_F(InternalsEventWithPerfCountersTest, SetPerfCounter_negativeInvalidASInterface) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    CommandQueue *pCmdQ = new CommandQueue(mockContext, pDevice, props);
    performanceCountersBase->setAutoSamplingStartFunc(autoSamplingStartFailing);
    bool ret = false;
    ret = pCmdQ->setPerfCountersEnabled(true, 1);
    EXPECT_FALSE(ret);
    delete pCmdQ;
}

TEST_F(InternalsEventWithPerfCountersTest, SetPerfCounter_AvailFalse) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    CommandQueue *pCmdQ = new CommandQueue(mockContext, pDevice, props);

    bool ret = false;
    ret = pCmdQ->setPerfCountersEnabled(true, 1);
    EXPECT_TRUE(ret);
    performanceCountersBase->setAvailableFlag(false);
    ret = pCmdQ->setPerfCountersEnabled(false, 0);
    EXPECT_TRUE(ret);
    performanceCountersBase->shutdown();
    delete pCmdQ;
}

TEST_F(EventTest, GivenNullptrWhenpeekIsSubmittedThenFalse) {
    Event ev(this->pCmdQ, CL_COMMAND_COPY_BUFFER, 3, 0);
    bool executionStatus = ev.peekIsSubmitted(nullptr);
    EXPECT_NE(true, executionStatus);
}

TEST_F(EventTest, GivenCL_SUBMITTEDWhenpeekIsSubmittedThenTrue) {
    Event ev(this->pCmdQ, CL_COMMAND_COPY_BUFFER, 3, 0);
    int32_t executionStatusSnapshot = CL_SUBMITTED;
    bool executionStatus = ev.peekIsSubmitted(&executionStatusSnapshot);
    EXPECT_EQ(true, executionStatus);
}

TEST_F(EventTest, GivenCompletedEventWhenQueryingExecutionStatusAfterFlushThenCsrIsNotFlushed) {
    cl_int ret;
    Event ev(this->pCmdQ, CL_COMMAND_COPY_BUFFER, 3, 3);
    auto &csr = this->pCmdQ->getCommandStreamReceiver();
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

HWTEST_F(EventTest, submitCommandOnEventCreatedOnMapBufferWithoutCommandUpdatesTaskCount) {
    MockEvent<Event> ev(this->pCmdQ, CL_COMMAND_MAP_BUFFER, Event::eventNotReady, Event::eventNotReady);

    EXPECT_EQ(Event::eventNotReady, ev.peekTaskCount());
    ev.submitCommand(false);
    EXPECT_EQ(0u, ev.peekTaskCount());
}

HWTEST_F(EventTest, submitCommandOnEventCreatedOnMapImageWithoutCommandUpdatesTaskCount) {
    MockEvent<Event> ev(this->pCmdQ, CL_COMMAND_MAP_IMAGE, Event::eventNotReady, Event::eventNotReady);

    EXPECT_EQ(Event::eventNotReady, ev.peekTaskCount());
    ev.submitCommand(false);
    EXPECT_EQ(0u, ev.peekTaskCount());
}

TEST_F(EventTest, givenCmdQueueWithoutProfilingWhenIsCpuProfilingIsCalledThenFalseIsReturned) {
    MockEvent<Event> ev(this->pCmdQ, CL_COMMAND_MAP_IMAGE, Event::eventNotReady, Event::eventNotReady);
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
    std::unique_ptr<CommandQueue> pCmdQ(new CommandQueue(&mockContext, pDevice, props));

    MockEvent<Event> ev(pCmdQ.get(), CL_COMMAND_MAP_IMAGE, Event::eventNotReady, Event::eventNotReady);
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

TEST_F(EventTest, addChildForEventUncompleted) {
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

    Event childEvent0(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, Event::eventNotReady, Event::eventNotReady);
    Event childEvent1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, Event::eventNotReady, Event::eventNotReady);

    auto &csr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> &>(pCmdQ->getCommandStreamReceiver());
    csr.taskLevel = initialTaskLevel;

    parentEventWithGreaterTaskLevel.addChild(childEvent0);
    parentEventWithLowerTaskLevel.addChild(childEvent1);

    parentEventWithGreaterTaskLevel.setStatus(CL_COMPLETE);
    parentEventWithLowerTaskLevel.setStatus(CL_COMPLETE);

    EXPECT_EQ(parentEventWithGreaterTaskLevel.getTaskLevel() + 1, childEvent0.getTaskLevel());
    EXPECT_EQ(csr.taskLevel, childEvent1.getTaskLevel());
}

TEST_F(EventTest, addChildForEventCompleted) {
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
        MyCsr(const HardwareInfo &hwInfo, const ExecutionEnvironment &executionEnvironment) : UltCommandStreamReceiver<FamilyType>(hwInfo, const_cast<ExecutionEnvironment &>(executionEnvironment)) {}
        MOCK_METHOD3(waitForCompletionWithTimeout, bool(bool enableTimeout, int64_t timeoutMs, uint32_t taskCountToWait));
    };
    HardwareInfo localHwInfo = pDevice->getHardwareInfo();
    localHwInfo.capabilityTable.kmdNotifyProperties.enableKmdNotify = true;
    localHwInfo.capabilityTable.kmdNotifyProperties.enableQuickKmdSleep = true;
    localHwInfo.capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds = 1;
    localHwInfo.capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds = 2;

    auto csr = new ::testing::NiceMock<MyCsr>(localHwInfo, *pDevice->executionEnvironment);
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
        MyCsr(const HardwareInfo &hwInfo, const ExecutionEnvironment &executionEnvironment) : UltCommandStreamReceiver<FamilyType>(hwInfo, const_cast<ExecutionEnvironment &>(executionEnvironment)) {}
        MOCK_METHOD3(waitForCompletionWithTimeout, bool(bool enableTimeout, int64_t timeoutMs, uint32_t taskCountToWait));
    };
    HardwareInfo localHwInfo = pDevice->getHardwareInfo();
    localHwInfo.capabilityTable.kmdNotifyProperties.enableKmdNotify = true;
    localHwInfo.capabilityTable.kmdNotifyProperties.enableQuickKmdSleep = true;
    localHwInfo.capabilityTable.kmdNotifyProperties.enableQuickKmdSleepForSporadicWaits = false;
    localHwInfo.capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds = 1;
    localHwInfo.capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds = 2;

    auto csr = new ::testing::NiceMock<MyCsr>(localHwInfo, *pDevice->executionEnvironment);
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
    auto pCmdQ = std::unique_ptr<CommandQueue>(new CommandQueue(mockContext, pDevice, 0));
    MockEvent<Event> *event = new MockEvent<Event>(pCmdQ.get(), CL_COMMAND_MARKER, 0, 0);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.flushStamp->setStamp(5);

    FlushStamp expectedFlushStamp = 0;
    EXPECT_EQ(expectedFlushStamp, event->flushStamp->peekStamp());
    event->setCommand(std::unique_ptr<Command>(new CommandMarker(*pCmdQ.get(), csr, CL_COMMAND_MARKER, 4096u)));
    event->submitCommand(false);
    EXPECT_EQ(csr.flushStamp->peekStamp(), event->flushStamp->peekStamp());
    delete event;
}

HWTEST_F(InternalsEventTest, givenAbortedCommandWhenSubmitCalledThenDontUpdateFlushStamp) {
    auto pCmdQ = std::unique_ptr<CommandQueue>(new CommandQueue(mockContext, pDevice, 0));
    MockEvent<Event> *event = new MockEvent<Event>(pCmdQ.get(), CL_COMMAND_MARKER, 0, 0);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.flushStamp->setStamp(5);

    MockKernelWithInternals mockKernelWithInternals(*pDevice);
    auto pKernel = mockKernelWithInternals.mockKernel;

    auto cmdStream = new LinearStream(alignedMalloc(4096, 4096), 4096);
    IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    pCmdQ->allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, 4096u, dsh);
    pCmdQ->allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, 4096u, ioh);
    pCmdQ->allocateHeapMemory(IndirectHeap::SURFACE_STATE, 4096u, ssh);
    using UniqueIH = std::unique_ptr<IndirectHeap>;
    auto blockedCommandsData = new KernelOperation(std::unique_ptr<LinearStream>(cmdStream), UniqueIH(dsh),
                                                   UniqueIH(ioh), UniqueIH(ssh), *pCmdQ->getCommandStreamReceiver().getInternalAllocationStorage());
    PreemptionMode preemptionMode = pDevice->getPreemptionMode();
    std::vector<Surface *> v;
    auto cmd = new CommandComputeKernel(*pCmdQ, std::unique_ptr<KernelOperation>(blockedCommandsData), v, false, false, false, nullptr, preemptionMode, pKernel, 1);
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
