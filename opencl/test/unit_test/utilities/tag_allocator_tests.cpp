/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/fixtures/memory_allocator_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

#include <cstdint>

using namespace NEO;

struct TagAllocatorTest : public Test<MemoryAllocatorFixture> {
    class MockTimestampPackets32 : public TimestampPackets<uint32_t> {
      public:
        void setTagToReadyState() {
            initialize();

            uint32_t zeros[4] = {};

            for (uint32_t i = 0; i < TimestampPacketSizeControl::preferredPacketCount; i++) {
                assignDataToAllTimestamps(i, zeros);
            }
        }

        void setToNonReadyState() {
            packets[0].contextEnd = 1;
        }
    };

    void SetUp() override {
        DebugManager.flags.CreateMultipleSubDevices.set(4);
        MemoryAllocatorFixture::SetUp();
    }

    const DeviceBitfield deviceBitfield{0xf};
    DebugManagerStateRestore restorer;
};

struct TimeStamps {
    void initialize() {
        start = 1;
        end = 2;
    }
    static constexpr AllocationType getAllocationType() {
        return AllocationType::PROFILING_TAG_BUFFER;
    }

    static constexpr TagNodeType getTagNodeType() { return TagNodeType::HwTimeStamps; }

    uint64_t getContextStartValue(uint32_t packetIndex) const {
        return start;
    }

    uint64_t getGlobalStartValue(uint32_t packetIndex) const {
        return start;
    }

    uint64_t getContextEndValue(uint32_t packetIndex) const {
        return end;
    }

    uint64_t getGlobalEndValue(uint32_t packetIndex) const {
        return end;
    }

    void const *getContextEndAddress(uint32_t packetIndex) const { return &end; }

    uint64_t start;
    uint64_t end;

    uint64_t ContextCompleteTS;
    uint64_t GlobalEndTS;
};

template <typename TagType>
class MockTagAllocator : public TagAllocator<TagType> {
    using BaseClass = TagAllocator<TagType>;
    using TagNodeT = TagNode<TagType>;

  public:
    using BaseClass::deferredTags;
    using BaseClass::doNotReleaseNodes;
    using BaseClass::freeTags;
    using BaseClass::gfxAllocations;
    using BaseClass::populateFreeTags;
    using BaseClass::releaseDeferredTags;
    using BaseClass::returnTagToDeferredPool;
    using BaseClass::rootDeviceIndices;
    using BaseClass::TagAllocator;
    using BaseClass::usedTags;
    using BaseClass::TagAllocatorBase::cleanUpResources;

    MockTagAllocator(uint32_t rootDeviceIndex, MemoryManager *memoryManager, size_t tagCount,
                     size_t tagAlignment, size_t tagSize, bool doNotReleaseNodes, DeviceBitfield deviceBitfield)
        : BaseClass(std::vector<uint32_t>{rootDeviceIndex}, memoryManager, tagCount, tagAlignment, tagSize, doNotReleaseNodes, deviceBitfield) {
    }

    MockTagAllocator(MemoryManager *memMngr, size_t tagCount, size_t tagAlignment, bool disableCompletionCheck, DeviceBitfield deviceBitfield)
        : MockTagAllocator(0, memMngr, tagCount, tagAlignment, sizeof(TagType), disableCompletionCheck, deviceBitfield) {
    }

    MockTagAllocator(MemoryManager *memMngr, size_t tagCount, size_t tagAlignment, DeviceBitfield deviceBitfield)
        : MockTagAllocator(memMngr, tagCount, tagAlignment, false, deviceBitfield) {
    }

    GraphicsAllocation *getGraphicsAllocation(size_t id = 0) {
        return this->gfxAllocations[id]->getDefaultGraphicsAllocation();
    }

    TagNodeT *getFreeTagsHead() {
        return this->freeTags.peekHead();
    }

    TagNodeT *getUsedTagsHead() {
        return this->usedTags.peekHead();
    }

    size_t getGraphicsAllocationsCount() {
        return this->gfxAllocations.size();
    }

    size_t getTagPoolCount() {
        return this->tagPoolMemory.size();
    }
};

