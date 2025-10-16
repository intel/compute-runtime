/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/source/utilities/wait_util.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/source/command_queue/hardware_interface.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/command_queue/hardware_interface_helper.h"
#include "opencl/test/unit_test/helpers/timestamp_packet_tests.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

using namespace NEO;

HWCMDTEST_F(IGFX_GEN12LP_CORE, TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEstimatingStreamSizeThenAddPipeControl) {
    MockKernelWithInternals kernel2(*device);
    MockMultiDispatchInfo multiDispatchInfo(device.get(), std::vector<Kernel *>({kernel->mockKernel, kernel2.mockKernel}));
    auto mockCmdQHw = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = false;
    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*mockCmdQHw, CsrDependencies(), false, false, false, multiDispatchInfo, nullptr, 0, false, false, false, nullptr);
    auto sizeWithDisabled = mockCmdQHw->requestedCmdStreamSize;

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*mockCmdQHw, CsrDependencies(), false, false, false, multiDispatchInfo, nullptr, 0, false, false, false, nullptr);
    auto sizeWithEnabled = mockCmdQHw->requestedCmdStreamSize;

    auto extendedSize = sizeWithDisabled + sizeof(typename FamilyType::PIPE_CONTROL);
    EXPECT_EQ(sizeWithEnabled, extendedSize);
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledAndOoqWhenEstimatingStreamSizeThenDontAddAdditionalSize) {
    MockMultiDispatchInfo multiDispatchInfo(device.get(), std::vector<Kernel *>({kernel->mockKernel}));
    auto mockCmdQHw = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);
    mockCmdQHw->setOoqEnabled();

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = false;
    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*mockCmdQHw, CsrDependencies(), false, false,
                                                            false, multiDispatchInfo, nullptr, 0, false, false, false, nullptr);
    auto sizeWithDisabled = mockCmdQHw->requestedCmdStreamSize;

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    MockTimestampPacketContainer timestamp1(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp2(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 2);
    MockTimestampPacketContainer timestamp3(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 3);
    MockTimestampPacketContainer timestamp4(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 4);
    MockTimestampPacketContainer timestamp5(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 5);

    Event event1(mockCmdQHw.get(), 0, 0, 0);
    event1.addTimestampPacketNodes(timestamp1);
    Event event2(mockCmdQHw.get(), 0, 0, 0);
    event2.addTimestampPacketNodes(timestamp2);
    Event event3(mockCmdQHw.get(), 0, 0, 0);
    event3.addTimestampPacketNodes(timestamp3);
    Event event4(mockCmdQHw.get(), 0, 0, 0);
    event4.addTimestampPacketNodes(timestamp4);
    Event event5(mockCmdQHw.get(), 0, 0, 0);
    event5.addTimestampPacketNodes(timestamp5);

    const cl_uint numEventsOnWaitlist = 5;
    cl_event waitlist[] = {&event1, &event2, &event3, &event4, &event5};

    EventsRequest eventsRequest(numEventsOnWaitlist, waitlist, nullptr);
    CsrDependencies csrDeps;
    eventsRequest.fillCsrDependenciesForTimestampPacketContainer(
        csrDeps, device->getGpgpuCommandStreamReceiver(), CsrDependencies::DependenciesType::onCsr);

    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*mockCmdQHw, csrDeps, false, false, false, multiDispatchInfo, nullptr, 0, false, false, false, nullptr);
    auto sizeWithEnabled = mockCmdQHw->requestedCmdStreamSize;

    size_t sizeForNodeDependency = 0;
    for (auto timestampPacketContainer : csrDeps.timestampPacketContainer) {
        for (auto &node : timestampPacketContainer->peekNodes()) {
            sizeForNodeDependency += TimestampPacketHelper::getRequiredCmdStreamSizeForSemaphoreNodeDependency<FamilyType>(*node);
        }
    }
    size_t extendedSize = sizeWithDisabled + EnqueueOperation<FamilyType>::getSizeRequiredForTimestampPacketWrite() + sizeForNodeDependency;

    EXPECT_EQ(sizeWithEnabled, extendedSize);
}

HWTEST_F(TimestampPacketTests, givenCrossCsrDependenciesWhenFillCsrDepsThenFlushCacheIfNeeded) {
    auto mockCmdQHw = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);
    mockCmdQHw->getUltCommandStreamReceiver().timestampPacketWriteEnabled = true;
    mockCmdQHw->getUltCommandStreamReceiver().taskCount = 1;
    mockCmdQHw->getUltCommandStreamReceiver().latestFlushedTaskCount = 0;

    cl_queue_properties props[] = {CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_LOW_KHR, 0};
    auto mockCmdQ2 = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), props);
    mockCmdQ2->getUltCommandStreamReceiver().timestampPacketWriteEnabled = true;
    mockCmdQ2->getUltCommandStreamReceiver().taskCount = 1;
    mockCmdQ2->getUltCommandStreamReceiver().latestFlushedTaskCount = 0;

    const cl_uint eventsOnWaitlist = 2;
    MockTimestampPacketContainer timestamp(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp2(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);

    Event event(mockCmdQ, 0, 0, 0);
    event.addTimestampPacketNodes(timestamp);
    Event event2(mockCmdQ2.get(), 0, 0, 0);
    event2.addTimestampPacketNodes(timestamp2);

    cl_event waitlist[] = {&event, &event2};
    EventsRequest eventsRequest(eventsOnWaitlist, waitlist, nullptr);
    CsrDependencies csrDeps;

    eventsRequest.fillCsrDependenciesForTimestampPacketContainer(csrDeps, mockCmdQ->getGpgpuCommandStreamReceiver(), CsrDependencies::DependenciesType::all);

    const auto &productHelper = device->getProductHelper();
    if (productHelper.isDcFlushAllowed()) {
        EXPECT_TRUE(mockCmdQ2->getUltCommandStreamReceiver().flushBatchedSubmissionsCalled);
    } else {
        EXPECT_FALSE(mockCmdQ2->getUltCommandStreamReceiver().flushBatchedSubmissionsCalled);
    }

    mockCmdQHw->getUltCommandStreamReceiver().latestFlushedTaskCount = 1;
    *mockCmdQHw->getUltCommandStreamReceiver().tagAddress = 1;
    mockCmdQ2->getUltCommandStreamReceiver().latestFlushedTaskCount = 1;
    *mockCmdQ2->getUltCommandStreamReceiver().tagAddress = 1;
}

HWTEST_F(TimestampPacketTests, givenCrossCsrDependenciesWhenFillCsrDepsThendependentCsrIsStoredInSet) {
    auto mockCmdQHw = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);
    mockCmdQHw->getUltCommandStreamReceiver().timestampPacketWriteEnabled = true;
    mockCmdQHw->getUltCommandStreamReceiver().taskCount = 1;
    mockCmdQHw->getUltCommandStreamReceiver().latestFlushedTaskCount = 0;

    cl_queue_properties props[] = {CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_LOW_KHR, 0};
    auto mockCmdQ2 = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), props);
    mockCmdQ2->getUltCommandStreamReceiver().timestampPacketWriteEnabled = true;
    mockCmdQ2->getUltCommandStreamReceiver().taskCount = 1;
    mockCmdQ2->getUltCommandStreamReceiver().latestFlushedTaskCount = 0;

    const cl_uint eventsOnWaitlist = 2;
    MockTimestampPacketContainer timestamp(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp2(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);

    Event event(mockCmdQ, 0, 0, 0);
    event.addTimestampPacketNodes(timestamp);
    Event event2(mockCmdQ2.get(), 0, 0, 0);
    event2.addTimestampPacketNodes(timestamp2);

    cl_event waitlist[] = {&event, &event2};
    EventsRequest eventsRequest(eventsOnWaitlist, waitlist, nullptr);
    CsrDependencies csrDeps;

    eventsRequest.fillCsrDependenciesForTimestampPacketContainer(csrDeps, mockCmdQ->getGpgpuCommandStreamReceiver(), CsrDependencies::DependenciesType::all);

    const auto &productHelper = device->getProductHelper();
    if (productHelper.isDcFlushAllowed()) {
        EXPECT_NE(csrDeps.csrWithMultiEngineDependencies.size(), 0u);
    } else {
        EXPECT_EQ(csrDeps.csrWithMultiEngineDependencies.size(), 0u);
    }
    EXPECT_TRUE(csrDeps.containsCrossEngineDependency);
    mockCmdQHw->getUltCommandStreamReceiver().latestFlushedTaskCount = 1;
    *mockCmdQHw->getUltCommandStreamReceiver().tagAddress = 1;
    mockCmdQ2->getUltCommandStreamReceiver().latestFlushedTaskCount = 1;
    *mockCmdQ2->getUltCommandStreamReceiver().tagAddress = 1;
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEstimatingStreamSizeWithWaitlistThenAddSizeForSemaphores) {
    MockKernelWithInternals kernel2(*device);
    MockMultiDispatchInfo multiDispatchInfo(device.get(), std::vector<Kernel *>({kernel->mockKernel, kernel2.mockKernel}));
    auto mockCmdQHw = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = false;
    const auto &hwInfo = device->getHardwareInfo();
    const auto &productHelper = device->getProductHelper();
    const bool isResolveDependenciesByPipeControlsEnabled = productHelper.isResolveDependenciesByPipeControlsSupported(hwInfo, mockCmdQHw->isOOQEnabled(), mockCmdQHw->taskCount, mockCmdQHw->getGpgpuCommandStreamReceiver());
    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*mockCmdQHw, CsrDependencies(), false, false, false, multiDispatchInfo, nullptr, 0, false, false, isResolveDependenciesByPipeControlsEnabled, nullptr);
    auto sizeWithDisabled = mockCmdQHw->requestedCmdStreamSize;

    csr.timestampPacketWriteEnabled = true;

    MockTimestampPacketContainer timestamp1(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp2(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 2);
    MockTimestampPacketContainer timestamp3(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 3);
    MockTimestampPacketContainer timestamp4(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 4);
    MockTimestampPacketContainer timestamp5(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 5);

    Event event1(mockCmdQHw.get(), 0, 0, 0);
    event1.addTimestampPacketNodes(timestamp1);
    Event event2(mockCmdQHw.get(), 0, 0, 0);
    event2.addTimestampPacketNodes(timestamp2);
    Event event3(mockCmdQHw.get(), 0, 0, 0);
    event3.addTimestampPacketNodes(timestamp3);
    Event event4(mockCmdQHw.get(), 0, 0, 0);
    event4.addTimestampPacketNodes(timestamp4);
    Event event5(mockCmdQHw.get(), 0, 0, 0);
    event5.addTimestampPacketNodes(timestamp5);

    const cl_uint numEventsOnWaitlist = 5;
    cl_event waitlist[] = {&event1, &event2, &event3, &event4, &event5};

    EventsRequest eventsRequest(numEventsOnWaitlist, waitlist, nullptr);
    CsrDependencies csrDeps;
    eventsRequest.fillCsrDependenciesForTimestampPacketContainer(csrDeps, device->getGpgpuCommandStreamReceiver(), CsrDependencies::DependenciesType::onCsr);

    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*mockCmdQHw, csrDeps, false, false, false, multiDispatchInfo, nullptr, 0, false, false, isResolveDependenciesByPipeControlsEnabled, nullptr);
    auto sizeWithEnabled = mockCmdQHw->requestedCmdStreamSize;

    size_t sizeForNodeDependency = 0;
    for (auto timestampPacketContainer : csrDeps.timestampPacketContainer) {
        for (auto &node : timestampPacketContainer->peekNodes()) {
            sizeForNodeDependency += TimestampPacketHelper::getRequiredCmdStreamSizeForSemaphoreNodeDependency<FamilyType>(*node);
        }
    }

    size_t sizeForPipeControl = 0;
    if (isResolveDependenciesByPipeControlsEnabled) {
        sizeForPipeControl = MemorySynchronizationCommands<FamilyType>::getSizeForStallingBarrier();
    }

    size_t extendedSize = sizeWithDisabled + EnqueueOperation<FamilyType>::getSizeRequiredForTimestampPacketWrite() + sizeForNodeDependency + sizeForPipeControl;

    EXPECT_EQ(sizeWithEnabled, extendedSize);
}

