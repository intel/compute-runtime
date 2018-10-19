/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/command_queue/hardware_interface.h"
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

#include "gmock/gmock.h"
#include "test.h"

using namespace OCLRT;

struct TimestampPacketSimpleTests : public ::testing::Test {
    class MockTimestampPacket : public TimestampPacket {
      public:
        using TimestampPacket::data;
        using TimestampPacket::implicitDependenciesCount;
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

    class MockTimestampPacketContainer : public TimestampPacketContainer {
      public:
        using TimestampPacketContainer::timestampPacketNodes;

        MockTimestampPacketContainer(MemoryManager *memoryManager, size_t numberOfPreallocatedTags) : TimestampPacketContainer(memoryManager) {
            for (size_t i = 0; i < numberOfPreallocatedTags; i++) {
                add(memoryManager->getTimestampPacketAllocator()->getTag());
            }
        }

        TagNode<TimestampPacket> *getNode(size_t position) {
            return timestampPacketNodes.at(position);
        }
    };

    void setTagToReadyState(TimestampPacket *tag) {
        memset(reinterpret_cast<void *>(tag->pickAddressForDataWrite(TimestampPacket::DataIndex::ContextStart)), 0, timestampDataSize);
        auto dependenciesCount = reinterpret_cast<std::atomic<uint32_t> *>(reinterpret_cast<void *>(tag->pickImplicitDependenciesCountWriteAddress()));
        dependenciesCount->store(0);
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
    void verifySemaphore(MI_SEMAPHORE_WAIT *semaphoreCmd, TimestampPacket *timestampPacket) {
        EXPECT_NE(nullptr, semaphoreCmd);
        EXPECT_EQ(semaphoreCmd->getCompareOperation(), MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
        EXPECT_EQ(1u, semaphoreCmd->getSemaphoreDataDword());
        EXPECT_EQ(timestampPacket->pickAddressForDataWrite(TimestampPacket::DataIndex::ContextEnd),
                  semaphoreCmd->getSemaphoreGraphicsAddress());
    };

    template <typename MI_ATOMIC>
    void verifyMiAtomic(MI_ATOMIC *miAtomicCmd, TimestampPacket *timestampPacket) {
        EXPECT_NE(nullptr, miAtomicCmd);
        auto writeAddress = timestampPacket->pickImplicitDependenciesCountWriteAddress();
        EXPECT_EQ(MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_4B_DECREMENT, miAtomicCmd->getAtomicOpcode());
        EXPECT_EQ(static_cast<uint32_t>(writeAddress & 0x0000FFFFFFFFULL), miAtomicCmd->getMemoryAddress());
        EXPECT_EQ(static_cast<uint32_t>(writeAddress >> 32), miAtomicCmd->getMemoryAddressHigh());
    };

    void verifyDependencyCounterValues(TimestampPacketContainer *timestmapPacketContainer, uint32_t expectedValue) {
        auto &nodes = timestmapPacketContainer->peekNodes();
        EXPECT_NE(0u, nodes.size());
        for (auto &node : nodes) {
            auto dependenciesCount = reinterpret_cast<std::atomic<uint32_t> *>(reinterpret_cast<void *>(node->tag->pickImplicitDependenciesCountWriteAddress()));
            EXPECT_EQ(expectedValue, dependenciesCount->load());
        }
    }

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

TEST_F(TimestampPacketSimpleTests, givenImplicitDependencyWhenEndTagIsWrittenThenCantBeReleased) {
    MockTimestampPacket timestampPacket;
    auto contextEndIndex = static_cast<uint32_t>(TimestampPacket::DataIndex::ContextEnd);
    auto globalEndIndex = static_cast<uint32_t>(TimestampPacket::DataIndex::GlobalEnd);

    timestampPacket.data[contextEndIndex] = 0;
    timestampPacket.data[globalEndIndex] = 0;
    timestampPacket.implicitDependenciesCount.store(1);
    EXPECT_FALSE(timestampPacket.canBeReleased());
    timestampPacket.implicitDependenciesCount.store(0);
    EXPECT_TRUE(timestampPacket.canBeReleased());
}

TEST_F(TimestampPacketSimpleTests, whenNewTagIsTakenThenReinitialize) {
    MockMemoryManager memoryManager;
    MockTagAllocator<MockTimestampPacket> allocator(&memoryManager, 1);

    auto firstNode = allocator.getTag();
    firstNode->tag->data = {{5, 6, 7, 8}};
    auto dependenciesCount = reinterpret_cast<std::atomic<uint32_t> *>(reinterpret_cast<void *>(firstNode->tag->pickImplicitDependenciesCountWriteAddress()));

    setTagToReadyState(firstNode->tag);
    allocator.returnTag(firstNode);
    (*dependenciesCount)++;

    auto secondNode = allocator.getTag();
    EXPECT_EQ(secondNode, firstNode);

    EXPECT_EQ(0u, dependenciesCount->load());
    for (uint32_t i = 0; i < static_cast<uint32_t>(TimestampPacket::DataIndex::Max); i++) {
        EXPECT_EQ(1u, secondNode->tag->data[i]);
    }
}

TEST_F(TimestampPacketSimpleTests, whenObjectIsCreatedThenInitializeAllStamps) {
    MockTimestampPacket timestampPacket;
    auto maxElements = static_cast<uint32_t>(TimestampPacket::DataIndex::Max);
    EXPECT_EQ(4u, maxElements);

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

HWCMDTEST_F(IGFX_GEN8_CORE, TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEstimatingStreamSizeThenAddPipeControl) {
    MockKernelWithInternals kernel2(*device);
    MockMultiDispatchInfo multiDispatchInfo(std::vector<Kernel *>({kernel->mockKernel, kernel2.mockKernel}));

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = false;
    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*mockCmdQ, 0, false, false, multiDispatchInfo);
    auto sizeWithDisabled = mockCmdQ->requestedCmdStreamSize;

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*mockCmdQ, 0, false, false, multiDispatchInfo);
    auto sizeWithEnabled = mockCmdQ->requestedCmdStreamSize;

    auto extendedSize = sizeWithDisabled + sizeof(typename FamilyType::PIPE_CONTROL) +
                        sizeof(typename FamilyType::MI_SEMAPHORE_WAIT) + sizeof(typename FamilyType::MI_ATOMIC);

    EXPECT_EQ(sizeWithEnabled, extendedSize);
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledAndOoqWhenEstimatingStreamSizeDontDontAddAdditionalSize) {
    MockMultiDispatchInfo multiDispatchInfo(std::vector<Kernel *>({kernel->mockKernel}));
    mockCmdQ->setOoqEnabled();

    cl_uint numEventsOnWaitlist = 5;

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = false;
    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*mockCmdQ, numEventsOnWaitlist, false, false, multiDispatchInfo);
    auto sizeWithDisabled = mockCmdQ->requestedCmdStreamSize;

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*mockCmdQ, numEventsOnWaitlist, false, false, multiDispatchInfo);
    auto sizeWithEnabled = mockCmdQ->requestedCmdStreamSize;