TEST_F(TagAllocatorTest, givenTagNodeTypeWhenCopyingOrMovingThenDisallow) {
    EXPECT_FALSE(std::is_move_constructible<TagNode<TimeStamps>>::value);
    EXPECT_FALSE(std::is_copy_constructible<TagNode<TimeStamps>>::value);
}

TEST_F(TagAllocatorTest, WhenTagAllocatorIsCreatedThenItIsCorrectlyInitialized) {

    MockTagAllocator<TimeStamps> tagAllocator(memoryManager, 100, 64, deviceBitfield);

    ASSERT_NE(nullptr, tagAllocator.getGraphicsAllocation());

    ASSERT_NE(nullptr, tagAllocator.getFreeTagsHead());
    EXPECT_EQ(nullptr, tagAllocator.getUsedTagsHead());

    void *gfxMemory = tagAllocator.getGraphicsAllocation()->getUnderlyingBuffer();
    void *head = reinterpret_cast<void *>(tagAllocator.getFreeTagsHead()->tagForCpuAccess);
    EXPECT_EQ(gfxMemory, head);
}

TEST_F(TagAllocatorTest, WhenGettingAndReturningTagThenFreeAndUsedListsAreUpdated) {

    MockTagAllocator<TimeStamps> tagAllocator(memoryManager, 10, 16, deviceBitfield);

    ASSERT_NE(nullptr, tagAllocator.getGraphicsAllocation());
    ASSERT_NE(nullptr, tagAllocator.getFreeTagsHead());
    EXPECT_EQ(nullptr, tagAllocator.getUsedTagsHead());

    auto tagNode = static_cast<TagNode<TimeStamps> *>(tagAllocator.getTag());

    EXPECT_NE(nullptr, tagNode);

    IDList<TagNode<TimeStamps>> &freeList = tagAllocator.freeTags;
    IDList<TagNode<TimeStamps>> &usedList = tagAllocator.usedTags;

    bool isFoundOnUsedList = usedList.peekContains(*tagNode);
    bool isFoundOnFreeList = freeList.peekContains(*tagNode);

    EXPECT_FALSE(isFoundOnFreeList);
    EXPECT_TRUE(isFoundOnUsedList);

    tagAllocator.returnTag(tagNode);

    isFoundOnUsedList = usedList.peekContains(*tagNode);
    isFoundOnFreeList = freeList.peekContains(*tagNode);

    EXPECT_TRUE(isFoundOnFreeList);
    EXPECT_FALSE(isFoundOnUsedList);
}

TEST_F(TagAllocatorTest, WhenTagAllocatorIsCreatedThenItPopulatesTagsWithProperDeviceBitfield) {
    size_t alignment = 64;

    EXPECT_NE(deviceBitfield, memoryManager->recentlyPassedDeviceBitfield);
    MockTagAllocator<TimeStamps> tagAllocator(memoryManager, 10, alignment, deviceBitfield);
    EXPECT_EQ(deviceBitfield, memoryManager->recentlyPassedDeviceBitfield);
}

TEST_F(TagAllocatorTest, WhenTagIsAllocatedThenItIsAligned) {
    size_t alignment = 64;
    MockTagAllocator<TimeStamps> tagAllocator(memoryManager, 10, alignment, deviceBitfield);

    ASSERT_NE(nullptr, tagAllocator.getFreeTagsHead());

    TagNode<TimeStamps> *tagNode = static_cast<TagNode<TimeStamps> *>(tagAllocator.getTag());

    ASSERT_NE(nullptr, tagNode);
    EXPECT_EQ(0u, (uintptr_t)tagNode->tagForCpuAccess % alignment);

    tagAllocator.returnTag(tagNode);
}

