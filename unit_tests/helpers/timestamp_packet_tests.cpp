/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/command_queue/hardware_interface.h"
#include "runtime/event/user_event.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/timestamp_packet.h"
#include "runtime/platform/platform.h"
#include "runtime/utilities/tag_allocator.h"
#include "test.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_mdi.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include "unit_tests/utilities/base_object_utils.h"

#include "gmock/gmock.h"

using namespace NEO;

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

        MockTimestampPacketContainer(TagAllocator<TimestampPacket> &tagAllocator, size_t numberOfPreallocatedTags) {
            for (size_t i = 0; i < numberOfPreallocatedTags; i++) {
                add(tagAllocator.getTag());
            }
        }

        TagNode<TimestampPacket> *getNode(size_t position) {
            return timestampPacketNodes.at(position);
        }
    };

    void setTagToReadyState(TagNode<TimestampPacket> *tagNode) {
        auto dataAddress = TimestampPacketHelper::getGpuAddressForDataWrite(*tagNode, TimestampPacket::DataIndex::ContextStart);
        auto atomicAddress = TimestampPacketHelper::getImplicitDependenciesCounGpuWriteAddress(*tagNode);
        memset(reinterpret_cast<void *>(dataAddress), 0, timestampDataSize);
        auto dependenciesCount = reinterpret_cast<std::atomic<uint32_t> *>(reinterpret_cast<void *>(atomicAddress));
        dependenciesCount->store(0);
    }

    const size_t timestampDataSize = sizeof(uint32_t) * static_cast<size_t>(TimestampPacket::DataIndex::Max);
    const size_t gws[3] = {1, 1, 1};
};

struct TimestampPacketTests : public TimestampPacketSimpleTests {
    void SetUp() override {
        executionEnvironment = platformImpl->peekExecutionEnvironment();
        device = std::unique_ptr<MockDevice>(Device::create<MockDevice>(nullptr, executionEnvironment, 0u));
        context = new MockContext(device.get());
        kernel = std::make_unique<MockKernelWithInternals>(*device, context);
        mockCmdQ = new MockCommandQueue(context, device.get(), nullptr);
    }

    void TearDown() override {
        mockCmdQ->release();
        context->release();
    }

    template <typename MI_SEMAPHORE_WAIT>
    void verifySemaphore(MI_SEMAPHORE_WAIT *semaphoreCmd, TagNode<TimestampPacket> *timestampPacketNode) {
        EXPECT_NE(nullptr, semaphoreCmd);
        EXPECT_EQ(semaphoreCmd->getCompareOperation(), MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
        EXPECT_EQ(1u, semaphoreCmd->getSemaphoreDataDword());

        auto dataAddress = TimestampPacketHelper::getGpuAddressForDataWrite(*timestampPacketNode, TimestampPacket::DataIndex::ContextEnd);

        EXPECT_EQ(dataAddress, semaphoreCmd->getSemaphoreGraphicsAddress());
    };

    template <typename MI_ATOMIC>
    void verifyMiAtomic(MI_ATOMIC *miAtomicCmd, TagNode<TimestampPacket> *timestampPacketNode) {
        EXPECT_NE(nullptr, miAtomicCmd);
        auto writeAddress = TimestampPacketHelper::getImplicitDependenciesCounGpuWriteAddress(*timestampPacketNode);

        EXPECT_EQ(MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_4B_DECREMENT, miAtomicCmd->getAtomicOpcode());
        EXPECT_EQ(static_cast<uint32_t>(writeAddress & 0x0000FFFFFFFFULL), miAtomicCmd->getMemoryAddress());
        EXPECT_EQ(static_cast<uint32_t>(writeAddress >> 32), miAtomicCmd->getMemoryAddressHigh());
    };

    void verifyDependencyCounterValues(TimestampPacketContainer *timestampPacketContainer, uint32_t expectedValue) {
        auto &nodes = timestampPacketContainer->peekNodes();
        EXPECT_NE(0u, nodes.size());
        for (auto &node : nodes) {
            auto atomicAddress = TimestampPacketHelper::getImplicitDependenciesCounGpuWriteAddress(*node);
            auto dependenciesCount = reinterpret_cast<std::atomic<uint32_t> *>(reinterpret_cast<void *>(atomicAddress));
            EXPECT_EQ(expectedValue, dependenciesCount->load());
        }
    }

    ExecutionEnvironment *executionEnvironment;
    std::unique_ptr<MockDevice> device;
    MockContext *context;
    std::unique_ptr<MockKernelWithInternals> kernel;
    MockCommandQueue *mockCmdQ;
};

HWTEST_F(TimestampPacketTests, givenTagNodeWhenSemaphoreAndAtomicAreProgrammedThenUseGpuAddress) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;

    struct MockTagNode : public TagNode<TimestampPacket> {
        using TagNode<TimestampPacket>::gpuAddress;
    };