HWTEST_F(TimestampPacketTests, givenEventsRequestWithEventsWithoutTimestampsWhenComputingCsrDependenciesThenDoNotAddThem) {
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = false;

    Event eventWithoutTimestampContainer1(mockCmdQ, 0, 0, 0);
    Event eventWithoutTimestampContainer2(mockCmdQ, 0, 0, 0);
    Event eventWithoutTimestampContainer3(mockCmdQ, 0, 0, 0);
    Event eventWithoutTimestampContainer4(mockCmdQ, 0, 0, 0);
    Event eventWithoutTimestampContainer5(mockCmdQ, 0, 0, 0);

    const cl_uint numEventsOnWaitlist = 5;
    cl_event waitlist[] = {&eventWithoutTimestampContainer1, &eventWithoutTimestampContainer2, &eventWithoutTimestampContainer3,
                           &eventWithoutTimestampContainer4, &eventWithoutTimestampContainer5};

    EventsRequest eventsRequest(numEventsOnWaitlist, waitlist, nullptr);
    CsrDependencies csrDepsEmpty;
    eventsRequest.fillCsrDependenciesForTimestampPacketContainer(csrDepsEmpty, device->getGpgpuCommandStreamReceiver(), CsrDependencies::DependenciesType::onCsr);
    EXPECT_EQ(0u, csrDepsEmpty.timestampPacketContainer.size());

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    MockTimestampPacketContainer timestamp1(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp2(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 2);
    MockTimestampPacketContainer timestamp3(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 3);
    MockTimestampPacketContainer timestamp4(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 4);
    MockTimestampPacketContainer timestamp5(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 5);

    Event event1(mockCmdQ, 0, 0, 0);
    event1.addTimestampPacketNodes(timestamp1);

    Event eventWithEmptyTimestampContainer2(mockCmdQ, 0, 0, 0);
    // event2 does not have timestamp

    Event event3(mockCmdQ, 0, 0, 0);
    event3.addTimestampPacketNodes(timestamp3);

    Event eventWithEmptyTimestampContainer4(mockCmdQ, 0, 0, 0);
    // event4 does not have timestamp

    Event event5(mockCmdQ, 0, 0, 0);
    event5.addTimestampPacketNodes(timestamp5);

    cl_event waitlist2[] = {&event1, &eventWithEmptyTimestampContainer2, &event3, &eventWithEmptyTimestampContainer4, &event5};
    EventsRequest eventsRequest2(numEventsOnWaitlist, waitlist2, nullptr);
    CsrDependencies csrDepsSize3;
    eventsRequest2.fillCsrDependenciesForTimestampPacketContainer(csrDepsSize3, device->getGpgpuCommandStreamReceiver(), CsrDependencies::DependenciesType::onCsr);

    EXPECT_EQ(3u, csrDepsSize3.timestampPacketContainer.size());

    size_t sizeForNodeDependency = 0;
    for (auto timestampPacketContainer : csrDepsSize3.timestampPacketContainer) {
        for (auto &node : timestampPacketContainer->peekNodes()) {
            sizeForNodeDependency += TimestampPacketHelper::getRequiredCmdStreamSizeForSemaphoreNodeDependency<FamilyType>(*node);
        }
    }

    size_t expectedSize = sizeForNodeDependency;
    EXPECT_EQ(expectedSize, TimestampPacketHelper::getRequiredCmdStreamSize<FamilyType>(csrDepsSize3, false));
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, TimestampPacketTests, givenTimestampPacketWhenDispatchingGpuWalkerThenAddTwoPcForLastWalker) {
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    MockTimestampPacketContainer timestampPacket(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 2);

    MockKernelWithInternals kernel2(*device);

    MockMultiDispatchInfo multiDispatchInfo(device.get(), std::vector<Kernel *>({kernel->mockKernel, kernel2.mockKernel}));

    auto &cmdStream = mockCmdQ->getCS(0);

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    HardwareInterfaceWalkerArgs walkerArgs = createHardwareInterfaceWalkerArgs(CL_COMMAND_NDRANGE_KERNEL);
    walkerArgs.currentTimestampPacketNodes = &timestampPacket;
    HardwareInterface<FamilyType>::template dispatchWalker<GPGPU_WALKER>(
        *mockCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        walkerArgs);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    uint32_t walkersFound = 0;
    for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
        if (genCmdCast<GPGPU_WALKER *>(*it)) {
            if (MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(device->getRootDeviceEnvironment())) {
                auto pipeControl = genCmdCast<PIPE_CONTROL *>(*++it);
                EXPECT_NE(nullptr, pipeControl);
            }
            auto pipeControl = genCmdCast<PIPE_CONTROL *>(*++it);
            EXPECT_NE(nullptr, pipeControl);
            auto expectedAddress = TimestampPacketHelper::getContextEndGpuAddress(*timestampPacket.getNode(walkersFound));

            EXPECT_EQ(1u, pipeControl->getCommandStreamerStallEnable());
            EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, pipeControl->getPostSyncOperation());
            EXPECT_EQ(0u, pipeControl->getImmediateData());
            EXPECT_EQ(expectedAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));

            walkersFound++;
        }
    }
    EXPECT_EQ(2u, walkersFound);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, TimestampPacketTests, givenTimestampPacketDisabledWhenDispatchingGpuWalkerThenDontAddPipeControls) {
    MockTimestampPacketContainer timestampPacket(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockMultiDispatchInfo multiDispatchInfo(device.get(), kernel->mockKernel);
    auto &cmdStream = mockCmdQ->getCS(0);

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = false;
    HardwareInterfaceWalkerArgs walkerArgs = createHardwareInterfaceWalkerArgs(CL_COMMAND_NDRANGE_KERNEL);
    walkerArgs.currentTimestampPacketNodes = &timestampPacket;
    HardwareInterface<FamilyType>::template dispatchWalker<typename FamilyType::DefaultWalkerType>(
        *mockCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        walkerArgs);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    auto cmdItor = find<typename FamilyType::PIPE_CONTROL *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_EQ(hwParser.cmdList.end(), cmdItor);
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEnqueueingThenObtainNewStampAndPassToEvent) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;

    auto mockTagAllocator = new MockTagAllocator<>(device->getRootDeviceIndex(), executionEnvironment->memoryManager.get());
    csr.timestampPacketAllocator.reset(mockTagAllocator);
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);

    cl_event event1, event2;

    // obtain first node for cmdQ and event1
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event1);
    ASSERT_GT(cmdQ->timestampPacketContainer->peekNodes().size(), 0u);
    auto node1 = cmdQ->timestampPacketContainer->peekNodes().at(0);
    EXPECT_NE(nullptr, node1);
    EXPECT_EQ(node1, cmdQ->timestampPacketContainer->peekNodes().at(0));

    // obtain new node for cmdQ and event2
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event2);
    ASSERT_GT(cmdQ->timestampPacketContainer->peekNodes().size(), 0u);
    auto node2 = cmdQ->timestampPacketContainer->peekNodes().at(0);
    EXPECT_NE(nullptr, node2);
    EXPECT_EQ(node2, cmdQ->timestampPacketContainer->peekNodes().at(0));
    EXPECT_EQ(0u, mockTagAllocator->returnedToFreePoolNodes.size()); // nothing returned. event1 owns previous node
    EXPECT_EQ(1u, mockTagAllocator->releaseReferenceNodes.size());   // cmdQ released first node
    EXPECT_EQ(node1, mockTagAllocator->releaseReferenceNodes.at(0));

    EXPECT_NE(node1, node2);
    setTagToReadyState<FamilyType>(node1);
    setTagToReadyState<FamilyType>(node2);

    clReleaseEvent(event2);
    EXPECT_EQ(0u, mockTagAllocator->returnedToFreePoolNodes.size()); // nothing returned. cmdQ owns node2
    EXPECT_EQ(3u, mockTagAllocator->releaseReferenceNodes.size());   // event2 released node2 + deferred queue node
    EXPECT_EQ(node2, mockTagAllocator->releaseReferenceNodes.at(2));

    clReleaseEvent(event1);
    EXPECT_EQ(1u, mockTagAllocator->returnedToFreePoolNodes.size());
    EXPECT_EQ(4u, mockTagAllocator->releaseReferenceNodes.size()); // event1 released node1 + deferred queue node
    EXPECT_EQ(node1, mockTagAllocator->releaseReferenceNodes.at(3));
    {
        TimestampPacketContainer release;
        cmdQ->deferredTimestampPackets->swapNodes(release);
    }

    EXPECT_EQ(1u, mockTagAllocator->returnedToFreePoolNodes.size()); // removed last reference on node1
    EXPECT_EQ(node1, mockTagAllocator->returnedToFreePoolNodes.at(0));

    cmdQ.reset(nullptr);
    EXPECT_EQ(2u, mockTagAllocator->returnedToFreePoolNodes.size()); // removed last reference on node2
    EXPECT_EQ(node2, mockTagAllocator->returnedToFreePoolNodes.at(1));
    EXPECT_EQ(5u, mockTagAllocator->releaseReferenceNodes.size()); // cmdQ released node2
    EXPECT_EQ(node2, mockTagAllocator->releaseReferenceNodes.at(4));
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEnqueueingThenWriteWalkerStamp) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);
    using WalkerType = typename FamilyType::DefaultWalkerType;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(1u, cmdQ->timestampPacketContainer->peekNodes().size());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdQ->getCS(0), 0);
    hwParser.findHardwareCommands<FamilyType>();

    auto it = hwParser.itorWalker;
    auto walker = genCmdCast<WalkerType *>(*it);

    ASSERT_NE(nullptr, walker);
    if (MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(device->getRootDeviceEnvironment())) {
        auto pipeControl = genCmdCast<PIPE_CONTROL *>(*++it);
        EXPECT_NE(nullptr, pipeControl);
    }
    it = find<PIPE_CONTROL *>(++it, hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), it);
    auto pipeControl = genCmdCast<PIPE_CONTROL *>(*it);
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, pipeControl->getPostSyncOperation());
}

