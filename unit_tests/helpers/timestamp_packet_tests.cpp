/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/timestamp_packet.h"
#include "runtime/utilities/tag_allocator.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_mdi.h"
#include "unit_tests/mocks/mock_memory_manager.h"

#include "test.h"

using namespace OCLRT;

struct TimestampPacketSimpleTests : public ::testing::Test {
    class MockTimestampPacket : public TimestampPacket {
      public:
        using TimestampPacket::data;
    };

    template <typename TagType = TimestampPacket>
    class MockTagAllocator : public TagAllocator<TagType> {
      public:
        using BaseClass = TagAllocator<TagType>;
        using BaseClass::freeTags;
        using BaseClass::usedTags;
        using NodeType = typename BaseClass::NodeType;

        MockTagAllocator(MemoryManager *memoryManager, size_t tagCount = 10) : BaseClass(memoryManager, tagCount, 10) {}

        void returnTag(NodeType *node) override {
            releaseReferenceNodes.push_back(node);
            BaseClass::returnTag(node);
        }

        void returnTagToFreePool(NodeType *node) override {
            returnedToFreePoolNodes.push_back(node);
            BaseClass::returnTagToFreePool(node);
        }

        std::vector<NodeType *> releaseReferenceNodes;
        std::vector<NodeType *> returnedToFreePoolNodes;
    };

    void setTagToReadyState(TimestampPacket *tag) {
        memset(reinterpret_cast<void *>(tag->pickAddressForDataWrite(TimestampPacket::DataIndex::ContextStart)), 0, timestampDataSize);
    }

    const size_t timestampDataSize = sizeof(uint32_t) * static_cast<size_t>(TimestampPacket::DataIndex::Max);
    const size_t gws[3] = {1, 1, 1};
};

struct TimestampPacketTests : public TimestampPacketSimpleTests {
    void SetUp() override {
        executionEnvironment.incRefInternal();
        device = std::unique_ptr<MockDevice>(Device::create<MockDevice>(nullptr, &executionEnvironment, 0u));
        context = std::make_unique<MockContext>(device.get());
        kernel = std::make_unique<MockKernelWithInternals>(*device, context.get());
        mockCmdQ = std::make_unique<MockCommandQueue>(context.get(), device.get(), nullptr);
    }