    TimestampPacket tag;
    MockTagNode mockNode;
    mockNode.tagForCpuAccess = &tag;
    mockNode.gpuAddress = 0x1230000;

    auto &cmdStream = mockCmdQ->getCS(0);

    TimestampPacketHelper::programSemaphoreWithImplicitDependency<FamilyType>(cmdStream, mockNode);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);
    auto it = hwParser.cmdList.begin();
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), &mockNode);
    verifyMiAtomic(genCmdCast<MI_ATOMIC *>(*it++), &mockNode);
}

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
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockMemoryManager memoryManager(executionEnvironment);
    MockTagAllocator<TimestampPacket> allocator(&memoryManager, 1);

    auto firstNode = allocator.getTag();
    for (uint32_t i = 0; i < static_cast<uint32_t>(TimestampPacket::DataIndex::Max) * TimestampPacketSizeControl::preferedChunkCount; i++) {
        auto dataAccess = reinterpret_cast<uint32_t *>(ptrOffset(firstNode->tagForCpuAccess, i * sizeof(uint32_t)));
        *dataAccess = i;
    }

    auto atomicAddress = TimestampPacketHelper::getImplicitDependenciesCounGpuWriteAddress(*firstNode);
    auto dependenciesCount = reinterpret_cast<std::atomic<uint32_t> *>(reinterpret_cast<void *>(atomicAddress));

    setTagToReadyState(firstNode);
    allocator.returnTag(firstNode);
    (*dependenciesCount)++;

    auto secondNode = allocator.getTag();
    EXPECT_EQ(secondNode, firstNode);

    EXPECT_EQ(0u, dependenciesCount->load());
    for (uint32_t i = 0; i < static_cast<uint32_t>(TimestampPacket::DataIndex::Max) * TimestampPacketSizeControl::preferedChunkCount; i++) {
        auto dataAccess = reinterpret_cast<uint32_t *>(ptrOffset(firstNode->tagForCpuAccess, i * sizeof(uint32_t)));
        EXPECT_EQ(1u, *dataAccess);
    }
}

TEST_F(TimestampPacketSimpleTests, whenObjectIsCreatedThenInitializeAllStamps) {
    MockTimestampPacket timestampPacket;
    auto entityElements = static_cast<uint32_t>(TimestampPacket::DataIndex::Max);
    auto allElements = entityElements * TimestampPacketSizeControl::preferedChunkCount;
    EXPECT_EQ(4u, entityElements);
    EXPECT_EQ(64u, allElements);

    EXPECT_EQ(allElements, timestampPacket.data.size());

    for (uint32_t i = 0; i < allElements; i++) {
        EXPECT_EQ(1u, timestampPacket.data[i]);
    }
}

TEST_F(TimestampPacketSimpleTests, whenAskedForStampAddressThenReturnWithValidOffset) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockMemoryManager memoryManager(executionEnvironment);
    MockTagAllocator<TimestampPacket> allocator(&memoryManager, 1);

    auto node = allocator.getTag();
    auto tag = node->tagForCpuAccess;

    for (size_t i = 0; i < static_cast<uint32_t>(TimestampPacket::DataIndex::Max); i++) {
        auto dataIndex = static_cast<TimestampPacket::DataIndex>(i);
        auto address = TimestampPacketHelper::getGpuAddressForDataWrite(*node, dataIndex);

        EXPECT_EQ(address, reinterpret_cast<uint64_t>(ptrOffset(tag, i * sizeof(uint32_t))));
    }
}

HWTEST_F(TimestampPacketTests, givenCommandStreamReceiverHwWhenObtainingPreferredTagPoolSizeThenReturnCorrectValue) {
    CommandStreamReceiverHw<FamilyType> csr(*executionEnvironment);
    EXPECT_EQ(512u, csr.getPreferredTagPoolSize());
}