TEST_F(TagAllocatorTest, givenTagAllocatorWhenAllNodesWereUsedThenCreateNewGraphicsAllocation) {

    // Big alignment to force only 4 tags
    size_t alignment = 1024;
    MockTagAllocator<TimeStamps> tagAllocator(memoryManager, 4, alignment, deviceBitfield);

    ASSERT_NE(nullptr, tagAllocator.getFreeTagsHead());

    TagNode<TimeStamps> *tagNodes[4];

    for (size_t i = 0; i < 4; i++) {
        tagNodes[i] = static_cast<TagNode<TimeStamps> *>(tagAllocator.getTag());
        EXPECT_NE(nullptr, tagNodes[i]);
    }
    EXPECT_EQ(1u, tagAllocator.getGraphicsAllocationsCount());
    EXPECT_EQ(1u, tagAllocator.getTagPoolCount());

    TagNode<TimeStamps> *tagNode = static_cast<TagNode<TimeStamps> *>(tagAllocator.getTag());
    EXPECT_NE(nullptr, tagNode);

    EXPECT_EQ(2u, tagAllocator.getGraphicsAllocationsCount());
    EXPECT_EQ(2u, tagAllocator.getTagPoolCount());
}

TEST_F(TagAllocatorTest, givenInputTagCountWhenCreatingAllocatorThenRequestedNumberOfNodesIsCreated) {
    class MyMockMemoryManager : public MockMemoryManager {
      public:
        using MockMemoryManager::MockMemoryManager;
        GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) override {
            return new MemoryAllocation(0, TimestampPackets<uint32_t>::getAllocationType(), nullptr, nullptr, 0, MemoryConstants::pageSize,
                                        1, MemoryPool::System4KBPages, false, false, MemoryManager::maxOsContextCount);
        }
    };

    auto mockMemoryManager = std::make_unique<MyMockMemoryManager>(true, true, *executionEnvironment);

    const size_t tagsCount = 3;
    MockTagAllocator<TimestampPackets<uint32_t>> tagAllocator(mockMemoryManager.get(), tagsCount, 1, deviceBitfield);

    size_t nodesFound = 0;
    auto head = tagAllocator.freeTags.peekHead();

    while (head) {
        nodesFound++;
        head = head->next;
    }
    EXPECT_EQ(tagsCount, nodesFound);
}

TEST_F(TagAllocatorTest, GivenSpecificOrderWhenReturningTagsThenFreeListIsUpdatedCorrectly) {

    // Big alignment to force only 4 tags
    size_t alignment = 1024;
    MockTagAllocator<TimeStamps> tagAllocator(memoryManager, 4, alignment, deviceBitfield);

    ASSERT_NE(nullptr, tagAllocator.getFreeTagsHead());

    TagNode<TimeStamps> *tagNodes[4];

    for (int i = 0; i < 4; i++) {
        tagNodes[i] = static_cast<TagNode<TimeStamps> *>(tagAllocator.getTag());
        EXPECT_NE(nullptr, tagNodes[i]);
    }
    EXPECT_EQ(1u, tagAllocator.getGraphicsAllocationsCount());
    EXPECT_EQ(1u, tagAllocator.getTagPoolCount());

    TagNode<TimeStamps> *tagNode2 = static_cast<TagNode<TimeStamps> *>(tagAllocator.getTag());
    EXPECT_NE(nullptr, tagNode2);
    EXPECT_EQ(2u, tagAllocator.getGraphicsAllocationsCount());
    EXPECT_EQ(2u, tagAllocator.getTagPoolCount());

    IDList<TagNode<TimeStamps>> &freeList = tagAllocator.freeTags;
    bool isFoundOnFreeList = freeList.peekContains(*tagNodes[0]);
    EXPECT_FALSE(isFoundOnFreeList);

    tagAllocator.returnTag(tagNodes[2]);
    isFoundOnFreeList = freeList.peekContains(*tagNodes[2]);
    EXPECT_TRUE(isFoundOnFreeList);
    EXPECT_NE(nullptr, tagAllocator.getFreeTagsHead());

    tagAllocator.returnTag(tagNodes[3]);
    isFoundOnFreeList = freeList.peekContains(*tagNodes[3]);
    EXPECT_TRUE(isFoundOnFreeList);

    tagAllocator.returnTag(tagNodes[1]);
    isFoundOnFreeList = freeList.peekContains(*tagNodes[1]);
    EXPECT_TRUE(isFoundOnFreeList);

    isFoundOnFreeList = freeList.peekContains(*tagNodes[0]);
    EXPECT_FALSE(isFoundOnFreeList);

    tagAllocator.returnTag(tagNodes[0]);
}

