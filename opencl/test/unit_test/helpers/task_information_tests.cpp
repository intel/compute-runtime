/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/internal_allocation_storage.h"

#include "opencl/source/event/user_event.h"
#include "opencl/source/helpers/task_information.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/fixtures/dispatch_flags_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_csr.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "test.h"

#include <memory>

using namespace NEO;

TEST(CommandTest, mapUnmapSubmitWithoutTerminateFlagFlushesCsr) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    std::unique_ptr<MockCommandQueue> cmdQ(new MockCommandQueue(nullptr, device.get(), nullptr));
    MockCommandStreamReceiver csr(*device->getExecutionEnvironment(), device->getRootDeviceIndex());
    MockBuffer buffer;

    auto initialTaskCount = csr.peekTaskCount();

    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    std::unique_ptr<Command> command(new CommandMapUnmap(MapOperationType::MAP, buffer, size, offset, false, *cmdQ));
    CompletionStamp completionStamp = command->submit(20, false);

    auto expectedTaskCount = initialTaskCount + 1;
    EXPECT_EQ(expectedTaskCount, completionStamp.taskCount);
}

TEST(CommandTest, mapUnmapSubmitWithTerminateFlagAbortsFlush) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    std::unique_ptr<MockCommandQueue> cmdQ(new MockCommandQueue(nullptr, device.get(), nullptr));
    MockCommandStreamReceiver csr(*device->getExecutionEnvironment(), device->getRootDeviceIndex());
    MockBuffer buffer;

    auto initialTaskCount = csr.peekTaskCount();

    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    std::unique_ptr<Command> command(new CommandMapUnmap(MapOperationType::MAP, buffer, size, offset, false, *cmdQ));
    CompletionStamp completionStamp = command->submit(20, true);

    auto submitTaskCount = csr.peekTaskCount();
    EXPECT_EQ(initialTaskCount, submitTaskCount);

    auto expectedTaskCount = 0u;
    EXPECT_EQ(expectedTaskCount, completionStamp.taskCount);
}

TEST(CommandTest, markerSubmitWithoutTerminateFlagDosntFlushCsr) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    std::unique_ptr<MockCommandQueue> cmdQ(new MockCommandQueue(nullptr, device.get(), nullptr));
    MockCommandStreamReceiver csr(*device->getExecutionEnvironment(), device->getRootDeviceIndex());
    MockBuffer buffer;

    auto initialTaskCount = csr.peekTaskCount();
    std::unique_ptr<Command> command(new CommandWithoutKernel(*cmdQ));
    CompletionStamp completionStamp = command->submit(20, false);

    EXPECT_EQ(initialTaskCount, completionStamp.taskCount);
    EXPECT_EQ(initialTaskCount, csr.peekTaskCount());
}

TEST(CommandTest, markerSubmitWithTerminateFlagAbortsFlush) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    std::unique_ptr<MockCommandQueue> cmdQ(new MockCommandQueue(nullptr, device.get(), nullptr));
    MockCommandStreamReceiver csr(*device->getExecutionEnvironment(), device->getRootDeviceIndex());
    MockBuffer buffer;

    auto initialTaskCount = csr.peekTaskCount();
    std::unique_ptr<Command> command(new CommandWithoutKernel(*cmdQ));
    CompletionStamp completionStamp = command->submit(20, true);

    auto submitTaskCount = csr.peekTaskCount();
    EXPECT_EQ(initialTaskCount, submitTaskCount);

    auto expectedTaskCount = 0u;
    EXPECT_EQ(expectedTaskCount, completionStamp.taskCount);
}