HWCMDTEST_F(IGFX_GEN8_CORE, TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEstimatingStreamSizeThenAddPipeControl) {
    MockKernelWithInternals kernel2(*device);
    MockMultiDispatchInfo multiDispatchInfo(std::vector<Kernel *>({kernel->mockKernel, kernel2.mockKernel}));

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = false;
    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*mockCmdQ, CsrDependencies(), false, false, multiDispatchInfo, nullptr, 0);
    auto sizeWithDisabled = mockCmdQ->requestedCmdStreamSize;

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*mockCmdQ, CsrDependencies(), false, false, multiDispatchInfo, nullptr, 0);
    auto sizeWithEnabled = mockCmdQ->requestedCmdStreamSize;

    auto extendedSize = sizeWithDisabled + sizeof(typename FamilyType::PIPE_CONTROL);
    EXPECT_EQ(sizeWithEnabled, extendedSize);
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledAndOoqWhenEstimatingStreamSizeDontDontAddAdditionalSize) {
    MockMultiDispatchInfo multiDispatchInfo(std::vector<Kernel *>({kernel->mockKernel}));
    mockCmdQ->setOoqEnabled();

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = false;
    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*mockCmdQ, CsrDependencies(), false, false, multiDispatchInfo, nullptr, 0);
    auto sizeWithDisabled = mockCmdQ->requestedCmdStreamSize;

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    MockTimestampPacketContainer timestamp1(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp2(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 2);
    MockTimestampPacketContainer timestamp3(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 3);
    MockTimestampPacketContainer timestamp4(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 4);
    MockTimestampPacketContainer timestamp5(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 5);

    Event event1(mockCmdQ, 0, 0, 0);
    event1.addTimestampPacketNodes(timestamp1);
    Event event2(mockCmdQ, 0, 0, 0);
    event2.addTimestampPacketNodes(timestamp2);
    Event event3(mockCmdQ, 0, 0, 0);
    event3.addTimestampPacketNodes(timestamp3);
    Event event4(mockCmdQ, 0, 0, 0);
    event4.addTimestampPacketNodes(timestamp4);
    Event event5(mockCmdQ, 0, 0, 0);
    event5.addTimestampPacketNodes(timestamp5);

    const cl_uint numEventsOnWaitlist = 5;
    cl_event waitlist[] = {&event1, &event2, &event3, &event4, &event5};

    EventsRequest eventsRequest(numEventsOnWaitlist, waitlist, nullptr);
    CsrDependencies csrDeps;
    csrDeps.fillFromEventsRequestAndMakeResident(eventsRequest, device->getCommandStreamReceiver(), CsrDependencies::DependenciesType::OnCsr);

    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*mockCmdQ, csrDeps, false, false, multiDispatchInfo, nullptr, 0);
    auto sizeWithEnabled = mockCmdQ->requestedCmdStreamSize;

    size_t extendedSize = sizeWithDisabled + EnqueueOperation<FamilyType>::getSizeRequiredForTimestampPacketWrite() +
                          ((1 + 2 + 3 + 4 + 5) * (sizeof(typename FamilyType::MI_SEMAPHORE_WAIT) + sizeof(typename FamilyType::MI_ATOMIC)));

    EXPECT_EQ(sizeWithEnabled, extendedSize);
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEstimatingStreamSizeWithWaitlistThenAddSizeForSemaphores) {
    MockKernelWithInternals kernel2(*device);
    MockMultiDispatchInfo multiDispatchInfo(std::vector<Kernel *>({kernel->mockKernel, kernel2.mockKernel}));

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = false;
    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*mockCmdQ, CsrDependencies(), false, false, multiDispatchInfo, nullptr, 0);
    auto sizeWithDisabled = mockCmdQ->requestedCmdStreamSize;

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    MockTimestampPacketContainer timestamp1(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp2(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 2);
    MockTimestampPacketContainer timestamp3(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 3);
    MockTimestampPacketContainer timestamp4(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 4);
    MockTimestampPacketContainer timestamp5(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 5);

    Event event1(mockCmdQ, 0, 0, 0);
    event1.addTimestampPacketNodes(timestamp1);
    Event event2(mockCmdQ, 0, 0, 0);
    event2.addTimestampPacketNodes(timestamp2);
    Event event3(mockCmdQ, 0, 0, 0);
    event3.addTimestampPacketNodes(timestamp3);
    Event event4(mockCmdQ, 0, 0, 0);
    event4.addTimestampPacketNodes(timestamp4);
    Event event5(mockCmdQ, 0, 0, 0);
    event5.addTimestampPacketNodes(timestamp5);

    const cl_uint numEventsOnWaitlist = 5;
    cl_event waitlist[] = {&event1, &event2, &event3, &event4, &event5};

    EventsRequest eventsRequest(numEventsOnWaitlist, waitlist, nullptr);
    CsrDependencies csrDeps;
    csrDeps.fillFromEventsRequestAndMakeResident(eventsRequest, device->getCommandStreamReceiver(), CsrDependencies::DependenciesType::OnCsr);

    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*mockCmdQ, csrDeps, false, false, multiDispatchInfo, nullptr, 0);
    auto sizeWithEnabled = mockCmdQ->requestedCmdStreamSize;

    size_t extendedSize = sizeWithDisabled + EnqueueOperation<FamilyType>::getSizeRequiredForTimestampPacketWrite() +
                          ((1 + 2 + 3 + 4 + 5) * (sizeof(typename FamilyType::MI_SEMAPHORE_WAIT) + sizeof(typename FamilyType::MI_ATOMIC)));

    EXPECT_EQ(sizeWithEnabled, extendedSize);
}