    size_t extendedSize = sizeWithDisabled + EnqueueOperation<FamilyType>::getSizeRequiredForTimestampPacketWrite() +
                          (numEventsOnWaitlist * (sizeof(typename FamilyType::MI_SEMAPHORE_WAIT) + sizeof(typename FamilyType::MI_ATOMIC)));

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
                          ((numEventsOnWaitlist + 1) * (sizeof(typename FamilyType::MI_SEMAPHORE_WAIT) + sizeof(typename FamilyType::MI_ATOMIC)));

    EXPECT_EQ(sizeWithEnabled, extendedSize);
}

HWCMDTEST_F(IGFX_GEN8_CORE, TimestampPacketTests, givenTimestampPacketWhenDispatchingGpuWalkerThenAddTwoPcForLastWalker) {
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    MockTimestampPacketContainer timestampPacket(device->getMemoryManager(), 2);

    MockKernelWithInternals kernel2(*device);

    MockMultiDispatchInfo multiDispatchInfo(std::vector<Kernel *>({kernel->mockKernel, kernel2.mockKernel}));

    auto &cmdStream = mockCmdQ->getCS(0);

    HardwareInterface<FamilyType>::dispatchWalker(
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
            auto pipeControl = genCmdCast<PIPE_CONTROL *>(*++it);
            EXPECT_NE(nullptr, pipeControl);
            verifyPipeControl(pipeControl, timestampPacket.getNode(walkersFound)->tag->pickAddressForDataWrite(TimestampPacket::DataIndex::ContextEnd));
            walkersFound++;
        }
    }
    EXPECT_EQ(2u, walkersFound);
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEnqueueingThenObtainNewStampAndPassToEvent) {
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    auto mockMemoryManager = new MockMemoryManager(*device->getExecutionEnvironment());
    device->injectMemoryManager(mockMemoryManager);
    context->setMemoryManager(mockMemoryManager);
    auto mockTagAllocator = new MockTagAllocator<>(mockMemoryManager);
    mockMemoryManager->timestampPacketAllocator.reset(mockTagAllocator);
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);

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

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEnqueueingThenWriteWalkerStamp) {
    using GPGPU_WALKER = typename FamilyType::WALKER_TYPE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(1u, cmdQ->timestampPacketContainer->peekNodes().size());

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
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;

    auto device2 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(nullptr, &executionEnvironment, 1u));

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    device2->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    MockContext context2(device2.get());

    auto cmdQ1 = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    auto cmdQ2 = std::make_unique<MockCommandQueueHw<FamilyType>>(&context2, device2.get(), nullptr);

    const cl_uint eventsOnWaitlist = 6;
    MockTimestampPacketContainer timestamp3(device->getMemoryManager(), 1);
    MockTimestampPacketContainer timestamp4(device->getMemoryManager(), 1);
    MockTimestampPacketContainer timestamp5(device->getMemoryManager(), 1);
    MockTimestampPacketContainer timestamp6(device->getMemoryManager(), 2);

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
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), timestamp4.getNode(0)->tag);
    verifyMiAtomic(genCmdCast<MI_ATOMIC *>(*it++), timestamp4.getNode(0)->tag);
    verifyDependencyCounterValues(event4.getTimestampPacketNodes(), 1);
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), timestamp6.getNode(0)->tag);
    verifyMiAtomic(genCmdCast<MI_ATOMIC *>(*it++), timestamp6.getNode(0)->tag);
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), timestamp6.getNode(1)->tag);
    verifyMiAtomic(genCmdCast<MI_ATOMIC *>(*it++), timestamp6.getNode(1)->tag);
    verifyDependencyCounterValues(event6.getTimestampPacketNodes(), 1);

    while (it != hwParser.cmdList.end()) {
        EXPECT_EQ(nullptr, genCmdCast<MI_SEMAPHORE_WAIT *>(*it));
        it++;
    }
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEnqueueingBlockedThenProgramSemaphoresOnCsrStreamOnFlush) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto device2 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(nullptr, &executionEnvironment, 1u));

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    device2->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    MockContext context2(device2.get());

    auto cmdQ1 = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    auto cmdQ2 = std::make_unique<MockCommandQueueHw<FamilyType>>(&context2, device2.get(), nullptr);

    MockTimestampPacketContainer timestamp0(device->getMemoryManager(), 1);
    MockTimestampPacketContainer timestamp1(device->getMemoryManager(), 1);

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
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), timestamp1.getNode(0)->tag);
    verifyMiAtomic(genCmdCast<typename FamilyType::MI_ATOMIC *>(*it++), timestamp1.getNode(0)->tag);
    verifyDependencyCounterValues(event1.getTimestampPacketNodes(), 1);

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
    device2->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    MockContext context2(device2.get());

    MockMultiDispatchInfo multiDispatchInfo(std::vector<Kernel *>({kernel->mockKernel}));

    MockCommandQueue mockCmdQ2(&context2, device2.get(), nullptr);
    auto &cmdStream = mockCmdQ->getCS(0);

    const cl_uint eventsOnWaitlist = 6;
    MockTimestampPacketContainer timestamp3(device->getMemoryManager(), 1);
    MockTimestampPacketContainer timestamp4(device->getMemoryManager(), 1);
    MockTimestampPacketContainer timestamp5(device->getMemoryManager(), 2);
    MockTimestampPacketContainer timestamp6(device->getMemoryManager(), 1);

    UserEvent event1;
    UserEvent event2;
    Event event3(mockCmdQ.get(), 0, 0, 0);
    event3.addTimestampPacketNodes(timestamp3);
    Event event4(&mockCmdQ2, 0, 0, 0);
    event4.addTimestampPacketNodes(timestamp4);
    Event event5(mockCmdQ.get(), 0, 0, 0);
    event5.addTimestampPacketNodes(timestamp5);
    Event event6(&mockCmdQ2, 0, 0, 0);
    event6.addTimestampPacketNodes(timestamp6);

    cl_event waitlist[] = {&event1, &event2, &event3, &event4, &event5, &event6};

    HardwareInterface<FamilyType>::dispatchWalker(
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
                verifySemaphore(semaphoreCmd, timestamp3.getNode(0)->tag);
                verifyMiAtomic(genCmdCast<typename FamilyType::MI_ATOMIC *>(*++it), timestamp3.getNode(0)->tag);
                verifyDependencyCounterValues(event3.getTimestampPacketNodes(), 1);
            } else if (semaphoresFound == 2) {
                verifySemaphore(semaphoreCmd, timestamp5.getNode(0)->tag);
                verifyMiAtomic(genCmdCast<typename FamilyType::MI_ATOMIC *>(*++it), timestamp5.getNode(0)->tag);
                verifyDependencyCounterValues(event5.getTimestampPacketNodes(), 1);
            } else if (semaphoresFound == 3) {
                verifySemaphore(semaphoreCmd, timestamp5.getNode(1)->tag);
                verifyMiAtomic(genCmdCast<typename FamilyType::MI_ATOMIC *>(*++it), timestamp5.getNode(1)->tag);
                verifyDependencyCounterValues(event5.getTimestampPacketNodes(), 1);
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

HWTEST_F(TimestampPacketTests, givenAlreadyAssignedNodeWhenEnqueueingNonBlockedThenMakeItResident) {
    auto mockMemoryManager = new MockMemoryManager(*device->getExecutionEnvironment());
    device->injectMemoryManager(mockMemoryManager);
    context->setMemoryManager(mockMemoryManager);
    auto mockTagAllocator = new MockTagAllocator<>(mockMemoryManager, 1);
    mockMemoryManager->timestampPacketAllocator.reset(mockTagAllocator);

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    TimestampPacketContainer previousNodes(device->getMemoryManager());
    cmdQ->obtainNewTimestampPacketNodes(1, previousNodes);
    auto firstNode = cmdQ->timestampPacketContainer->peekNodes().at(0);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.storeMakeResidentAllocations = true;
    csr.timestampPacketWriteEnabled = true;

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    auto secondNode = cmdQ->timestampPacketContainer->peekNodes().at(0);

    EXPECT_NE(firstNode->getGraphicsAllocation(), secondNode->getGraphicsAllocation());
    EXPECT_TRUE(csr.isMadeResident(firstNode->getGraphicsAllocation()));
}

HWTEST_F(TimestampPacketTests, givenAlreadyAssignedNodeWhenEnqueueingBlockedThenMakeItResident) {
    auto mockMemoryManager = new MockMemoryManager(*device->getExecutionEnvironment());
    device->injectMemoryManager(mockMemoryManager);
    context->setMemoryManager(mockMemoryManager);
    auto mockTagAllocator = new MockTagAllocator<>(mockMemoryManager, 1);
    mockMemoryManager->timestampPacketAllocator.reset(mockTagAllocator);

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    TimestampPacketContainer previousNodes(device->getMemoryManager());
    cmdQ->obtainNewTimestampPacketNodes(1, previousNodes);
    auto firstNode = cmdQ->timestampPacketContainer->peekNodes().at(0);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.storeMakeResidentAllocations = true;
    csr.timestampPacketWriteEnabled = true;

    UserEvent userEvent;
    cl_event clEvent = &userEvent;
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 1, &clEvent, nullptr);
    auto secondNode = cmdQ->timestampPacketContainer->peekNodes().at(0);

    EXPECT_NE(firstNode->getGraphicsAllocation(), secondNode->getGraphicsAllocation());
    EXPECT_FALSE(csr.isMadeResident(firstNode->getGraphicsAllocation()));
    userEvent.setStatus(CL_COMPLETE);
    EXPECT_TRUE(csr.isMadeResident(firstNode->getGraphicsAllocation()));
}

