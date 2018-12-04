/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

#include "runtime/event/user_event.h"
#include "runtime/helpers/task_information.h"
#include "runtime/memory_manager/internal_allocation_storage.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_kernel.h"

#include <memory>

using namespace OCLRT;

TEST(CommandTest, mapUnmapSubmitWithoutTerminateFlagFlushesCsr) {
    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    std::unique_ptr<MockCommandQueue> cmdQ(new MockCommandQueue(nullptr, device.get(), nullptr));
    MockCommandStreamReceiver csr(*device->getExecutionEnvironment());
    MockBuffer buffer;

    auto initialTaskCount = csr.peekTaskCount();

    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    std::unique_ptr<Command> command(new CommandMapUnmap(MapOperationType::MAP, buffer, size, offset, false, csr, *cmdQ.get()));
    CompletionStamp completionStamp = command->submit(20, false);

    auto expectedTaskCount = initialTaskCount + 1;
    EXPECT_EQ(expectedTaskCount, completionStamp.taskCount);
}

TEST(CommandTest, mapUnmapSubmitWithTerminateFlagAbortsFlush) {
    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    std::unique_ptr<MockCommandQueue> cmdQ(new MockCommandQueue(nullptr, device.get(), nullptr));
    MockCommandStreamReceiver csr(*device->getExecutionEnvironment());
    MockBuffer buffer;

    auto initialTaskCount = csr.peekTaskCount();

    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    std::unique_ptr<Command> command(new CommandMapUnmap(MapOperationType::MAP, buffer, size, offset, false, csr, *cmdQ.get()));
    CompletionStamp completionStamp = command->submit(20, true);

    auto submitTaskCount = csr.peekTaskCount();
    EXPECT_EQ(initialTaskCount, submitTaskCount);

    auto expectedTaskCount = 0u;
    EXPECT_EQ(expectedTaskCount, completionStamp.taskCount);
}

TEST(CommandTest, markerSubmitWithoutTerminateFlagFlushesCsr) {
    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    std::unique_ptr<MockCommandQueue> cmdQ(new MockCommandQueue(nullptr, device.get(), nullptr));
    MockCommandStreamReceiver csr(*device->getExecutionEnvironment());
    MockBuffer buffer;

    auto initialTaskCount = csr.peekTaskCount();
    std::unique_ptr<Command> command(new CommandMarker(*cmdQ.get(), csr, CL_COMMAND_MARKER, 0));
    CompletionStamp completionStamp = command->submit(20, false);

    auto expectedTaskCount = initialTaskCount + 1;
    EXPECT_EQ(expectedTaskCount, completionStamp.taskCount);
}

TEST(CommandTest, markerSubmitWithTerminateFlagAbortsFlush) {
    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    std::unique_ptr<MockCommandQueue> cmdQ(new MockCommandQueue(nullptr, device.get(), nullptr));
    MockCommandStreamReceiver csr(*device->getExecutionEnvironment());
    MockBuffer buffer;

    auto initialTaskCount = csr.peekTaskCount();
    std::unique_ptr<Command> command(new CommandMarker(*cmdQ.get(), csr, CL_COMMAND_MARKER, 0));
    CompletionStamp completionStamp = command->submit(20, true);

    auto submitTaskCount = csr.peekTaskCount();
    EXPECT_EQ(initialTaskCount, submitTaskCount);

    auto expectedTaskCount = 0u;
    EXPECT_EQ(expectedTaskCount, completionStamp.taskCount);
}

TEST(CommandTest, givenWaitlistRequestWhenCommandComputeKernelIsCreatedThenMakeLocalCopyOfWaitlist) {
    using UniqueIH = std::unique_ptr<IndirectHeap>;
    class MockCommandComputeKernel : public CommandComputeKernel {
      public:
        using CommandComputeKernel::eventsWaitlist;
        MockCommandComputeKernel(CommandQueue &commandQueue, KernelOperation *kernelResources, std::vector<Surface *> &surfaces, Kernel *kernel)
            : CommandComputeKernel(commandQueue, std::unique_ptr<KernelOperation>(kernelResources), surfaces, false, false, false, nullptr, PreemptionMode::Disabled, kernel, 0) {}
    };

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    CommandQueue cmdQ(nullptr, device.get(), nullptr);
    MockKernelWithInternals kernel(*device);

    IndirectHeap *ih1 = nullptr, *ih2 = nullptr, *ih3 = nullptr;
    cmdQ.allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, 1, ih1);
    cmdQ.allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, 1, ih2);
    cmdQ.allocateHeapMemory(IndirectHeap::SURFACE_STATE, 1, ih3);
    auto cmdStream = new LinearStream(alignedMalloc(1, 1), 1);

    std::vector<Surface *> surfaces;
    auto kernelOperation = new KernelOperation(std::unique_ptr<LinearStream>(cmdStream), UniqueIH(ih1), UniqueIH(ih2), UniqueIH(ih3),
                                               *device->getDefaultEngine().commandStreamReceiver->getInternalAllocationStorage());

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