HWTEST_F(TimestampPacketTests, givenEventsRequestWithEventsWithoutTimestampsWhenComputeCsrDepsThanDoNotAddthemToCsrDeps) {
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
    csrDepsEmpty.fillFromEventsRequestAndMakeResident(eventsRequest, device->getCommandStreamReceiver(), CsrDependencies::DependenciesType::OnCsr);
    EXPECT_EQ(0u, csrDepsEmpty.size());

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    MockTimestampPacketContainer timestamp1(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp2(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 2);
    MockTimestampPacketContainer timestamp3(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 3);
    MockTimestampPacketContainer timestamp4(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 4);
    MockTimestampPacketContainer timestamp5(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 5);

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
    csrDepsSize3.fillFromEventsRequestAndMakeResident(eventsRequest2, device->getCommandStreamReceiver(), CsrDependencies::DependenciesType::OnCsr);

    EXPECT_EQ(3u, csrDepsSize3.size());

    size_t expectedSize = (1 + 3 + 5) * (sizeof(typename FamilyType::MI_SEMAPHORE_WAIT) + sizeof(typename FamilyType::MI_ATOMIC));
    EXPECT_EQ(expectedSize, TimestampPacketHelper::getRequiredCmdStreamSize<FamilyType>(csrDepsSize3));
}

HWCMDTEST_F(IGFX_GEN8_CORE, TimestampPacketTests, givenTimestampPacketWhenDispatchingGpuWalkerThenAddTwoPcForLastWalker) {
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    MockTimestampPacketContainer timestampPacket(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 2);

    MockKernelWithInternals kernel2(*device);

    MockMultiDispatchInfo multiDispatchInfo(std::vector<Kernel *>({kernel->mockKernel, kernel2.mockKernel}));

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
            auto dataAddress = TimestampPacketHelper::getGpuAddressForDataWrite(*timestampPacket.getNode(walkersFound), TimestampPacket::DataIndex::ContextEnd);

            verifyPipeControl(pipeControl, dataAddress);
            walkersFound++;
        }
    }
    EXPECT_EQ(2u, walkersFound);
}

HWCMDTEST_F(IGFX_GEN8_CORE, TimestampPacketTests, givenTimestampPacketDisabledWhenDispatchingGpuWalkerThenDontAddPipeControls) {
    MockTimestampPacketContainer timestampPacket(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockMultiDispatchInfo multiDispatchInfo(kernel->mockKernel);
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
        device->getPreemptionMode(),
        false);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    auto cmdItor = find<typename FamilyType::PIPE_CONTROL *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_EQ(hwParser.cmdList.end(), cmdItor);
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEnqueueingThenObtainNewStampAndPassToEvent) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;

    auto mockTagAllocator = new MockTagAllocator<>(executionEnvironment->memoryManager.get());
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
    setTagToReadyState(node1);
    setTagToReadyState(node2);

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
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);

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
    auto device2 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(nullptr, executionEnvironment, 1u));
    MockContext context2(device2.get());
    auto cmdQ2 = std::make_unique<MockCommandQueueHw<FamilyType>>(&context2, device2.get(), nullptr);

    MockTimestampPacketContainer timestamp1(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp2(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 2);
    MockTimestampPacketContainer timestamp3(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 3);
    MockTimestampPacketContainer timestamp4(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 4);
    MockTimestampPacketContainer timestamp5(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 5);

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
    DispatchFlags flags;

    auto sizeWithoutEvents = csr.getRequiredCmdStreamSize(flags, *device);

    flags.csrDependencies.fillFromEventsRequestAndMakeResident(eventsRequest, csr, NEO::CsrDependencies::DependenciesType::OutOfCsr);
    auto sizeWithEvents = csr.getRequiredCmdStreamSize(flags, *device);

    size_t extendedSize = sizeWithoutEvents + ((1 + 2 + 3 + 4 + 5) * (sizeof(typename FamilyType::MI_SEMAPHORE_WAIT) + sizeof(typename FamilyType::MI_ATOMIC)));

    EXPECT_EQ(sizeWithEvents, extendedSize);
}