HWTEST_F(TimestampPacketTests, givenAlreadyAssignedNodeWhenEnqueueingThenDontKeepDependencyOnPreviousNodeIfItsReady) {
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    MockCommandQueueHw<FamilyType> cmdQ(context.get(), device.get(), nullptr);
    TimestampPacketContainer previousNodes(device->getMemoryManager());
    cmdQ.obtainNewTimestampPacketNodes(1, previousNodes);
    auto firstNode = cmdQ.timestampPacketContainer->peekNodes().at(0);
    setTagToReadyState(firstNode->tag);

    cmdQ.enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ.commandStream, 0);

    uint32_t semaphoresFound = 0;
    uint32_t atomicsFound = 0;
    for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
        if (genCmdCast<typename FamilyType::MI_SEMAPHORE_WAIT *>(*it)) {
            semaphoresFound++;
        }
        if (genCmdCast<typename FamilyType::MI_ATOMIC *>(*it)) {
            atomicsFound++;
        }
    }
    EXPECT_EQ(0u, semaphoresFound);
    EXPECT_EQ(0u, atomicsFound);
}

HWTEST_F(TimestampPacketTests, givenAlreadyAssignedNodeWhenEnqueueingThenKeepDependencyOnPreviousNodeIfItsNotReady) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    MockTimestampPacketContainer firstNode(device->getMemoryManager(), 0);

    MockCommandQueueHw<FamilyType> cmdQ(context.get(), device.get(), nullptr);
    TimestampPacketContainer previousNodes(device->getMemoryManager());
    cmdQ.obtainNewTimestampPacketNodes(2, previousNodes);
    firstNode.add(cmdQ.timestampPacketContainer->peekNodes().at(0));
    firstNode.add(cmdQ.timestampPacketContainer->peekNodes().at(1));
    auto firstTag0 = firstNode.getNode(0);
    auto firstTag1 = firstNode.getNode(1);

    verifyDependencyCounterValues(&firstNode, 0);
    cmdQ.enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    verifyDependencyCounterValues(&firstNode, 1);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ.commandStream, 0);

    auto it = hwParser.cmdList.begin();
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it), firstTag0->tag);
    verifyMiAtomic(genCmdCast<MI_ATOMIC *>(*++it), firstTag0->tag);

    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*++it), firstTag1->tag);
    verifyMiAtomic(genCmdCast<MI_ATOMIC *>(*++it), firstTag1->tag);

    while (it != hwParser.cmdList.end()) {
        EXPECT_EQ(nullptr, genCmdCast<MI_SEMAPHORE_WAIT *>(*it));
        it++;
    }
}

