/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/utilities/hw_timestamps.h"
#include "shared/source/utilities/perf_counter.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/fixtures/memory_allocator_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_timestamp_packet.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

#include <cstdint>

using namespace NEO;

struct TagAllocatorTest : public Test<MemoryAllocatorFixture> {
    void SetUp() override {
        debugManager.flags.CreateMultipleSubDevices.set(4);
        MemoryAllocatorFixture::setUp();
    }

    const DeviceBitfield deviceBitfield{0xf};
    DebugManagerStateRestore restorer;
};

struct TimeStamps {
    using ValueT = uint64_t;
    void initialize(uint64_t initValue) {
        initializeCount++;
        start = 1;
        end = 2;
    }
    static constexpr AllocationType getAllocationType() {
        return AllocationType::profilingTagBuffer;
    }

    static constexpr TagNodeType getTagNodeType() { return TagNodeType::hwTimeStamps; }

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

    uint64_t contextCompleteTS;
    uint64_t globalEndTS;

    uint32_t initializeCount = 0;
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
        : BaseClass(RootDeviceIndicesContainer{rootDeviceIndex}, memoryManager, tagCount, tagAlignment, tagSize, 0, doNotReleaseNodes, true, deviceBitfield) {
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

    memoryManager->recentlyPassedDeviceBitfield = 0;
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
            return new MemoryAllocation(0, 1u /*num gmms*/, TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount>::getAllocationType(), nullptr, nullptr, 0, MemoryConstants::pageSize,
                                        1, MemoryPool::system4KBPages, false, false, MemoryManager::maxOsContextCount);
        }
    };

    auto mockMemoryManager = std::make_unique<MyMockMemoryManager>(true, true, *executionEnvironment);

    const size_t tagsCount = 3;
    MockTagAllocator<TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount>> tagAllocator(mockMemoryManager.get(), tagsCount, 1, deviceBitfield);

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

TEST_F(TagAllocatorTest, givenReinitializationDisabledWhenGettingNewTagThenDontInitialize) {
    MockTagAllocator<TimeStamps> tagAllocator1(RootDeviceIndicesContainer{0}, memoryManager, 1, 2, sizeof(TimeStamps), 0, false, true, deviceBitfield);
    MockTagAllocator<TimeStamps> tagAllocator2(RootDeviceIndicesContainer{0}, memoryManager, 1, 2, sizeof(TimeStamps), 0, false, false, deviceBitfield);

    tagAllocator1.freeTags.peekHead()->tagForCpuAccess->initializeCount = 0;
    tagAllocator2.freeTags.peekHead()->tagForCpuAccess->initializeCount = 0;

    auto node1 = static_cast<TagNode<TimeStamps> *>(tagAllocator1.getTag());
    auto node2 = static_cast<TagNode<TimeStamps> *>(tagAllocator2.getTag());
    EXPECT_EQ(1u, node1->tagForCpuAccess->initializeCount);
    EXPECT_EQ(0u, node2->tagForCpuAccess->initializeCount);

    node1->returnTag();
    node2->returnTag();

    node1 = static_cast<TagNode<TimeStamps> *>(tagAllocator1.getTag());
    node2 = static_cast<TagNode<TimeStamps> *>(tagAllocator2.getTag());
    EXPECT_EQ(2u, node1->tagForCpuAccess->initializeCount);
    EXPECT_EQ(0u, node2->tagForCpuAccess->initializeCount);
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
    EXPECT_TRUE(tagAllocator.freeTags.peekIsEmpty()); // empty again - new pool was not allocated
}

