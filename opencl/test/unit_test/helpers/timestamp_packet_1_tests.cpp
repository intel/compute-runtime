/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/unit_test/utilities/base_object_utils.h"

#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/source/command_queue/hardware_interface.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/helpers/timestamp_packet_tests.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

using namespace NEO;

HWTEST_F(TimestampPacketTests, givenTagNodeWhenSemaphoreIsProgrammedThenUseGpuAddress) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    TimestampPackets<uint32_t> tag;
    MockTagNode mockNode;
    mockNode.tagForCpuAccess = &tag;
    mockNode.gpuAddress = 0x1230000;
    auto &cmdStream = mockCmdQ->getCS(0);

    TimestampPacketHelper::programSemaphore<FamilyType>(cmdStream, mockNode);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);
    auto it = hwParser.cmdList.begin();
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), &mockNode, 0);
}

HWTEST_F(TimestampPacketTests, givenTagNodeWithPacketsUsed2WhenSemaphoreIsProgrammedThenUseGpuAddress) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    TimestampPackets<uint32_t> tag;
    MockTagNode mockNode;
    mockNode.tagForCpuAccess = &tag;
    mockNode.gpuAddress = 0x1230000;
    mockNode.setPacketsUsed(2);
    auto &cmdStream = mockCmdQ->getCS(0);

    TimestampPacketHelper::programSemaphore<FamilyType>(cmdStream, mockNode);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);
    auto it = hwParser.cmdList.begin();
    for (uint32_t packetId = 0; packetId < mockNode.getPacketsUsed(); packetId++) {
        verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), &mockNode, packetId);
    }
}

TEST_F(TimestampPacketTests, givenTagNodeWhatAskingForGpuAddressesThenReturnCorrectValue) {
    TimestampPackets<uint32_t> tag;
    MockTagNode mockNode;
    mockNode.tagForCpuAccess = &tag;
    mockNode.gpuAddress = 0x1230000;

    auto expectedEndAddress = mockNode.getGpuAddress() + (2 * sizeof(uint32_t));
    EXPECT_EQ(expectedEndAddress, TimestampPacketHelper::getContextEndGpuAddress(mockNode));
}

TEST_F(TimestampPacketSimpleTests, givenTimestampPacketContainerWhenMovedThenMoveAllNodes) {
    EXPECT_TRUE(std::is_move_constructible<TimestampPacketContainer>::value);
    EXPECT_TRUE(std::is_move_assignable<TimestampPacketContainer>::value);
    EXPECT_FALSE(std::is_copy_assignable<TimestampPacketContainer>::value);
    EXPECT_FALSE(std::is_copy_constructible<TimestampPacketContainer>::value);

    struct MockTagNode : public TagNode<TimestampPackets<uint32_t>> {
        void returnTag() override {
            returnCalls++;
        }
        using TagNode<TimestampPackets<uint32_t>>::refCount;
        uint32_t returnCalls = 0;
    };

    MockTagNode node0;
    MockTagNode node1;

    {
        TimestampPacketContainer timestampPacketContainer0;
        TimestampPacketContainer timestampPacketContainer1;

        timestampPacketContainer0.add(&node0);
        timestampPacketContainer0.add(&node1);

        timestampPacketContainer1 = std::move(timestampPacketContainer0);
        EXPECT_EQ(0u, node0.returnCalls);
        EXPECT_EQ(0u, node1.returnCalls);
        EXPECT_EQ(2u, timestampPacketContainer1.peekNodes().size());
        EXPECT_EQ(&node0, timestampPacketContainer1.peekNodes()[0]);
        EXPECT_EQ(&node1, timestampPacketContainer1.peekNodes()[1]);
    }
    EXPECT_EQ(1u, node0.returnCalls);
    EXPECT_EQ(1u, node1.returnCalls);
}

HWTEST_F(TimestampPacketSimpleTests, whenNewTagIsTakenThenReinitialize) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(executionEnvironment);
    MockTagAllocator<MockTimestampPacketStorage> allocator(0, &memoryManager, 1);

    using MockNode = TagNode<MockTimestampPacketStorage>;

    auto firstNode = static_cast<MockNode *>(allocator.getTag());
    auto i = 0u;
    for (auto &packet : firstNode->tagForCpuAccess->packets) {
        packet.contextStart = i++;
        packet.globalStart = i++;
        packet.contextEnd = i++;
        packet.globalEnd = i++;
    }

    setTagToReadyState<FamilyType>(firstNode);
    allocator.returnTag(firstNode);

    auto secondNode = allocator.getTag();
    EXPECT_EQ(secondNode, firstNode);

    for (const auto &packet : firstNode->tagForCpuAccess->packets) {
        EXPECT_EQ(1u, packet.contextStart);
        EXPECT_EQ(1u, packet.globalStart);
        EXPECT_EQ(1u, packet.contextEnd);
        EXPECT_EQ(1u, packet.globalEnd);
    }
    EXPECT_EQ(1u, firstNode->getPacketsUsed());
}

TEST_F(TimestampPacketSimpleTests, whenObjectIsCreatedThenInitializeAllStamps) {
    MockTimestampPacketStorage timestampPacketStorage;
    EXPECT_EQ(TimestampPacketSizeControl::preferredPacketCount * sizeof(timestampPacketStorage.packets[0]), sizeof(timestampPacketStorage.packets));

    for (const auto &packet : timestampPacketStorage.packets) {
        EXPECT_EQ(1u, packet.contextStart);
        EXPECT_EQ(1u, packet.globalStart);
        EXPECT_EQ(1u, packet.contextEnd);
        EXPECT_EQ(1u, packet.globalEnd);
    }
}

HWTEST_F(TimestampPacketTests, givenCommandStreamReceiverHwWhenObtainingPreferredTagPoolSizeThenReturnCorrectValue) {
    OsContext &osContext = *executionEnvironment->memoryManager->getRegisteredEngines()[0].osContext;

    CommandStreamReceiverHw<FamilyType> csr(*executionEnvironment, 0, osContext.getDeviceBitfield());
    EXPECT_EQ(2048u, csr.getPreferredTagPoolSize());
}