    template <typename MI_SEMAPHORE_WAIT>
    void verifySemaphore(MI_SEMAPHORE_WAIT *semaphoreCmd, Event *compareEvent) {
        EXPECT_NE(nullptr, semaphoreCmd);
        EXPECT_EQ(semaphoreCmd->getCompareOperation(), MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
        EXPECT_EQ(1u, semaphoreCmd->getSemaphoreDataDword());
        EXPECT_EQ(compareEvent->getTimestampPacketNode()->tag->pickAddressForDataWrite(TimestampPacket::DataIndex::ContextEnd),
                  semaphoreCmd->getSemaphoreGraphicsAddress());
    };

    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<MockDevice> device;
    std::unique_ptr<MockContext> context;
    std::unique_ptr<MockKernelWithInternals> kernel;
    std::unique_ptr<MockCommandQueue> mockCmdQ;
};

TEST_F(TimestampPacketSimpleTests, whenEndTagIsNotOneThenCanBeReleased) {
    MockTimestampPacket timestampPacket;
    auto contextEndIndex = static_cast<uint32_t>(TimestampPacket::DataIndex::ContextEnd);
    auto globalEndIndex = static_cast<uint32_t>(TimestampPacket::DataIndex::GlobalEnd);

    timestampPacket.data[contextEndIndex] = 1;
    timestampPacket.data[globalEndIndex] = 1;
    EXPECT_FALSE(timestampPacket.canBeReleased());

    timestampPacket.data[contextEndIndex] = 1;
    timestampPacket.data[globalEndIndex] = 0;
    EXPECT_FALSE(timestampPacket.canBeReleased());

    timestampPacket.data[contextEndIndex] = 0;
    timestampPacket.data[globalEndIndex] = 1;
    EXPECT_FALSE(timestampPacket.canBeReleased());

    timestampPacket.data[contextEndIndex] = 0;
    timestampPacket.data[globalEndIndex] = 0;
    EXPECT_TRUE(timestampPacket.canBeReleased());
}

TEST_F(TimestampPacketSimpleTests, whenNewTagIsTakenThenReinitialize) {
    MockMemoryManager memoryManager;
    MockTagAllocator<MockTimestampPacket> allocator(&memoryManager, 1);

    auto firstNode = allocator.getTag();
    firstNode->tag->data = {{5, 6, 7, 8, 9}};

    allocator.returnTag(firstNode);

    auto secondNode = allocator.getTag();
    EXPECT_EQ(secondNode, firstNode);

    for (uint32_t i = 0; i < static_cast<uint32_t>(TimestampPacket::DataIndex::Max); i++) {
        EXPECT_EQ(1u, secondNode->tag->data[i]);
    }
}

TEST_F(TimestampPacketSimpleTests, whenObjectIsCreatedThenInitializeAllStamps) {
    MockTimestampPacket timestampPacket;
    auto maxElements = static_cast<uint32_t>(TimestampPacket::DataIndex::Max);
    EXPECT_EQ(5u, maxElements);

    EXPECT_EQ(maxElements, timestampPacket.data.size());

    for (uint32_t i = 0; i < maxElements; i++) {
        EXPECT_EQ(1u, timestampPacket.data[i]);
    }
}

TEST_F(TimestampPacketSimpleTests, whenAskedForStampAddressThenReturnWithValidOffset) {
    MockTimestampPacket timestampPacket;

    for (size_t i = 0; i < static_cast<uint32_t>(TimestampPacket::DataIndex::Max); i++) {
        auto address = timestampPacket.pickAddressForDataWrite(static_cast<TimestampPacket::DataIndex>(i));
        EXPECT_EQ(address, reinterpret_cast<uint64_t>(&timestampPacket.data[i]));
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEstimatingStreamSizeThenAddTwoPipeControls) {
    MockKernelWithInternals kernel2(*device);
    MockMultiDispatchInfo multiDispatchInfo(std::vector<Kernel *>({kernel->mockKernel, kernel2.mockKernel}));

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = false;
    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*mockCmdQ, 0, false, false, multiDispatchInfo);
    auto sizeWithDisabled = mockCmdQ->requestedCmdStreamSize;

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*mockCmdQ, 0, false, false, multiDispatchInfo);
    auto sizeWithEnabled = mockCmdQ->requestedCmdStreamSize;

    auto extendedSize = sizeWithDisabled + (2 * sizeof(typename FamilyType::PIPE_CONTROL)) + sizeof(typename FamilyType::MI_SEMAPHORE_WAIT);

    EXPECT_EQ(sizeWithEnabled, extendedSize);
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEstimatingStreamSizeWithWaitlistThenAddSizeForSemaphores) {
    MockKernelWithInternals kernel2(*device);
    MockMultiDispatchInfo multiDispatchInfo(std::vector<Kernel *>({kernel->mockKernel, kernel2.mockKernel}));

    cl_uint numEventsOnWaitlist = 5;

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = false;
    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*mockCmdQ, numEventsOnWaitlist, false, false, multiDispatchInfo);
    auto sizeWithDisabled = mockCmdQ->requestedCmdStreamSize;

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*mockCmdQ, numEventsOnWaitlist, false, false, multiDispatchInfo);
    auto sizeWithEnabled = mockCmdQ->requestedCmdStreamSize;

    size_t extendedSize = sizeWithDisabled + EnqueueOperation<FamilyType>::getSizeRequiredForTimestampPacketWrite() +
                          ((numEventsOnWaitlist + 1) * sizeof(typename FamilyType::MI_SEMAPHORE_WAIT));

    EXPECT_EQ(sizeWithEnabled, extendedSize);
}