TEST_F(TagAllocatorTest, WhenGettingTagsFromTwoPoolsThenTagsAreDifferent) {

    // Big alignment to force only 1 tag
    size_t alignment = 4096;
    MockTagAllocator<TimeStamps> tagAllocator(memoryManager, 1, alignment, deviceBitfield);

    ASSERT_NE(nullptr, tagAllocator.getFreeTagsHead());

    TagNode<TimeStamps> *tagNode1, *tagNode2;

    tagNode1 = static_cast<TagNode<TimeStamps> *>(tagAllocator.getTag());
    ASSERT_NE(nullptr, tagNode1);

    tagNode2 = static_cast<TagNode<TimeStamps> *>(tagAllocator.getTag());
    ASSERT_NE(nullptr, tagNode2);

    EXPECT_EQ(2u, tagAllocator.getGraphicsAllocationsCount());
    EXPECT_EQ(2u, tagAllocator.getTagPoolCount());
    EXPECT_NE(tagNode1->getBaseGraphicsAllocation(), tagNode2->getBaseGraphicsAllocation());

    tagAllocator.returnTag(tagNode1);
    tagAllocator.returnTag(tagNode2);
}

TEST_F(TagAllocatorTest, WhenCleaningUpResourcesThenAllResourcesAreReleased) {

    // Big alignment to force only 1 tag
    size_t alignment = 4096;
    MockTagAllocator<TimeStamps> tagAllocator(memoryManager, 1, alignment, deviceBitfield);

    TagNode<TimeStamps> *tagNode1, *tagNode2;

    // Allocate first Pool
    tagNode1 = static_cast<TagNode<TimeStamps> *>(tagAllocator.getTag());
    EXPECT_NE(nullptr, tagNode1);

    // Allocate second Pool
    tagNode2 = static_cast<TagNode<TimeStamps> *>(tagAllocator.getTag());
    ASSERT_NE(nullptr, tagNode2);

    // Two pools should have different gfxAllocations
    EXPECT_NE(tagNode1->getBaseGraphicsAllocation(), tagNode2->getBaseGraphicsAllocation());

    // Return tags
    tagAllocator.returnTag(tagNode1);
    tagAllocator.returnTag(tagNode2);

    // Should cleanup all resources
    tagAllocator.cleanUpResources();

    EXPECT_EQ(0u, tagAllocator.getGraphicsAllocationsCount());
}

TEST_F(TagAllocatorTest, whenNewTagIsTakenThenItIsInitialized) {
    MockTagAllocator<TimeStamps> tagAllocator(memoryManager, 1, 2, deviceBitfield);
    tagAllocator.getFreeTagsHead()->tagForCpuAccess->start = 3;
    tagAllocator.getFreeTagsHead()->tagForCpuAccess->end = 4;
    tagAllocator.getFreeTagsHead()->setProfilingCapable(false);

    auto node = static_cast<TagNode<TimeStamps> *>(tagAllocator.getTag());
    EXPECT_EQ(1u, node->tagForCpuAccess->start);
    EXPECT_EQ(2u, node->tagForCpuAccess->end);
    EXPECT_TRUE(node->isProfilingCapable());
}

TEST_F(TagAllocatorTest, givenMultipleReferencesOnTagWhenReleasingThenReturnWhenAllRefCountsAreReleased) {
    MockTagAllocator<TimeStamps> tagAllocator(memoryManager, 2, 1, deviceBitfield);

    auto tag = tagAllocator.getTag();
    EXPECT_NE(nullptr, tagAllocator.getUsedTagsHead());
    tagAllocator.returnTag(tag);
    EXPECT_EQ(nullptr, tagAllocator.getUsedTagsHead()); // only 1 reference

    tag = tagAllocator.getTag();
    tag->incRefCount();
    EXPECT_NE(nullptr, tagAllocator.getUsedTagsHead());

    tagAllocator.returnTag(tag);
    EXPECT_NE(nullptr, tagAllocator.getUsedTagsHead()); // 1 reference left
    tagAllocator.returnTag(tag);
    EXPECT_EQ(nullptr, tagAllocator.getUsedTagsHead());
}