HWTEST_F(TimestampPacketTests, givenDebugFlagSetWhenCreatingAllocatorThenUseCorrectSize) {
    OsContext &osContext = *executionEnvironment->memoryManager->getRegisteredEngines()[0].osContext;

    {
        CommandStreamReceiverHw<FamilyType> csr(*executionEnvironment, 0, osContext.getDeviceBitfield());
        csr.setupContext(osContext);

        auto allocator = csr.getTimestampPacketAllocator();
        auto tag = allocator->getTag();
        auto size = tag->getSinglePacketSize();
        EXPECT_EQ(4u * sizeof(typename FamilyType::TimestampPacketType), size);
    }

    {
        DebugManager.flags.OverrideTimestampPacketSize.set(4);

        CommandStreamReceiverHw<FamilyType> csr(*executionEnvironment, 0, osContext.getDeviceBitfield());
        csr.setupContext(osContext);

        auto allocator = csr.getTimestampPacketAllocator();
        auto tag = allocator->getTag();
        auto size = tag->getSinglePacketSize();
        EXPECT_EQ(4u * sizeof(uint32_t), size);
    }

    {
        DebugManager.flags.OverrideTimestampPacketSize.set(8);

        CommandStreamReceiverHw<FamilyType> csr(*executionEnvironment, 0, osContext.getDeviceBitfield());
        csr.setupContext(osContext);

        auto allocator = csr.getTimestampPacketAllocator();
        auto tag = allocator->getTag();
        auto size = tag->getSinglePacketSize();
        EXPECT_EQ(4u * sizeof(uint64_t), size);
    }

    {
        DebugManager.flags.OverrideTimestampPacketSize.set(-1);
        CommandStreamReceiverHw<FamilyType> csr(*executionEnvironment, 0, osContext.getDeviceBitfield());
        csr.setupContext(osContext);

        DebugManager.flags.OverrideTimestampPacketSize.set(12);
        EXPECT_ANY_THROW(csr.getTimestampPacketAllocator());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, TimestampPacketTests, givenInvalidDebugFlagSetWhenCreatingCsrThenExceptionIsThrown) {
    OsContext &osContext = *executionEnvironment->memoryManager->getRegisteredEngines()[0].osContext;
    DebugManager.flags.OverrideTimestampPacketSize.set(12);

    EXPECT_ANY_THROW(CommandStreamReceiverHw<FamilyType> csr(*executionEnvironment, 0, osContext.getDeviceBitfield()));
}

HWTEST_F(TimestampPacketTests, givenTagAlignmentWhenCreatingAllocatorThenGpuAddressIsAligned) {
    auto csr = executionEnvironment->memoryManager->getRegisteredEngines()[0].commandStreamReceiver;

    auto &hwHelper = HwHelper::get(device->getHardwareInfo().platform.eRenderCoreFamily);

    auto allocator = csr->getTimestampPacketAllocator();

    auto tag1 = allocator->getTag();
    auto tag2 = allocator->getTag();

    EXPECT_TRUE(isAligned(tag1->getGpuAddress(), hwHelper.getTimestampPacketAllocatorAlignment()));
    EXPECT_TRUE(isAligned(tag2->getGpuAddress(), hwHelper.getTimestampPacketAllocatorAlignment()));
}

HWTEST_F(TimestampPacketTests, givenDebugFlagSetWhenCreatingTimestampPacketAllocatorThenDisableReusingAndLimitPoolSize) {
    DebugManagerStateRestore restore;
    DebugManager.flags.DisableTimestampPacketOptimizations.set(true);
    OsContext &osContext = *executionEnvironment->memoryManager->getRegisteredEngines()[0].osContext;

    CommandStreamReceiverHw<FamilyType> csr(*executionEnvironment, 0, osContext.getDeviceBitfield());
    csr.setupContext(osContext);
    EXPECT_EQ(1u, csr.getPreferredTagPoolSize());

    auto tag = csr.getTimestampPacketAllocator()->getTag();
    setTagToReadyState<FamilyType>(tag);

    EXPECT_FALSE(tag->canBeReleased());
}

HWCMDTEST_F(IGFX_GEN8_CORE, TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEstimatingStreamSizeThenAddPipeControl) {
    MockKernelWithInternals kernel2(*device);
    MockMultiDispatchInfo multiDispatchInfo(device.get(), std::vector<Kernel *>({kernel->mockKernel, kernel2.mockKernel}));
    auto mockCmdQHw = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = false;
    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*mockCmdQHw, CsrDependencies(), false, false, false, multiDispatchInfo, nullptr, 0, false, false);
    auto sizeWithDisabled = mockCmdQHw->requestedCmdStreamSize;

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*mockCmdQHw, CsrDependencies(), false, false, false, multiDispatchInfo, nullptr, 0, false, false);
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
                                                            false, multiDispatchInfo, nullptr, 0, false, false);
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
        csrDeps, device->getGpgpuCommandStreamReceiver(), CsrDependencies::DependenciesType::OnCsr);

    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*mockCmdQHw, csrDeps, false, false, false, multiDispatchInfo, nullptr, 0, false, false);
    auto sizeWithEnabled = mockCmdQHw->requestedCmdStreamSize;

    size_t sizeForNodeDependency = 0;
    for (auto timestampPacketContainer : csrDeps.timestampPacketContainer) {
        for (auto &node : timestampPacketContainer->peekNodes()) {
            sizeForNodeDependency += TimestampPacketHelper::getRequiredCmdStreamSizeForNodeDependency<FamilyType>(*node);
        }
    }
    size_t extendedSize = sizeWithDisabled + EnqueueOperation<FamilyType>::getSizeRequiredForTimestampPacketWrite() + sizeForNodeDependency;

    EXPECT_EQ(sizeWithEnabled, extendedSize);
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEstimatingStreamSizeWithWaitlistThenAddSizeForSemaphores) {
    MockKernelWithInternals kernel2(*device);
    MockMultiDispatchInfo multiDispatchInfo(device.get(), std::vector<Kernel *>({kernel->mockKernel, kernel2.mockKernel}));
    auto mockCmdQHw = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = false;
    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*mockCmdQHw, CsrDependencies(), false, false, false, multiDispatchInfo, nullptr, 0, false, false);
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
    eventsRequest.fillCsrDependenciesForTimestampPacketContainer(csrDeps, device->getGpgpuCommandStreamReceiver(), CsrDependencies::DependenciesType::OnCsr);

    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*mockCmdQHw, csrDeps, false, false, false, multiDispatchInfo, nullptr, 0, false, false);
    auto sizeWithEnabled = mockCmdQHw->requestedCmdStreamSize;

    size_t sizeForNodeDependency = 0;
    for (auto timestampPacketContainer : csrDeps.timestampPacketContainer) {
        for (auto &node : timestampPacketContainer->peekNodes()) {
            sizeForNodeDependency += TimestampPacketHelper::getRequiredCmdStreamSizeForNodeDependency<FamilyType>(*node);
        }
    }

    size_t extendedSize = sizeWithDisabled + EnqueueOperation<FamilyType>::getSizeRequiredForTimestampPacketWrite() + sizeForNodeDependency;

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
    eventsRequest.fillCsrDependenciesForTimestampPacketContainer(csrDepsEmpty, device->getGpgpuCommandStreamReceiver(), CsrDependencies::DependenciesType::OnCsr);
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
    eventsRequest2.fillCsrDependenciesForTimestampPacketContainer(csrDepsSize3, device->getGpgpuCommandStreamReceiver(), CsrDependencies::DependenciesType::OnCsr);

    EXPECT_EQ(3u, csrDepsSize3.timestampPacketContainer.size());

    size_t sizeForNodeDependency = 0;
    for (auto timestampPacketContainer : csrDepsSize3.timestampPacketContainer) {
        for (auto &node : timestampPacketContainer->peekNodes()) {
            sizeForNodeDependency += TimestampPacketHelper::getRequiredCmdStreamSizeForNodeDependency<FamilyType>(*node);
        }
    }

    size_t expectedSize = sizeForNodeDependency;
    EXPECT_EQ(expectedSize, TimestampPacketHelper::getRequiredCmdStreamSize<FamilyType>(csrDepsSize3));
}