HWTEST_F(TimestampPacketTests, givenAlreadyAssignedNodeWhenEnqueueingToOoqThenDontKeepDependencyOnPreviousNodeIfItsNotReady) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    cl_queue_properties properties[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    MockCommandQueueHw<FamilyType> cmdQ(context.get(), device.get(), properties);
    TimestampPacketContainer previousNodes(device->getMemoryManager());
    cmdQ.obtainNewTimestampPacketNodes(1, previousNodes);

    cmdQ.enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdQ.commandStream, 0);

    uint32_t semaphoresFound = 0;
    uint32_t atomicsFound = 0;
    for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
        if (genCmdCast<typename FamilyType::MI_SEMAPHORE_WAIT *>(*it)) {
            semaphoresFound++;
        }
        if (genCmdCast<typename FamilyType::MI_ATOMIC *>(*it)) {
            atomicsFound++;
        }
    }
    EXPECT_EQ(0u, semaphoresFound);
    EXPECT_EQ(0u, atomicsFound);
}

HWTEST_F(TimestampPacketTests, givenEventsWaitlistFromDifferentDevicesWhenEnqueueingThenMakeAllTimestampsResident) {
    TagAllocator<TimestampPacket> tagAllocator(executionEnvironment.memoryManager.get(), 1, 1);
    auto device2 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(nullptr, &executionEnvironment, 1u));

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    device2->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    auto &ultCsr = device->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.timestampPacketWriteEnabled = true;
    ultCsr.storeMakeResidentAllocations = true;
    MockContext context2(device2.get());

    auto cmdQ1 = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    auto cmdQ2 = std::make_unique<MockCommandQueueHw<FamilyType>>(&context2, device2.get(), nullptr);

    MockTimestampPacketContainer node1(device->getMemoryManager(), 0);
    MockTimestampPacketContainer node2(device->getMemoryManager(), 0);

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

    EXPECT_NE(tagNode1->getGraphicsAllocation(), tagNode2->getGraphicsAllocation());
    EXPECT_TRUE(ultCsr.isMadeResident(tagNode1->getGraphicsAllocation()));
    EXPECT_TRUE(ultCsr.isMadeResident(tagNode2->getGraphicsAllocation()));
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWhenEnqueueingNonBlockedThenMakeItResident) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;
    csr.storeMakeResidentAllocations = true;

    MockKernelWithInternals mockKernel(*device, context.get());
    MockCommandQueueHw<FamilyType> cmdQ(context.get(), device.get(), nullptr);

    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    auto timestampPacketNode = cmdQ.timestampPacketContainer->peekNodes().at(0);

    EXPECT_TRUE(csr.isMadeResident(timestampPacketNode->getGraphicsAllocation()));
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWhenEnqueueingBlockedThenMakeItResidentOnSubmit) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;
    MockKernelWithInternals mockKernel(*device, context.get());
    MockCommandQueueHw<FamilyType> cmdQ(context.get(), device.get(), nullptr);

    csr.storeMakeResidentAllocations = true;

    UserEvent userEvent;
    cl_event clEvent = &userEvent;

    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 1, &clEvent, nullptr);
    auto timestampPacketNode = cmdQ.timestampPacketContainer->peekNodes().at(0);

    EXPECT_FALSE(csr.isMadeResident(timestampPacketNode->getGraphicsAllocation()));
    userEvent.setStatus(CL_COMPLETE);
    EXPECT_TRUE(csr.isMadeResident(timestampPacketNode->getGraphicsAllocation()));
}