TEST_F(TagAllocatorTest, givenNotReadyTagWhenReturnedThenMoveToFreeList) {
    MockTagAllocator<MockTimestampPackets32> tagAllocator(memoryManager, 1, 1, deviceBitfield);
    auto node = static_cast<TagNode<MockTimestampPackets32> *>(tagAllocator.getTag());

    node->tagForCpuAccess->setToNonReadyState();
    EXPECT_TRUE(tagAllocator.deferredTags.peekIsEmpty());
    tagAllocator.returnTag(node);
    EXPECT_TRUE(tagAllocator.deferredTags.peekIsEmpty());
    EXPECT_FALSE(tagAllocator.freeTags.peekIsEmpty());
}

TEST_F(TagAllocatorTest, givenTagNodeWhenCompletionCheckIsDisabledThenStatusIsMarkedAsNotReady) {
    MockTagAllocator<TimeStamps> tagAllocator(memoryManager, 1, 1, deviceBitfield);
    EXPECT_FALSE(tagAllocator.doNotReleaseNodes);
    auto node = tagAllocator.getTag();

    EXPECT_TRUE(node->canBeReleased());

    node->setDoNotReleaseNodes(true);
    EXPECT_FALSE(node->canBeReleased());

    tagAllocator.returnTag(node);
    EXPECT_FALSE(tagAllocator.deferredTags.peekIsEmpty());
    EXPECT_TRUE(tagAllocator.freeTags.peekIsEmpty());

    tagAllocator.releaseDeferredTags();

    EXPECT_FALSE(tagAllocator.deferredTags.peekIsEmpty());
    EXPECT_TRUE(tagAllocator.freeTags.peekIsEmpty());
}

TEST_F(TagAllocatorTest, givenTagAllocatorWhenDisabledCompletionCheckThenNodeInheritsItsState) {
    MockTagAllocator<TimeStamps> tagAllocator(memoryManager, 1, 1, true, deviceBitfield);
    EXPECT_TRUE(tagAllocator.doNotReleaseNodes);

    auto node = tagAllocator.getTag();

    EXPECT_FALSE(node->canBeReleased());

    node->setDoNotReleaseNodes(false);
    EXPECT_TRUE(node->canBeReleased());

    tagAllocator.returnTag(node);
    EXPECT_TRUE(tagAllocator.deferredTags.peekIsEmpty());
    EXPECT_FALSE(tagAllocator.freeTags.peekIsEmpty());
}

TEST_F(TagAllocatorTest, givenReadyTagWhenReturnedThenMoveToFreeList) {
    MockTagAllocator<MockTimestampPackets32> tagAllocator(memoryManager, 1, 1, deviceBitfield);
    auto node = static_cast<TagNode<MockTimestampPackets32> *>(tagAllocator.getTag());

    node->tagForCpuAccess->setTagToReadyState();
    EXPECT_TRUE(tagAllocator.deferredTags.peekIsEmpty());
    tagAllocator.returnTag(node);
    EXPECT_TRUE(tagAllocator.deferredTags.peekIsEmpty());
    EXPECT_FALSE(tagAllocator.freeTags.peekIsEmpty());
}

TEST_F(TagAllocatorTest, givenEmptyFreeListWhenAskingForNewTagThenTryToReleaseDeferredListFirst) {
    MockTagAllocator<MockTimestampPackets32> tagAllocator(memoryManager, 1, 1, deviceBitfield);
    auto node = static_cast<TagNode<MockTimestampPackets32> *>(tagAllocator.getTag());

    tagAllocator.returnTagToDeferredPool(node);
    EXPECT_TRUE(tagAllocator.freeTags.peekIsEmpty());
    node = static_cast<TagNode<MockTimestampPackets32> *>(tagAllocator.getTag());
    EXPECT_NE(nullptr, node);
    EXPECT_TRUE(tagAllocator.freeTags.peekIsEmpty()); // empty again - new pool wasnt allocated
}