HWTEST_F(TimestampPacketTests, whenEstimatingSizeForNodeDependencyThenReturnCorrectValue) {
    TimestampPackets<uint32_t> tag;
    MockTagNode mockNode;
    mockNode.tagForCpuAccess = &tag;
    mockNode.gpuAddress = 0x1230000;

    size_t sizeForNodeDependency = 0;
    sizeForNodeDependency += TimestampPacketHelper::getRequiredCmdStreamSizeForNodeDependency<FamilyType>(mockNode);

    size_t expectedSize = mockNode.getPacketsUsed() * sizeof(typename FamilyType::MI_SEMAPHORE_WAIT);

    EXPECT_EQ(expectedSize, sizeForNodeDependency);
}

HWCMDTEST_F(IGFX_GEN8_CORE, TimestampPacketTests, givenTimestampPacketWhenDispatchingGpuWalkerThenAddTwoPcForLastWalker) {
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    MockTimestampPacketContainer timestampPacket(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 2);

    MockKernelWithInternals kernel2(*device);

    MockMultiDispatchInfo multiDispatchInfo(device.get(), std::vector<Kernel *>({kernel->mockKernel, kernel2.mockKernel}));

    auto &cmdStream = mockCmdQ->getCS(0);

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    HardwareInterface<FamilyType>::dispatchWalker(
        *mockCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &timestampPacket,
        CL_COMMAND_NDRANGE_KERNEL);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    uint32_t walkersFound = 0;
    for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
        if (genCmdCast<GPGPU_WALKER *>(*it)) {
            if (MemorySynchronizationCommands<FamilyType>::isPipeControlWArequired(device->getHardwareInfo())) {
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

HWCMDTEST_F(IGFX_GEN8_CORE, TimestampPacketTests, givenTimestampPacketDisabledWhenDispatchingGpuWalkerThenDontAddPipeControls) {
    MockTimestampPacketContainer timestampPacket(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockMultiDispatchInfo multiDispatchInfo(device.get(), kernel->mockKernel);
    auto &cmdStream = mockCmdQ->getCS(0);

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = false;

    HardwareInterface<FamilyType>::dispatchWalker(
        *mockCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &timestampPacket,
        CL_COMMAND_NDRANGE_KERNEL);

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
    auto node1 = cmdQ->timestampPacketContainer->peekNodes().at(0);
    EXPECT_NE(nullptr, node1);
    EXPECT_EQ(node1, cmdQ->timestampPacketContainer->peekNodes().at(0));

    // obtain new node for cmdQ and event2
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event2);
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
    EXPECT_EQ(2u, mockTagAllocator->releaseReferenceNodes.size());   // event2 released  node2
    EXPECT_EQ(node2, mockTagAllocator->releaseReferenceNodes.at(1));

    clReleaseEvent(event1);
    EXPECT_EQ(0u, mockTagAllocator->returnedToFreePoolNodes.size());
    EXPECT_EQ(3u, mockTagAllocator->releaseReferenceNodes.size()); // event1 released node1
    EXPECT_EQ(node1, mockTagAllocator->releaseReferenceNodes.at(2));
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
    using GPGPU_WALKER = typename FamilyType::WALKER_TYPE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(1u, cmdQ->timestampPacketContainer->peekNodes().size());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdQ->getCS(0), 0);

    bool walkerFound = false;
    for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
        if (genCmdCast<GPGPU_WALKER *>(*it)) {
            if (MemorySynchronizationCommands<FamilyType>::isPipeControlWArequired(device->getHardwareInfo())) {
                auto pipeControl = genCmdCast<PIPE_CONTROL *>(*++it);
                EXPECT_NE(nullptr, pipeControl);
            }
            walkerFound = true;
            it = find<PIPE_CONTROL *>(++it, hwParser.cmdList.end());
            ASSERT_NE(hwParser.cmdList.end(), it);
            auto pipeControl = genCmdCast<PIPE_CONTROL *>(*it);
            ASSERT_NE(nullptr, pipeControl);
            EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, pipeControl->getPostSyncOperation());
        }
    }
    EXPECT_TRUE(walkerFound);
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

    eventsRequest.fillCsrDependenciesForTimestampPacketContainer(flags.csrDependencies, csr, NEO::CsrDependencies::DependenciesType::OutOfCsr);
    auto sizeWithEvents = csr.getRequiredCmdStreamSize(flags, device->getDevice());

    size_t sizeForNodeDependency = 0;
    for (auto timestampPacketContainer : flags.csrDependencies.timestampPacketContainer) {
        for (auto &node : timestampPacketContainer->peekNodes()) {
            sizeForNodeDependency += TimestampPacketHelper::getRequiredCmdStreamSizeForNodeDependency<FamilyType>(*node);
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

    eventsRequest.fillCsrDependenciesForTimestampPacketContainer(flags.csrDependencies, csr, NEO::CsrDependencies::DependenciesType::OutOfCsr);
    auto sizeWithEvents = csr.getRequiredCmdStreamSize(flags, device->getDevice());

    size_t sizeForNodeDependency = 0;
    for (auto timestampPacketContainer : flags.csrDependencies.timestampPacketContainer) {
        for (auto &node : timestampPacketContainer->peekNodes()) {
            sizeForNodeDependency += TimestampPacketHelper::getRequiredCmdStreamSizeForNodeDependency<FamilyType>(*node);
        }
    }

    size_t extendedSize = sizeWithoutEvents + sizeForNodeDependency;

    EXPECT_EQ(sizeWithEvents, extendedSize);
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEnqueueingThenProgramSemaphoresOnCsrStream) {
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
    auto &cmdStream = device->getUltCommandStreamReceiver<FamilyType>().commandStream;

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    auto it = hwParser.cmdList.begin();
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), timestamp4.getNode(0), 0);
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), timestamp6.getNode(0), 0);
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), timestamp6.getNode(1), 0);

    while (it != hwParser.cmdList.end()) {
        EXPECT_EQ(nullptr, genCmdCast<MI_SEMAPHORE_WAIT *>(*it));
        it++;
    }
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEnqueueingThenTrackOwnershipUntilQueueIsCompleted) {
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);

    TimestampPacketContainer *timestampPacketContainer = cmdQ->timestampPacketContainer.get();
    TimestampPacketContainer *deferredTimestampPackets = cmdQ->deferredTimestampPackets.get();
    uint64_t latestNode = 0;

    {
        cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

        latestNode = timestampPacketContainer->peekNodes()[0]->getGpuAddress();
        EXPECT_EQ(0u, deferredTimestampPackets->peekNodes().size());
    }

    {
        cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

        EXPECT_EQ(1u, deferredTimestampPackets->peekNodes().size());
        EXPECT_EQ(latestNode, deferredTimestampPackets->peekNodes().at(0u)->getGpuAddress());
        latestNode = timestampPacketContainer->peekNodes()[0]->getGpuAddress();
    }

    {
        cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

        EXPECT_EQ(2u, deferredTimestampPackets->peekNodes().size());
        EXPECT_EQ(latestNode, deferredTimestampPackets->peekNodes().at(1u)->getGpuAddress());
        latestNode = timestampPacketContainer->peekNodes()[0]->getGpuAddress();
    }

    cmdQ->flush();
    EXPECT_EQ(2u, deferredTimestampPackets->peekNodes().size());
    cmdQ->finish();
    EXPECT_EQ(0u, deferredTimestampPackets->peekNodes().size());
}