HWTEST_F(TimestampPacketTests, givenEventsRequestWhenEstimatingStreamSizeForCsrThenAddSizeForSemaphores) {
    auto device2 = std::make_unique<MockClDevice>(Device::create<MockDevice>(executionEnvironment, 0u));
    MockContext context2(device2.get());
    auto cmdQ2 = std::make_unique<MockCommandQueueHw<FamilyType>>(&context2, device2.get(), nullptr);

    MockTimestampPacketContainer timestamp1(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp2(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 2);
    MockTimestampPacketContainer timestamp3(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 3);
    MockTimestampPacketContainer timestamp4(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 4);
    MockTimestampPacketContainer timestamp5(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 5);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    auto &csr2 = device2->getUltCommandStreamReceiver<FamilyType>();
    csr2.timestampPacketWriteEnabled = true;

    Event event1(cmdQ2.get(), 0, 0, 0);
    event1.addTimestampPacketNodes(timestamp1);
    Event event2(cmdQ2.get(), 0, 0, 0);
    event2.addTimestampPacketNodes(timestamp2);
    Event event3(cmdQ2.get(), 0, 0, 0);
    event3.addTimestampPacketNodes(timestamp3);
    Event event4(cmdQ2.get(), 0, 0, 0);
    event4.addTimestampPacketNodes(timestamp4);
    Event event5(cmdQ2.get(), 0, 0, 0);
    event5.addTimestampPacketNodes(timestamp5);

    const cl_uint numEventsOnWaitlist = 5;
    cl_event waitlist[] = {&event1, &event2, &event3, &event4, &event5};

    EventsRequest eventsRequest(numEventsOnWaitlist, waitlist, nullptr);
    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();

    auto sizeWithoutEvents = csr.getRequiredCmdStreamSize(flags, device->getDevice());

    eventsRequest.fillCsrDependenciesForTimestampPacketContainer(flags.csrDependencies, csr, NEO::CsrDependencies::DependenciesType::outOfCsr);
    auto sizeWithEvents = csr.getRequiredCmdStreamSize(flags, device->getDevice());

    size_t sizeForNodeDependency = 0;
    for (auto timestampPacketContainer : flags.csrDependencies.timestampPacketContainer) {
        for (auto &node : timestampPacketContainer->peekNodes()) {
            sizeForNodeDependency += TimestampPacketHelper::getRequiredCmdStreamSizeForSemaphoreNodeDependency<FamilyType>(*node);
        }
    }
    size_t extendedSize = sizeWithoutEvents + sizeForNodeDependency;

    EXPECT_EQ(sizeWithEvents, extendedSize);
}

HWTEST_F(TimestampPacketTests, givenEventsRequestWhenEstimatingStreamSizeForDifferentCsrFromSameDeviceThenAddSizeForSemaphores) {
    // Create second (LOW_PRIORITY) queue on the same device
    cl_queue_properties props[] = {CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_LOW_KHR, 0};
    auto cmdQ2 = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), props);

    MockTimestampPacketContainer timestamp1(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp2(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 2);
    MockTimestampPacketContainer timestamp3(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 3);
    MockTimestampPacketContainer timestamp4(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 4);
    MockTimestampPacketContainer timestamp5(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 5);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    auto &csr2 = cmdQ2->getUltCommandStreamReceiver();
    csr2.timestampPacketWriteEnabled = true;

    Event event1(cmdQ2.get(), 0, 0, 0);
    event1.addTimestampPacketNodes(timestamp1);
    Event event2(cmdQ2.get(), 0, 0, 0);
    event2.addTimestampPacketNodes(timestamp2);
    Event event3(cmdQ2.get(), 0, 0, 0);
    event3.addTimestampPacketNodes(timestamp3);
    Event event4(cmdQ2.get(), 0, 0, 0);
    event4.addTimestampPacketNodes(timestamp4);
    Event event5(cmdQ2.get(), 0, 0, 0);
    event5.addTimestampPacketNodes(timestamp5);

    const cl_uint numEventsOnWaitlist = 5;
    cl_event waitlist[] = {&event1, &event2, &event3, &event4, &event5};

    EventsRequest eventsRequest(numEventsOnWaitlist, waitlist, nullptr);
    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();

    auto sizeWithoutEvents = csr.getRequiredCmdStreamSize(flags, device->getDevice());

    eventsRequest.fillCsrDependenciesForTimestampPacketContainer(flags.csrDependencies, csr, NEO::CsrDependencies::DependenciesType::outOfCsr);
    auto sizeWithEvents = csr.getRequiredCmdStreamSize(flags, device->getDevice());

    size_t sizeForNodeDependency = 0;
    for (auto timestampPacketContainer : flags.csrDependencies.timestampPacketContainer) {
        for (auto &node : timestampPacketContainer->peekNodes()) {
            sizeForNodeDependency += TimestampPacketHelper::getRequiredCmdStreamSizeForSemaphoreNodeDependency<FamilyType>(*node);
        }
    }

    size_t extendedSize = sizeWithoutEvents + sizeForNodeDependency;

    EXPECT_EQ(sizeWithEvents, extendedSize);
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEnqueueingThenProgramSemaphoresOnQueueStream) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto device2 = std::make_unique<MockClDevice>(Device::create<MockDevice>(executionEnvironment, 0u));

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    device2->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    MockContext context2(device2.get());

    auto cmdQ1 = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);
    auto cmdQ2 = std::make_unique<MockCommandQueueHw<FamilyType>>(&context2, device2.get(), nullptr);

    const cl_uint eventsOnWaitlist = 6;
    MockTimestampPacketContainer timestamp3(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp4(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp5(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp6(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 2);

    UserEvent event1;
    event1.setStatus(CL_COMPLETE);
    UserEvent event2;
    event2.setStatus(CL_COMPLETE);
    Event event3(cmdQ1.get(), 0, 0, 0);
    event3.addTimestampPacketNodes(timestamp3);
    Event event4(cmdQ2.get(), 0, 0, 0);
    event4.addTimestampPacketNodes(timestamp4);
    Event event5(cmdQ1.get(), 0, 0, 0);
    event5.addTimestampPacketNodes(timestamp5);
    Event event6(cmdQ2.get(), 0, 0, 0);
    event6.addTimestampPacketNodes(timestamp6);

    cl_event waitlist[] = {&event1, &event2, &event3, &event4, &event5, &event6};

    cmdQ1->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, eventsOnWaitlist, waitlist, nullptr);
    auto &cmdStream = *cmdQ1->commandStream;

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    auto queueSemaphores = findAll<MI_SEMAPHORE_WAIT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    auto expectedQueueSemaphoresCount = 5u;
    if (UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(device->getRootDeviceEnvironment())) {
        expectedQueueSemaphoresCount += 1;
    }

    EXPECT_EQ(expectedQueueSemaphoresCount, queueSemaphores.size());
    ASSERT_GE(queueSemaphores.size(), 5u);
    verifySemaphore<FamilyType>(genCmdCast<MI_SEMAPHORE_WAIT *>(*(queueSemaphores[0])), timestamp3.getNode(0), 0);
    verifySemaphore<FamilyType>(genCmdCast<MI_SEMAPHORE_WAIT *>(*(queueSemaphores[1])), timestamp5.getNode(0), 0);
    verifySemaphore<FamilyType>(genCmdCast<MI_SEMAPHORE_WAIT *>(*(queueSemaphores[2])), timestamp4.getNode(0), 0);
    verifySemaphore<FamilyType>(genCmdCast<MI_SEMAPHORE_WAIT *>(*(queueSemaphores[3])), timestamp6.getNode(0), 0);
    verifySemaphore<FamilyType>(genCmdCast<MI_SEMAPHORE_WAIT *>(*(queueSemaphores[4])), timestamp6.getNode(1), 0);
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEnqueueingThenTrackOwnershipUntilQueueIsCompleted) {
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);

    TimestampPacketContainer *timestampPacketContainer = cmdQ->timestampPacketContainer.get();
    TimestampPacketContainer *deferredTimestampPackets = cmdQ->deferredTimestampPackets.get();
    uint64_t latestNode = 0;

    {
        cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
        ASSERT_GT(timestampPacketContainer->peekNodes().size(), 0u);
        latestNode = timestampPacketContainer->peekNodes()[0]->getGpuAddress();
        EXPECT_EQ(0u, deferredTimestampPackets->peekNodes().size());
    }

    {
        cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

        EXPECT_EQ(1u, deferredTimestampPackets->peekNodes().size());
        ASSERT_GT(deferredTimestampPackets->peekNodes().size(), 0u);
        EXPECT_EQ(latestNode, deferredTimestampPackets->peekNodes().at(0u)->getGpuAddress());
        latestNode = timestampPacketContainer->peekNodes()[0]->getGpuAddress();
    }

    {
        cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

        EXPECT_EQ(2u, deferredTimestampPackets->peekNodes().size());
        ASSERT_GT(deferredTimestampPackets->peekNodes().size(), 1u);
        EXPECT_EQ(latestNode, deferredTimestampPackets->peekNodes().at(1u)->getGpuAddress());
        latestNode = timestampPacketContainer->peekNodes()[0]->getGpuAddress();
    }

    cmdQ->flush();
    EXPECT_EQ(2u, deferredTimestampPackets->peekNodes().size());
    cmdQ->finish(false);
    EXPECT_EQ(0u, deferredTimestampPackets->peekNodes().size());
}

HWTEST_F(TimestampPacketTests, givenWaitlistWithTimestampPacketWhenEnqueueingThenDeferWaitlistNodes) {
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);

    TimestampPacketContainer *deferredTimestampPackets = cmdQ->deferredTimestampPackets.get();

    MockTimestampPacketContainer timestamp(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);

    Event waitlistEvent(cmdQ.get(), 0, 0, 0);
    waitlistEvent.addTimestampPacketNodes(timestamp);

    cl_event waitlist[] = {&waitlistEvent};

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 1, waitlist, nullptr);

    EXPECT_EQ(1u, deferredTimestampPackets->peekNodes().size());
    ASSERT_GT(timestamp.peekNodes().size(), 0u);
    ASSERT_GT(deferredTimestampPackets->peekNodes().size(), 0u);
    EXPECT_EQ(timestamp.peekNodes()[0]->getGpuAddress(), deferredTimestampPackets->peekNodes()[0]->getGpuAddress());

    cmdQ->flush();
    EXPECT_EQ(1u, deferredTimestampPackets->peekNodes().size());
    ASSERT_GT(timestamp.peekNodes().size(), 0u);
    ASSERT_GT(deferredTimestampPackets->peekNodes().size(), 0u);
    EXPECT_EQ(timestamp.peekNodes()[0]->getGpuAddress(), deferredTimestampPackets->peekNodes()[0]->getGpuAddress());

    cmdQ->finish(false);
    EXPECT_EQ(0u, deferredTimestampPackets->peekNodes().size());
}