HWCMDTEST_F(IGFX_GEN8_CORE, TimestampPacketTests, givenTimestampPacketWhenDispatchingGpuWalkerThenAddTwoPcForLastWalker) {
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    MockTimestampPacket timestampPacket;

    MockKernelWithInternals kernel2(*device);

    MockMultiDispatchInfo multiDispatchInfo(std::vector<Kernel *>({kernel->mockKernel, kernel2.mockKernel}));

    auto &cmdStream = mockCmdQ->getCS(0);

    GpgpuWalkerHelper<FamilyType>::dispatchWalker(
        *mockCmdQ,
        multiDispatchInfo,
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &timestampPacket,
        device->getPreemptionMode(),
        false);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    auto verifyPipeControl = [](PIPE_CONTROL *pipeControl, uint64_t expectedAddress) {
        EXPECT_EQ(1u, pipeControl->getCommandStreamerStallEnable());
        EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, pipeControl->getPostSyncOperation());
        EXPECT_EQ(0u, pipeControl->getImmediateData());
        EXPECT_EQ(static_cast<uint32_t>(expectedAddress & 0x0000FFFFFFFFULL), pipeControl->getAddress());
        EXPECT_EQ(static_cast<uint32_t>(expectedAddress >> 32), pipeControl->getAddressHigh());
    };

    uint32_t walkersFound = 0;
    for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
        if (genCmdCast<GPGPU_WALKER *>(*it)) {
            walkersFound++;
            if (walkersFound == 1) {
                EXPECT_EQ(nullptr, genCmdCast<PIPE_CONTROL *>(*--it));
                it++;
                EXPECT_EQ(nullptr, genCmdCast<PIPE_CONTROL *>(*++it));
                it--;
            } else if (walkersFound == 2) {
                auto pipeControl = genCmdCast<PIPE_CONTROL *>(*--it);
                EXPECT_NE(nullptr, pipeControl);
                verifyPipeControl(pipeControl, timestampPacket.pickAddressForDataWrite(TimestampPacket::DataIndex::Submit));
                it++;
                pipeControl = genCmdCast<PIPE_CONTROL *>(*++it);
                EXPECT_NE(nullptr, pipeControl);
                verifyPipeControl(pipeControl, timestampPacket.pickAddressForDataWrite(TimestampPacket::DataIndex::ContextEnd));
                it--;
            }
        }
    }
    EXPECT_EQ(2u, walkersFound);
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEnqueueingThenObtainNewStampAndPassToEvent) {
    auto mockMemoryManager = new MockMemoryManager();
    device->injectMemoryManager(mockMemoryManager);
    context->setMemoryManager(mockMemoryManager);
    auto mockTagAllocator = new MockTagAllocator<>(mockMemoryManager);
    mockMemoryManager->timestampPacketAllocator.reset(mockTagAllocator);
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = false;

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(nullptr, cmdQ->timestampPacketNode);
    EXPECT_EQ(nullptr, mockTagAllocator->usedTags.peekHead());

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    cl_event event1, event2;

    // obtain first node for cmdQ and event1
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event1);
    auto node1 = cmdQ->timestampPacketNode;
    EXPECT_NE(nullptr, node1);
    EXPECT_EQ(node1, cmdQ->timestampPacketNode);

    // obtain new node for cmdQ and event2
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event2);
    auto node2 = cmdQ->timestampPacketNode;
    EXPECT_NE(nullptr, node2);
    EXPECT_EQ(node2, cmdQ->timestampPacketNode);
    EXPECT_EQ(0u, mockTagAllocator->returnedToFreePoolNodes.size()); // nothing returned. event1 owns previous node
    EXPECT_EQ(1u, mockTagAllocator->releaseReferenceNodes.size());   // cmdQ released first node
    EXPECT_EQ(node1, mockTagAllocator->releaseReferenceNodes.at(0));

    EXPECT_NE(node1, node2);
    setTagToReadyState(node1->tag);
    setTagToReadyState(node2->tag);

    clReleaseEvent(event2);
    EXPECT_EQ(0u, mockTagAllocator->returnedToFreePoolNodes.size()); // nothing returned. cmdQ owns node2
    EXPECT_EQ(2u, mockTagAllocator->releaseReferenceNodes.size());   // event2 released  node2
    EXPECT_EQ(node2, mockTagAllocator->releaseReferenceNodes.at(1));

    clReleaseEvent(event1);
    EXPECT_EQ(1u, mockTagAllocator->returnedToFreePoolNodes.size()); // removed last reference on node1
    EXPECT_EQ(node1, mockTagAllocator->returnedToFreePoolNodes.at(0));
    EXPECT_EQ(3u, mockTagAllocator->releaseReferenceNodes.size()); // event1 released node1
    EXPECT_EQ(node1, mockTagAllocator->releaseReferenceNodes.at(2));

    cmdQ.reset(nullptr);
    EXPECT_EQ(2u, mockTagAllocator->returnedToFreePoolNodes.size()); // removed last reference on node2
    EXPECT_EQ(node2, mockTagAllocator->returnedToFreePoolNodes.at(1));
    EXPECT_EQ(4u, mockTagAllocator->releaseReferenceNodes.size()); // cmdQ released node2
    EXPECT_EQ(node2, mockTagAllocator->releaseReferenceNodes.at(3));
}