TEST_F(TagAllocatorTest, givenTagAllocatorWhenGraphicsAllocationIsCreatedThenSetValidllocationType) {
    MockTagAllocator<TimestampPackets<uint32_t>> timestampPacketAllocator(mockRootDeviceIndex, memoryManager, 1, 1, sizeof(TimestampPackets<uint32_t>), false, mockDeviceBitfield);
    MockTagAllocator<HwTimeStamps> hwTimeStampsAllocator(mockRootDeviceIndex, memoryManager, 1, 1, sizeof(HwTimeStamps), false, mockDeviceBitfield);
    MockTagAllocator<HwPerfCounter> hwPerfCounterAllocator(mockRootDeviceIndex, memoryManager, 1, 1, sizeof(HwPerfCounter), false, mockDeviceBitfield);

    auto timestampPacketTag = timestampPacketAllocator.getTag();
    auto hwTimeStampsTag = hwTimeStampsAllocator.getTag();
    auto hwPerfCounterTag = hwPerfCounterAllocator.getTag();

    EXPECT_EQ(AllocationType::TIMESTAMP_PACKET_TAG_BUFFER, timestampPacketTag->getBaseGraphicsAllocation()->getAllocationType());
    EXPECT_EQ(AllocationType::PROFILING_TAG_BUFFER, hwTimeStampsTag->getBaseGraphicsAllocation()->getAllocationType());
    EXPECT_EQ(AllocationType::PROFILING_TAG_BUFFER, hwPerfCounterTag->getBaseGraphicsAllocation()->getAllocationType());
}

TEST_F(TagAllocatorTest, givenMultipleRootDevicesWhenPopulatingTagsThenCreateMultiGraphicsAllocation) {
    constexpr uint32_t maxRootDeviceIndex = 4;

    auto executionEnvironment = std::make_unique<NEO::ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(maxRootDeviceIndex + 1);
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
    }

    auto testMemoryManager = new MockMemoryManager(false, false, *executionEnvironment);
    executionEnvironment->memoryManager.reset(testMemoryManager);

    const std::set<uint32_t> indices = {0, 2, maxRootDeviceIndex};

    const std::vector<uint32_t> indicesVector = {indices.begin(), indices.end()};

    MockTagAllocator<TimestampPackets<uint32_t>> timestampPacketAllocator(indicesVector, testMemoryManager, 1, 1, sizeof(TimestampPackets<uint32_t>), false, mockDeviceBitfield);

    EXPECT_EQ(1u, timestampPacketAllocator.getGraphicsAllocationsCount());

    auto multiGraphicsAllocation = timestampPacketAllocator.gfxAllocations[0].get();

    for (uint32_t i = 0; i <= maxRootDeviceIndex; i++) {
        if (indices.find(i) != indices.end()) {
            EXPECT_NE(nullptr, multiGraphicsAllocation->getGraphicsAllocation(i));
        } else {
            EXPECT_EQ(nullptr, multiGraphicsAllocation->getGraphicsAllocation(i));
        }
    }
}