HWTEST_F(TimestampPacketTests, givenTimestampWaitEnabledWhenEnqueueWithEventThenEventHasCorrectTimestampsToCheckForCompletion) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UpdateTaskCountFromWait.set(3);
    debugManager.flags.EnableTimestampWaitForEvents.set(1);
    debugManager.flags.EnableTimestampWaitForQueues.set(1);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;
    csr.callBaseWaitForCompletionWithTimeout = false;
    *csr.getTagAddress() = 0u;
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);
    cmdQ->waitUntilCompleteReturnValue = WaitStatus::ready;

    cl_event clEvent1;
    cl_event clEvent2;

    TimestampPacketContainer *deferredTimestampPackets = cmdQ->deferredTimestampPackets.get();
    TimestampPacketContainer *timestampPacketContainer = cmdQ->timestampPacketContainer.get();

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &clEvent1);
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &clEvent2);
    cmdQ->flush();

    Event &event1 = static_cast<Event &>(*clEvent1);
    Event &event2 = static_cast<Event &>(*clEvent2);

    EXPECT_EQ(1u, deferredTimestampPackets->peekNodes().size());
    EXPECT_EQ(1u, timestampPacketContainer->peekNodes().size());

    EXPECT_FALSE(csr.downloadAllocationCalled);
    EXPECT_FALSE(event1.isCompleted());
    EXPECT_FALSE(event2.isCompleted());
    EXPECT_TRUE(csr.downloadAllocationCalled);
    csr.downloadAllocationCalled = false;

    typename FamilyType::TimestampPacketType timestampData[] = {2, 2, 2, 2};
    ASSERT_GT(deferredTimestampPackets->peekNodes().size(), 0u);
    for (uint32_t i = 0; i < deferredTimestampPackets->peekNodes()[0]->getPacketsUsed(); i++) {
        deferredTimestampPackets->peekNodes()[0]->assignDataToAllTimestamps(i, timestampData);
    }

    EXPECT_TRUE(event1.isCompleted());
    EXPECT_FALSE(event2.isCompleted());
    EXPECT_TRUE(csr.downloadAllocationCalled);
    csr.downloadAllocationCalled = false;

    ASSERT_GT(deferredTimestampPackets->peekNodes().size(), 0u);
    for (uint32_t i = 0; i < deferredTimestampPackets->peekNodes()[0]->getPacketsUsed(); i++) {
        timestampPacketContainer->peekNodes()[0]->assignDataToAllTimestamps(i, timestampData);
    }

    EXPECT_TRUE(event1.isCompleted());
    EXPECT_TRUE(event2.isCompleted());
    EXPECT_TRUE(csr.downloadAllocationCalled);
    csr.downloadAllocationCalled = false;

    cmdQ->finish(false);

    EXPECT_TRUE(event1.isCompleted());
    EXPECT_TRUE(event2.isCompleted());
    EXPECT_EQ(csr.waitForCompletionWithTimeoutTaskCountCalled, 0u);
    EXPECT_EQ(3u, csr.downloadAllocationsCalledCount);

    for (CopyEngineState &state : cmdQ->bcsStates) {
        if (state.isValid()) {
            auto bcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(cmdQ->getBcsCommandStreamReceiver(state.engineType));
            EXPECT_EQ(bcsCsr->waitForCompletionWithTimeoutTaskCountCalled, 0u);
            EXPECT_EQ(1u, csr.downloadAllocationsCalledCount);
        }
    }

    clReleaseEvent(clEvent1);
    clReleaseEvent(clEvent2);
    *csr.getTagAddress() = csr.peekTaskCount();
}

HWTEST_F(TimestampPacketTests, givenTimestampWaitEnabledWhenWaitDoesNotReturnReadyThenEventIsReportedAsNotReady) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UpdateTaskCountFromWait.set(3);
    debugManager.flags.EnableTimestampWaitForEvents.set(1);
    debugManager.flags.EnableTimestampWaitForQueues.set(1);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    if (!csr.getDcFlushSupport()) {
        GTEST_SKIP();
    }
    csr.timestampPacketWriteEnabled = true;
    csr.callBaseWaitForCompletionWithTimeout = false;
    *csr.getTagAddress() = 0u;
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);
    cmdQ->waitUntilCompleteReturnValue = WaitStatus::notReady;

    cl_event clEvent;
    TimestampPacketContainer *timestampPacketContainer = cmdQ->timestampPacketContainer.get();
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &clEvent);
    cmdQ->flush();

    EXPECT_EQ(1u, timestampPacketContainer->peekNodes().size());

    Event &event = static_cast<Event &>(*clEvent);
    EXPECT_FALSE(event.isCompleted());

    typename FamilyType::TimestampPacketType timestampData[] = {2, 2, 2, 2};
    for (uint32_t i = 0; i < timestampPacketContainer->peekNodes()[0]->getPacketsUsed(); i++) {
        timestampPacketContainer->peekNodes()[0]->assignDataToAllTimestamps(i, timestampData);
    }
    EXPECT_FALSE(event.isCompleted());

    cmdQ->waitUntilCompleteReturnValue = WaitStatus::ready;
    EXPECT_TRUE(event.isCompleted());

    clReleaseEvent(clEvent);
    *csr.getTagAddress() = csr.peekTaskCount();
}

HWTEST_F(TimestampPacketTests, givenEnableTimestampWaitForQueuesWhenFinishWithoutEnqueueThenDoNotWaitOnTimestamp) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UpdateTaskCountFromWait.set(3);
    debugManager.flags.EnableTimestampWaitForQueues.set(1);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;
    csr.callBaseWaitForCompletionWithTimeout = false;
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);

    TimestampPacketContainer *deferredTimestampPackets = cmdQ->deferredTimestampPackets.get();
    TimestampPacketContainer *timestampPacketContainer = cmdQ->timestampPacketContainer.get();

    EXPECT_EQ(0u, deferredTimestampPackets->peekNodes().size());
    EXPECT_EQ(0u, timestampPacketContainer->peekNodes().size());

    cmdQ->finish(false);

    EXPECT_EQ(csr.waitForCompletionWithTimeoutTaskCountCalled, 1u);
}

HWTEST_F(TimestampPacketTests, givenEnableTimestampWaitForQueuesWhenFinishThenWaitOnTimestamp) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UpdateTaskCountFromWait.set(3);
    debugManager.flags.EnableTimestampWaitForQueues.set(1);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;
    csr.callBaseWaitForCompletionWithTimeout = false;
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);

    TimestampPacketContainer *deferredTimestampPackets = cmdQ->deferredTimestampPackets.get();
    TimestampPacketContainer *timestampPacketContainer = cmdQ->timestampPacketContainer.get();

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    cmdQ->flush();

    EXPECT_EQ(1u, deferredTimestampPackets->peekNodes().size());
    EXPECT_EQ(1u, timestampPacketContainer->peekNodes().size());

    typename FamilyType::TimestampPacketType timestampData[] = {2, 2, 2, 2};
    ASSERT_GT(deferredTimestampPackets->peekNodes().size(), 0u);
    for (uint32_t i = 0; i < deferredTimestampPackets->peekNodes()[0]->getPacketsUsed(); i++) {
        timestampPacketContainer->peekNodes()[0]->assignDataToAllTimestamps(i, timestampData);
    }

    cmdQ->finish(false);

    EXPECT_EQ(csr.waitForCompletionWithTimeoutTaskCountCalled, 0u);
}

HWTEST_F(TimestampPacketTests, givenOOQAndEnableTimestampWaitForQueuesWhenFinishThenDontWaitOnTimestamp) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UpdateTaskCountFromWait.set(3);
    debugManager.flags.EnableTimestampWaitForQueues.set(1);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;
    csr.callBaseWaitForCompletionWithTimeout = false;
    cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), props);

    TimestampPacketContainer *deferredTimestampPackets = cmdQ->deferredTimestampPackets.get();
    TimestampPacketContainer *timestampPacketContainer = cmdQ->timestampPacketContainer.get();

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    cmdQ->flush();

    EXPECT_EQ(0u, deferredTimestampPackets->peekNodes().size());
    EXPECT_EQ(0u, timestampPacketContainer->peekNodes().size());

    cmdQ->finish(false);

    EXPECT_EQ(csr.waitForCompletionWithTimeoutTaskCountCalled, 1u);

    cmdQ.reset();
}

HWTEST_F(TimestampPacketTests, givenOOQAndWithoutEventWhenEnqueueCalledThenMoveCurrentNodeToDeferredContainer) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UpdateTaskCountFromWait.set(3);
    debugManager.flags.EnableTimestampWaitForQueues.set(0);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;
    csr.callBaseWaitForCompletionWithTimeout = false;
    cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), props);

    TimestampPacketContainer *deferredTimestampPackets = cmdQ->deferredTimestampPackets.get();
    TimestampPacketContainer *timestampPacketContainer = cmdQ->timestampPacketContainer.get();

    cl_event event;

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event);

    EXPECT_EQ(0u, deferredTimestampPackets->peekNodes().size());
    EXPECT_EQ(1u, timestampPacketContainer->peekNodes().size());

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    cmdQ->flush();

    EXPECT_EQ(1u, deferredTimestampPackets->peekNodes().size());
    EXPECT_EQ(0u, timestampPacketContainer->peekNodes().size());

    cmdQ->finish(false);

    clReleaseEvent(event);

    cmdQ.reset();
}

HWTEST_F(TimestampPacketTests, whenReleaseEventThenWait) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableTimestampWaitForQueues.set(0);
    debugManager.flags.BlockingEventRelease.set(1);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;
    csr.callBaseWaitForCompletionWithTimeout = false;
    cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), props);

    cl_event event;

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event);
    cmdQ->finish(false);
    EXPECT_EQ(csr.waitForCompletionWithTimeoutTaskCountCalled, 1u);

    clReleaseEvent(event);

    EXPECT_EQ(csr.waitForCompletionWithTimeoutTaskCountCalled, 2u);
    cmdQ.reset();
}

HWTEST_F(TimestampPacketTests, whenReleaseEventThenDownloadAllocations) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableTimestampWaitForQueues.set(0);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;
    csr.callBaseWaitForCompletionWithTimeout = false;
    cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), props);

    cl_event event;

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event);
    cmdQ->finish(false);
    EXPECT_EQ(csr.waitForCompletionWithTimeoutTaskCountCalled, 1u);

    clReleaseEvent(event);

    EXPECT_NE(csr.downloadAllocationsCalledCount.load(), 0u);
}

HWTEST_F(TimestampPacketTests, givenEventWhenReleasingThenCheckQueueResources) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UpdateTaskCountFromWait.set(3);
    debugManager.flags.EnableTimestampWaitForQueues.set(0);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;
    csr.callBaseWaitForCompletionWithTimeout = false;

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);

    TimestampPacketContainer *deferredTimestampPackets = cmdQ->deferredTimestampPackets.get();
    TimestampPacketContainer *timestampPacketContainer = cmdQ->timestampPacketContainer.get();

    cl_event clEvent;
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &clEvent);
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    cmdQ->flush();

    auto tagAddress = csr.getTagAddress();
    *tagAddress = csr.heaplessStateInitialized ? 2 : 1;

    EXPECT_EQ(csr.heaplessStateInitialized ? 3u : 2u, csr.taskCount);
    EXPECT_EQ(csr.heaplessStateInitialized ? 3u : 2u, cmdQ->taskCount);

    clWaitForEvents(1, &clEvent);

    EXPECT_EQ(1u, deferredTimestampPackets->peekNodes().size());
    EXPECT_EQ(1u, timestampPacketContainer->peekNodes().size());

    *tagAddress = csr.heaplessStateInitialized ? 3 : 2;

    clReleaseEvent(clEvent);

    EXPECT_EQ(0u, deferredTimestampPackets->peekNodes().size());
    EXPECT_EQ(1u, timestampPacketContainer->peekNodes().size());

    cmdQ.reset();
}

HWTEST_F(TimestampPacketTests, givenAllEnginesReadyWhenWaitingForEventThenClearDeferredNodes) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UpdateTaskCountFromWait.set(3);
    debugManager.flags.EnableTimestampWaitForQueues.set(1);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;
    csr.callBaseWaitForCompletionWithTimeout = false;
    cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), props);

    TimestampPacketContainer *deferredTimestampPackets = cmdQ->deferredTimestampPackets.get();
    TimestampPacketContainer *timestampPacketContainer = cmdQ->timestampPacketContainer.get();

    cl_event event1, event2;

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event1);
    auto node1 = timestampPacketContainer->peekNodes()[0];

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event2);
    auto node2 = timestampPacketContainer->peekNodes()[0];

    cmdQ->flush();

    EXPECT_EQ(cmdQ->heaplessStateInitEnabled ? 3u : 2u, csr.taskCount);

    auto tagAddress = csr.getTagAddress();
    *tagAddress = cmdQ->heaplessStateInitEnabled ? 2 : 1;

    auto eventObj1 = castToObjectOrAbort<Event>(event1);
    auto eventObj2 = castToObjectOrAbort<Event>(event2);

    auto contextEnd1 = ptrOffset(node1->getCpuBase(), node1->getContextEndOffset());
    auto contextEnd2 = ptrOffset(node2->getCpuBase(), node2->getContextEndOffset());

    *reinterpret_cast<typename FamilyType::TimestampPacketType *>(contextEnd1) = 0;
    *reinterpret_cast<typename FamilyType::TimestampPacketType *>(contextEnd2) = 0;

    EXPECT_EQ(1u, deferredTimestampPackets->peekNodes().size());
    EXPECT_EQ(1u, timestampPacketContainer->peekNodes().size());

    eventObj1->wait(false, false);
    EXPECT_EQ(1u, deferredTimestampPackets->peekNodes().size());
    EXPECT_EQ(1u, timestampPacketContainer->peekNodes().size());

    *tagAddress = cmdQ->heaplessStateInitEnabled ? 3 : 2;

    eventObj1->wait(false, false);
    EXPECT_EQ(0u, deferredTimestampPackets->peekNodes().size());
    EXPECT_EQ(1u, timestampPacketContainer->peekNodes().size());

    eventObj2->wait(false, false);
    EXPECT_EQ(0u, deferredTimestampPackets->peekNodes().size());
    EXPECT_EQ(1u, timestampPacketContainer->peekNodes().size());

    clReleaseEvent(event1);
    clReleaseEvent(event2);

    cmdQ.reset();
}