HWTEST_F(TimestampPacketTests, givenEventsRequestWhenEstimatingStreamSizeForDifferentCsrFromSameDeviceThenAddSizeForSemaphores) {
    // Create second (LOW_PRIORITY) queue on the same device
    cl_queue_properties props[] = {CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_LOW_KHR, 0};
    auto cmdQ2 = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), props);

    MockTimestampPacketContainer timestamp1(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp2(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 2);
    MockTimestampPacketContainer timestamp3(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 3);
    MockTimestampPacketContainer timestamp4(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 4);
    MockTimestampPacketContainer timestamp5(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 5);

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
    DispatchFlags flags;

    auto sizeWithoutEvents = csr.getRequiredCmdStreamSize(flags, *device.get());

    flags.csrDependencies.fillFromEventsRequestAndMakeResident(eventsRequest, csr, NEO::CsrDependencies::DependenciesType::OutOfCsr);
    auto sizeWithEvents = csr.getRequiredCmdStreamSize(flags, *device.get());

    size_t extendedSize = sizeWithoutEvents + ((1 + 2 + 3 + 4 + 5) * (sizeof(typename FamilyType::MI_SEMAPHORE_WAIT) + sizeof(typename FamilyType::MI_ATOMIC)));

    EXPECT_EQ(sizeWithEvents, extendedSize);
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEnqueueingThenProgramSemaphoresOnCsrStream) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;

    auto device2 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(nullptr, executionEnvironment, 1u));

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    device2->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    MockContext context2(device2.get());

    auto cmdQ1 = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);
    auto cmdQ2 = std::make_unique<MockCommandQueueHw<FamilyType>>(&context2, device2.get(), nullptr);

    const cl_uint eventsOnWaitlist = 6;
    MockTimestampPacketContainer timestamp3(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp4(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp5(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp6(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 2);

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
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), timestamp4.getNode(0));
    verifyMiAtomic(genCmdCast<MI_ATOMIC *>(*it++), timestamp4.getNode(0));
    verifyDependencyCounterValues(event4.getTimestampPacketNodes(), 1);
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), timestamp6.getNode(0));
    verifyMiAtomic(genCmdCast<MI_ATOMIC *>(*it++), timestamp6.getNode(0));
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), timestamp6.getNode(1));
    verifyMiAtomic(genCmdCast<MI_ATOMIC *>(*it++), timestamp6.getNode(1));
    verifyDependencyCounterValues(event6.getTimestampPacketNodes(), 1);

    while (it != hwParser.cmdList.end()) {
        EXPECT_EQ(nullptr, genCmdCast<MI_SEMAPHORE_WAIT *>(*it));
        it++;
    }
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledOnDifferentCSRsFromOneDeviceWhenEnqueueingThenProgramSemaphoresOnCsrStream) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    auto cmdQ1 = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);

    // Create second (LOW_PRIORITY) queue on the same device
    cl_queue_properties props[] = {CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_LOW_KHR, 0};
    auto cmdQ2 = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), props);
    cmdQ2->getUltCommandStreamReceiver().timestampPacketWriteEnabled = true;

    const cl_uint eventsOnWaitlist = 6;
    MockTimestampPacketContainer timestamp3(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp4(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp5(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp6(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 2);

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
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), timestamp4.getNode(0));
    verifyMiAtomic(genCmdCast<MI_ATOMIC *>(*it++), timestamp4.getNode(0));
    verifyDependencyCounterValues(event4.getTimestampPacketNodes(), 1);
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), timestamp6.getNode(0));
    verifyMiAtomic(genCmdCast<MI_ATOMIC *>(*it++), timestamp6.getNode(0));
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), timestamp6.getNode(1));
    verifyMiAtomic(genCmdCast<MI_ATOMIC *>(*it++), timestamp6.getNode(1));
    verifyDependencyCounterValues(event6.getTimestampPacketNodes(), 1);

    while (it != hwParser.cmdList.end()) {
        EXPECT_EQ(nullptr, genCmdCast<MI_SEMAPHORE_WAIT *>(*it));
        it++;
    }
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenEnqueueingBlockedThenProgramSemaphoresOnCsrStreamOnFlush) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto device2 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(nullptr, executionEnvironment, 1u));

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    device2->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    auto context2 = new MockContext(device2.get());

    auto cmdQ1 = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));
    auto cmdQ2 = new MockCommandQueueHw<FamilyType>(context2, device2.get(), nullptr);

    MockTimestampPacketContainer timestamp0(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp1(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 1);

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
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), timestamp1.getNode(0));
    verifyMiAtomic(genCmdCast<typename FamilyType::MI_ATOMIC *>(*it++), timestamp1.getNode(0));
    verifyDependencyCounterValues(event1.getTimestampPacketNodes(), 1);

    while (it != hwParser.cmdList.end()) {
        EXPECT_EQ(nullptr, genCmdCast<MI_SEMAPHORE_WAIT *>(*it));
        it++;
    }

    cmdQ2->release();
    context2->release();
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledOnDifferentCSRsFromOneDeviceWhenEnqueueingBlockedThenProgramSemaphoresOnCsrStreamOnFlush) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto device2 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(nullptr, executionEnvironment, 1u));

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    auto cmdQ1 = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));

    // Create second (LOW_PRIORITY) queue on the same device
    cl_queue_properties props[] = {CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_LOW_KHR, 0};
    auto cmdQ2 = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), props));
    cmdQ2->getUltCommandStreamReceiver().timestampPacketWriteEnabled = true;

    MockTimestampPacketContainer timestamp0(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp1(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 1);

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
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), timestamp1.getNode(0));
    verifyMiAtomic(genCmdCast<typename FamilyType::MI_ATOMIC *>(*it++), timestamp1.getNode(0));
    verifyDependencyCounterValues(event1.getTimestampPacketNodes(), 1);

    while (it != hwParser.cmdList.end()) {
        EXPECT_EQ(nullptr, genCmdCast<MI_SEMAPHORE_WAIT *>(*it));
        it++;
    }

    cmdQ2->isQueueBlocked();
    cmdQ1->isQueueBlocked();
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledWhenDispatchingThenProgramSemaphoresForWaitlist) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using WALKER = WALKER_TYPE<FamilyType>;

    auto device2 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(nullptr, executionEnvironment, 1u));
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    device2->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    MockContext context2(device2.get());

    MockMultiDispatchInfo multiDispatchInfo(std::vector<Kernel *>({kernel->mockKernel}));

    MockCommandQueue mockCmdQ2(&context2, device2.get(), nullptr);
    auto &cmdStream = mockCmdQ->getCS(0);

    const cl_uint eventsOnWaitlist = 6;
    MockTimestampPacketContainer timestamp3(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp4(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp5(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 2);
    MockTimestampPacketContainer timestamp6(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp7(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 1);

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
    csrDeps.fillFromEventsRequestAndMakeResident(eventsRequest, mockCmdQ->getCommandStreamReceiver(), CsrDependencies::DependenciesType::OnCsr);

    HardwareInterface<FamilyType>::dispatchWalker(
        *mockCmdQ,
        multiDispatchInfo,
        csrDeps,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &timestamp7,
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
                verifySemaphore(semaphoreCmd, timestamp3.getNode(0));
                verifyMiAtomic(genCmdCast<typename FamilyType::MI_ATOMIC *>(*++it), timestamp3.getNode(0));
                verifyDependencyCounterValues(event3.getTimestampPacketNodes(), 1);
            } else if (semaphoresFound == 2) {
                verifySemaphore(semaphoreCmd, timestamp5.getNode(0));
                verifyMiAtomic(genCmdCast<typename FamilyType::MI_ATOMIC *>(*++it), timestamp5.getNode(0));
                verifyDependencyCounterValues(event5.getTimestampPacketNodes(), 1);
            } else if (semaphoresFound == 3) {
                verifySemaphore(semaphoreCmd, timestamp5.getNode(1));
                verifyMiAtomic(genCmdCast<typename FamilyType::MI_ATOMIC *>(*++it), timestamp5.getNode(1));
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

HWTEST_F(TimestampPacketTests, givenTimestampPacketWriteEnabledOnDifferentCSRsFromOneDeviceWhenDispatchingThenProgramSemaphoresForWaitlist) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using WALKER = WALKER_TYPE<FamilyType>;

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    MockMultiDispatchInfo multiDispatchInfo(std::vector<Kernel *>({kernel->mockKernel}));

    // Create second (LOW_PRIORITY) queue on the same device
    cl_queue_properties props[] = {CL_QUEUE_PRIORITY_KHR, CL_QUEUE_PRIORITY_LOW_KHR, 0};
    auto mockCmdQ2 = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), props);
    mockCmdQ2->getUltCommandStreamReceiver().timestampPacketWriteEnabled = true;

    auto &cmdStream = mockCmdQ->getCS(0);

    const cl_uint eventsOnWaitlist = 6;
    MockTimestampPacketContainer timestamp3(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp4(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp5(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 2);
    MockTimestampPacketContainer timestamp6(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp7(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 1);

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
    csrDeps.fillFromEventsRequestAndMakeResident(eventsRequest, mockCmdQ->getCommandStreamReceiver(), CsrDependencies::DependenciesType::OnCsr);

    HardwareInterface<FamilyType>::dispatchWalker(
        *mockCmdQ,
        multiDispatchInfo,
        csrDeps,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &timestamp7,
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
                verifySemaphore(semaphoreCmd, timestamp3.getNode(0));
                verifyMiAtomic(genCmdCast<typename FamilyType::MI_ATOMIC *>(*++it), timestamp3.getNode(0));
                verifyDependencyCounterValues(event3.getTimestampPacketNodes(), 1);
            } else if (semaphoresFound == 2) {
                verifySemaphore(semaphoreCmd, timestamp5.getNode(0));
                verifyMiAtomic(genCmdCast<typename FamilyType::MI_ATOMIC *>(*++it), timestamp5.getNode(0));
                verifyDependencyCounterValues(event5.getTimestampPacketNodes(), 1);
            } else if (semaphoresFound == 3) {
                verifySemaphore(semaphoreCmd, timestamp5.getNode(1));
                verifyMiAtomic(genCmdCast<typename FamilyType::MI_ATOMIC *>(*++it), timestamp5.getNode(1));
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
    auto mockTagAllocator = new MockTagAllocator<>(executionEnvironment->memoryManager.get(), 1);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketAllocator.reset(mockTagAllocator);
    csr.timestampPacketWriteEnabled = true;

    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);
    TimestampPacketContainer previousNodes;
    cmdQ->obtainNewTimestampPacketNodes(1, previousNodes);
    auto firstNode = cmdQ->timestampPacketContainer->peekNodes().at(0);

    csr.storeMakeResidentAllocations = true;
    csr.timestampPacketWriteEnabled = true;

    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    auto secondNode = cmdQ->timestampPacketContainer->peekNodes().at(0);

    EXPECT_NE(firstNode->getBaseGraphicsAllocation(), secondNode->getBaseGraphicsAllocation());
    EXPECT_TRUE(csr.isMadeResident(firstNode->getBaseGraphicsAllocation()));
}

HWTEST_F(TimestampPacketTests, givenAlreadyAssignedNodeWhenEnqueueingBlockedThenMakeItResident) {
    auto mockTagAllocator = new MockTagAllocator<>(executionEnvironment->memoryManager.get(), 1);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketAllocator.reset(mockTagAllocator);
    csr.timestampPacketWriteEnabled = true;

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));
    TimestampPacketContainer previousNodes;
    cmdQ->obtainNewTimestampPacketNodes(1, previousNodes);
    auto firstNode = cmdQ->timestampPacketContainer->peekNodes().at(0);

    csr.storeMakeResidentAllocations = true;
    csr.timestampPacketWriteEnabled = true;

    UserEvent userEvent;
    cl_event clEvent = &userEvent;
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 1, &clEvent, nullptr);
    auto secondNode = cmdQ->timestampPacketContainer->peekNodes().at(0);

    EXPECT_NE(firstNode->getBaseGraphicsAllocation(), secondNode->getBaseGraphicsAllocation());
    EXPECT_FALSE(csr.isMadeResident(firstNode->getBaseGraphicsAllocation()));
    userEvent.setStatus(CL_COMPLETE);
    EXPECT_TRUE(csr.isMadeResident(firstNode->getBaseGraphicsAllocation()));
    cmdQ->isQueueBlocked();
}

HWTEST_F(TimestampPacketTests, givenAlreadyAssignedNodeWhenEnqueueingThenDontKeepDependencyOnPreviousNodeIfItsReady) {
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    MockCommandQueueHw<FamilyType> cmdQ(context, device.get(), nullptr);
    TimestampPacketContainer previousNodes;
    cmdQ.obtainNewTimestampPacketNodes(1, previousNodes);
    auto firstNode = cmdQ.timestampPacketContainer->peekNodes().at(0);
    setTagToReadyState(firstNode);

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
    MockTimestampPacketContainer firstNode(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 0);

    MockCommandQueueHw<FamilyType> cmdQ(context, device.get(), nullptr);
    TimestampPacketContainer previousNodes;
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
    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*it), firstTag0);
    verifyMiAtomic(genCmdCast<MI_ATOMIC *>(*++it), firstTag0);

    verifySemaphore(genCmdCast<MI_SEMAPHORE_WAIT *>(*++it), firstTag1);
    verifyMiAtomic(genCmdCast<MI_ATOMIC *>(*++it), firstTag1);

    while (it != hwParser.cmdList.end()) {
        EXPECT_EQ(nullptr, genCmdCast<MI_SEMAPHORE_WAIT *>(*it));
        it++;
    }
}