TEST_F(TagAllocatorTest, givenTagAllocatorWhenGraphicsAllocationIsCreatedThenSetValidllocationType) {
    MockTagAllocator<TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount>> timestampPacketAllocator(mockRootDeviceIndex, memoryManager, 1, 1, sizeof(TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount>), false, mockDeviceBitfield);
    MockTagAllocator<HwTimeStamps> hwTimeStampsAllocator(mockRootDeviceIndex, memoryManager, 1, 1, sizeof(HwTimeStamps), false, mockDeviceBitfield);
    MockTagAllocator<HwPerfCounter> hwPerfCounterAllocator(mockRootDeviceIndex, memoryManager, 1, 1, sizeof(HwPerfCounter), false, mockDeviceBitfield);
    MockTagAllocator<DeviceAllocNodeType<true>> inOrderDeviceAllocator(mockRootDeviceIndex, memoryManager, 1, 1, sizeof(HwPerfCounter), false, mockDeviceBitfield);
    MockTagAllocator<DeviceAllocNodeType<false>> inOrderHostAllocator(mockRootDeviceIndex, memoryManager, 1, 1, sizeof(HwPerfCounter), false, mockDeviceBitfield);

    auto timestampPacketTag = timestampPacketAllocator.getTag();
    auto hwTimeStampsTag = hwTimeStampsAllocator.getTag();
    auto hwPerfCounterTag = hwPerfCounterAllocator.getTag();
    auto inOrderDeviceTag = inOrderDeviceAllocator.getTag();
    auto inOrderHostTag = inOrderHostAllocator.getTag();

    EXPECT_EQ(AllocationType::timestampPacketTagBuffer, timestampPacketTag->getBaseGraphicsAllocation()->getAllocationType());
    EXPECT_EQ(AllocationType::profilingTagBuffer, hwTimeStampsTag->getBaseGraphicsAllocation()->getAllocationType());
    EXPECT_EQ(AllocationType::profilingTagBuffer, hwPerfCounterTag->getBaseGraphicsAllocation()->getAllocationType());
    EXPECT_EQ(AllocationType::gpuTimestampDeviceBuffer, inOrderDeviceTag->getBaseGraphicsAllocation()->getAllocationType());
    EXPECT_EQ(AllocationType::timestampPacketTagBuffer, inOrderHostTag->getBaseGraphicsAllocation()->getAllocationType());
}