TEST(CommandTest, givenWaitlistRequestWhenCommandComputeKernelIsCreatedThenMakeLocalCopyOfWaitlist) {
    class MockCommandComputeKernel : public CommandComputeKernel {
      public:
        using CommandComputeKernel::eventsWaitlist;
        MockCommandComputeKernel(CommandQueue &commandQueue, std::unique_ptr<KernelOperation> &kernelOperation, std::vector<Surface *> &surfaces, Kernel *kernel)
            : CommandComputeKernel(commandQueue, kernelOperation, surfaces, false, false, false, nullptr, PreemptionMode::Disabled, kernel, 0) {}
    };

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    MockCommandQueue cmdQ(nullptr, device.get(), nullptr);
    MockKernelWithInternals kernel(*device);

    IndirectHeap *ih1 = nullptr, *ih2 = nullptr, *ih3 = nullptr;
    cmdQ.allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, 1, ih1);
    cmdQ.allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, 1, ih2);
    cmdQ.allocateHeapMemory(IndirectHeap::SURFACE_STATE, 1, ih3);
    auto cmdStream = new LinearStream(device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), 1, GraphicsAllocation::AllocationType::COMMAND_BUFFER}));

    std::vector<Surface *> surfaces;
    auto kernelOperation = std::make_unique<KernelOperation>(cmdStream, *device->getDefaultEngine().commandStreamReceiver->getInternalAllocationStorage());
    kernelOperation->setHeaps(ih1, ih2, ih3);

    UserEvent event1, event2, event3;
    cl_event waitlist[] = {&event1, &event2};
    EventsRequest eventsRequest(2, waitlist, nullptr);

    MockCommandComputeKernel command(cmdQ, kernelOperation, surfaces, kernel);

    event1.incRefInternal();
    event2.incRefInternal();

    command.setEventsRequest(eventsRequest);

    waitlist[1] = &event3;

    EXPECT_EQ(static_cast<cl_event>(&event1), command.eventsWaitlist[0]);
    EXPECT_EQ(static_cast<cl_event>(&event2), command.eventsWaitlist[1]);
}

TEST(KernelOperationDestruction, givenKernelOperationWhenItIsDestructedThenAllAllocationsAreStoredInInternalStorageForReuse) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    MockCommandQueue cmdQ(nullptr, device.get(), nullptr);
    InternalAllocationStorage &allocationStorage = *device->getDefaultEngine().commandStreamReceiver->getInternalAllocationStorage();
    auto &allocationsForReuse = allocationStorage.getAllocationsForReuse();

    IndirectHeap *ih1 = nullptr, *ih2 = nullptr, *ih3 = nullptr;
    cmdQ.allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, 1, ih1);
    cmdQ.allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, 1, ih2);
    cmdQ.allocateHeapMemory(IndirectHeap::SURFACE_STATE, 1, ih3);
    auto cmdStream = new LinearStream(device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), 1, GraphicsAllocation::AllocationType::COMMAND_BUFFER}));

    auto &heapAllocation1 = *ih1->getGraphicsAllocation();
    auto &heapAllocation2 = *ih2->getGraphicsAllocation();
    auto &heapAllocation3 = *ih3->getGraphicsAllocation();
    auto &cmdStreamAllocation = *cmdStream->getGraphicsAllocation();

    auto kernelOperation = std::make_unique<KernelOperation>(cmdStream, allocationStorage);
    kernelOperation->setHeaps(ih1, ih2, ih3);
    EXPECT_TRUE(allocationsForReuse.peekIsEmpty());

    kernelOperation.reset();
    EXPECT_TRUE(allocationsForReuse.peekContains(cmdStreamAllocation));
    EXPECT_TRUE(allocationsForReuse.peekContains(heapAllocation1));
    EXPECT_TRUE(allocationsForReuse.peekContains(heapAllocation2));
    EXPECT_TRUE(allocationsForReuse.peekContains(heapAllocation3));
}

template <typename GfxFamily>
class MockCsr1 : public CommandStreamReceiverHw<GfxFamily> {
  public:
    CompletionStamp flushTask(LinearStream &commandStream, size_t commandStreamStart,
                              const IndirectHeap &dsh, const IndirectHeap &ioh,
                              const IndirectHeap &ssh, uint32_t taskLevel, DispatchFlags &dispatchFlags, Device &device) override {
        passedDispatchFlags = dispatchFlags;
        return CompletionStamp();
    }
    MockCsr1(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex) : CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiverHw(executionEnvironment, rootDeviceIndex) {}
    DispatchFlags passedDispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    using CommandStreamReceiver::timestampPacketWriteEnabled;
};

HWTEST_F(DispatchFlagsTests, givenCommandMapUnmapWhenSubmitThenPassCorrectDispatchFlags) {
    using CsrType = MockCsr1<FamilyType>;
    SetUpImpl<CsrType>();

    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    auto mockCsr = static_cast<CsrType *>(&mockCmdQ->getGpgpuCommandStreamReceiver());

    MockBuffer buffer;

    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    std::unique_ptr<Command> command(new CommandMapUnmap(MapOperationType::MAP, buffer, size, offset, false, *mockCmdQ));
    command->submit(20, false);
    PreemptionFlags flags = {};
    PreemptionMode devicePreemption = mockCmdQ->getDevice().getPreemptionMode();

    EXPECT_EQ(mockCmdQ->flushStamp->getStampReference(), mockCsr->passedDispatchFlags.flushStampReference);
    EXPECT_EQ(mockCmdQ->getThrottle(), mockCsr->passedDispatchFlags.throttle);
    EXPECT_EQ(PreemptionHelper::taskPreemptionMode(devicePreemption, flags), mockCsr->passedDispatchFlags.preemptionMode);
    EXPECT_EQ(GrfConfig::DefaultGrfNumber, mockCsr->passedDispatchFlags.numGrfRequired);
    EXPECT_EQ(L3CachingSettings::l3CacheOn, mockCsr->passedDispatchFlags.l3CacheSettings);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.blocking);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.dcFlush);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.useSLM);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.guardCommandBufferWithPipeControl);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.gsba32BitRequired);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.requiresCoherency);
    EXPECT_EQ(mockCmdQ->getPriority() == QueuePriority::LOW, mockCsr->passedDispatchFlags.lowPriority);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.implicitFlush);
    EXPECT_EQ(mockCmdQ->getGpgpuCommandStreamReceiver().isNTo1SubmissionModelEnabled(), mockCsr->passedDispatchFlags.outOfOrderExecutionAllowed);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.epilogueRequired);
}