TEST_F(TimestampPacketTests, givenDispatchSizeWhenAskingForNewTimestampsThenObtainEnoughTags) {
    size_t dispatchSize = 3;

    mockCmdQ->timestampPacketContainer = std::make_unique<MockTimestampPacketContainer>(device->getMemoryManager(), 0);
    EXPECT_EQ(0u, mockCmdQ->timestampPacketContainer->peekNodes().size());

    TimestampPacketContainer previousNodes(device->getMemoryManager());
    mockCmdQ->obtainNewTimestampPacketNodes(dispatchSize, previousNodes);
    EXPECT_EQ(dispatchSize, mockCmdQ->timestampPacketContainer->peekNodes().size());
}

HWTEST_F(TimestampPacketTests, givenWaitlistAndOutputEventWhenEnqueueingWithoutKernelThenInheritTimestampPacketsWithoutSubmitting) {
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    MockCommandQueueHw<FamilyType> cmdQ(context.get(), device.get(), nullptr);

    MockKernelWithInternals mockKernel(*device, context.get());
    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr); // obtain first TimestmapPacket

    TimestampPacketContainer cmdQNodes(device->getMemoryManager());
    cmdQNodes.assignAndIncrementNodesRefCounts(*cmdQ.timestampPacketContainer);

    MockTimestampPacketContainer node1(device->getMemoryManager(), 1);
    MockTimestampPacketContainer node2(device->getMemoryManager(), 1);

    Event event0(&cmdQ, 0, 0, 0);
    event0.addTimestampPacketNodes(node1);
    Event event1(&cmdQ, 0, 0, 0);
    event1.addTimestampPacketNodes(node2);

    cl_event waitlist[] = {&event0, &event1};

    cl_event clOutEvent;
    cmdQ.enqueueMarkerWithWaitList(2, waitlist, &clOutEvent);

    auto outEvent = castToObject<Event>(clOutEvent);

    EXPECT_EQ(cmdQ.timestampPacketContainer->peekNodes().at(0), cmdQNodes.peekNodes().at(0)); // no new nodes obtained
    EXPECT_EQ(1u, cmdQ.timestampPacketContainer->peekNodes().size());

    auto &eventsNodes = outEvent->getTimestampPacketNodes()->peekNodes();
    EXPECT_EQ(3u, eventsNodes.size());
    EXPECT_EQ(cmdQNodes.peekNodes().at(0), eventsNodes.at(0));
    EXPECT_EQ(event0.getTimestampPacketNodes()->peekNodes().at(0), eventsNodes.at(1));
    EXPECT_EQ(event1.getTimestampPacketNodes()->peekNodes().at(0), eventsNodes.at(2));

    clReleaseEvent(clOutEvent);
}