HWTEST_F(TimestampPacketTests, givenNewSubmissionWhileWaitingThenDontReleaseDeferredNodes) {
    class MyMockCmdQ : public MockCommandQueueHw<FamilyType> {
      public:
        using MockCommandQueueHw<FamilyType>::MockCommandQueueHw;

        WaitStatus waitUntilComplete(TaskCountType gpgpuTaskCountToWait, std::span<CopyEngineState> copyEnginesToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, bool cleanTemporaryAllocationList, bool skipWait) override {
            this->taskCount++;

            return this->MockCommandQueueHw<FamilyType>::waitUntilComplete(gpgpuTaskCountToWait, copyEnginesToWait, flushStampToWait, useQuickKmdSleep, cleanTemporaryAllocationList, skipWait);
        }
    };

    DebugManagerStateRestore restorer;
    debugManager.flags.UpdateTaskCountFromWait.set(3);
    debugManager.flags.EnableTimestampWaitForQueues.set(0);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;
    csr.callBaseWaitForCompletionWithTimeout = false;

    auto cmdQ = std::make_unique<MyMockCmdQ>(context, device.get(), nullptr);

    TimestampPacketContainer *deferredTimestampPackets = cmdQ->deferredTimestampPackets.get();
    TimestampPacketContainer *timestampPacketContainer = cmdQ->timestampPacketContainer.get();

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    cmdQ->flush();

    EXPECT_EQ(cmdQ->getHeaplessStateInitEnabled() ? 3u : 2u, csr.taskCount);
    EXPECT_EQ(cmdQ->getHeaplessStateInitEnabled() ? 3u : 2u, cmdQ->taskCount);

    auto tagAddress = csr.getTagAddress();
    *tagAddress = cmdQ->getHeaplessStateInitEnabled() ? 3 : 2;

    cmdQ->finish(false);

    EXPECT_EQ(1u, deferredTimestampPackets->peekNodes().size());
    EXPECT_EQ(1u, timestampPacketContainer->peekNodes().size());

    *tagAddress = 3;

    cmdQ.reset();
}

HWTEST_F(TimestampPacketTests, givenNewBcsSubmissionWhileWaitingThenDontReleaseDeferredNodes) {
    class MyMockCmdQ : public MockCommandQueueHw<FamilyType> {
      public:
        using MockCommandQueueHw<FamilyType>::MockCommandQueueHw;

        WaitStatus waitUntilComplete(TaskCountType gpgpuTaskCountToWait, std::span<CopyEngineState> copyEnginesToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, bool cleanTemporaryAllocationList, bool skipWait) override {
            this->bcsStates[0].taskCount++;

            return this->MockCommandQueueHw<FamilyType>::waitUntilComplete(gpgpuTaskCountToWait, copyEnginesToWait, flushStampToWait, useQuickKmdSleep, cleanTemporaryAllocationList, skipWait);
        }
    };

    DebugManagerStateRestore restorer;
    debugManager.flags.UpdateTaskCountFromWait.set(3);
    debugManager.flags.EnableTimestampWaitForQueues.set(0);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;
    csr.callBaseWaitForCompletionWithTimeout = false;

    EngineControl bcsEngine(&csr, &csr.getOsContext());

    auto cmdQ = std::make_unique<MyMockCmdQ>(context, device.get(), nullptr);

    TimestampPacketContainer *deferredTimestampPackets = cmdQ->deferredTimestampPackets.get();
    TimestampPacketContainer *timestampPacketContainer = cmdQ->timestampPacketContainer.get();

    cmdQ->bcsStates[0].engineType = aub_stream::EngineType::ENGINE_BCS;

    cmdQ->bcsEngines[0] = &bcsEngine;

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    cmdQ->flush();

    EXPECT_EQ(cmdQ->getHeaplessStateInitEnabled() ? 3u : 2u, csr.taskCount);
    cmdQ->bcsStates[0].taskCount = cmdQ->getHeaplessStateInitEnabled() ? 3 : 2;

    auto tagAddress = csr.getTagAddress();
    *tagAddress = cmdQ->getHeaplessStateInitEnabled() ? 3 : 2;

    cmdQ->finish(false);

    EXPECT_EQ(cmdQ->getHeaplessStateInitEnabled() ? 4u : 3u, cmdQ->bcsStates[0].taskCount);

    EXPECT_EQ(1u, deferredTimestampPackets->peekNodes().size());
    EXPECT_EQ(1u, timestampPacketContainer->peekNodes().size());

    *tagAddress = cmdQ->getHeaplessStateInitEnabled() ? 4 : 3;

    cmdQ->bcsEngines[0] = nullptr;
    cmdQ->bcsStates[0].engineType = aub_stream::EngineType::NUM_ENGINES;

    cmdQ.reset();
}

namespace CpuIntrinsicsTests {
extern std::atomic<uint32_t> pauseCounter;
extern volatile TagAddressType *pauseAddress;
extern TaskCountType pauseValue;
extern uint32_t pauseOffset;
extern std::function<void()> setupPauseAddress;
} // namespace CpuIntrinsicsTests