HWTEST_F(TimestampPacketTests, givenAlreadyAssignedNodeWhenEnqueueingToOoqThenDontKeepDependencyOnPreviousNodeIfItsNotReady) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    cl_queue_properties properties[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    MockCommandQueueHw<FamilyType> cmdQ(context, device.get(), properties);
    TimestampPacketContainer previousNodes;
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
    TagAllocator<TimestampPacket> tagAllocator(executionEnvironment->memoryManager.get(), 1, 1);
    auto device2 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(nullptr, executionEnvironment, 1u));

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
    EXPECT_TRUE(ultCsr.isMadeResident(tagNode1->getBaseGraphicsAllocation()));
    EXPECT_TRUE(ultCsr.isMadeResident(tagNode2->getBaseGraphicsAllocation()));
}

HWTEST_F(TimestampPacketTests, givenEventsWaitlistFromDifferentCSRsWhenEnqueueingThenMakeAllTimestampsResident) {
    TagAllocator<TimestampPacket> tagAllocator(executionEnvironment->memoryManager.get(), 1, 1);

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
    EXPECT_TRUE(ultCsr.isMadeResident(tagNode1->getBaseGraphicsAllocation()));
    EXPECT_TRUE(ultCsr.isMadeResident(tagNode2->getBaseGraphicsAllocation()));
}