HWTEST_F(TimestampPacketTests, givenTimestampWaitEnabledWhenEnqueueWithEventThenEventHasCorrectTimestampsToCheckForCompletion) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UpdateTaskCountFromWait.set(3);
    DebugManager.flags.EnableTimestampWait.set(1);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;
    csr.callBaseWaitForCompletionWithTimeout = false;
    *csr.getTagAddress() = 0u;
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);

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

    EXPECT_FALSE(event1.isCompleted());
    EXPECT_FALSE(event2.isCompleted());

    typename FamilyType::TimestampPacketType timestampData[] = {2, 2, 2, 2};
    for (uint32_t i = 0; i < deferredTimestampPackets->peekNodes()[0]->getPacketsUsed(); i++) {
        deferredTimestampPackets->peekNodes()[0]->assignDataToAllTimestamps(i, timestampData);
    }

    EXPECT_TRUE(event1.isCompleted());
    EXPECT_FALSE(event2.isCompleted());

    for (uint32_t i = 0; i < deferredTimestampPackets->peekNodes()[0]->getPacketsUsed(); i++) {
        timestampPacketContainer->peekNodes()[0]->assignDataToAllTimestamps(i, timestampData);
    }

    EXPECT_TRUE(event1.isCompleted());
    EXPECT_TRUE(event2.isCompleted());

    cmdQ->finish();

    EXPECT_TRUE(event1.isCompleted());
    EXPECT_TRUE(event2.isCompleted());
    EXPECT_EQ(csr.waitForCompletionWithTimeoutTaskCountCalled, 0u);

    clReleaseEvent(clEvent1);
    clReleaseEvent(clEvent2);
    *csr.getTagAddress() = csr.peekTaskCount();
}

HWTEST_F(TimestampPacketTests, givenEnableTimestampWaitWhenFinishWithoutEnqueueThenDoNotWaitOnTimestamp) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UpdateTaskCountFromWait.set(3);
    DebugManager.flags.EnableTimestampWait.set(1);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;
    csr.callBaseWaitForCompletionWithTimeout = false;
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);

    TimestampPacketContainer *deferredTimestampPackets = cmdQ->deferredTimestampPackets.get();
    TimestampPacketContainer *timestampPacketContainer = cmdQ->timestampPacketContainer.get();

    EXPECT_EQ(0u, deferredTimestampPackets->peekNodes().size());
    EXPECT_EQ(0u, timestampPacketContainer->peekNodes().size());

    cmdQ->finish();

    EXPECT_EQ(csr.waitForCompletionWithTimeoutTaskCountCalled, 1u);
}

HWTEST_F(TimestampPacketTests, givenEnableTimestampWaitWhenFinishThenWaitOnTimestamp) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UpdateTaskCountFromWait.set(3);
    DebugManager.flags.EnableTimestampWait.set(1);

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
    for (uint32_t i = 0; i < deferredTimestampPackets->peekNodes()[0]->getPacketsUsed(); i++) {
        timestampPacketContainer->peekNodes()[0]->assignDataToAllTimestamps(i, timestampData);
    }

    cmdQ->finish();

    EXPECT_EQ(csr.waitForCompletionWithTimeoutTaskCountCalled, 0u);
}

HWTEST_F(TimestampPacketTests, givenOOQAndEnableTimestampWaitWhenFinishThenWaitOnTimestamp) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UpdateTaskCountFromWait.set(3);
    DebugManager.flags.EnableTimestampWait.set(1);

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

    EXPECT_EQ(1u, deferredTimestampPackets->peekNodes().size());
    EXPECT_EQ(1u, timestampPacketContainer->peekNodes().size());

    typename FamilyType::TimestampPacketType timestampData[] = {2, 2, 2, 2};
    for (uint32_t i = 0; i < deferredTimestampPackets->peekNodes()[0]->getPacketsUsed(); i++) {
        deferredTimestampPackets->peekNodes()[0]->assignDataToAllTimestamps(i, timestampData);
        timestampPacketContainer->peekNodes()[0]->assignDataToAllTimestamps(i, timestampData);
    }

    cmdQ->finish();

    EXPECT_EQ(csr.waitForCompletionWithTimeoutTaskCountCalled, 0u);

    cmdQ.reset();
}

namespace CpuIntrinsicsTests {
extern std::atomic<uint32_t> pauseCounter;
extern volatile uint32_t *pauseAddress;
extern uint32_t pauseValue;
extern uint32_t pauseOffset;
extern std::function<void()> setupPauseAddress;
} // namespace CpuIntrinsicsTests