HWTEST_F(TimestampPacketTests, givenEnableTimestampWaitForQueuesWhenFinishThenCallWaitUtils) {
    VariableBackup<WaitUtils::WaitpkgUse> backupWaitpkgUse(&WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::noUse);
    VariableBackup<uint32_t> backupWaitCount(&WaitUtils::waitCount, 1);

    DebugManagerStateRestore restorer;
    debugManager.flags.UpdateTaskCountFromWait.set(3);
    debugManager.flags.EnableTimestampWaitForQueues.set(1);

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), props);

    TimestampPacketContainer *deferredTimestampPackets = cmdQ->deferredTimestampPackets.get();
    TimestampPacketContainer *timestampPacketContainer = cmdQ->timestampPacketContainer.get();

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    cmdQ->flush();

    EXPECT_EQ(0u, deferredTimestampPackets->peekNodes().size());
    EXPECT_EQ(0u, timestampPacketContainer->peekNodes().size());

    VariableBackup<volatile TagAddressType *> backupPauseAddress(&CpuIntrinsicsTests::pauseAddress);
    VariableBackup<TaskCountType> backupPauseValue(&CpuIntrinsicsTests::pauseValue);
    VariableBackup<uint32_t> backupPauseOffset(&CpuIntrinsicsTests::pauseOffset);
    VariableBackup<std::function<void()>> backupSetupPauseAddress(&CpuIntrinsicsTests::setupPauseAddress);

    auto &csr = cmdQ->getGpgpuCommandStreamReceiver();
    *csr.getTagAddress() = cmdQ->getHeaplessStateInitEnabled() ? 1 : 0;

    CpuIntrinsicsTests::pauseAddress = csr.getTagAddress();
    CpuIntrinsicsTests::pauseValue = cmdQ->getHeaplessStateInitEnabled() ? 4u : 3u;
    CpuIntrinsicsTests::setupPauseAddress = [&]() {
        CpuIntrinsicsTests::pauseAddress = csr.getTagAddress();
    };
    CpuIntrinsicsTests::pauseCounter = 0u;
    EXPECT_FALSE(device->getUltCommandStreamReceiver<FamilyType>().downloadAllocationCalled);

    cmdQ->finish(false);

    EXPECT_EQ(1u, CpuIntrinsicsTests::pauseCounter);
    EXPECT_TRUE(device->getUltCommandStreamReceiver<FamilyType>().downloadAllocationCalled);

    cmdQ.reset();
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEnqueueingToOoqThenMoveToDeferredList) {
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);
    cmdQ->setOoqEnabled();

    TimestampPacketContainer *deferredTimestampPackets = cmdQ->deferredTimestampPackets.get();

    cl_event event;

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event);
    EXPECT_EQ(0u, deferredTimestampPackets->peekNodes().size());

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(1u, deferredTimestampPackets->peekNodes().size());

    clReleaseEvent(event);
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEnqueueingBlockingThenTrackOwnershipUntilQueueIsCompleted) {
    debugManager.flags.MakeEachEnqueueBlocking.set(true);

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);

    TimestampPacketContainer *deferredTimestampPackets = cmdQ->deferredTimestampPackets.get();

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(0u, deferredTimestampPackets->peekNodes().size());

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(0u, deferredTimestampPackets->peekNodes().size());
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEnqueueingOnDifferentRootDeviceThenDontProgramSemaphoresOnCsrStream) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto device2 = std::make_unique<MockClDevice>(Device::create<MockDevice>(executionEnvironment, 1u));

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    device2->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    MockContext context2(device2.get());

    auto cmdQ1 = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);
    auto cmdQ2 = std::make_unique<MockCommandQueueHw<FamilyType>>(&context2, device2.get(), nullptr);

    const cl_uint eventsOnWaitlist = 6;
    MockTimestampPacketContainer timestamp3(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp4(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp5(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp6(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 2);

    UserEvent event1;
    event1.setStatus(CL_COMPLETE);
    UserEvent event2;
    event2.setStatus(CL_COMPLETE);
    Event event3(cmdQ1.get(), 0, 0, 0);
    event3.addTimestampPacketNodes(timestamp3);
    Event event4(cmdQ2.get(), 0, 0, 0);
    event4.addTimestampPacketNodes(timestamp4);
    Event event5(cmdQ1.get(), 0, 0, 0);
    event5.addTimestampPacketNodes(timestamp5);
    Event event6(cmdQ2.get(), 0, 0, 0);
    event6.addTimestampPacketNodes(timestamp6);

    cl_event waitlist[] = {&event1, &event2, &event3, &event4, &event5, &event6};

    cmdQ1->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, eventsOnWaitlist, waitlist, nullptr);

    auto &cmdStream = device->getUltCommandStreamReceiver<FamilyType>().commandStream;

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    auto it = hwParser.cmdList.begin();

    while (it != hwParser.cmdList.end()) {
        auto semaphoreWait = genCmdCast<MI_SEMAPHORE_WAIT *>(*it);
        if (semaphoreWait) {
            EXPECT_TRUE(UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWait(*semaphoreWait));
        }
        it++;
    }
}

HWTEST_F(TimestampPacketTests, givenAllDependencyTypesModeWhenFillingFromDifferentCsrsThenPushEverything) {
    auto device2 = std::make_unique<MockClDevice>(Device::create<MockDevice>(executionEnvironment, 0u));

    auto &csr1 = device->getUltCommandStreamReceiver<FamilyType>();
    auto &csr2 = device2->getUltCommandStreamReceiver<FamilyType>();
    csr1.timestampPacketWriteEnabled = true;
    csr2.timestampPacketWriteEnabled = true;

    MockContext context2(device2.get());

    auto cmdQ1 = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);
    auto cmdQ2 = std::make_unique<MockCommandQueueHw<FamilyType>>(&context2, device2.get(), nullptr);

    const cl_uint eventsOnWaitlist = 2;
    MockTimestampPacketContainer timestamp1(*csr1.getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp2(*csr2.getTimestampPacketAllocator(), 1);

    Event event1(cmdQ1.get(), 0, 0, 0);
    event1.addTimestampPacketNodes(timestamp1);
    Event event2(cmdQ2.get(), 0, 0, 0);
    event2.addTimestampPacketNodes(timestamp2);

    cl_event waitlist[] = {&event1, &event2};
    EventsRequest eventsRequest(eventsOnWaitlist, waitlist, nullptr);

    CsrDependencies csrDependencies;
    eventsRequest.fillCsrDependenciesForTimestampPacketContainer(csrDependencies, csr1, CsrDependencies::DependenciesType::all);
    EXPECT_EQ(static_cast<size_t>(eventsOnWaitlist), csrDependencies.timestampPacketContainer.size());
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledOnDifferentCSRsFromOneDeviceWhenEnqueueingThenProgramSemaphoresOnQueueStream) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    auto cmdQ1 = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);

    // Create second (LOW_PRIORITY) queue on the same device
    cl_queue_properties props[] = {CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_LOW_KHR, 0};
    auto cmdQ2 = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), props);
    cmdQ2->getUltCommandStreamReceiver().timestampPacketWriteEnabled = true;

    const cl_uint eventsOnWaitlist = 6;
    MockTimestampPacketContainer timestamp3(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp4(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp5(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp6(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 2);

    UserEvent event1;
    event1.setStatus(CL_COMPLETE);
    UserEvent event2;
    event2.setStatus(CL_COMPLETE);
    Event event3(cmdQ1.get(), 0, 0, 0);
    event3.addTimestampPacketNodes(timestamp3);
    Event event4(cmdQ2.get(), 0, 0, 0);
    event4.addTimestampPacketNodes(timestamp4);
    Event event5(cmdQ1.get(), 0, 0, 0);
    event5.addTimestampPacketNodes(timestamp5);
    Event event6(cmdQ2.get(), 0, 0, 0);
    event6.addTimestampPacketNodes(timestamp6);

    cl_event waitlist[] = {&event1, &event2, &event3, &event4, &event5, &event6};

    cmdQ1->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, eventsOnWaitlist, waitlist, nullptr);
    auto &cmdStream = *cmdQ1->commandStream;

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    auto queueSemaphores = findAll<MI_SEMAPHORE_WAIT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    auto expectedQueueSemaphoresCount = 5u;
    if (UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(device->getRootDeviceEnvironment())) {
        expectedQueueSemaphoresCount += 1;
    }
    EXPECT_EQ(expectedQueueSemaphoresCount, queueSemaphores.size());
    ASSERT_GE(queueSemaphores.size(), 5u);
    verifySemaphore<FamilyType>(genCmdCast<MI_SEMAPHORE_WAIT *>(*(queueSemaphores[0])), timestamp3.getNode(0), 0);
    verifySemaphore<FamilyType>(genCmdCast<MI_SEMAPHORE_WAIT *>(*(queueSemaphores[1])), timestamp5.getNode(0), 0);
    verifySemaphore<FamilyType>(genCmdCast<MI_SEMAPHORE_WAIT *>(*(queueSemaphores[2])), timestamp4.getNode(0), 0);
    verifySemaphore<FamilyType>(genCmdCast<MI_SEMAPHORE_WAIT *>(*(queueSemaphores[3])), timestamp6.getNode(0), 0);
    verifySemaphore<FamilyType>(genCmdCast<MI_SEMAPHORE_WAIT *>(*(queueSemaphores[4])), timestamp6.getNode(1), 0);
}

using TimestampPacketTestsWithMockCsrHw2 = TimestampPacketTestsWithMockCsrT<MockCsrHw2>;

HWTEST_TEMPLATED_F(TimestampPacketTestsWithMockCsrHw2, givenTimestampPacketWriteEnabledWhenEnqueueingBlockedThenProgramSemaphoresOnQueueStreamOnFlush) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&device->getGpgpuCommandStreamReceiver());
    mockCsr->timestampPacketWriteEnabled = true;
    mockCsr->storeFlushedTaskStream = true;

    auto device2 = std::make_unique<MockClDevice>(Device::create<MockDevice>(executionEnvironment, 0u));
    device2->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    auto context2 = new MockContext(device2.get());

    auto cmdQ1 = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));
    auto cmdQ2 = new MockCommandQueueHw<FamilyType>(context2, device2.get(), nullptr);

    MockTimestampPacketContainer timestamp0(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp1(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);

    UserEvent userEvent;
    Event event0(cmdQ1.get(), 0, 0, 0);
    event0.addTimestampPacketNodes(timestamp0);
    Event event1(cmdQ2, 0, 0, 0);
    event1.addTimestampPacketNodes(timestamp1);

    cl_event waitlist[] = {&userEvent, &event0, &event1};
    cmdQ1->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 3, waitlist, nullptr);

    mockCsr->commandStream.replaceBuffer(mockCsr->commandStream.getCpuBase(), mockCsr->commandStream.getMaxAvailableSpace());

    auto initialCsrStreamOffset = mockCsr->commandStream.getUsed();
    EXPECT_EQ(0u, initialCsrStreamOffset);
    userEvent.setStatus(CL_COMPLETE);
    cmdQ1->isQueueBlocked();
    cmdQ2->isQueueBlocked();

    HardwareParse hwParserCsr;
    HardwareParse hwParserCmdQ;
    LinearStream taskStream(mockCsr->storedTaskStream.get(), mockCsr->storedTaskStreamSize);
    taskStream.getSpace(mockCsr->storedTaskStreamSize);
    hwParserCsr.parseCommands<FamilyType>(mockCsr->commandStream, initialCsrStreamOffset);
    hwParserCmdQ.parseCommands<FamilyType>(taskStream, 0);

    auto queueSemaphores = findAll<MI_SEMAPHORE_WAIT *>(hwParserCmdQ.cmdList.begin(), hwParserCmdQ.cmdList.end());
    auto expectedQueueSemaphoresCount = 2u;
    if (UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(device->getRootDeviceEnvironment())) {
        expectedQueueSemaphoresCount += 1;
    }
    EXPECT_EQ(expectedQueueSemaphoresCount, queueSemaphores.size());
    ASSERT_GE(queueSemaphores.size(), 2u);
    verifySemaphore<FamilyType>(genCmdCast<MI_SEMAPHORE_WAIT *>(*(queueSemaphores[0])), timestamp0.getNode(0), 0);
    verifySemaphore<FamilyType>(genCmdCast<MI_SEMAPHORE_WAIT *>(*(queueSemaphores[1])), timestamp1.getNode(0), 0);

    auto csrSemaphores = findAll<MI_SEMAPHORE_WAIT *>(hwParserCsr.cmdList.begin(), hwParserCsr.cmdList.end());
    EXPECT_EQ(0u, csrSemaphores.size());

    EXPECT_TRUE(mockCsr->passedDispatchFlags.blocking);
    if (mockCsr->isUpdateTagFromWaitEnabled()) {
        EXPECT_FALSE(mockCsr->passedDispatchFlags.guardCommandBufferWithPipeControl);
    } else {
        EXPECT_TRUE(mockCsr->passedDispatchFlags.guardCommandBufferWithPipeControl);
    }
    EXPECT_EQ(device->getPreemptionMode(), mockCsr->passedDispatchFlags.preemptionMode);

    cmdQ2->release();
    context2->release();
}