TEST_F(TagAllocatorTest, givenMultipleRootDevicesWhenPopulatingTagsThenCreateMultiGraphicsAllocation) {
    constexpr uint32_t maxRootDeviceIndex = 4;

    auto executionEnvironment = std::make_unique<NEO::ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(maxRootDeviceIndex + 1);
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }

    auto testMemoryManager = new MockMemoryManager(false, false, *executionEnvironment);
    executionEnvironment->memoryManager.reset(testMemoryManager);

    const RootDeviceIndicesContainer indices = {0, 2, maxRootDeviceIndex};

    MockTagAllocator<TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount>> timestampPacketAllocator(indices, testMemoryManager, 1, 1,
                                                                                                                          sizeof(TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount>), 0, false,
                                                                                                                          true, mockDeviceBitfield);

    EXPECT_EQ(1u, timestampPacketAllocator.getGraphicsAllocationsCount());

    auto multiGraphicsAllocation = timestampPacketAllocator.gfxAllocations[0].get();

    for (uint32_t i = 0; i <= maxRootDeviceIndex; i++) {
        if (std::find(indices.begin(), indices.end(), i) != indices.end()) {
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
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }

    auto testMemoryManager = new MockMemoryManager(false, false, *executionEnvironment);
    executionEnvironment->memoryManager.reset(testMemoryManager);

    const RootDeviceIndicesContainer indicesVector = {0, 1};

    MockTagAllocator<TimestampPackets<uint32_t, FamilyType::timestampPacketCount>> timestampPacketAllocator(indicesVector, testMemoryManager, 1, 1, sizeof(TimestampPackets<uint32_t, FamilyType::timestampPacketCount>),
                                                                                                            0, false, true, mockDeviceBitfield);

    EXPECT_EQ(1u, timestampPacketAllocator.getGraphicsAllocationsCount());

    auto multiGraphicsAllocation = timestampPacketAllocator.gfxAllocations[0].get();

    auto rootCsr0 = std::unique_ptr<UltCommandStreamReceiver<FamilyType>>(static_cast<UltCommandStreamReceiver<FamilyType> *>(createCommandStream(*executionEnvironment, 0, 1)));
    auto osContext0 = testMemoryManager->createAndRegisterOsContext(rootCsr0.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular}, true));
    rootCsr0->setupContext(*osContext0);

    auto rootCsr1 = std::unique_ptr<UltCommandStreamReceiver<FamilyType>>(static_cast<UltCommandStreamReceiver<FamilyType> *>(createCommandStream(*executionEnvironment, 1, 1)));
    auto osContext1 = testMemoryManager->createAndRegisterOsContext(rootCsr1.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular}, true));
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
        TagNode<DeviceAllocNodeType<true>> inOrder = {};

        EXPECT_ANY_THROW(inOrder.getGlobalStartOffset());
        EXPECT_ANY_THROW(inOrder.getContextStartOffset());
        EXPECT_ANY_THROW(inOrder.getContextEndOffset());
        EXPECT_ANY_THROW(inOrder.getGlobalEndOffset());
        EXPECT_ANY_THROW(inOrder.getContextStartValue(0));
        EXPECT_ANY_THROW(inOrder.getGlobalStartValue(0));
        EXPECT_ANY_THROW(inOrder.getContextEndValue(0));
        EXPECT_ANY_THROW(inOrder.getGlobalEndValue(0));
        EXPECT_ANY_THROW(inOrder.getContextEndAddress(0));
        EXPECT_ANY_THROW(inOrder.getContextCompleteRef());
        EXPECT_ANY_THROW(inOrder.getGlobalEndRef());
        EXPECT_ANY_THROW(inOrder.getSinglePacketSize());
        EXPECT_ANY_THROW(inOrder.assignDataToAllTimestamps(0, nullptr));
    }

    {
        TagNode<DeviceAllocNodeType<false>> inOrder = {};

        EXPECT_ANY_THROW(inOrder.getGlobalStartOffset());
        EXPECT_ANY_THROW(inOrder.getContextStartOffset());
        EXPECT_ANY_THROW(inOrder.getContextEndOffset());
        EXPECT_ANY_THROW(inOrder.getGlobalEndOffset());
        EXPECT_ANY_THROW(inOrder.getContextStartValue(0));
        EXPECT_ANY_THROW(inOrder.getGlobalStartValue(0));
        EXPECT_ANY_THROW(inOrder.getContextEndValue(0));
        EXPECT_ANY_THROW(inOrder.getGlobalEndValue(0));
        EXPECT_ANY_THROW(inOrder.getContextEndAddress(0));
        EXPECT_ANY_THROW(inOrder.getContextCompleteRef());
        EXPECT_ANY_THROW(inOrder.getGlobalEndRef());
        EXPECT_ANY_THROW(inOrder.getSinglePacketSize());
        EXPECT_ANY_THROW(inOrder.assignDataToAllTimestamps(0, nullptr));
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
        using TsType = TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount>;
        TagNode<TsType> timestampPacketsNode = {};
        TsType data = {};
        timestampPacketsNode.tagForCpuAccess = &data;

        EXPECT_ANY_THROW(timestampPacketsNode.getContextCompleteRef());
        EXPECT_ANY_THROW(timestampPacketsNode.getGlobalEndRef());
        EXPECT_ANY_THROW(timestampPacketsNode.getQueryHandleRef());
        EXPECT_NO_THROW(timestampPacketsNode.getContextEndValue(0));
        EXPECT_NO_THROW(timestampPacketsNode.getContextStartValue(0));
        EXPECT_NO_THROW(timestampPacketsNode.getGlobalEndValue(0));
        EXPECT_NO_THROW(timestampPacketsNode.getGlobalStartValue(0));
    }
}
