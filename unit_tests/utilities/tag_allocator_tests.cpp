/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "test.h"
#include "gtest/gtest.h"
#include "runtime/utilities/tag_allocator.h"
#include "unit_tests/fixtures/memory_allocator_fixture.h"

#include <cstdint>

using namespace OCLRT;

typedef Test<MemoryAllocatorFixture> TagAllocatorTest;

struct timeStamps {
    void initialize() {
        start = 1;
        end = 2;
        release = true;
    }
    bool canBeReleased() const { return release; }
    bool release;
    uint64_t start;
    uint64_t end;
};

class MockTagAllocator : public TagAllocator<timeStamps> {
  public:
    using TagAllocator<timeStamps>::populateFreeTags;
    using TagAllocator<timeStamps>::deferredTags;

    MockTagAllocator(MemoryManager *memMngr, size_t tagCount, size_t tagAlignment) : TagAllocator<timeStamps>(memMngr, tagCount, tagAlignment) {
    }

    GraphicsAllocation *getGraphicsAllocation(size_t id = 0) {
        return TagAllocator<timeStamps>::gfxAllocations[id];
    }

    TagNode<timeStamps> *getFreeTagsHead() {
        return TagAllocator<timeStamps>::freeTags.peekHead();
    }

    TagNode<timeStamps> *getUsedTagsHead() {
        return TagAllocator<timeStamps>::usedTags.peekHead();
    }

    IDList<TagNode<timeStamps>> &getFreeTags() {
        return TagAllocator<timeStamps>::freeTags;
    }

    IDList<TagNode<timeStamps>> &getUsedTags() {
        return TagAllocator<timeStamps>::usedTags;
    }

    size_t getGraphicsAllocationsCount() {
        return gfxAllocations.size();
    }

    size_t getTagPoolCount() {
        return tagPoolMemory.size();
    }
};

TEST_F(TagAllocatorTest, Initialize) {

    MockTagAllocator tagAllocator(memoryManager, 100, 64);

    ASSERT_NE(nullptr, tagAllocator.getGraphicsAllocation());

    ASSERT_NE(nullptr, tagAllocator.getFreeTagsHead());
    EXPECT_EQ(nullptr, tagAllocator.getUsedTagsHead());

    void *gfxMemory = tagAllocator.getGraphicsAllocation()->getUnderlyingBuffer();
    void *head = reinterpret_cast<void *>(tagAllocator.getFreeTagsHead()->tag);
    EXPECT_EQ(gfxMemory, head);
}