HWTEST_F(TimestampPacketTests, givenTimestampPacketWhenEnqueueingNonBlockedThenMakeItResident) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;
    csr.storeMakeResidentAllocations = true;

    MockKernelWithInternals mockKernel(*device, context);
    MockCommandQueueHw<FamilyType> cmdQ(context, device.get(), nullptr);

    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    auto timestampPacketNode = cmdQ.timestampPacketContainer->peekNodes().at(0);

    EXPECT_TRUE(csr.isMadeResident(timestampPacketNode->getBaseGraphicsAllocation()));
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

    EXPECT_FALSE(csr.isMadeResident(timestampPacketNode->getBaseGraphicsAllocation()));
    userEvent.setStatus(CL_COMPLETE);
    EXPECT_TRUE(csr.isMadeResident(timestampPacketNode->getBaseGraphicsAllocation()));
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

    mockCmdQ->timestampPacketContainer = std::make_unique<MockTimestampPacketContainer>(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 0);
    EXPECT_EQ(0u, mockCmdQ->timestampPacketContainer->peekNodes().size());

    TimestampPacketContainer previousNodes;
    mockCmdQ->obtainNewTimestampPacketNodes(dispatchSize, previousNodes);
    EXPECT_EQ(dispatchSize, mockCmdQ->timestampPacketContainer->peekNodes().size());
}