HWTEST_F(TimestampPacketTests, givenEnableTimestampWaitWhenFinishThenCallWaitUtils) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UpdateTaskCountFromWait.set(3);
    DebugManager.flags.EnableTimestampWait.set(1);

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), props);

    TimestampPacketContainer *deferredTimestampPackets = cmdQ->deferredTimestampPackets.get();
    TimestampPacketContainer *timestampPacketContainer = cmdQ->timestampPacketContainer.get();

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    cmdQ->flush();

    EXPECT_EQ(1u, deferredTimestampPackets->peekNodes().size());
    EXPECT_EQ(1u, timestampPacketContainer->peekNodes().size());

    VariableBackup<volatile uint32_t *> backupPauseAddress(&CpuIntrinsicsTests::pauseAddress);
    VariableBackup<uint32_t> backupPauseValue(&CpuIntrinsicsTests::pauseValue);
    VariableBackup<uint32_t> backupPauseOffset(&CpuIntrinsicsTests::pauseOffset);
    VariableBackup<std::function<void()>> backupSetupPauseAddress(&CpuIntrinsicsTests::setupPauseAddress);

    deferredTimestampPackets->peekNodes()[0]->setPacketsUsed(1u);
    timestampPacketContainer->peekNodes()[0]->setPacketsUsed(1u);

    CpuIntrinsicsTests::pauseAddress = reinterpret_cast<volatile uint32_t *>(const_cast<void *>(timestampPacketContainer->peekNodes()[0]->getContextEndAddress(0u)));
    CpuIntrinsicsTests::pauseValue = 2u;
    CpuIntrinsicsTests::setupPauseAddress = [&]() {
        CpuIntrinsicsTests::pauseAddress = reinterpret_cast<volatile uint32_t *>(const_cast<void *>(deferredTimestampPackets->peekNodes()[0]->getContextEndAddress(0u)));
    };
    CpuIntrinsicsTests::pauseCounter = 0u;
    EXPECT_FALSE(device->getUltCommandStreamReceiver<FamilyType>().downloadAllocationCalled);

    cmdQ->finish();

    EXPECT_EQ(2u, CpuIntrinsicsTests::pauseCounter);
    EXPECT_TRUE(device->getUltCommandStreamReceiver<FamilyType>().downloadAllocationCalled);

    cmdQ.reset();
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEnqueueingToOoqThenMoveToDeferredList) {
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);
    cmdQ->setOoqEnabled();

    TimestampPacketContainer *deferredTimestampPackets = cmdQ->deferredTimestampPackets.get();

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(0u, deferredTimestampPackets->peekNodes().size());

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(1u, deferredTimestampPackets->peekNodes().size());
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEnqueueingBlockingThenTrackOwnershipUntilQueueIsCompleted) {
    DebugManager.flags.MakeEachEnqueueBlocking.set(true);

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
    eventsRequest.fillCsrDependenciesForTimestampPacketContainer(csrDependencies, csr1, CsrDependencies::DependenciesType::All);
    EXPECT_EQ(static_cast<size_t>(eventsOnWaitlist), csrDependencies.timestampPacketContainer.size());
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledOnDifferentCSRsFromOneDeviceWhenEnqueueingThenProgramSemaphoresOnCsrStream) {
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
    auto &cmdStream = device->getUltCommandStreamReceiver<FamilyType>().commandStream;

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    auto it = hwParser.cmdList.begin();
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), timestamp4.getNode(0), 0);
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), timestamp6.getNode(0), 0);
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), timestamp6.getNode(1), 0);

    while (it != hwParser.cmdList.end()) {
        EXPECT_EQ(nullptr, genCmdCast<MI_SEMAPHORE_WAIT *>(*it));
        it++;
    }
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEnqueueingBlockedThenProgramSemaphoresOnCsrStreamOnFlush) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto device2 = std::make_unique<MockClDevice>(Device::create<MockDevice>(executionEnvironment, 0u));

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
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
    auto &cmdStream = device->getUltCommandStreamReceiver<FamilyType>().commandStream;
    EXPECT_EQ(0u, cmdStream.getUsed());
    userEvent.setStatus(CL_COMPLETE);
    cmdQ1->isQueueBlocked();
    cmdQ2->isQueueBlocked();

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    auto it = hwParser.cmdList.begin();
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), timestamp1.getNode(0), 0);

    while (it != hwParser.cmdList.end()) {
        EXPECT_EQ(nullptr, genCmdCast<MI_SEMAPHORE_WAIT *>(*it));
        it++;
    }

    cmdQ2->release();
    context2->release();
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledOnDifferentCSRsFromOneDeviceWhenEnqueueingBlockedThenProgramSemaphoresOnCsrStreamOnFlush) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto device2 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 1u));

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

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
    auto &cmdStream = device->getUltCommandStreamReceiver<FamilyType>().commandStream;
    EXPECT_EQ(0u, cmdStream.getUsed());
    userEvent.setStatus(CL_COMPLETE);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    auto it = hwParser.cmdList.begin();
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), timestamp1.getNode(0), 0);

    while (it != hwParser.cmdList.end()) {
        EXPECT_EQ(nullptr, genCmdCast<MI_SEMAPHORE_WAIT *>(*it));
        it++;
    }

    cmdQ2->isQueueBlocked();
    cmdQ1->isQueueBlocked();
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenDispatchingThenProgramSemaphoresForWaitlist) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using WALKER = typename FamilyType::WALKER_TYPE;

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
    eventsRequest.fillCsrDependenciesForTimestampPacketContainer(csrDeps, mockCmdQ->getGpgpuCommandStreamReceiver(), CsrDependencies::DependenciesType::OnCsr);

    HardwareInterface<FamilyType>::dispatchWalker(
        *mockCmdQ,
        multiDispatchInfo,
        csrDeps,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &timestamp7,
        CL_COMMAND_NDRANGE_KERNEL);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    uint32_t semaphoresFound = 0;
    uint32_t walkersFound = 0;

    for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*it);
        if (semaphoreCmd) {
            semaphoresFound++;
            if (semaphoresFound == 1) {
                verifySemaphore(semaphoreCmd, timestamp3.getNode(0), 0);
            } else if (semaphoresFound == 2) {
                verifySemaphore(semaphoreCmd, timestamp5.getNode(0), 0);
            } else if (semaphoresFound == 3) {
                verifySemaphore(semaphoreCmd, timestamp5.getNode(1), 0);
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
    using WALKER = typename FamilyType::WALKER_TYPE;

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
    eventsRequest.fillCsrDependenciesForTimestampPacketContainer(csrDeps, mockCmdQ->getGpgpuCommandStreamReceiver(), CsrDependencies::DependenciesType::OnCsr);

    HardwareInterface<FamilyType>::dispatchWalker(
        *mockCmdQ,
        multiDispatchInfo,
        csrDeps,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &timestamp7,
        CL_COMMAND_NDRANGE_KERNEL);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    uint32_t semaphoresFound = 0;
    uint32_t walkersFound = 0;

    for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*it);
        if (semaphoreCmd) {
            semaphoresFound++;
            if (semaphoresFound == 1) {
                verifySemaphore(semaphoreCmd, timestamp3.getNode(0), 0);
            } else if (semaphoresFound == 2) {
                verifySemaphore(semaphoreCmd, timestamp5.getNode(0), 0);
            } else if (semaphoresFound == 3) {
                verifySemaphore(semaphoreCmd, timestamp5.getNode(1), 0);
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
    DebugManager.flags.ResolveDependenciesViaPipeControls.set(1);

    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using WALKER = typename FamilyType::WALKER_TYPE;

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    MockMultiDispatchInfo multiDispatchInfo(device.get(), std::vector<Kernel *>({kernel->mockKernel}));

    auto &cmdStream = mockCmdQ->getCS(0);
    mockCmdQ->updateLatestSentEnqueueType(NEO::EnqueueProperties::Operation::Blit);
    const cl_uint eventsOnWaitlist = 1;
    MockTimestampPacketContainer timestamp(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);

    Event event(mockCmdQ, 0, 0, 0);
    event.addTimestampPacketNodes(timestamp);

    cl_event waitlist[] = {&event};

    EventsRequest eventsRequest(eventsOnWaitlist, waitlist, nullptr);
    CsrDependencies csrDeps;
    eventsRequest.fillCsrDependenciesForTimestampPacketContainer(csrDeps, mockCmdQ->getGpgpuCommandStreamReceiver(), CsrDependencies::DependenciesType::OnCsr);

    HardwareInterface<FamilyType>::dispatchWalker(
        *mockCmdQ,
        multiDispatchInfo,
        csrDeps,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        CL_COMMAND_NDRANGE_KERNEL);

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

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledAndDependenciesResolvedViaPipeControlsIfPreviousOperationIsGPUKernelThenDoNotProgramSemaphores) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ResolveDependenciesViaPipeControls.set(1);

    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using WALKER = typename FamilyType::WALKER_TYPE;

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    MockMultiDispatchInfo multiDispatchInfo(device.get(), std::vector<Kernel *>({kernel->mockKernel}));

    auto &cmdStream = mockCmdQ->getCS(0);
    mockCmdQ->updateLatestSentEnqueueType(NEO::EnqueueProperties::Operation::GpuKernel);
    const cl_uint eventsOnWaitlist = 1;
    MockTimestampPacketContainer timestamp(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);

    Event event(mockCmdQ, 0, 0, 0);
    event.addTimestampPacketNodes(timestamp);

    cl_event waitlist[] = {&event};

    EventsRequest eventsRequest(eventsOnWaitlist, waitlist, nullptr);
    CsrDependencies csrDeps;
    eventsRequest.fillCsrDependenciesForTimestampPacketContainer(csrDeps, mockCmdQ->getGpgpuCommandStreamReceiver(), CsrDependencies::DependenciesType::OnCsr);

    HardwareInterface<FamilyType>::dispatchWalker(
        *mockCmdQ,
        multiDispatchInfo,
        csrDeps,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        CL_COMMAND_NDRANGE_KERNEL);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    uint32_t semaphoresFound = 0;

    for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*it);
        if (semaphoreCmd) {
            semaphoresFound++;
        }
    }
    EXPECT_EQ(0u, semaphoresFound);
}

