/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gtest/gtest.h"
#include "runtime/memory_manager/graphics_allocation.h"

using namespace OCLRT;

TEST(GraphicsAllocationTest, givenGraphicsAllocationWhenIsCreatedThenTaskCountsAreInitializedProperly) {
    GraphicsAllocation graphicsAllocation1(nullptr, 0u, 0u, 0u);
    GraphicsAllocation graphicsAllocation2(nullptr, 0u, 0u);
    for (auto i = 0u; i < maxOsContextCount; i++) {
        EXPECT_EQ(ObjectNotUsed, graphicsAllocation1.getTaskCount(i));
        EXPECT_EQ(ObjectNotUsed, graphicsAllocation2.getTaskCount(i));
        EXPECT_EQ(ObjectNotResident, graphicsAllocation1.getResidencyTaskCount(i));
        EXPECT_EQ(ObjectNotResident, graphicsAllocation2.getResidencyTaskCount(i));
    }
}

TEST(GraphicsAllocationTest, givenGraphicsAllocationWhenUpdatedTaskCountThenAllocationWasUsed) {
    GraphicsAllocation graphicsAllocation(nullptr, 0u, 0u);
    EXPECT_FALSE(graphicsAllocation.peekWasUsed());
    graphicsAllocation.updateTaskCount(0u, 0u);
    EXPECT_TRUE(graphicsAllocation.peekWasUsed());
}
TEST(GraphicsAllocationTest, givenGraphicsAllocationWhenUpdatedTaskCountThenOnlyOneTaskCountIsUpdated) {
    GraphicsAllocation graphicsAllocation(nullptr, 0u, 0u);
    graphicsAllocation.updateTaskCount(1u, 0u);
    EXPECT_EQ(1u, graphicsAllocation.getTaskCount(0u));
    for (auto i = 1u; i < maxOsContextCount; i++) {
        EXPECT_EQ(ObjectNotUsed, graphicsAllocation.getTaskCount(i));
    }
    graphicsAllocation.updateTaskCount(2u, 1u);
    EXPECT_EQ(1u, graphicsAllocation.getTaskCount(0u));
    EXPECT_EQ(2u, graphicsAllocation.getTaskCount(1u));
    for (auto i = 2u; i < maxOsContextCount; i++) {
        EXPECT_EQ(ObjectNotUsed, graphicsAllocation.getTaskCount(i));
    }
}
TEST(GraphicsAllocationTest, givenGraphicsAllocationWhenUpdatedTaskCountToObjectNotUsedValueThenUnregisterContext) {
    GraphicsAllocation graphicsAllocation(nullptr, 0u, 0u);
    EXPECT_FALSE(graphicsAllocation.peekWasUsed());
    graphicsAllocation.updateTaskCount(0u, 0u);
    EXPECT_TRUE(graphicsAllocation.peekWasUsed());
    graphicsAllocation.updateTaskCount(ObjectNotUsed, 0u);
    EXPECT_FALSE(graphicsAllocation.peekWasUsed());
}
TEST(GraphicsAllocationTest, whenTwoContextsUpdatedTaskCountAndOneOfThemUnregisteredThenOneContextUsageRemains) {
    GraphicsAllocation graphicsAllocation(nullptr, 0u, 0u);
    EXPECT_FALSE(graphicsAllocation.peekWasUsed());
    graphicsAllocation.updateTaskCount(0u, 0u);
    graphicsAllocation.updateTaskCount(0u, 1u);
    EXPECT_TRUE(graphicsAllocation.peekWasUsed());
    graphicsAllocation.updateTaskCount(ObjectNotUsed, 0u);
    EXPECT_TRUE(graphicsAllocation.peekWasUsed());
    graphicsAllocation.updateTaskCount(ObjectNotUsed, 0u);
    EXPECT_TRUE(graphicsAllocation.peekWasUsed());
    graphicsAllocation.updateTaskCount(ObjectNotUsed, 1u);
    EXPECT_FALSE(graphicsAllocation.peekWasUsed());
}

TEST(GraphicsAllocationTest, givenGraphicsAllocationWhenUpdatedResidencyTaskCountToNonDefaultValueThenAllocationIsResident) {
    GraphicsAllocation graphicsAllocation(nullptr, 0u, 0u);
    EXPECT_FALSE(graphicsAllocation.isResident(0u));
    int residencyTaskCount = 1;
    graphicsAllocation.updateResidencyTaskCount(residencyTaskCount, 0u);
    EXPECT_EQ(residencyTaskCount, graphicsAllocation.getResidencyTaskCount(0u));
    EXPECT_TRUE(graphicsAllocation.isResident(0u));
    graphicsAllocation.updateResidencyTaskCount(ObjectNotResident, 0u);
    EXPECT_EQ(ObjectNotResident, graphicsAllocation.getResidencyTaskCount(0u));
    EXPECT_FALSE(graphicsAllocation.isResident(0u));
}

TEST(GraphicsAllocationTest, givenResidentGraphicsAllocationWhenResetResidencyTaskCountThenAllocationIsNotResident) {
    GraphicsAllocation graphicsAllocation(nullptr, 0u, 0u);
    graphicsAllocation.updateResidencyTaskCount(1, 0u);
    EXPECT_TRUE(graphicsAllocation.isResident(0u));

    graphicsAllocation.resetResidencyTaskCount(0u);
    EXPECT_FALSE(graphicsAllocation.isResident(0u));
}