HWCMDTEST_F(IGFX_GEN8_CORE, TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEnqueueingThenWriteWalkerStamp) {
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_NE(nullptr, cmdQ->timestampPacketNode);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdQ->getCS(0), 0);

    bool walkerFound = false;
    for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
        if (genCmdCast<GPGPU_WALKER *>(*it)) {
            walkerFound = true;
            auto pipeControl = genCmdCast<PIPE_CONTROL *>(*++it);
            ASSERT_NE(nullptr, pipeControl);
            EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, pipeControl->getPostSyncOperation());
        }
    }
    EXPECT_TRUE(walkerFound);
}

HWTEST_F(TimestampPacketTests, givenEventsRequestWhenEstimatingStreamSizeForCsrThenAddSizeForSemaphores) {
    cl_uint numEventsOnWaitlist = 5;
    EventsRequest eventsRequest(numEventsOnWaitlist, nullptr, nullptr);
    DispatchFlags flags;

    auto sizeWithoutEvents = device->getUltCommandStreamReceiver<FamilyType>().getRequiredCmdStreamSize(flags, *device.get());

    flags.outOfDeviceDependencies = &eventsRequest;
    auto sizeWithEvents = device->getUltCommandStreamReceiver<FamilyType>().getRequiredCmdStreamSize(flags, *device.get());

    size_t extendedSize = sizeWithoutEvents + (numEventsOnWaitlist * sizeof(typename FamilyType::MI_SEMAPHORE_WAIT));

    EXPECT_EQ(sizeWithEvents, extendedSize);
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEnqueueingThenProgramSemaphoresOnCsrStream) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto device2 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(nullptr, &executionEnvironment, 1u));

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    MockContext context2(device2.get());

    auto cmdQ1 = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    auto cmdQ2 = std::make_unique<MockCommandQueueHw<FamilyType>>(&context2, device2.get(), nullptr);

    const cl_uint eventsOnWaitlist = 6;
    TagNode<TimestampPacket> *tagNodes[eventsOnWaitlist];
    for (size_t i = 0; i < eventsOnWaitlist; i++) {
        tagNodes[i] = executionEnvironment.memoryManager->getTimestampPacketAllocator()->getTag();
    }

    UserEvent event1;
    event1.setStatus(CL_COMPLETE);
    UserEvent event2;
    event2.setStatus(CL_COMPLETE);
    Event event3(cmdQ1.get(), 0, 0, 0);
    event3.setTimestampPacketNode(tagNodes[2]);
    Event event4(cmdQ2.get(), 0, 0, 0);
    event4.setTimestampPacketNode(tagNodes[3]);
    Event event5(cmdQ1.get(), 0, 0, 0);
    event5.setTimestampPacketNode(tagNodes[4]);
    Event event6(cmdQ2.get(), 0, 0, 0);
    event6.setTimestampPacketNode(tagNodes[5]);

    cl_event waitlist[] = {&event1, &event2, &event3, &event4, &event5, &event6};

    cmdQ1->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, eventsOnWaitlist, waitlist, nullptr);
    auto &cmdStream = device->getUltCommandStreamReceiver<FamilyType>().commandStream;

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    auto it = hwParser.cmdList.begin();
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), &event4);
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), &event6);

    while (it != hwParser.cmdList.end()) {
        EXPECT_EQ(nullptr, genCmdCast<MI_SEMAPHORE_WAIT *>(*it));
        it++;
    }
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEnqueueingBlockedThenProgramSemaphoresOnCsrStreamOnFlush) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto device2 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(nullptr, &executionEnvironment, 1u));

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    MockContext context2(device2.get());

    auto cmdQ1 = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    auto cmdQ2 = std::make_unique<MockCommandQueueHw<FamilyType>>(&context2, device2.get(), nullptr);

    UserEvent userEvent;
    Event event0(cmdQ1.get(), 0, 0, 0);
    event0.setTimestampPacketNode(executionEnvironment.memoryManager->getTimestampPacketAllocator()->getTag());
    Event event1(cmdQ2.get(), 0, 0, 0);
    event1.setTimestampPacketNode(executionEnvironment.memoryManager->getTimestampPacketAllocator()->getTag());

    cl_event waitlist[] = {&userEvent, &event0, &event1};
    cmdQ1->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 3, waitlist, nullptr);
    auto &cmdStream = device->getUltCommandStreamReceiver<FamilyType>().commandStream;
    EXPECT_EQ(0u, cmdStream.getUsed());
    userEvent.setStatus(CL_COMPLETE);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    auto it = hwParser.cmdList.begin();
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), &event1);

    while (it != hwParser.cmdList.end()) {
        EXPECT_EQ(nullptr, genCmdCast<MI_SEMAPHORE_WAIT *>(*it));
        it++;
    }
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenDispatchingThenProgramSemaphoresForWaitlist) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using WALKER = WALKER_TYPE<FamilyType>;

    auto device2 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(nullptr, &executionEnvironment, 1u));
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    MockContext context2(device2.get());

    MockMultiDispatchInfo multiDispatchInfo(std::vector<Kernel *>({kernel->mockKernel}));

    MockCommandQueue mockCmdQ2(&context2, device2.get(), nullptr);
    auto &cmdStream = mockCmdQ->getCS(0);

    const cl_uint eventsOnWaitlist = 6;
    TagNode<TimestampPacket> *tagNodes[eventsOnWaitlist];
    for (size_t i = 0; i < eventsOnWaitlist; i++) {
        tagNodes[i] = executionEnvironment.memoryManager->getTimestampPacketAllocator()->getTag();
    }

    UserEvent event1;
    UserEvent event2;
    Event event3(mockCmdQ.get(), 0, 0, 0);
    event3.setTimestampPacketNode(tagNodes[2]);
    Event event4(&mockCmdQ2, 0, 0, 0);
    event4.setTimestampPacketNode(tagNodes[3]);
    Event event5(mockCmdQ.get(), 0, 0, 0);
    event5.setTimestampPacketNode(tagNodes[4]);
    Event event6(&mockCmdQ2, 0, 0, 0);
    event6.setTimestampPacketNode(tagNodes[5]);

    cl_event waitlist[] = {&event1, &event2, &event3, &event4, &event5, &event6};

    GpgpuWalkerHelper<FamilyType>::dispatchWalker(
        *mockCmdQ,
        multiDispatchInfo,
        eventsOnWaitlist,
        waitlist,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        device->getPreemptionMode(),
        false);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    uint32_t semaphoresFound = 0;
    uint32_t walkersFound = 0;

    for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*it);
        if (semaphoreCmd) {
            semaphoresFound++;
            if (semaphoresFound == 1) {
                verifySemaphore(semaphoreCmd, &event3);
            } else if (semaphoresFound == 2) {
                verifySemaphore(semaphoreCmd, &event5);
            }
        }
        if (genCmdCast<WALKER *>(*it)) {
            walkersFound++;
            EXPECT_EQ(2u, semaphoresFound); // semaphores from events programmed before walker
        }
    }
    EXPECT_EQ(1u, walkersFound);
    EXPECT_EQ(2u, semaphoresFound); // total number of semaphores found in cmdList
}