TEST_F(TagAllocatorTest, GetReturnTagCheckFreeAndUsedLists) {

    MockTagAllocator tagAllocator(memoryManager, 10, 16);

    ASSERT_NE(nullptr, tagAllocator.getGraphicsAllocation());
    ASSERT_NE(nullptr, tagAllocator.getFreeTagsHead());
    EXPECT_EQ(nullptr, tagAllocator.getUsedTagsHead());

    TagNode<timeStamps> *tagNode = tagAllocator.getTag();

    EXPECT_NE(nullptr, tagNode);

    IDList<TagNode<timeStamps>> &freeList = tagAllocator.getFreeTags();
    IDList<TagNode<timeStamps>> &usedList = tagAllocator.getUsedTags();

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

TEST_F(TagAllocatorTest, TagAlignment) {

    size_t alignment = 64;
    MockTagAllocator tagAllocator(memoryManager, 10, alignment);

    ASSERT_NE(nullptr, tagAllocator.getFreeTagsHead());

    TagNode<timeStamps> *tagNode = tagAllocator.getTag();

    ASSERT_NE(nullptr, tagNode);
    EXPECT_EQ(0u, (uintptr_t)tagNode->tag % alignment);

    tagAllocator.returnTag(tagNode);
}

TEST_F(TagAllocatorTest, givenTagAllocatorWhenAllNodesWereUsedThenCreateNewGraphicsAllocation) {

    // Big alignment to force only 4 tags
    size_t alignment = 1024;
    MockTagAllocator tagAllocator(memoryManager, 4, alignment);

    ASSERT_NE(nullptr, tagAllocator.getFreeTagsHead());

    TagNode<timeStamps> *tagNodes[4];

    for (size_t i = 0; i < 4; i++) {
        tagNodes[i] = tagAllocator.getTag();
        EXPECT_NE(nullptr, tagNodes[i]);
    }
    EXPECT_EQ(1u, tagAllocator.getGraphicsAllocationsCount());
    EXPECT_EQ(1u, tagAllocator.getTagPoolCount());

    TagNode<timeStamps> *tagNode = tagAllocator.getTag();
    EXPECT_NE(nullptr, tagNode);

    EXPECT_EQ(2u, tagAllocator.getGraphicsAllocationsCount());
    EXPECT_EQ(2u, tagAllocator.getTagPoolCount());
}

TEST_F(TagAllocatorTest, GetTagsAndReturnInDifferentOrder) {

    // Big alignment to force only 4 tags
    size_t alignment = 1024;
    MockTagAllocator tagAllocator(memoryManager, 4, alignment);

    ASSERT_NE(nullptr, tagAllocator.getFreeTagsHead());

    TagNode<timeStamps> *tagNodes[4];

    for (int i = 0; i < 4; i++) {
        tagNodes[i] = tagAllocator.getTag();
        EXPECT_NE(nullptr, tagNodes[i]);
    }
    EXPECT_EQ(1u, tagAllocator.getGraphicsAllocationsCount());
    EXPECT_EQ(1u, tagAllocator.getTagPoolCount());

    TagNode<timeStamps> *tagNode2 = tagAllocator.getTag();
    EXPECT_NE(nullptr, tagNode2);
    EXPECT_EQ(2u, tagAllocator.getGraphicsAllocationsCount());
    EXPECT_EQ(2u, tagAllocator.getTagPoolCount());

    IDList<TagNode<timeStamps>> &freeList = tagAllocator.getFreeTags();
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

TEST_F(TagAllocatorTest, GetTagsFromTwoPools) {

    // Big alignment to force only 1 tag
    size_t alignment = 4096;
    MockTagAllocator tagAllocator(memoryManager, 1, alignment);

    ASSERT_NE(nullptr, tagAllocator.getFreeTagsHead());

    TagNode<timeStamps> *tagNode1, *tagNode2;

    tagNode1 = tagAllocator.getTag();
    ASSERT_NE(nullptr, tagNode1);

    tagNode2 = tagAllocator.getTag();
    ASSERT_NE(nullptr, tagNode2);

    EXPECT_EQ(2u, tagAllocator.getGraphicsAllocationsCount());
    EXPECT_EQ(2u, tagAllocator.getTagPoolCount());
    EXPECT_NE(tagNode1->getGraphicsAllocation(), tagNode2->getGraphicsAllocation());

    tagAllocator.returnTag(tagNode1);
    tagAllocator.returnTag(tagNode2);
}

TEST_F(TagAllocatorTest, CleanupResources) {

    // Big alignment to force only 1 tag
    size_t alignment = 4096;
    MockTagAllocator tagAllocator(memoryManager, 1, alignment);

    TagNode<timeStamps> *tagNode1, *tagNode2;

    // Allocate first Pool
    tagNode1 = tagAllocator.getTag();
    EXPECT_NE(nullptr, tagNode1);

    // Allocate second Pool
    tagNode2 = tagAllocator.getTag();
    ASSERT_NE(nullptr, tagNode2);

    // Two pools should have different gfxAllocations
    EXPECT_NE(tagNode1->getGraphicsAllocation(), tagNode2->getGraphicsAllocation());

    // Return tags
    tagAllocator.returnTag(tagNode1);
    tagAllocator.returnTag(tagNode2);

    // Should cleanup all resources
    tagAllocator.cleanUpResources();

    EXPECT_EQ(0u, tagAllocator.getGraphicsAllocationsCount());
    EXPECT_EQ(0u, tagAllocator.getTagPoolCount());
}

TEST_F(TagAllocatorTest, whenNewTagIsTakenThenInitialize) {
    MockTagAllocator tagAllocator(memoryManager, 1, 2);
    tagAllocator.getFreeTagsHead()->tag->start = 3;
    tagAllocator.getFreeTagsHead()->tag->end = 4;

    auto node = tagAllocator.getTag();
    EXPECT_EQ(1u, node->tag->start);
    EXPECT_EQ(2u, node->tag->end);
}

TEST_F(TagAllocatorTest, givenMultipleReferencesOnTagWhenReleasingThenReturnWhenAllRefCountsAreReleased) {
    MockTagAllocator tagAllocator(memoryManager, 2, 1);

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

TEST_F(TagAllocatorTest, givenNotReadyTagWhenReturnedThenMoveToDeferredList) {
    MockTagAllocator tagAllocator(memoryManager, 1, 1);
    auto node = tagAllocator.getTag();

    node->tag->release = false;
    EXPECT_TRUE(tagAllocator.deferredTags.peekIsEmpty());
    tagAllocator.returnTag(node);
    EXPECT_FALSE(tagAllocator.deferredTags.peekIsEmpty());
    EXPECT_TRUE(tagAllocator.getFreeTags().peekIsEmpty());
}

TEST_F(TagAllocatorTest, givenReadyTagWhenReturnedThenMoveToFreeList) {
    MockTagAllocator tagAllocator(memoryManager, 1, 1);
    auto node = tagAllocator.getTag();

    node->tag->release = true;
    EXPECT_TRUE(tagAllocator.deferredTags.peekIsEmpty());
    tagAllocator.returnTag(node);
    EXPECT_TRUE(tagAllocator.deferredTags.peekIsEmpty());
    EXPECT_FALSE(tagAllocator.getFreeTags().peekIsEmpty());
}