HWTEST_TEMPLATED_F(TimestampPacketTestsWithMockCsrHw2, givenTimestampPacketWriteEnabledOnDifferentCSRsFromOneDeviceWhenEnqueueingBlockedThenProgramSemaphoresOnQueueStreamOnFlush) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&device->getGpgpuCommandStreamReceiver());
    mockCsr->timestampPacketWriteEnabled = true;
    mockCsr->storeFlushedTaskStream = true;

    auto cmdQ1 = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));

    // Create second (LOW_PRIORITY) queue on the same device
    cl_queue_properties props[] = {CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_LOW_KHR, 0};
    auto cmdQ2 = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), props));
    cmdQ2->getUltCommandStreamReceiver().timestampPacketWriteEnabled = true;

    MockTimestampPacketContainer timestamp0(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp1(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);

    UserEvent userEvent;
    Event event0(cmdQ1.get(), 0, 0, 0);
    event0.addTimestampPacketNodes(timestamp0);
    Event event1(cmdQ2.get(), 0, 0, 0);
    event1.addTimestampPacketNodes(timestamp1);

    cl_event waitlist[] = {&userEvent, &event0, &event1};
    cmdQ1->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 3, waitlist, nullptr);

    mockCsr->commandStream.replaceBuffer(mockCsr->commandStream.getCpuBase(), mockCsr->commandStream.getMaxAvailableSpace());

    auto initialCsrStreamOffset = mockCsr->commandStream.getUsed();
    EXPECT_EQ(0u, initialCsrStreamOffset);
    userEvent.setStatus(CL_COMPLETE);

    HardwareParse hwParserCsr;
    HardwareParse hwParserCmdQ;
    LinearStream taskStream(mockCsr->storedTaskStream.get(), mockCsr->storedTaskStreamSize);
    taskStream.getSpace(mockCsr->storedTaskStreamSize);
    hwParserCsr.parseCommands<FamilyType>(mockCsr->commandStream, initialCsrStreamOffset);
    hwParserCmdQ.parseCommands<FamilyType>(taskStream, 0);

    auto queueSemaphores = findAll<MI_SEMAPHORE_WAIT *>(hwParserCmdQ.cmdList.begin(), hwParserCmdQ.cmdList.end());
    auto expectedQueueSemaphoresCount = 2u;
    if (UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(device->getRootDeviceEnvironment())) {
        expectedQueueSemaphoresCount += 1;
    }
    EXPECT_EQ(expectedQueueSemaphoresCount, queueSemaphores.size());
    ASSERT_GE(queueSemaphores.size(), 2u);
    verifySemaphore<FamilyType>(genCmdCast<MI_SEMAPHORE_WAIT *>(*(queueSemaphores[0])), timestamp0.getNode(0), 0);
    verifySemaphore<FamilyType>(genCmdCast<MI_SEMAPHORE_WAIT *>(*(queueSemaphores[1])), timestamp1.getNode(0), 0);

    auto csrSemaphores = findAll<MI_SEMAPHORE_WAIT *>(hwParserCsr.cmdList.begin(), hwParserCsr.cmdList.end());
    EXPECT_EQ(0u, csrSemaphores.size());

    cmdQ2->isQueueBlocked();
    cmdQ1->isQueueBlocked();
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenDispatchingThenProgramSemaphoresForWaitlist) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using WALKER = typename FamilyType::DefaultWalkerType;

    auto device2 = std::make_unique<MockClDevice>(Device::create<MockDevice>(executionEnvironment, 0u));
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    device2->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    MockContext context2(device2.get());

    MockMultiDispatchInfo multiDispatchInfo(device.get(), std::vector<Kernel *>({kernel->mockKernel}));

    MockCommandQueue mockCmdQ2(&context2, device2.get(), nullptr, false);
    auto &cmdStream = mockCmdQ->getCS(0);

    const cl_uint eventsOnWaitlist = 6;
    MockTimestampPacketContainer timestamp3(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp4(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp5(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 2);
    MockTimestampPacketContainer timestamp6(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp7(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);

    UserEvent event1;
    UserEvent event2;
    Event event3(mockCmdQ, 0, 0, 0);
    event3.addTimestampPacketNodes(timestamp3);
    Event event4(&mockCmdQ2, 0, 0, 0);
    event4.addTimestampPacketNodes(timestamp4);
    Event event5(mockCmdQ, 0, 0, 0);
    event5.addTimestampPacketNodes(timestamp5);
    Event event6(&mockCmdQ2, 0, 0, 0);
    event6.addTimestampPacketNodes(timestamp6);

    cl_event waitlist[] = {&event1, &event2, &event3, &event4, &event5, &event6};

    EventsRequest eventsRequest(eventsOnWaitlist, waitlist, nullptr);
    CsrDependencies csrDeps;
    eventsRequest.fillCsrDependenciesForTimestampPacketContainer(csrDeps, mockCmdQ->getGpgpuCommandStreamReceiver(), CsrDependencies::DependenciesType::onCsr);

    HardwareInterfaceWalkerArgs walkerArgs = createHardwareInterfaceWalkerArgs(CL_COMMAND_NDRANGE_KERNEL);
    walkerArgs.currentTimestampPacketNodes = &timestamp7;
    HardwareInterface<FamilyType>::template dispatchWalker<WALKER>(
        *mockCmdQ,
        multiDispatchInfo,
        csrDeps,
        walkerArgs);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    uint32_t semaphoresFound = 0;
    uint32_t walkersFound = 0;

    for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*it);
        if (semaphoreCmd) {
            semaphoresFound++;
            if (semaphoresFound == 1) {
                verifySemaphore<FamilyType>(semaphoreCmd, timestamp3.getNode(0), 0);
            } else if (semaphoresFound == 2) {
                verifySemaphore<FamilyType>(semaphoreCmd, timestamp5.getNode(0), 0);
            } else if (semaphoresFound == 3) {
                verifySemaphore<FamilyType>(semaphoreCmd, timestamp5.getNode(1), 0);
            }
        }
        if (genCmdCast<WALKER *>(*it)) {
            walkersFound++;
            EXPECT_EQ(3u, semaphoresFound); // semaphores from events programmed before walker
        }
    }
    EXPECT_EQ(1u, walkersFound);
    EXPECT_EQ(3u, semaphoresFound); // total number of semaphores found in cmdList
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledOnDifferentCSRsFromOneDeviceWhenDispatchingThenProgramSemaphoresForWaitlist) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using WALKER = typename FamilyType::DefaultWalkerType;

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    MockMultiDispatchInfo multiDispatchInfo(device.get(), std::vector<Kernel *>({kernel->mockKernel}));

    // Create second (LOW_PRIORITY) queue on the same device
    cl_queue_properties props[] = {CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_LOW_KHR, 0};
    auto mockCmdQ2 = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), props);
    mockCmdQ2->getUltCommandStreamReceiver().timestampPacketWriteEnabled = true;

    auto &cmdStream = mockCmdQ->getCS(0);

    const cl_uint eventsOnWaitlist = 6;
    MockTimestampPacketContainer timestamp3(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp4(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp5(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 2);
    MockTimestampPacketContainer timestamp6(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp7(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);

    UserEvent event1;
    UserEvent event2;
    Event event3(mockCmdQ, 0, 0, 0);
    event3.addTimestampPacketNodes(timestamp3);
    Event event4(mockCmdQ2.get(), 0, 0, 0);
    event4.addTimestampPacketNodes(timestamp4);
    Event event5(mockCmdQ, 0, 0, 0);
    event5.addTimestampPacketNodes(timestamp5);
    Event event6(mockCmdQ2.get(), 0, 0, 0);
    event6.addTimestampPacketNodes(timestamp6);

    cl_event waitlist[] = {&event1, &event2, &event3, &event4, &event5, &event6};

    EventsRequest eventsRequest(eventsOnWaitlist, waitlist, nullptr);
    CsrDependencies csrDeps;
    eventsRequest.fillCsrDependenciesForTimestampPacketContainer(csrDeps, mockCmdQ->getGpgpuCommandStreamReceiver(), CsrDependencies::DependenciesType::onCsr);

    HardwareInterfaceWalkerArgs walkerArgs = createHardwareInterfaceWalkerArgs(CL_COMMAND_NDRANGE_KERNEL);
    walkerArgs.currentTimestampPacketNodes = &timestamp7;
    HardwareInterface<FamilyType>::template dispatchWalker<WALKER>(
        *mockCmdQ,
        multiDispatchInfo,
        csrDeps,
        walkerArgs);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    uint32_t semaphoresFound = 0;
    uint32_t walkersFound = 0;

    for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*it);
        if (semaphoreCmd) {
            semaphoresFound++;
            if (semaphoresFound == 1) {
                verifySemaphore<FamilyType>(semaphoreCmd, timestamp3.getNode(0), 0);
            } else if (semaphoresFound == 2) {
                verifySemaphore<FamilyType>(semaphoreCmd, timestamp5.getNode(0), 0);
            } else if (semaphoresFound == 3) {
                verifySemaphore<FamilyType>(semaphoreCmd, timestamp5.getNode(1), 0);
            }
        }
        if (genCmdCast<WALKER *>(*it)) {
            walkersFound++;
            EXPECT_EQ(3u, semaphoresFound); // semaphores from events programmed before walker
        }
    }
    EXPECT_EQ(1u, walkersFound);
    EXPECT_EQ(3u, semaphoresFound); // total number of semaphores found in cmdList
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledAndDependenciesResolvedViaPipeControlsIfPreviousOperationIsBlitThenStillProgramSemaphores) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ResolveDependenciesViaPipeControls.set(1);

    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using WALKER = typename FamilyType::DefaultWalkerType;

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    MockMultiDispatchInfo multiDispatchInfo(device.get(), std::vector<Kernel *>({kernel->mockKernel}));

    auto &cmdStream = mockCmdQ->getCS(0);
    mockCmdQ->updateLatestSentEnqueueType(NEO::EnqueueProperties::Operation::blit);
    const cl_uint eventsOnWaitlist = 1;
    MockTimestampPacketContainer timestamp(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);

    Event event(mockCmdQ, 0, 0, 0);
    event.addTimestampPacketNodes(timestamp);

    cl_event waitlist[] = {&event};

    EventsRequest eventsRequest(eventsOnWaitlist, waitlist, nullptr);
    CsrDependencies csrDeps;
    eventsRequest.fillCsrDependenciesForTimestampPacketContainer(csrDeps, mockCmdQ->getGpgpuCommandStreamReceiver(), CsrDependencies::DependenciesType::onCsr);

    HardwareInterfaceWalkerArgs walkerArgs = createHardwareInterfaceWalkerArgs(CL_COMMAND_NDRANGE_KERNEL);
    HardwareInterface<FamilyType>::template dispatchWalker<WALKER>(
        *mockCmdQ,
        multiDispatchInfo,
        csrDeps,
        walkerArgs);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    uint32_t semaphoresFound = 0;

    for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*it);
        if (semaphoreCmd) {
            semaphoresFound++;
        }
    }
    EXPECT_EQ(1u, semaphoresFound); // total number of semaphores found in cmdList
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledAndDependenciesResolvedViaPipeControlsAndSingleIOQWhenEnqueueKernelThenDoNotProgramSemaphoresButProgramPipeControlBeforeGpgpuWalker) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ResolveDependenciesViaPipeControls.set(1);
    debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(1);
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    using StallingBarrierType = typename FamilyType::StallingBarrierType;

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    auto cmdQ1 = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);

    const cl_uint eventsOnWaitlist = 4;
    MockTimestampPacketContainer timestamp3(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp4(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);

    UserEvent event1;
    event1.setStatus(CL_COMPLETE);
    UserEvent event2;
    event2.setStatus(CL_COMPLETE);
    Event event3(cmdQ1.get(), 0, 0, 0);
    event3.addTimestampPacketNodes(timestamp3);
    Event event4(cmdQ1.get(), 0, 0, 0);
    event4.addTimestampPacketNodes(timestamp4);

    cl_event waitlist[] = {&event1, &event2, &event3, &event4};
    ASSERT_EQ(cmdQ1->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0u, nullptr, nullptr), CL_SUCCESS);
    ASSERT_EQ(cmdQ1->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, eventsOnWaitlist, waitlist, nullptr), CL_SUCCESS);
    ASSERT_NE(cmdQ1->commandStream, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ1->commandStream, 0u);

    auto it = hwParser.cmdList.begin();
    size_t pipeControlCountFirstEnqueue = 0u;
    size_t pipeControlCountSecondEnqueue = 0u;
    size_t semaphoreWaitCount = 0u;
    size_t currentEnqueue = 1u;
    bool stallingBarrierProgrammed = false;
    while (it != hwParser.cmdList.end()) {
        MI_SEMAPHORE_WAIT *semaphoreWaitCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*it);
        PIPE_CONTROL *pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*it);
        StallingBarrierType *stallingBarrierCmd = genCmdCast<StallingBarrierType *>(*it);
        MI_BATCH_BUFFER_END *miBatchBufferEnd = genCmdCast<MI_BATCH_BUFFER_END *>(*it);
        if (pipeControlCmd != nullptr) {
            if (currentEnqueue == 1) {
                ++pipeControlCountFirstEnqueue;
            } else if (currentEnqueue == 2) {
                ++pipeControlCountSecondEnqueue;
            }
        } else if (stallingBarrierCmd != nullptr) {
            EXPECT_EQ(2u, currentEnqueue);
            stallingBarrierProgrammed = true;
        } else if (semaphoreWaitCmd != nullptr) {
            ++semaphoreWaitCount;
        } else if (miBatchBufferEnd != nullptr) {
            if (++currentEnqueue > 2) {
                break;
            }
        }
        ++it;
    }
    EXPECT_EQ(semaphoreWaitCount, 0u);
    auto stallingBarrierAsPC = stallingBarrierProgrammed ? 0 : 1;
    EXPECT_EQ(pipeControlCountSecondEnqueue, pipeControlCountFirstEnqueue + stallingBarrierAsPC);
}