HWTEST_F(TimestampPacketTests, givenAlreadyAssignedNodeWhenEnqueueingNonBlockedThenMakeItResident) {
    auto mockTagAllocator = new MockTagAllocator<>(device->getRootDeviceIndex(), executionEnvironment->memoryManager.get(), 1);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketAllocator.reset(mockTagAllocator);
    csr.timestampPacketWriteEnabled = true;

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);
    TimestampPacketContainer previousNodes;
    cmdQ->obtainNewTimestampPacketNodes(1, previousNodes, false, cmdQ->getGpgpuCommandStreamReceiver());
    auto firstNode = cmdQ->timestampPacketContainer->peekNodes().at(0);

    csr.storeMakeResidentAllocations = true;
    csr.timestampPacketWriteEnabled = true;

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    auto secondNode = cmdQ->timestampPacketContainer->peekNodes().at(0);

    EXPECT_NE(firstNode->getBaseGraphicsAllocation(), secondNode->getBaseGraphicsAllocation());
    EXPECT_TRUE(csr.isMadeResident(firstNode->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation(), csr.taskCount));
}

HWTEST_F(TimestampPacketTests, givenAlreadyAssignedNodeWhenEnqueueingBlockedThenMakeItResident) {
    auto mockTagAllocator = new MockTagAllocator<>(device->getRootDeviceIndex(), executionEnvironment->memoryManager.get(), 1);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketAllocator.reset(mockTagAllocator);
    csr.timestampPacketWriteEnabled = true;

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));
    TimestampPacketContainer previousNodes;
    cmdQ->obtainNewTimestampPacketNodes(1, previousNodes, false, cmdQ->getGpgpuCommandStreamReceiver());
    auto firstNode = cmdQ->timestampPacketContainer->peekNodes().at(0);

    csr.storeMakeResidentAllocations = true;
    csr.timestampPacketWriteEnabled = true;

    UserEvent userEvent;
    cl_event clEvent = &userEvent;
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 1, &clEvent, nullptr);
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
    firstNode.add(cmdQ.timestampPacketContainer->peekNodes().at(0));
    firstNode.add(cmdQ.timestampPacketContainer->peekNodes().at(1));
    auto firstTag0 = firstNode.getNode(0);
    auto firstTag1 = firstNode.getNode(1);

    cmdQ.enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ.commandStream, 0);

    auto it = hwParser.cmdList.begin();
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it), firstTag0, 0);

    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*++it), firstTag1, 0);
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
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    cl_queue_properties properties[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    MockCommandQueueHw<FamilyType> cmdQ(context, device.get(), properties);
    TimestampPacketContainer previousNodes;
    cmdQ.obtainNewTimestampPacketNodes(1, previousNodes, false, cmdQ.getGpgpuCommandStreamReceiver());

    cmdQ.enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ.commandStream, 0);

    uint32_t semaphoresFound = 0;
    for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
        if (genCmdCast<typename FamilyType::MI_SEMAPHORE_WAIT *>(*it)) {
            semaphoresFound++;
        }
    }
    uint32_t expectedSemaphoresCount = (UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(device->getHardwareInfo()) ? 1 : 0);
    EXPECT_EQ(expectedSemaphoresCount, semaphoresFound);
}

HWTEST_F(TimestampPacketTests, givenAlreadyAssignedNodeWhenEnqueueingWithOmitTimestampPacketDependenciesThenDontKeepDependencyOnPreviousNodeIfItsNotReady) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    DebugManagerStateRestore restore;
    DebugManager.flags.OmitTimestampPacketDependencies.set(true);

    MockCommandQueueHw<FamilyType> cmdQ(context, device.get(), nullptr);
    TimestampPacketContainer previousNodes;
    cmdQ.obtainNewTimestampPacketNodes(1, previousNodes, false, cmdQ.getGpgpuCommandStreamReceiver());

    cmdQ.enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ.commandStream, 0);

    uint32_t semaphoresFound = 0;
    for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
        if (genCmdCast<typename FamilyType::MI_SEMAPHORE_WAIT *>(*it)) {
            semaphoresFound++;
        }
    }
    uint32_t expectedSemaphoresCount = (UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(device->getHardwareInfo()) ? 1 : 0);
    EXPECT_EQ(expectedSemaphoresCount, semaphoresFound);
}

HWTEST_F(TimestampPacketTests, givenEventsWaitlistFromDifferentDevicesWhenEnqueueingThenMakeAllTimestampsResident) {
    MockTagAllocator<TimestampPackets<uint32_t>> tagAllocator(device->getRootDeviceIndex(), executionEnvironment->memoryManager.get(), 1, 1,
                                                              sizeof(TimestampPackets<uint32_t>), false, device->getDeviceBitfield());
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

HWTEST_F(TimestampPacketTests, givenEventsWaitlistFromDifferentCSRsWhenEnqueueingThenMakeAllTimestampsResident) {
    MockTagAllocator<TimestampPackets<uint32_t>> tagAllocator(device->getRootDeviceIndex(), executionEnvironment->memoryManager.get(), 1, 1,
                                                              sizeof(TimestampPackets<uint32_t>), false, device->getDeviceBitfield());

    auto &ultCsr = device->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.timestampPacketWriteEnabled = true;
    ultCsr.storeMakeResidentAllocations = true;

    auto cmdQ1 = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);

    // Create second (LOW_PRIORITY) queue on the same device
    cl_queue_properties props[] = {CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_LOW_KHR, 0};
    auto cmdQ2 = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), props);
    cmdQ2->getUltCommandStreamReceiver().timestampPacketWriteEnabled = true;

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