HWTEST_F(DispatchFlagsTests, givenCommandComputeKernelWhenSubmitThenPassCorrectDispatchFlags) {
    using CsrType = MockCsr1<FamilyType>;
    SetUpImpl<CsrType>();
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    auto mockCsr = static_cast<CsrType *>(&mockCmdQ->getGpgpuCommandStreamReceiver());

    IndirectHeap *ih1 = nullptr, *ih2 = nullptr, *ih3 = nullptr;
    mockCmdQ->allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, 1, ih1);
    mockCmdQ->allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, 1, ih2);
    mockCmdQ->allocateHeapMemory(IndirectHeap::SURFACE_STATE, 1, ih3);

    PreemptionMode preemptionMode = device->getPreemptionMode();
    auto cmdStream = new LinearStream(device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), 1, GraphicsAllocation::AllocationType::COMMAND_BUFFER}));

    std::vector<Surface *> surfaces;
    auto kernelOperation = std::make_unique<KernelOperation>(cmdStream, *mockCmdQ->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    MockKernelWithInternals kernel(*device);
    kernelOperation->setHeaps(ih1, ih2, ih3);

    bool flushDC = false;
    bool slmUsed = false;
    bool ndRangeKernel = false;
    bool requiresCoherency = false;
    for (auto &surface : surfaces) {
        requiresCoherency |= surface->IsCoherent;
    }
    std::unique_ptr<Command> command(new CommandComputeKernel(*mockCmdQ, kernelOperation, surfaces, flushDC, slmUsed, ndRangeKernel, nullptr, preemptionMode, kernel, 1));
    command->submit(20, false);

    EXPECT_FALSE(mockCsr->passedDispatchFlags.pipelineSelectArgs.specialPipelineSelectMode);
    EXPECT_EQ(kernel.mockKernel->isVmeKernel(), mockCsr->passedDispatchFlags.pipelineSelectArgs.mediaSamplerRequired);
    EXPECT_EQ(mockCmdQ->flushStamp->getStampReference(), mockCsr->passedDispatchFlags.flushStampReference);
    EXPECT_EQ(mockCmdQ->getThrottle(), mockCsr->passedDispatchFlags.throttle);
    EXPECT_EQ(preemptionMode, mockCsr->passedDispatchFlags.preemptionMode);
    EXPECT_EQ(kernel.mockKernel->getKernelInfo().patchInfo.executionEnvironment->NumGRFRequired, mockCsr->passedDispatchFlags.numGrfRequired);
    EXPECT_EQ(L3CachingSettings::l3CacheOn, mockCsr->passedDispatchFlags.l3CacheSettings);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.blocking);
    EXPECT_EQ(flushDC, mockCsr->passedDispatchFlags.dcFlush);
    EXPECT_EQ(slmUsed, mockCsr->passedDispatchFlags.useSLM);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.guardCommandBufferWithPipeControl);
    EXPECT_EQ(ndRangeKernel, mockCsr->passedDispatchFlags.gsba32BitRequired);
    EXPECT_EQ(requiresCoherency, mockCsr->passedDispatchFlags.requiresCoherency);
    EXPECT_EQ(mockCmdQ->getPriority() == QueuePriority::LOW, mockCsr->passedDispatchFlags.lowPriority);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.implicitFlush);
    EXPECT_EQ(mockCmdQ->getGpgpuCommandStreamReceiver().isNTo1SubmissionModelEnabled(), mockCsr->passedDispatchFlags.outOfOrderExecutionAllowed);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.epilogueRequired);
}

