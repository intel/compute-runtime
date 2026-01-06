/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/mocks/mock_timestamp_packet.h"
#include "shared/test/common/test_macros/hw_test.h"

#include <memory>

using namespace NEO;

namespace {
template <typename FamilyType>
void setTagToReadyState(TagNodeBase *tagNode) {
    auto packetsUsed = tagNode->getPacketsUsed();
    tagNode->initialize();

    typename FamilyType::TimestampPacketType zeros[4] = {};

    for (uint32_t i = 0; i < FamilyType::timestampPacketCount; i++) {
        tagNode->assignDataToAllTimestamps(i, zeros);
    }
    tagNode->setPacketsUsed(packetsUsed);
}
} // namespace

struct TimestampPacketTests : public ::testing::Test {
    struct MockTagNode : public TagNode<TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount>> {
        using TagNode<TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount>>::gpuAddress;
    };
    template <typename FamilyType>
    void verifySemaphore(typename FamilyType::MI_SEMAPHORE_WAIT *semaphoreCmd, TagNodeBase *timestampPacketNode, uint32_t packetId) {
        using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
        EXPECT_NE(nullptr, semaphoreCmd);
        EXPECT_EQ(semaphoreCmd->getCompareOperation(), MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
        EXPECT_EQ(1u, semaphoreCmd->getSemaphoreDataDword());

        uint64_t compareOffset = packetId * TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount>::getSinglePacketSize();
        auto dataAddress = TimestampPacketHelper::getContextEndGpuAddress(*timestampPacketNode) + compareOffset;

        EXPECT_EQ(dataAddress, semaphoreCmd->getSemaphoreGraphicsAddress());
    };
};

HWTEST_F(TimestampPacketTests, givenTagNodeWhenSemaphoreIsProgrammedThenUseGpuAddress) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount> tag;
    MockTagNode mockNode;
    mockNode.tagForCpuAccess = &tag;
    mockNode.gpuAddress = 0x1230000;

    StackVec<char, 4096> buffer(4096);
    LinearStream cmdStream(buffer.begin(), buffer.size());

    TimestampPacketHelper::programSemaphore<FamilyType>(cmdStream, mockNode);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);
    auto it = hwParser.cmdList.begin();
    verifySemaphore<FamilyType>(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), &mockNode, 0);
}

HWTEST_F(TimestampPacketTests, givenTagNodeWithPacketsUsed2WhenSemaphoreIsProgrammedThenUseGpuAddress) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount> tag;
    MockTagNode mockNode;
    mockNode.tagForCpuAccess = &tag;
    mockNode.gpuAddress = 0x1230000;
    mockNode.setPacketsUsed(2);

    StackVec<char, 4096> buffer(4096);
    LinearStream cmdStream(buffer.begin(), buffer.size());

    TimestampPacketHelper::programSemaphore<FamilyType>(cmdStream, mockNode);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);
    auto it = hwParser.cmdList.begin();
    for (uint32_t packetId = 0; packetId < mockNode.getPacketsUsed(); packetId++) {
        verifySemaphore<FamilyType>(genCmdCast<MI_SEMAPHORE_WAIT *>(*it++), &mockNode, packetId);
    }
}

TEST_F(TimestampPacketTests, givenTagNodeWhatAskingForGpuAddressesThenReturnCorrectValue) {
    TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount> tag;
    MockTagNode mockNode;
    mockNode.tagForCpuAccess = &tag;
    mockNode.gpuAddress = 0x1230000;

    auto expectedEndAddress = mockNode.getGpuAddress() + (2 * sizeof(uint32_t));
    EXPECT_EQ(expectedEndAddress, TimestampPacketHelper::getContextEndGpuAddress(mockNode));
}