HWTEST_F(TimestampPacketTests, givenWaitlistAndOutputEventWhenEnqueueingWithoutKernelThenInheritTimestampPacketsWithoutSubmitting) {
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));

    MockKernelWithInternals mockKernel(*device, context);
    cmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr); // obtain first TimestampPacket

    TimestampPacketContainer cmdQNodes;
    cmdQNodes.assignAndIncrementNodesRefCounts(*cmdQ->timestampPacketContainer);

    MockTimestampPacketContainer node1(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer node2(*device->getCommandStreamReceiver().getTimestampPacketAllocator(), 1);

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

HWTEST_F(TimestampPacketTests, givenEmptyWaitlistAndNoOutputEventWhenEnqueueingMarkerThenDoNothing) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;

    auto cmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, device.get(), nullptr));

    cmdQ->enqueueMarkerWithWaitList(0, nullptr, nullptr);
    EXPECT_EQ(0u, cmdQ->timestampPacketContainer->peekNodes().size());
    EXPECT_FALSE(csr.stallingPipeControlOnNextFlushRequired);
}

HWTEST_F(TimestampPacketTests, whenEnqueueingBarrierThenRequestPipeControlOnCsrFlush) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;

    EXPECT_FALSE(csr.stallingPipeControlOnNextFlushRequired);

    MockCommandQueueHw<FamilyType> cmdQ(context, device.get(), nullptr);

    MockKernelWithInternals mockKernel(*device, context);
    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr); // obtain first TimestampPacket

    TimestampPacketContainer cmdQNodes;
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

    MockCommandQueueHw<FamilyType> cmdQ(context, device.get(), nullptr);

    cmdQ.enqueueBarrierWithWaitList(0, nullptr, nullptr);

    EXPECT_FALSE(csr.stallingPipeControlOnNextFlushRequired);
}

HWTEST_F(TimestampPacketTests, givenBlockedQueueWhenEnqueueingBarrierThenRequestPipeControlOnCsrFlush) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;
    EXPECT_FALSE(csr.stallingPipeControlOnNextFlushRequired);

    MockCommandQueueHw<FamilyType> cmdQ(context, device.get(), nullptr);

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

    MockCommandQueueHw<FamilyType> cmdQ(context, device.get(), nullptr);

    MockKernelWithInternals mockKernel(*device, context);
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

HWTEST_F(TimestampPacketTests, givenKernelWhichDoesntRequiersFlushWhenEnquingKernelThenOneNodeCreated) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(false);
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;

    auto mockTagAllocator = new MockTagAllocator<>(executionEnvironment->memoryManager.get());
    csr.timestampPacketAllocator.reset(mockTagAllocator);
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);
    // obtain first node for cmdQ and event1
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    auto size = cmdQ->timestampPacketContainer->peekNodes().size();
    EXPECT_EQ(size, 1u);
}

HWTEST_F(TimestampPacketTests, givenKernelWhichRequiersFlushWhenEnquingKernelThenTwoNodesCreated) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(true);
    DebugManager.flags.EnableCacheFlushAfterWalkerForAllQueues.set(true);

    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;

    auto mockTagAllocator = new MockTagAllocator<>(executionEnvironment->memoryManager.get());
    csr.timestampPacketAllocator.reset(mockTagAllocator);
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, device.get(), nullptr);
    kernel->mockKernel->svmAllocationsRequireCacheFlush = true;
    // obtain first node for cmdQ and event1
    cmdQ->enqueueKernel(kernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    auto node1 = cmdQ->timestampPacketContainer->peekNodes().at(0);
    auto node2 = cmdQ->timestampPacketContainer->peekNodes().at(1);
    auto size = cmdQ->timestampPacketContainer->peekNodes().size();
    EXPECT_EQ(size, 2u);
    EXPECT_NE(nullptr, node1);
    EXPECT_NE(nullptr, node2);
    EXPECT_NE(node1, node2);
}