HWTEST_F(TagAllocatorTest, givenMultipleRootDevicesWhenCallingMakeResidentThenUseCorrectRootDeviceIndex) {
    constexpr uint32_t maxRootDeviceIndex = 1;

    auto executionEnvironment = std::make_unique<NEO::ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(maxRootDeviceIndex + 1);
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
    }

    auto testMemoryManager = new MockMemoryManager(false, false, *executionEnvironment);
    executionEnvironment->memoryManager.reset(testMemoryManager);

    const std::vector<uint32_t> indicesVector = {0, 1};

    MockTagAllocator<TimestampPackets<uint32_t>> timestampPacketAllocator(indicesVector, testMemoryManager, 1, 1, sizeof(TimestampPackets<uint32_t>), false, mockDeviceBitfield);

    EXPECT_EQ(1u, timestampPacketAllocator.getGraphicsAllocationsCount());

    auto multiGraphicsAllocation = timestampPacketAllocator.gfxAllocations[0].get();

    auto rootCsr0 = std::unique_ptr<UltCommandStreamReceiver<FamilyType>>(static_cast<UltCommandStreamReceiver<FamilyType> *>(createCommandStream(*executionEnvironment, 0, 1)));
    auto osContext0 = testMemoryManager->createAndRegisterOsContext(rootCsr0.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular}, true));
    rootCsr0->setupContext(*osContext0);

    auto rootCsr1 = std::unique_ptr<UltCommandStreamReceiver<FamilyType>>(static_cast<UltCommandStreamReceiver<FamilyType> *>(createCommandStream(*executionEnvironment, 1, 1)));
    auto osContext1 = testMemoryManager->createAndRegisterOsContext(rootCsr1.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular}, true));
    rootCsr1->setupContext(*osContext1);

    rootCsr0->storeMakeResidentAllocations = true;
    rootCsr1->storeMakeResidentAllocations = true;

    rootCsr0->makeResident(*multiGraphicsAllocation);
    EXPECT_TRUE(rootCsr0->isMadeResident(multiGraphicsAllocation->getGraphicsAllocation(0)));
    EXPECT_FALSE(rootCsr0->isMadeResident(multiGraphicsAllocation->getGraphicsAllocation(1)));

    rootCsr1->makeResident(*multiGraphicsAllocation);
    EXPECT_FALSE(rootCsr1->isMadeResident(multiGraphicsAllocation->getGraphicsAllocation(0)));
    EXPECT_TRUE(rootCsr1->isMadeResident(multiGraphicsAllocation->getGraphicsAllocation(1)));
}

TEST_F(TagAllocatorTest, givenNotSupportedTagTypeWhenCallingMethodThenAbortOrReturnInitialValue) {

    {
        TagNode<HwPerfCounter> perfCounterNode = {};

        EXPECT_ANY_THROW(perfCounterNode.getGlobalStartOffset());
        EXPECT_ANY_THROW(perfCounterNode.getContextStartOffset());
        EXPECT_ANY_THROW(perfCounterNode.getContextEndOffset());
        EXPECT_ANY_THROW(perfCounterNode.getGlobalEndOffset());
        EXPECT_ANY_THROW(perfCounterNode.getContextStartValue(0));
        EXPECT_ANY_THROW(perfCounterNode.getGlobalStartValue(0));
        EXPECT_ANY_THROW(perfCounterNode.getContextEndValue(0));
        EXPECT_ANY_THROW(perfCounterNode.getGlobalEndValue(0));
        EXPECT_ANY_THROW(perfCounterNode.getContextEndAddress(0));
        EXPECT_ANY_THROW(perfCounterNode.getContextCompleteRef());
        EXPECT_ANY_THROW(perfCounterNode.getGlobalEndRef());
        EXPECT_ANY_THROW(perfCounterNode.getSinglePacketSize());
        EXPECT_ANY_THROW(perfCounterNode.assignDataToAllTimestamps(0, nullptr));
    }

    {
        TagNode<HwTimeStamps> hwTimestampNode = {};

        EXPECT_ANY_THROW(hwTimestampNode.getGlobalStartOffset());
        EXPECT_ANY_THROW(hwTimestampNode.getContextStartOffset());
        EXPECT_ANY_THROW(hwTimestampNode.getContextEndOffset());
        EXPECT_ANY_THROW(hwTimestampNode.getGlobalEndOffset());
        EXPECT_ANY_THROW(hwTimestampNode.getSinglePacketSize());
        EXPECT_ANY_THROW(hwTimestampNode.assignDataToAllTimestamps(0, nullptr));
        EXPECT_ANY_THROW(hwTimestampNode.getQueryHandleRef());
    }

    {
        TagNode<TimestampPackets<uint32_t>> timestampPacketsNode = {};

        EXPECT_ANY_THROW(timestampPacketsNode.getContextCompleteRef());
        EXPECT_ANY_THROW(timestampPacketsNode.getGlobalEndRef());
        EXPECT_ANY_THROW(timestampPacketsNode.getQueryHandleRef());
    }
}
