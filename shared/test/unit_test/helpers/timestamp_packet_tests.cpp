/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
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

    for (uint32_t i = 0; i < TimestampPacketSizeControl::preferredPacketCount; i++) {
        tagNode->assignDataToAllTimestamps(i, zeros);
    }
    tagNode->setPacketsUsed(packetsUsed);
}
} // namespace

struct TimestampPacketTests : public ::testing::Test {
    struct MockTagNode : public TagNode<TimestampPackets<uint32_t>> {
        using TagNode<TimestampPackets<uint32_t>>::gpuAddress;
    };

    template <typename MI_SEMAPHORE_WAIT>
    void verifySemaphore(MI_SEMAPHORE_WAIT *semaphoreCmd, TagNodeBase *timestampPacketNode, uint32_t packetId) {
        EXPECT_NE(nullptr, semaphoreCmd);
        EXPECT_EQ(semaphoreCmd->getCompareOperation(), MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
        EXPECT_EQ(1u, semaphoreCmd->getSemaphoreDataDword());

        uint64_t compareOffset = packetId * TimestampPackets<uint32_t>::getSinglePacketSize();
        auto dataAddress = TimestampPacketHelper::getContextEndGpuAddress(*timestampPacketNode) + compareOffset;

        EXPECT_EQ(dataAddress, semaphoreCmd->getSemaphoreGraphicsAddress());
    };
};

HWTEST_F(TimestampPacketTests, givenTagNodeWhenSemaphoreIsProgrammedThenUseGpuAddress) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    TimestampPackets<uint32_t> tag;
    MockTagNode mockNode;
    mockNode.tagForCpuAccess = &tag;
    mockNode.gpuAddress = 0x1230000;

    StackVec<char, 4096> buffer(4096);
    LinearStream cmdStream(buffer.begin(), buffer.size());

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

    StackVec<char, 4096> buffer(4096);
    LinearStream cmdStream(buffer.begin(), buffer.size());

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

TEST_F(TimestampPacketTests, givenTimestampPacketContainerWhenMovedThenMoveAllNodes) {
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

TEST_F(TimestampPacketTests, whenObjectIsCreatedThenInitializeAllStamps) {
    MockTimestampPackets32 timestampPacketStorage;
    EXPECT_EQ(TimestampPacketSizeControl::preferredPacketCount * sizeof(timestampPacketStorage.packets[0]), sizeof(timestampPacketStorage.packets));

    for (const auto &packet : timestampPacketStorage.packets) {
        EXPECT_EQ(1u, packet.contextStart);
        EXPECT_EQ(1u, packet.globalStart);
        EXPECT_EQ(1u, packet.contextEnd);
        EXPECT_EQ(1u, packet.globalEnd);
    }
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

struct DeviceTimestampPacketTests : public ::testing::Test, DeviceFixture {
    DebugManagerStateRestore restore{};
    ExecutionEnvironment *executionEnvironment{nullptr};

    void SetUp() override {
        DebugManager.flags.EnableTimestampPacket.set(1);
        DeviceFixture::setUp();
        executionEnvironment = pDevice->executionEnvironment;
    }

    void TearDown() override {
        DeviceFixture::tearDown();
    };
};

HWTEST_F(DeviceTimestampPacketTests, givenCommandStreamReceiverHwWhenObtainingPreferredTagPoolSizeThenReturnCorrectValue) {
    OsContext &osContext = *executionEnvironment->memoryManager->getRegisteredEngines()[0].osContext;

    CommandStreamReceiverHw<FamilyType> csr(*executionEnvironment, 0, osContext.getDeviceBitfield());
    EXPECT_EQ(2048u, csr.getPreferredTagPoolSize());
}

HWTEST_F(DeviceTimestampPacketTests, givenDebugFlagSetWhenCreatingAllocatorThenUseCorrectSize) {
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

HWCMDTEST_F(IGFX_XE_HP_CORE, DeviceTimestampPacketTests, givenInvalidDebugFlagSetWhenCreatingCsrThenExceptionIsThrown) {
    OsContext &osContext = *executionEnvironment->memoryManager->getRegisteredEngines()[0].osContext;
    DebugManager.flags.OverrideTimestampPacketSize.set(12);

    EXPECT_ANY_THROW(CommandStreamReceiverHw<FamilyType> csr(*executionEnvironment, 0, osContext.getDeviceBitfield()));
}

HWTEST_F(DeviceTimestampPacketTests, givenTagAlignmentWhenCreatingAllocatorThenGpuAddressIsAligned) {
    auto csr = executionEnvironment->memoryManager->getRegisteredEngines()[0].commandStreamReceiver;

    auto &hwHelper = HwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily);

    auto allocator = csr->getTimestampPacketAllocator();

    auto tag1 = allocator->getTag();
    auto tag2 = allocator->getTag();

    EXPECT_TRUE(isAligned(tag1->getGpuAddress(), hwHelper.getTimestampPacketAllocatorAlignment()));
    EXPECT_TRUE(isAligned(tag2->getGpuAddress(), hwHelper.getTimestampPacketAllocatorAlignment()));
}

HWTEST_F(DeviceTimestampPacketTests, givenDebugFlagSetWhenCreatingTimestampPacketAllocatorThenDisableReusingAndLimitPoolSize) {
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