HWTEST_F(TimestampPacketTests, givenEmptyWaitlistAndNoOutputEventWhenEnqueueingMarkerThenDoNothing) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;

    MockCommandQueueHw<FamilyType> cmdQ(context.get(), device.get(), nullptr);

    cmdQ.enqueueMarkerWithWaitList(0, nullptr, nullptr);
    EXPECT_EQ(0u, cmdQ.timestampPacketContainer->peekNodes().size());
    EXPECT_FALSE(csr.stallingPipeControlOnNextFlushRequired);
}

HWTEST_F(TimestampPacketTests, whenEnqueueingBarrierThenRequestPipeControlOnCsrFlush) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;

    EXPECT_FALSE(csr.stallingPipeControlOnNextFlushRequired);

    MockCommandQueueHw<FamilyType> cmdQ(context.get(), device.get(), nullptr);

    MockKernelWithInternals mockKernel(*device, context.get());
    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr); // obtain first TimestmapPacket

    TimestampPacketContainer cmdQNodes(device->getMemoryManager());
    cmdQNodes.assignAndIncrementNodesRefCounts(*cmdQ.timestampPacketContainer);

    cmdQ.enqueueBarrierWithWaitList(0, nullptr, nullptr);

    EXPECT_EQ(cmdQ.timestampPacketContainer->peekNodes().at(0), cmdQNodes.peekNodes().at(0)); // dont obtain new node
    EXPECT_EQ(1u, cmdQ.timestampPacketContainer->peekNodes().size());

    EXPECT_TRUE(csr.stallingPipeControlOnNextFlushRequired);
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteDisabledWhenEnqueueingBarrierThenDontRequestPipeControlOnCsrFlush) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = false;

    EXPECT_FALSE(csr.stallingPipeControlOnNextFlushRequired);

    MockCommandQueueHw<FamilyType> cmdQ(context.get(), device.get(), nullptr);

    cmdQ.enqueueBarrierWithWaitList(0, nullptr, nullptr);

    EXPECT_FALSE(csr.stallingPipeControlOnNextFlushRequired);
}