TEST_F(TimestampPacketTests, givenAlreadyAssignedNodeWhenObtainingThenGetNewBeforeReleasing) {
    auto mockMemoryManager = new MockMemoryManager();
    device->injectMemoryManager(mockMemoryManager);
    context->setMemoryManager(mockMemoryManager);
    auto mockTagAllocator = new MockTagAllocator<>(mockMemoryManager, 1);
    mockMemoryManager->timestampPacketAllocator.reset(mockTagAllocator);

    mockCmdQ->obtainNewTimestampPacketNode();
    auto firstNode = mockCmdQ->timestampPacketNode;
    EXPECT_TRUE(mockTagAllocator->freeTags.peekIsEmpty());

    setTagToReadyState(firstNode->tag);

    mockCmdQ->obtainNewTimestampPacketNode();
    auto secondNode = mockCmdQ->timestampPacketNode;
    EXPECT_FALSE(mockTagAllocator->freeTags.peekIsEmpty()); // new pool allocated for secondNode
    EXPECT_NE(firstNode, secondNode);
}

HWTEST_F(TimestampPacketTests, givenAlreadyAssignedNodeWhenEnqueueingNonBlockedThenMakeItResident) {
    auto mockMemoryManager = new MockMemoryManager();
    device->injectMemoryManager(mockMemoryManager);
    context->setMemoryManager(mockMemoryManager);
    auto mockTagAllocator = new MockTagAllocator<>(mockMemoryManager, 1);
    mockMemoryManager->timestampPacketAllocator.reset(mockTagAllocator);

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    cmdQ->obtainNewTimestampPacketNode();
    auto firstNode = cmdQ->timestampPacketNode;

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.storeMakeResidentAllocations = true;
    csr.timestampPacketWriteEnabled = true;

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    auto secondNode = cmdQ->timestampPacketNode;

    EXPECT_NE(firstNode->getGraphicsAllocation(), secondNode->getGraphicsAllocation());
    EXPECT_TRUE(csr.isMadeResident(firstNode->getGraphicsAllocation()));
}