HWTEST_F(DispatchFlagsTests, givenCommandWithoutKernelWhenSubmitThenPassCorrectDispatchFlags) {
    using CsrType = MockCsr1<FamilyType>;
    SetUpImpl<CsrType>();

    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    auto mockCsr = static_cast<CsrType *>(&mockCmdQ->getGpgpuCommandStreamReceiver());

    mockCsr->timestampPacketWriteEnabled = true;
    mockCmdQ->timestampPacketContainer = std::make_unique<TimestampPacketContainer>();
    IndirectHeap *ih1 = nullptr, *ih2 = nullptr, *ih3 = nullptr;
    TimestampPacketDependencies timestampPacketDependencies;
    mockCmdQ->allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, 1, ih1);
    mockCmdQ->allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, 1, ih2);
    mockCmdQ->allocateHeapMemory(IndirectHeap::SURFACE_STATE, 1, ih3);

    auto cmdStream = new LinearStream(device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), 1, GraphicsAllocation::AllocationType::COMMAND_BUFFER}));
    auto kernelOperation = std::make_unique<KernelOperation>(cmdStream, *mockCmdQ->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    kernelOperation->setHeaps(ih1, ih2, ih3);
    std::unique_ptr<Command> command(new CommandWithoutKernel(*mockCmdQ, kernelOperation));
    command->setTimestampPacketNode(*mockCmdQ->timestampPacketContainer, std::move(timestampPacketDependencies));

    command->submit(20, false);

    EXPECT_EQ(mockCmdQ->flushStamp->getStampReference(), mockCsr->passedDispatchFlags.flushStampReference);
    EXPECT_EQ(mockCmdQ->getThrottle(), mockCsr->passedDispatchFlags.throttle);
    EXPECT_EQ(mockCmdQ->getDevice().getPreemptionMode(), mockCsr->passedDispatchFlags.preemptionMode);
    EXPECT_EQ(GrfConfig::DefaultGrfNumber, mockCsr->passedDispatchFlags.numGrfRequired);
    EXPECT_EQ(L3CachingSettings::l3CacheOn, mockCsr->passedDispatchFlags.l3CacheSettings);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.blocking);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.dcFlush);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.useSLM);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.guardCommandBufferWithPipeControl);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.gsba32BitRequired);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.requiresCoherency);
    EXPECT_EQ(mockCmdQ->getPriority() == QueuePriority::LOW, mockCsr->passedDispatchFlags.lowPriority);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.implicitFlush);
    EXPECT_EQ(mockCmdQ->getGpgpuCommandStreamReceiver().isNTo1SubmissionModelEnabled(), mockCsr->passedDispatchFlags.outOfOrderExecutionAllowed);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.epilogueRequired);
}

HWTEST_F(DispatchFlagsTests, givenCommandComputeKernelWhenSubmitThenPassCorrectDispatchHints) {
    using CsrType = MockCsr1<FamilyType>;
    SetUpImpl<CsrType>();
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    auto mockCsr = static_cast<CsrType *>(&mockCmdQ->getGpgpuCommandStreamReceiver());

    IndirectHeap *ih1 = nullptr, *ih2 = nullptr, *ih3 = nullptr;
    mockCmdQ->allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, 1, ih1);
    mockCmdQ->allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, 1, ih2);
    mockCmdQ->allocateHeapMemory(IndirectHeap::SURFACE_STATE, 1, ih3);
    mockCmdQ->dispatchHints = 1234;

    PreemptionMode preemptionMode = device->getPreemptionMode();
    auto cmdStream = new LinearStream(device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), 1, GraphicsAllocation::AllocationType::COMMAND_BUFFER}));

    std::vector<Surface *> surfaces;
    auto kernelOperation = std::make_unique<KernelOperation>(cmdStream, *mockCmdQ->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    MockKernelWithInternals kernel(*device);
    kernelOperation->setHeaps(ih1, ih2, ih3);

    bool flushDC = false;
    bool slmUsed = false;
    bool ndRangeKernel = false;
    bool requiresCoherency = false;
    for (auto &surface : surfaces) {
        requiresCoherency |= surface->IsCoherent;
    }
    std::unique_ptr<Command> command(new CommandComputeKernel(*mockCmdQ, kernelOperation, surfaces, flushDC, slmUsed, ndRangeKernel, nullptr, preemptionMode, kernel, 1));
    command->submit(20, false);

    EXPECT_TRUE(mockCsr->passedDispatchFlags.epilogueRequired);
    EXPECT_EQ(1234u, mockCsr->passedDispatchFlags.engineHints);
    EXPECT_EQ(kernel.mockKernel->getThreadArbitrationPolicy(), mockCsr->passedDispatchFlags.threadArbitrationPolicy);
}