HWTEST_F(TimestampPacketTests, givenBlockedQueueWhenEnqueueingBarrierThenRequestPipeControlOnCsrFlush) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;
    EXPECT_FALSE(csr.stallingPipeControlOnNextFlushRequired);

    MockCommandQueueHw<FamilyType> cmdQ(context.get(), device.get(), nullptr);

    UserEvent userEvent;
    cl_event waitlist[] = {&userEvent};
    cmdQ.enqueueBarrierWithWaitList(1, waitlist, nullptr);
    EXPECT_TRUE(csr.stallingPipeControlOnNextFlushRequired);
}

HWTEST_F(TimestampPacketTests, givenPipeControlRequestWhenEstimatingCsrStreamSizeThenAddSizeForPipeControl) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    DispatchFlags flags;

    csr.stallingPipeControlOnNextFlushRequired = false;
    auto sizeWithoutPcRequest = device->getUltCommandStreamReceiver<FamilyType>().getRequiredCmdStreamSize(flags, *device.get());

    csr.stallingPipeControlOnNextFlushRequired = true;
    auto sizeWithPcRequest = device->getUltCommandStreamReceiver<FamilyType>().getRequiredCmdStreamSize(flags, *device.get());

    size_t extendedSize = sizeWithoutPcRequest + sizeof(typename FamilyType::PIPE_CONTROL);

    EXPECT_EQ(sizeWithPcRequest, extendedSize);
}

HWTEST_F(TimestampPacketTests, givenPipeControlRequestWhenFlushingThenProgramPipeControlAndResetRequestFlag) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.stallingPipeControlOnNextFlushRequired = true;
    csr.timestampPacketWriteEnabled = true;

    MockCommandQueueHw<FamilyType> cmdQ(context.get(), device.get(), nullptr);

    MockKernelWithInternals mockKernel(*device, context.get());
    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_FALSE(csr.stallingPipeControlOnNextFlushRequired);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr.commandStream, 0);
    auto secondEnqueueOffset = csr.commandStream.getUsed();

    auto pipeControl = genCmdCast<typename FamilyType::PIPE_CONTROL *>(*hwParser.cmdList.begin());
    EXPECT_NE(nullptr, pipeControl);
    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_NO_WRITE, pipeControl->getPostSyncOperation());
    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());

    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(secondEnqueueOffset, csr.commandStream.getUsed()); // nothing programmed when flag is not set
}