HWTEST_F(TimestampPacketTests, givenAlreadyAssignedNodeWhenEnqueueingBlockedThenMakeItResident) {
    auto mockMemoryManager = new MockMemoryManager();
    device->injectMemoryManager(mockMemoryManager);
    context->setMemoryManager(mockMemoryManager);
    auto mockTagAllocator = new MockTagAllocator<>(mockMemoryManager, 1);
    mockMemoryManager->timestampPacketAllocator.reset(mockTagAllocator);

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    cmdQ->obtainNewTimestampPacketNode();
    auto firstNode = cmdQ->timestampPacketNode;

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.storeMakeResidentAllocations = true;
    csr.timestampPacketWriteEnabled = true;

    UserEvent userEvent;
    cl_event clEvent = &userEvent;
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 1, &clEvent, nullptr);
    auto secondNode = cmdQ->timestampPacketNode;

    EXPECT_NE(firstNode->getGraphicsAllocation(), secondNode->getGraphicsAllocation());
    EXPECT_FALSE(csr.isMadeResident(firstNode->getGraphicsAllocation()));
    userEvent.setStatus(CL_COMPLETE);
    EXPECT_TRUE(csr.isMadeResident(firstNode->getGraphicsAllocation()));
}

HWTEST_F(TimestampPacketTests, givenAlreadyAssignedNodeWhenEnqueueingThenDontKeepDependencyOnPreviousNodeIfItsReady) {
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    MockCommandQueueHw<FamilyType> cmdQ(context.get(), device.get(), nullptr);
    cmdQ.obtainNewTimestampPacketNode();
    auto firstNode = cmdQ.timestampPacketNode;
    setTagToReadyState(firstNode->tag);

    cmdQ.enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ.commandStream, 0);

    uint32_t semaphoresFound = 0;
    for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
        if (genCmdCast<typename FamilyType::MI_SEMAPHORE_WAIT *>(*it)) {
            semaphoresFound++;
        }
    }
    EXPECT_EQ(0u, semaphoresFound);
}