HWTEST2_F(TimestampPacketTests, givenTimestampPacketWriteEnabledAndDependenciesResolvedViaPipeControlsAndSingleIOQWhenEnqueueKernelThenDoNotProgramSemaphoresButProgramPipeControlWithProperFlagsBeforeGpgpuWalker, IsXeHpgCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ResolveDependenciesViaPipeControls.set(1);
    debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(1);
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    auto cmdQ1 = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);

    const cl_uint eventsOnWaitlist = 4;
    MockTimestampPacketContainer timestamp3(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp4(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);

    UserEvent event1;
    event1.setStatus(CL_COMPLETE);
    UserEvent event2;
    event2.setStatus(CL_COMPLETE);
    Event event3(cmdQ1.get(), 0, 0, 0);
    event3.addTimestampPacketNodes(timestamp3);
    Event event4(cmdQ1.get(), 0, 0, 0);
    event4.addTimestampPacketNodes(timestamp4);

    cl_event waitlist[] = {&event1, &event2, &event3, &event4};
    ASSERT_EQ(cmdQ1->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0u, nullptr, nullptr), CL_SUCCESS);
    ASSERT_EQ(cmdQ1->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, eventsOnWaitlist, waitlist, nullptr), CL_SUCCESS);
    ASSERT_NE(cmdQ1->commandStream, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ1->commandStream, 0u);

    auto it = hwParser.cmdList.begin();
    size_t pipeControlCountFirstEnqueue = 0u;
    size_t pipeControlCountSecondEnqueue = 0u;
    size_t semaphoreWaitCount = 0u;
    size_t currentEnqueue = 1u;
    while (it != hwParser.cmdList.end()) {
        MI_SEMAPHORE_WAIT *semaphoreWaitCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*it);
        PIPE_CONTROL *pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*it);
        MI_BATCH_BUFFER_END *miBatchBufferEnd = genCmdCast<MI_BATCH_BUFFER_END *>(*it);
        if (pipeControlCmd != nullptr) {
            if (currentEnqueue == 1) {
                ++pipeControlCountFirstEnqueue;
            } else if (currentEnqueue == 2) {
                if (++pipeControlCountSecondEnqueue == 1) {
                    EXPECT_FALSE(pipeControlCmd->getHdcPipelineFlush());
                    EXPECT_FALSE(pipeControlCmd->getUnTypedDataPortCacheFlush());
                    EXPECT_TRUE(pipeControlCmd->getCommandStreamerStallEnable());
                }
            }
        } else if (semaphoreWaitCmd != nullptr) {
            ++semaphoreWaitCount;
        } else if (miBatchBufferEnd != nullptr) {
            if (++currentEnqueue > 2) {
                break;
            }
        }
        ++it;
    }
    EXPECT_EQ(semaphoreWaitCount, 0u);
    EXPECT_EQ(pipeControlCountSecondEnqueue, pipeControlCountFirstEnqueue + 1);
}

HWTEST_F(TimestampPacketTests, givenAlreadyAssignedNodeWhenEnqueueingNonBlockedThenMakeItResident) {
    auto mockTagAllocator = new MockTagAllocator<>(device->getRootDeviceIndex(), executionEnvironment->memoryManager.get(), 1);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketAllocator.reset(mockTagAllocator);
    csr.timestampPacketWriteEnabled = true;

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);
    TimestampPacketContainer previousNodes;
    cmdQ->obtainNewTimestampPacketNodes(1, previousNodes, false, cmdQ->getGpgpuCommandStreamReceiver());
    ASSERT_GT(cmdQ->timestampPacketContainer->peekNodes().size(), 0u);
    auto firstNode = cmdQ->timestampPacketContainer->peekNodes().at(0);

    csr.storeMakeResidentAllocations = true;
    csr.timestampPacketWriteEnabled = true;

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    ASSERT_GT(cmdQ->timestampPacketContainer->peekNodes().size(), 0u);
    auto secondNode = cmdQ->timestampPacketContainer->peekNodes().at(0);

    EXPECT_NE(firstNode->getBaseGraphicsAllocation(), secondNode->getBaseGraphicsAllocation());
    EXPECT_TRUE(csr.isMadeResident(firstNode->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation(), csr.taskCount));
}

HWTEST_F(TimestampPacketTests, givenNonHwCsrWhenGettingNewTagThenSetupPageTables) {
    auto mockTagAllocator = new MockTagAllocator<>(device->getRootDeviceIndex(), executionEnvironment->memoryManager.get(), 1);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketAllocator.reset(mockTagAllocator);
    csr.timestampPacketWriteEnabled = true;

    csr.commandStreamReceiverType = CommandStreamReceiverType::tbx;

    constexpr uint32_t allBanks = std::numeric_limits<uint32_t>::max();

    auto allocation = mockTagAllocator->freeTags.peekHead()->getBaseGraphicsAllocation()->getGraphicsAllocation(csr.getRootDeviceIndex());
    EXPECT_TRUE(allocation->isTbxWritable(allBanks));
    EXPECT_EQ(0u, csr.writeMemoryParams.totalCallCount);

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);
    TimestampPacketContainer previousNodes;
    cmdQ->obtainNewTimestampPacketNodes(1, previousNodes, false, cmdQ->getGpgpuCommandStreamReceiver());
    EXPECT_GT(cmdQ->timestampPacketContainer->peekNodes().size(), 0u);

    EXPECT_EQ(1u, csr.writeMemoryParams.totalCallCount);
    EXPECT_EQ(0u, csr.writeMemoryParams.chunkWriteCallCount);
    EXPECT_EQ(allocation, csr.writeMemoryParams.latestGfxAllocation);
}

HWTEST_F(TimestampPacketTests, givenAlreadyAssignedNodeWhenEnqueueingBlockedThenMakeItResident) {
    auto mockTagAllocator = new MockTagAllocator<>(device->getRootDeviceIndex(), executionEnvironment->memoryManager.get(), 1);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketAllocator.reset(mockTagAllocator);
    csr.timestampPacketWriteEnabled = true;

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));
    TimestampPacketContainer previousNodes;
    cmdQ->obtainNewTimestampPacketNodes(1, previousNodes, false, cmdQ->getGpgpuCommandStreamReceiver());

    ASSERT_GT(cmdQ->timestampPacketContainer->peekNodes().size(), 0u);
    auto firstNode = cmdQ->timestampPacketContainer->peekNodes().at(0);

    csr.storeMakeResidentAllocations = true;
    csr.timestampPacketWriteEnabled = true;

    UserEvent userEvent;
    cl_event clEvent = &userEvent;
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 1, &clEvent, nullptr);
    ASSERT_GT(cmdQ->timestampPacketContainer->peekNodes().size(), 0u);
    auto secondNode = cmdQ->timestampPacketContainer->peekNodes().at(0);

    EXPECT_NE(firstNode->getBaseGraphicsAllocation(), secondNode->getBaseGraphicsAllocation());
    EXPECT_FALSE(csr.isMadeResident(firstNode->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation(), csr.taskCount));
    userEvent.setStatus(CL_COMPLETE);
    EXPECT_TRUE(csr.isMadeResident(firstNode->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation(), csr.taskCount));
    cmdQ->isQueueBlocked();
}

HWTEST_F(TimestampPacketTests, givenAlreadyAssignedNodeWhenEnqueueingThenKeepDependencyOnPreviousNodeIfItsNotReady) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    MockTimestampPacketContainer firstNode(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 0);

    MockCommandQueueHw<FamilyType> cmdQ(context, device.get(), nullptr);
    TimestampPacketContainer previousNodes;
    cmdQ.obtainNewTimestampPacketNodes(2, previousNodes, false, cmdQ.getGpgpuCommandStreamReceiver());
    ASSERT_GT(cmdQ.timestampPacketContainer->peekNodes().size(), 1u);
    firstNode.add(cmdQ.timestampPacketContainer->peekNodes().at(0));
    firstNode.add(cmdQ.timestampPacketContainer->peekNodes().at(1));
    auto firstTag0 = firstNode.getNode(0);
    auto firstTag1 = firstNode.getNode(1);

    cmdQ.enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ.commandStream, 0);

    auto it = hwParser.cmdList.begin();
    verifySemaphore<FamilyType>(genCmdCast<MI_SEMAPHORE_WAIT *>(*it), firstTag0, 0);

    verifySemaphore<FamilyType>(genCmdCast<MI_SEMAPHORE_WAIT *>(*++it), firstTag1, 0);
    it++;

    while (it != hwParser.cmdList.end()) {
        auto semaphoreWait = genCmdCast<MI_SEMAPHORE_WAIT *>(*it);
        if (semaphoreWait) {
            EXPECT_TRUE(UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWait(*semaphoreWait));
        }
        it++;
    }
}

HWTEST_F(TimestampPacketTests, givenAlreadyAssignedNodeWhenEnqueueingToOoqThenDontKeepDependencyOnPreviousNodeIfItsNotReady) {
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    cl_queue_properties properties[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    MockCommandQueueHw<FamilyType> cmdQ(context, device.get(), properties);
    TimestampPacketContainer previousNodes;
    cmdQ.obtainNewTimestampPacketNodes(1, previousNodes, false, cmdQ.getGpgpuCommandStreamReceiver());

    cmdQ.enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ.commandStream, 0);

    uint32_t semaphoresFound = 0;
    for (auto &it : hwParser.cmdList) {
        if (genCmdCast<typename FamilyType::MI_SEMAPHORE_WAIT *>(it)) {
            semaphoresFound++;
        }
    }
    uint32_t expectedSemaphoresCount = (UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(device->getRootDeviceEnvironment()) ? 1 : 0);
    EXPECT_EQ(expectedSemaphoresCount, semaphoresFound);
}

HWTEST_F(TimestampPacketTests, givenAlreadyAssignedNodeWhenEnqueueingWithOmitTimestampPacketDependenciesThenDontKeepDependencyOnPreviousNodeIfItsNotReady) {
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    DebugManagerStateRestore restore;
    debugManager.flags.OmitTimestampPacketDependencies.set(true);

    MockCommandQueueHw<FamilyType> cmdQ(context, device.get(), nullptr);
    TimestampPacketContainer previousNodes;
    cmdQ.obtainNewTimestampPacketNodes(1, previousNodes, false, cmdQ.getGpgpuCommandStreamReceiver());

    cmdQ.enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ.commandStream, 0);

    uint32_t semaphoresFound = 0;
    for (auto &it : hwParser.cmdList) {
        if (genCmdCast<typename FamilyType::MI_SEMAPHORE_WAIT *>(it)) {
            semaphoresFound++;
        }
    }
    uint32_t expectedSemaphoresCount = (UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(device->getRootDeviceEnvironment()) ? 1 : 0);
    EXPECT_EQ(expectedSemaphoresCount, semaphoresFound);
}

HWTEST_F(TimestampPacketTests, givenEventsWaitlistFromDifferentDevicesWhenEnqueueingThenMakeAllTimestampsResident) {
    MockTagAllocator<TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount>> tagAllocator(device->getRootDeviceIndex(), executionEnvironment->memoryManager.get(), 1, 1,
                                                                                                              sizeof(TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount>), false, device->getDeviceBitfield());
    auto device2 = std::make_unique<MockClDevice>(Device::create<MockDevice>(executionEnvironment, 0u));

    auto &ultCsr = device->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.timestampPacketWriteEnabled = true;
    ultCsr.storeMakeResidentAllocations = true;
    device2->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    MockContext context2(device2.get());

    auto cmdQ1 = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);
    auto cmdQ2 = std::make_unique<MockCommandQueueHw<FamilyType>>(&context2, device2.get(), nullptr);

    MockTimestampPacketContainer node1(*ultCsr.getTimestampPacketAllocator(), 0);
    MockTimestampPacketContainer node2(*ultCsr.getTimestampPacketAllocator(), 0);

    auto tagNode1 = tagAllocator.getTag();
    node1.add(tagNode1);
    auto tagNode2 = tagAllocator.getTag();
    node2.add(tagNode2);

    Event event0(cmdQ1.get(), 0, 0, 0);
    event0.addTimestampPacketNodes(node1);
    Event event1(cmdQ2.get(), 0, 0, 0);
    event1.addTimestampPacketNodes(node2);

    cl_event waitlist[] = {&event0, &event1};

    cmdQ1->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 2, waitlist, nullptr);

    EXPECT_NE(tagNode1->getBaseGraphicsAllocation(), tagNode2->getBaseGraphicsAllocation());
    EXPECT_TRUE(ultCsr.isMadeResident(tagNode1->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation(), ultCsr.taskCount));
    EXPECT_TRUE(ultCsr.isMadeResident(tagNode2->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation(), ultCsr.taskCount));
}