HWTEST_F(TimestampPacketTests, givenTimestampPacketWhenEnqueueingNonBlockedThenMakeItResident) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;
    csr.storeMakeResidentAllocations = true;

    MockKernelWithInternals mockKernel(*device, context);
    MockCommandQueueHw<FamilyType> cmdQ(context, device.get(), nullptr);

    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    auto timestampPacketNode = cmdQ.timestampPacketContainer->peekNodes().at(0);

    EXPECT_TRUE(csr.isMadeResident(timestampPacketNode->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation(), csr.taskCount));
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWhenEnqueueingBlockedThenMakeItResidentOnSubmit) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;

    MockKernelWithInternals mockKernel(*device, context);

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));

    csr.storeMakeResidentAllocations = true;

    UserEvent userEvent;
    cl_event clEvent = &userEvent;

    cmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 1, &clEvent, nullptr);
    auto timestampPacketNode = cmdQ->timestampPacketContainer->peekNodes().at(0);

    EXPECT_FALSE(csr.isMadeResident(timestampPacketNode->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation(), csr.taskCount));
    userEvent.setStatus(CL_COMPLETE);
    EXPECT_TRUE(csr.isMadeResident(timestampPacketNode->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation(), csr.taskCount));
    cmdQ->isQueueBlocked();
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEnqueueingBlockedThenVirtualEventIncrementsRefInternalAndDecrementsAfterCompleteEvent) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;
    MockKernelWithInternals mockKernelWithInternals(*device, context);
    auto mockKernel = mockKernelWithInternals.mockKernel;
    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));

    UserEvent userEvent;
    cl_event waitlist = &userEvent;

    auto internalCount = userEvent.getRefInternalCount();
    cmdQ->enqueueKernel(mockKernel, 1, nullptr, gws, nullptr, 1, &waitlist, nullptr);
    EXPECT_EQ(internalCount + 1, userEvent.getRefInternalCount());
    userEvent.setStatus(CL_COMPLETE);
    cmdQ->isQueueBlocked();
    EXPECT_EQ(internalCount, mockKernel->getRefInternalCount());
}

TEST_F(TimestampPacketTests, givenDispatchSizeWhenAskingForNewTimestampsThenObtainEnoughTags) {
    size_t dispatchSize = 3;

    mockCmdQ->timestampPacketContainer = std::make_unique<MockTimestampPacketContainer>(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 0);
    EXPECT_EQ(0u, mockCmdQ->timestampPacketContainer->peekNodes().size());

    TimestampPacketContainer previousNodes;
    mockCmdQ->obtainNewTimestampPacketNodes(dispatchSize, previousNodes, false, mockCmdQ->getGpgpuCommandStreamReceiver());
    EXPECT_EQ(dispatchSize, mockCmdQ->timestampPacketContainer->peekNodes().size());
}

HWTEST_F(TimestampPacketTests, givenWaitlistAndOutputEventWhenEnqueueingWithoutKernelThenInheritTimestampPacketsWithoutSubmitting) {
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));

    MockKernelWithInternals mockKernel(*device, context);
    cmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr); // obtain first TimestampPackets<uint32_t>

    TimestampPacketContainer cmdQNodes;
    cmdQNodes.assignAndIncrementNodesRefCounts(*cmdQ->timestampPacketContainer);

    MockTimestampPacketContainer node1(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer node2(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);

    Event event0(cmdQ.get(), 0, 0, 0);
    event0.addTimestampPacketNodes(node1);
    Event event1(cmdQ.get(), 0, 0, 0);
    event1.addTimestampPacketNodes(node2);
    UserEvent userEvent;
    Event eventWithoutContainer(nullptr, 0, 0, 0);

    uint32_t numEventsWithContainer = 2;
    uint32_t numEventsOnWaitlist = numEventsWithContainer + 2; // UserEvent + eventWithoutContainer

    cl_event waitlist[] = {&event0, &event1, &userEvent, &eventWithoutContainer};

    cl_event clOutEvent;
    cmdQ->enqueueMarkerWithWaitList(numEventsOnWaitlist, waitlist, &clOutEvent);

    auto outEvent = castToObject<Event>(clOutEvent);

    EXPECT_EQ(cmdQ->timestampPacketContainer->peekNodes().at(0), cmdQNodes.peekNodes().at(0)); // no new nodes obtained
    EXPECT_EQ(1u, cmdQ->timestampPacketContainer->peekNodes().size());

    auto &eventsNodes = outEvent->getTimestampPacketNodes()->peekNodes();
    EXPECT_EQ(numEventsWithContainer + 1, eventsNodes.size()); // numEventsWithContainer + command queue
    EXPECT_EQ(cmdQNodes.peekNodes().at(0), eventsNodes.at(0));
    EXPECT_EQ(event0.getTimestampPacketNodes()->peekNodes().at(0), eventsNodes.at(1));
    EXPECT_EQ(event1.getTimestampPacketNodes()->peekNodes().at(0), eventsNodes.at(2));

    clReleaseEvent(clOutEvent);
    userEvent.setStatus(CL_COMPLETE);
    cmdQ->isQueueBlocked();
}

