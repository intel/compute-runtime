/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/multi_graphics_allocation.h"

#include "gtest/gtest.h"

using namespace NEO;

struct MockMultiGraphicsAllocation : public MultiGraphicsAllocation {
    using MultiGraphicsAllocation::graphicsAllocations;
    using MultiGraphicsAllocation::MultiGraphicsAllocation;
};

TEST(MultiGraphicsAllocationTest, whenCreatingMultiGraphicsAllocationThenTheAllocationIsObtainableAsADefault) {
    GraphicsAllocation graphicsAllocation(1, // rootDeviceIndex
                                          GraphicsAllocation::AllocationType::BUFFER,
                                          nullptr, 0, 0, MemoryPool::System4KBPages, 0);

    MockMultiGraphicsAllocation multiGraphicsAllocation(1);
    EXPECT_EQ(2u, multiGraphicsAllocation.graphicsAllocations.size());

    multiGraphicsAllocation.addAllocation(&graphicsAllocation);

    EXPECT_EQ(&graphicsAllocation, multiGraphicsAllocation.getDefaultGraphicsAllocation());

    EXPECT_EQ(&graphicsAllocation, multiGraphicsAllocation.getGraphicsAllocation(graphicsAllocation.getRootDeviceIndex()));
    EXPECT_EQ(nullptr, multiGraphicsAllocation.getGraphicsAllocation(0));
}

TEST(MultiGraphicsAllocationTest, givenMultiGraphicsAllocationWhenAddingMultipleGraphicsAllocationsThenTheyAreObtainableByRootDeviceIndex) {
    GraphicsAllocation graphicsAllocation0(0, // rootDeviceIndex
                                           GraphicsAllocation::AllocationType::BUFFER,
                                           nullptr, 0, 0, MemoryPool::System4KBPages, 0);
    GraphicsAllocation graphicsAllocation1(1, // rootDeviceIndex
                                           GraphicsAllocation::AllocationType::BUFFER,
                                           nullptr, 0, 0, MemoryPool::System4KBPages, 0);

    MockMultiGraphicsAllocation multiGraphicsAllocation(1);

    EXPECT_EQ(2u, multiGraphicsAllocation.graphicsAllocations.size());
    multiGraphicsAllocation.addAllocation(&graphicsAllocation0);
    multiGraphicsAllocation.addAllocation(&graphicsAllocation1);

    EXPECT_EQ(&graphicsAllocation0, multiGraphicsAllocation.getGraphicsAllocation(graphicsAllocation0.getRootDeviceIndex()));
    EXPECT_EQ(&graphicsAllocation1, multiGraphicsAllocation.getGraphicsAllocation(graphicsAllocation1.getRootDeviceIndex()));
}

TEST(MultiGraphicsAllocationTest, givenMultiGraphicsAllocationWhenGettingAllocationTypeThenReturnAllocationTypeFromDefaultAllocation) {
    auto expectedAllocationType = GraphicsAllocation::AllocationType::BUFFER;
    GraphicsAllocation graphicsAllocation(1, // rootDeviceIndex
                                          expectedAllocationType,
                                          nullptr, 0, 0, MemoryPool::System4KBPages, 0);

    MockMultiGraphicsAllocation multiGraphicsAllocation(1);
    multiGraphicsAllocation.addAllocation(&graphicsAllocation);

    EXPECT_EQ(multiGraphicsAllocation.getDefaultGraphicsAllocation()->getAllocationType(), multiGraphicsAllocation.getAllocationType());
    EXPECT_EQ(expectedAllocationType, multiGraphicsAllocation.getAllocationType());
}

TEST(MultiGraphicsAllocationTest, givenMultiGraphicsAllocationWhenGettingCoherencyStatusThenReturnCoherencyStatusFromDefaultAllocation) {
    auto expectedAllocationType = GraphicsAllocation::AllocationType::BUFFER;
    GraphicsAllocation graphicsAllocation(1, // rootDeviceIndex
                                          expectedAllocationType,
                                          nullptr, 0, 0, MemoryPool::System4KBPages, 0);

    MockMultiGraphicsAllocation multiGraphicsAllocation(1);
    multiGraphicsAllocation.addAllocation(&graphicsAllocation);

    graphicsAllocation.setCoherent(true);
    EXPECT_TRUE(multiGraphicsAllocation.isCoherent());

    graphicsAllocation.setCoherent(false);
    EXPECT_FALSE(multiGraphicsAllocation.isCoherent());
}

TEST(MultiGraphicsAllocationTest, WhenCreatingMultiGraphicsAllocationWithoutGraphicsAllocationThenNoDefaultAllocationIsReturned) {
    MockMultiGraphicsAllocation multiGraphicsAllocation(1);
    EXPECT_EQ(nullptr, multiGraphicsAllocation.getDefaultGraphicsAllocation());
}

TEST(MultiGraphicsAllocationTest, givenMultiGraphicsAllocationwhenRemovingGraphicsAllocationThenTheAllocationIsNoLongerAvailable) {
    uint32_t rootDeviceIndex = 1u;
    GraphicsAllocation graphicsAllocation(rootDeviceIndex,
                                          GraphicsAllocation::AllocationType::BUFFER,
                                          nullptr, 0, 0, MemoryPool::System4KBPages, 0);

    MockMultiGraphicsAllocation multiGraphicsAllocation(rootDeviceIndex);
    EXPECT_EQ(2u, multiGraphicsAllocation.graphicsAllocations.size());

    multiGraphicsAllocation.addAllocation(&graphicsAllocation);

    EXPECT_EQ(&graphicsAllocation, multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));

    multiGraphicsAllocation.removeAllocation(rootDeviceIndex);

    EXPECT_EQ(nullptr, multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));
}