HWTEST_F(TimestampPacketTests, givenAlreadyAssignedNodeWhenEnqueueingThenKeepDependencyOnPreviousNodeIfItsNotReady) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    MockCommandQueueHw<FamilyType> cmdQ(context.get(), device.get(), nullptr);
    cmdQ.obtainNewTimestampPacketNode();
    auto firstNode = cmdQ.timestampPacketNode;

    cmdQ.enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ.commandStream, 0);

    auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*hwParser.cmdList.begin());
    EXPECT_NE(nullptr, semaphoreCmd);
    EXPECT_EQ(firstNode->tag->pickAddressForDataWrite(TimestampPacket::DataIndex::ContextEnd), semaphoreCmd->getSemaphoreGraphicsAddress());
    EXPECT_EQ(1u, semaphoreCmd->getSemaphoreDataDword());
    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, semaphoreCmd->getCompareOperation());

    uint32_t semaphoresFound = 0;
    auto it = hwParser.cmdList.begin();
    for (++it; it != hwParser.cmdList.end(); it++) {
        if (genCmdCast<typename FamilyType::MI_SEMAPHORE_WAIT *>(*it)) {
            semaphoresFound++;
        }
    }
    EXPECT_EQ(0u, semaphoresFound);
}

HWTEST_F(TimestampPacketTests, givenAlreadyAssignedNodeWhenEnqueueingToOoqThenDontKeepDependencyOnPreviousNodeIfItsNotReady) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    cl_queue_properties properties[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    MockCommandQueueHw<FamilyType> cmdQ(context.get(), device.get(), properties);
    cmdQ.obtainNewTimestampPacketNode();

    cmdQ.enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ.commandStream, 0);

    uint32_t semaphoresFound = 0;
    for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
        if (genCmdCast<typename FamilyType::MI_SEMAPHORE_WAIT *>(*it)) {
            semaphoresFound++;
        }
    }
    EXPECT_EQ(0u, semaphoresFound);
}

HWTEST_F(TimestampPacketTests, givenEventsWaitlistFromDifferentDevicesWhenEnqueueingThenMakeAllTimestampsResident) {
    auto device2 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(nullptr, &executionEnvironment, 1u));

    auto &ultCsr = device->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.timestampPacketWriteEnabled = true;
    ultCsr.storeMakeResidentAllocations = true;
    MockContext context2(device2.get());

    auto cmdQ1 = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    auto cmdQ2 = std::make_unique<MockCommandQueueHw<FamilyType>>(&context2, device2.get(), nullptr);

    TagAllocator<TimestampPacket> tagAllocator1(executionEnvironment.memoryManager.get(), 1, 1);
    auto tagNode1 = tagAllocator1.getTag();
    TagAllocator<TimestampPacket> tagAllocator2(executionEnvironment.memoryManager.get(), 1, 1);
    auto tagNode2 = tagAllocator2.getTag();

    Event event0(cmdQ1.get(), 0, 0, 0);
    event0.setTimestampPacketNode(tagNode1);
    Event event1(cmdQ2.get(), 0, 0, 0);
    event1.setTimestampPacketNode(tagNode2);

    cl_event waitlist[] = {&event0, &event1};

    cmdQ1->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 2, waitlist, nullptr);

    EXPECT_TRUE(ultCsr.isMadeResident(tagNode1->getGraphicsAllocation()));
    EXPECT_TRUE(ultCsr.isMadeResident(tagNode2->getGraphicsAllocation()));
}