HWTEST_F(TimestampPacketTests, givenBlockedEnqueueWithoutKernelWhenSubmittingThenDispatchBlockedCommands) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto mockCsr = new MockCsrHw2<FamilyType>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    device->resetCommandStreamReceiver(mockCsr);
    mockCsr->timestampPacketWriteEnabled = true;
    mockCsr->storeFlushedTaskStream = true;

    auto cmdQ0 = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));

    auto &secondEngine = device->getEngine(getChosenEngineType(device->getHardwareInfo()), EngineUsage::LowPriority);
    static_cast<UltCommandStreamReceiver<FamilyType> *>(secondEngine.commandStreamReceiver)->timestampPacketWriteEnabled = true;

    auto cmdQ1 = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));
    cmdQ1->gpgpuEngine = &secondEngine;
    cmdQ1->timestampPacketContainer = std::make_unique<TimestampPacketContainer>();
    EXPECT_NE(&cmdQ0->getGpgpuCommandStreamReceiver(), &cmdQ1->getGpgpuCommandStreamReceiver());

    MockTimestampPacketContainer node0(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer node1(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);

    Event event0(cmdQ0.get(), 0, 0, 0); // on the same CSR
    event0.addTimestampPacketNodes(node0);
    Event event1(cmdQ1.get(), 0, 0, 0); // on different CSR
    event1.addTimestampPacketNodes(node1);

    uint32_t numEventsOnWaitlist = 3;

    uint32_t commands[] = {CL_COMMAND_MARKER, CL_COMMAND_BARRIER};
    for (int i = 0; i < 2; i++) {
        UserEvent userEvent;
        cl_event waitlist[] = {&event0, &event1, &userEvent};
        if (commands[i] == CL_COMMAND_MARKER) {
            cmdQ0->enqueueMarkerWithWaitList(numEventsOnWaitlist, waitlist, nullptr);
        } else if (commands[i] == CL_COMMAND_BARRIER) {
            cmdQ0->enqueueBarrierWithWaitList(numEventsOnWaitlist, waitlist, nullptr);
        } else {
            EXPECT_TRUE(false);
        }

        auto initialCsrStreamOffset = mockCsr->commandStream.getUsed();
        userEvent.setStatus(CL_COMPLETE);

        HardwareParse hwParserCsr;
        HardwareParse hwParserCmdQ;
        LinearStream taskStream(mockCsr->storedTaskStream.get(), mockCsr->storedTaskStreamSize);
        taskStream.getSpace(mockCsr->storedTaskStreamSize);
        hwParserCsr.parseCommands<FamilyType>(mockCsr->commandStream, initialCsrStreamOffset);
        hwParserCmdQ.parseCommands<FamilyType>(taskStream, 0);

        auto queueSemaphores = findAll<MI_SEMAPHORE_WAIT *>(hwParserCmdQ.cmdList.begin(), hwParserCmdQ.cmdList.end());
        auto expectedQueueSemaphoresCount = 1u;
        if (UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(device->getHardwareInfo())) {
            expectedQueueSemaphoresCount += 1;
        }
        EXPECT_EQ(expectedQueueSemaphoresCount, queueSemaphores.size());
        verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*(queueSemaphores[0])), node0.getNode(0), 0);

        auto csrSemaphores = findAll<MI_SEMAPHORE_WAIT *>(hwParserCsr.cmdList.begin(), hwParserCsr.cmdList.end());
        EXPECT_EQ(1u, csrSemaphores.size());
        verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*(csrSemaphores[0])), node1.getNode(0), 0);

        EXPECT_TRUE(mockCsr->passedDispatchFlags.blocking);
        EXPECT_TRUE(mockCsr->passedDispatchFlags.guardCommandBufferWithPipeControl);
        EXPECT_EQ(device->getPreemptionMode(), mockCsr->passedDispatchFlags.preemptionMode);

        cmdQ0->isQueueBlocked();
    }
}

HWTEST_F(TimestampPacketTests, givenWaitlistAndOutputEventWhenEnqueueingMarkerWithoutKernelThenInheritTimestampPacketsAndProgramSemaphores) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto device2 = std::make_unique<MockClDevice>(Device::create<MockDevice>(executionEnvironment, 0u));

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    device2->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    MockContext context2(device2.get());

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));
    auto cmdQ2 = std::make_unique<MockCommandQueueHw<FamilyType>>(&context2, device2.get(), nullptr);

    MockTimestampPacketContainer node1(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer node2(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);

    Event event0(cmdQ.get(), 0, 0, 0);
    event0.addTimestampPacketNodes(node1);
    Event event1(cmdQ2.get(), 0, 0, 0);
    event1.addTimestampPacketNodes(node2);

    uint32_t numEventsOnWaitlist = 2;

    cl_event waitlist[] = {&event0, &event1};

    cmdQ->enqueueMarkerWithWaitList(numEventsOnWaitlist, waitlist, nullptr);

    HardwareParse hwParserCsr;
    HardwareParse hwParserCmdQ;
    hwParserCsr.parseCommands<FamilyType>(device->getUltCommandStreamReceiver<FamilyType>().commandStream, 0);
    hwParserCmdQ.parseCommands<FamilyType>(*cmdQ->commandStream, 0);

    auto csrSemaphores = findAll<MI_SEMAPHORE_WAIT *>(hwParserCsr.cmdList.begin(), hwParserCsr.cmdList.end());
    EXPECT_EQ(1u, csrSemaphores.size());
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*(csrSemaphores[0])), node2.getNode(0), 0);

    auto queueSemaphores = findAll<MI_SEMAPHORE_WAIT *>(hwParserCmdQ.cmdList.begin(), hwParserCmdQ.cmdList.end());
    auto expectedQueueSemaphoresCount = 1u;
    if (UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(device->getHardwareInfo())) {
        expectedQueueSemaphoresCount += 1;
    }
    EXPECT_EQ(expectedQueueSemaphoresCount, queueSemaphores.size());
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*(queueSemaphores[0])), node1.getNode(0), 0);
}

HWTEST_F(TimestampPacketTests, givenWaitlistAndOutputEventWhenEnqueueingBarrierWithoutKernelThenInheritTimestampPacketsAndProgramSemaphores) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto device2 = std::make_unique<MockClDevice>(Device::create<MockDevice>(executionEnvironment, 0u));

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    device2->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    MockContext context2(device2.get());

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));
    auto cmdQ2 = std::make_unique<MockCommandQueueHw<FamilyType>>(&context2, device2.get(), nullptr);

    MockTimestampPacketContainer node1(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer node2(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);

    Event event0(cmdQ.get(), 0, 0, 0);
    event0.addTimestampPacketNodes(node1);
    Event event1(cmdQ2.get(), 0, 0, 0);
    event1.addTimestampPacketNodes(node2);

    uint32_t numEventsOnWaitlist = 2;

    cl_event waitlist[] = {&event0, &event1};

    cmdQ->enqueueBarrierWithWaitList(numEventsOnWaitlist, waitlist, nullptr);

    HardwareParse hwParserCsr;
    HardwareParse hwParserCmdQ;
    hwParserCsr.parseCommands<FamilyType>(device->getUltCommandStreamReceiver<FamilyType>().commandStream, 0);
    hwParserCmdQ.parseCommands<FamilyType>(*cmdQ->commandStream, 0);

    auto csrSemaphores = findAll<MI_SEMAPHORE_WAIT *>(hwParserCsr.cmdList.begin(), hwParserCsr.cmdList.end());
    EXPECT_EQ(1u, csrSemaphores.size());
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*(csrSemaphores[0])), node2.getNode(0), 0);

    auto queueSemaphores = findAll<MI_SEMAPHORE_WAIT *>(hwParserCmdQ.cmdList.begin(), hwParserCmdQ.cmdList.end());
    auto expectedQueueSemaphoresCount = 1u;
    if (UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(device->getHardwareInfo())) {
        expectedQueueSemaphoresCount += 1;
    }
    EXPECT_EQ(expectedQueueSemaphoresCount, queueSemaphores.size());
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*(queueSemaphores[0])), node1.getNode(0), 0);
}