TEST_F(TimestampPacketTests, givenTimestampPacketContainerWhenMovedThenMoveAllNodes) {
    EXPECT_TRUE(std::is_move_constructible<TimestampPacketContainer>::value);
    EXPECT_TRUE(std::is_move_assignable<TimestampPacketContainer>::value);
    EXPECT_FALSE(std::is_copy_assignable<TimestampPacketContainer>::value);
    EXPECT_FALSE(std::is_copy_constructible<TimestampPacketContainer>::value);

    struct MockTagNode : public TagNode<TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount>> {
        void returnTag() override {
            returnCalls++;
        }
        using TagNode<TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount>>::refCount;
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

TEST_F(TimestampPacketTests, givenTagNodesWhenReleaseIsCalledThenReturnAllTagsToPool) {
    struct MockTagNode : public TagNode<TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount>> {
        void returnTag() override {
            returnCalls++;
        }
        uint32_t returnCalls = 0;
    };

    MockTagNode mockNode0;
    MockTagNode mockNode1;

    TimestampPacketContainer container;

    container.add(&mockNode0);
    container.add(&mockNode1);

    EXPECT_EQ(2u, container.peekNodes().size());
    EXPECT_EQ(0u, mockNode0.returnCalls);
    EXPECT_EQ(0u, mockNode1.returnCalls);

    container.releaseNodes();

    EXPECT_EQ(0u, container.peekNodes().size());
    EXPECT_EQ(1u, mockNode0.returnCalls);
    EXPECT_EQ(1u, mockNode1.returnCalls);
}

HWTEST_F(TimestampPacketTests, whenNewTagIsTakenThenReinitialize) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(executionEnvironment);
    MockTagAllocator<MockTimestampPackets32> allocator(0, &memoryManager, 1);

    using MockNode = TagNode<MockTimestampPackets32>;

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

HWTEST_F(TimestampPacketTests, GivenTagNodeWhenCallMarkAsAbortedThenClearTimestamps) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(executionEnvironment);
    MockTagAllocator<MockTimestampPackets32> allocator(0, &memoryManager, 1);

    using MockNode = TagNode<MockTimestampPackets32>;

    auto firstNode = static_cast<MockNode *>(allocator.getTag());
    auto initValue = 1u;
    for (auto &packet : firstNode->tagForCpuAccess->packets) {
        packet.contextStart = initValue;
        packet.globalStart = initValue;
        packet.contextEnd = initValue;
        packet.globalEnd = initValue;
    }
    firstNode->markAsAborted();

    for (const auto &packet : firstNode->tagForCpuAccess->packets) {
        EXPECT_EQ(0u, packet.contextStart);
        EXPECT_EQ(0u, packet.globalStart);
        EXPECT_EQ(0u, packet.contextEnd);
        EXPECT_EQ(0u, packet.globalEnd);
    }
}

TEST_F(TimestampPacketTests, whenObjectIsCreatedThenInitializeAllStamps) {
    MockTimestampPackets32 timestampPacketStorage;
    EXPECT_EQ(TimestampPacketConstants::preferredPacketCount * sizeof(timestampPacketStorage.packets[0]), sizeof(timestampPacketStorage.packets));

    for (const auto &packet : timestampPacketStorage.packets) {
        EXPECT_EQ(1u, packet.contextStart);
        EXPECT_EQ(1u, packet.globalStart);
        EXPECT_EQ(1u, packet.contextEnd);
        EXPECT_EQ(1u, packet.globalEnd);
    }
}

HWTEST_F(TimestampPacketTests, whenEstimatingSizeForNodeDependencyThenReturnCorrectValue) {
    TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount> tag;
    MockTagNode mockNode;
    mockNode.tagForCpuAccess = &tag;
    mockNode.gpuAddress = 0x1230000;

    size_t sizeForNodeDependency = 0;
    sizeForNodeDependency += TimestampPacketHelper::getRequiredCmdStreamSizeForSemaphoreNodeDependency<FamilyType>(mockNode);

    size_t expectedSize = mockNode.getPacketsUsed() * NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();

    EXPECT_EQ(expectedSize, sizeForNodeDependency);
}

struct DeviceTimestampPacketTests : public ::testing::Test, DeviceFixture {
    DebugManagerStateRestore restore{};
    ExecutionEnvironment *executionEnvironment{nullptr};

    void SetUp() override {
        debugManager.flags.EnableTimestampPacket.set(1);
        DeviceFixture::setUp();
        executionEnvironment = pDevice->executionEnvironment;
    }

    void TearDown() override {
        DeviceFixture::tearDown();
    };
};

HWTEST_F(DeviceTimestampPacketTests, givenCommandStreamReceiverHwWhenObtainingPreferredTagPoolSizeThenReturnCorrectValue) {
    OsContext &osContext = *executionEnvironment->memoryManager->getRegisteredEngines(mockRootDeviceIndex)[0].osContext;

    CommandStreamReceiverHw<FamilyType> csr(*executionEnvironment, 0, osContext.getDeviceBitfield());
    EXPECT_EQ(2048u, csr.getPreferredTagPoolSize());
}

HWTEST_F(DeviceTimestampPacketTests, givenDebugFlagSetWhenCreatingAllocatorThenUseCorrectSize) {
    OsContext &osContext = *executionEnvironment->memoryManager->getRegisteredEngines(mockRootDeviceIndex)[0].osContext;

    {
        CommandStreamReceiverHw<FamilyType> csr(*executionEnvironment, 0, osContext.getDeviceBitfield());
        csr.setupContext(osContext);

        auto allocator = csr.getTimestampPacketAllocator();
        auto tag = allocator->getTag();
        auto size = tag->getSinglePacketSize();
        EXPECT_EQ(4u * sizeof(typename FamilyType::TimestampPacketType), size);
    }

    {
        debugManager.flags.OverrideTimestampPacketSize.set(4);

        CommandStreamReceiverHw<FamilyType> csr(*executionEnvironment, 0, osContext.getDeviceBitfield());
        csr.setupContext(osContext);

        auto allocator = csr.getTimestampPacketAllocator();
        auto tag = allocator->getTag();
        auto size = tag->getSinglePacketSize();
        EXPECT_EQ(4u * sizeof(uint32_t), size);
    }

    {
        debugManager.flags.OverrideTimestampPacketSize.set(8);

        CommandStreamReceiverHw<FamilyType> csr(*executionEnvironment, 0, osContext.getDeviceBitfield());
        csr.setupContext(osContext);

        auto allocator = csr.getTimestampPacketAllocator();
        auto tag = allocator->getTag();
        auto size = tag->getSinglePacketSize();
        EXPECT_EQ(4u * sizeof(uint64_t), size);
    }

    {
        debugManager.flags.OverrideTimestampPacketSize.set(-1);
        CommandStreamReceiverHw<FamilyType> csr(*executionEnvironment, 0, osContext.getDeviceBitfield());
        csr.setupContext(osContext);

        debugManager.flags.OverrideTimestampPacketSize.set(12);
        EXPECT_ANY_THROW(csr.getTimestampPacketAllocator());
    }
}

HWTEST_F(DeviceTimestampPacketTests, givenTagAllocatorForTimstampAndQueryPacketCountThenResultIsCorrect) {
    OsContext &osContext = *executionEnvironment->memoryManager->getRegisteredEngines(mockRootDeviceIndex)[0].osContext;
    {
        using TimestampPacketType = typename FamilyType::TimestampPacketType;
        using TimestampPacketsT = NEO::TimestampPackets<TimestampPacketType, FamilyType::timestampPacketCount>;
        CommandStreamReceiverHw<FamilyType> csr(*executionEnvironment, 0, osContext.getDeviceBitfield());
        csr.setupContext(osContext);

        auto allocator = csr.getTimestampPacketAllocator();
        auto tag = static_cast<NEO::TagNode<TimestampPacketsT> *>(allocator->getTag());
        uint32_t packetCount = tag->tagForCpuAccess->getPacketCount();
        EXPECT_EQ(FamilyType::timestampPacketCount, packetCount);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DeviceTimestampPacketTests, givenInvalidDebugFlagSetWhenCreatingCsrThenExceptionIsThrown) {
    OsContext &osContext = *executionEnvironment->memoryManager->getRegisteredEngines(mockRootDeviceIndex)[0].osContext;
    debugManager.flags.OverrideTimestampPacketSize.set(12);

    EXPECT_ANY_THROW(CommandStreamReceiverHw<FamilyType> csr(*executionEnvironment, 0, osContext.getDeviceBitfield()));
}

HWTEST_F(DeviceTimestampPacketTests, givenTagAlignmentWhenCreatingAllocatorThenGpuAddressIsAligned) {
    auto csr = executionEnvironment->memoryManager->getRegisteredEngines(mockRootDeviceIndex)[0].commandStreamReceiver;

    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();

    auto allocator = csr->getTimestampPacketAllocator();

    auto tag1 = allocator->getTag();
    auto tag2 = allocator->getTag();

    EXPECT_TRUE(isAligned(tag1->getGpuAddress(), gfxCoreHelper.getTimestampPacketAllocatorAlignment()));
    EXPECT_TRUE(isAligned(tag2->getGpuAddress(), gfxCoreHelper.getTimestampPacketAllocatorAlignment()));
}

HWTEST_F(DeviceTimestampPacketTests, givenDebugFlagSetWhenCreatingTimestampPacketAllocatorThenDisableReusingAndLimitPoolSize) {
    DebugManagerStateRestore restore;
    debugManager.flags.DisableTimestampPacketOptimizations.set(true);
    OsContext &osContext = *executionEnvironment->memoryManager->getRegisteredEngines(mockRootDeviceIndex)[0].osContext;

    CommandStreamReceiverHw<FamilyType> csr(*executionEnvironment, 0, osContext.getDeviceBitfield());
    csr.setupContext(osContext);
    EXPECT_EQ(1u, csr.getPreferredTagPoolSize());

    auto tag = csr.getTimestampPacketAllocator()->getTag();
    setTagToReadyState<FamilyType>(tag);

    EXPECT_FALSE(tag->canBeReleased());
}

HWTEST_F(DeviceTimestampPacketTests, givenTimestampPacketTypeAndSizeWhenCheckingSizeOfTimestampPacketsThenItIsCorrect) {
    EXPECT_EQ((4 * FamilyType::timestampPacketCount) * sizeof(typename FamilyType::TimestampPacketType),
              sizeof(TimestampPackets<typename FamilyType::TimestampPacketType, FamilyType::timestampPacketCount>));
}

using TimestampPacketHelperTests = Test<DeviceFixture>;

HWTEST_F(TimestampPacketHelperTests, givenTagNodesInMultiRootSyncContainerWhenProgramingDependensiecThenSemaforesAreProgrammed) {
    StackVec<char, 4096> buffer(4096);
    LinearStream cmdStream(buffer.begin(), buffer.size());
    CsrDependencies deps;
    auto mockTagAllocator = std::make_unique<MockTagAllocator<>>(0, pDevice->getMemoryManager());
    TimestampPacketContainer container = {};
    container.add(mockTagAllocator->getTag());
    deps.multiRootTimeStampSyncContainer.push_back(&container);
    TimestampPacketHelper::programCsrDependenciesForForMultiRootDeviceSyncContainer<FamilyType>(cmdStream, deps);
    EXPECT_EQ(cmdStream.getUsed(), NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait());
}

HWTEST_F(TimestampPacketHelperTests, givenEmptyContainerMultiRootSyncContainerWhenProgramingDependensiecThenZeroSemaforesAreProgrammed) {
    StackVec<char, 4096> buffer(4096);
    LinearStream cmdStream(buffer.begin(), buffer.size());
    CsrDependencies deps;
    TimestampPacketContainer container = {};
    deps.multiRootTimeStampSyncContainer.push_back(&container);
    TimestampPacketHelper::programCsrDependenciesForForMultiRootDeviceSyncContainer<FamilyType>(cmdStream, deps);
    EXPECT_EQ(cmdStream.getUsed(), 0u);
}

HWTEST_F(TimestampPacketHelperTests, givenEmptyMultiRootSyncContainerWhenProgramingDependensiecThenZeroSemaforesAreProgrammed) {
    StackVec<char, 4096> buffer(4096);
    LinearStream cmdStream(buffer.begin(), buffer.size());
    CsrDependencies deps;
    TimestampPacketHelper::programCsrDependenciesForForMultiRootDeviceSyncContainer<FamilyType>(cmdStream, deps);
    EXPECT_EQ(cmdStream.getUsed(), 0u);
}
