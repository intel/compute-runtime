/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/mock_graphics_allocation.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(GraphicsAllocationTest, givenGraphicsAllocationWhenIsCreatedThenAllInspectionIdsAreSetToZero) {
    MockGraphicsAllocation graphicsAllocation(GraphicsAllocation::AllocationType::UNKNOWN, nullptr, 0u, 0u, maxOsContextCount, MemoryPool::MemoryNull, true);
    for (auto i = 0u; i < maxOsContextCount; i++) {
        EXPECT_EQ(0u, graphicsAllocation.getInspectionId(i));
    }
}

TEST(GraphicsAllocationTest, givenGraphicsAllocationWhenIsCreatedThenTaskCountsAreInitializedProperly) {
    GraphicsAllocation graphicsAllocation1(GraphicsAllocation::AllocationType::UNKNOWN, nullptr, 0u, 0u, 0u, MemoryPool::MemoryNull, true);
    GraphicsAllocation graphicsAllocation2(GraphicsAllocation::AllocationType::UNKNOWN, nullptr, 0u, 0u, MemoryPool::MemoryNull, true);
    for (auto i = 0u; i < maxOsContextCount; i++) {
        EXPECT_EQ(MockGraphicsAllocation::objectNotUsed, graphicsAllocation1.getTaskCount(i));
        EXPECT_EQ(MockGraphicsAllocation::objectNotUsed, graphicsAllocation2.getTaskCount(i));
        EXPECT_EQ(MockGraphicsAllocation::objectNotResident, graphicsAllocation1.getResidencyTaskCount(i));
        EXPECT_EQ(MockGraphicsAllocation::objectNotResident, graphicsAllocation2.getResidencyTaskCount(i));
    }
}

TEST(GraphicsAllocationTest, givenGraphicsAllocationWhenUpdatedTaskCountThenAllocationWasUsed) {
    MockGraphicsAllocation graphicsAllocation;
    EXPECT_FALSE(graphicsAllocation.isUsed());
    graphicsAllocation.updateTaskCount(0u, 0u);
    EXPECT_TRUE(graphicsAllocation.isUsed());
}
TEST(GraphicsAllocationTest, givenGraphicsAllocationWhenUpdatedTaskCountThenOnlyOneTaskCountIsUpdated) {
    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.updateTaskCount(1u, 0u);
    EXPECT_EQ(1u, graphicsAllocation.getTaskCount(0u));
    for (auto i = 1u; i < maxOsContextCount; i++) {
        EXPECT_EQ(MockGraphicsAllocation::objectNotUsed, graphicsAllocation.getTaskCount(i));
    }
    graphicsAllocation.updateTaskCount(2u, 1u);
    EXPECT_EQ(1u, graphicsAllocation.getTaskCount(0u));
    EXPECT_EQ(2u, graphicsAllocation.getTaskCount(1u));
    for (auto i = 2u; i < maxOsContextCount; i++) {
        EXPECT_EQ(MockGraphicsAllocation::objectNotUsed, graphicsAllocation.getTaskCount(i));
    }
}
TEST(GraphicsAllocationTest, givenGraphicsAllocationWhenUpdatedTaskCountToobjectNotUsedValueThenUnregisterContext) {
    MockGraphicsAllocation graphicsAllocation;
    EXPECT_FALSE(graphicsAllocation.isUsed());
    graphicsAllocation.updateTaskCount(0u, 0u);
    EXPECT_TRUE(graphicsAllocation.isUsed());
    graphicsAllocation.updateTaskCount(MockGraphicsAllocation::objectNotUsed, 0u);
    EXPECT_FALSE(graphicsAllocation.isUsed());
}
TEST(GraphicsAllocationTest, whenTwoContextsUpdatedTaskCountAndOneOfThemUnregisteredThenOneContextUsageRemains) {
    MockGraphicsAllocation graphicsAllocation;
    EXPECT_FALSE(graphicsAllocation.isUsed());
    graphicsAllocation.updateTaskCount(0u, 0u);
    graphicsAllocation.updateTaskCount(0u, 1u);
    EXPECT_TRUE(graphicsAllocation.isUsed());
    graphicsAllocation.updateTaskCount(MockGraphicsAllocation::objectNotUsed, 0u);
    EXPECT_TRUE(graphicsAllocation.isUsed());
    graphicsAllocation.updateTaskCount(MockGraphicsAllocation::objectNotUsed, 0u);
    EXPECT_TRUE(graphicsAllocation.isUsed());
    graphicsAllocation.updateTaskCount(MockGraphicsAllocation::objectNotUsed, 1u);
    EXPECT_FALSE(graphicsAllocation.isUsed());
}

TEST(GraphicsAllocationTest, givenGraphicsAllocationWhenUpdatedResidencyTaskCountToNonDefaultValueThenAllocationIsResident) {
    MockGraphicsAllocation graphicsAllocation;
    EXPECT_FALSE(graphicsAllocation.isResident(0u));
    uint32_t residencyTaskCount = 1u;
    graphicsAllocation.updateResidencyTaskCount(residencyTaskCount, 0u);
    EXPECT_EQ(residencyTaskCount, graphicsAllocation.getResidencyTaskCount(0u));
    EXPECT_TRUE(graphicsAllocation.isResident(0u));
    graphicsAllocation.updateResidencyTaskCount(MockGraphicsAllocation::objectNotResident, 0u);
    EXPECT_EQ(MockGraphicsAllocation::objectNotResident, graphicsAllocation.getResidencyTaskCount(0u));
    EXPECT_FALSE(graphicsAllocation.isResident(0u));
}

TEST(GraphicsAllocationTest, givenResidentGraphicsAllocationWhenResetResidencyTaskCountThenAllocationIsNotResident) {
    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.updateResidencyTaskCount(1u, 0u);
    EXPECT_TRUE(graphicsAllocation.isResident(0u));

    graphicsAllocation.releaseResidencyInOsContext(0u);
    EXPECT_FALSE(graphicsAllocation.isResident(0u));
}

TEST(GraphicsAllocationTest, givenNonResidentGraphicsAllocationWhenCheckIfResidencyTaskCountIsBelowAnyValueThenReturnTrue) {
    MockGraphicsAllocation graphicsAllocation;
    EXPECT_FALSE(graphicsAllocation.isResident(0u));
    EXPECT_TRUE(graphicsAllocation.isResidencyTaskCountBelow(0u, 0u));
}

TEST(GraphicsAllocationTest, givenResidentGraphicsAllocationWhenCheckIfResidencyTaskCountIsBelowCurrentResidencyTaskCountThenReturnFalse) {
    MockGraphicsAllocation graphicsAllocation;
    auto currentResidencyTaskCount = 1u;
    graphicsAllocation.updateResidencyTaskCount(currentResidencyTaskCount, 0u);
    EXPECT_TRUE(graphicsAllocation.isResident(0u));
    EXPECT_FALSE(graphicsAllocation.isResidencyTaskCountBelow(currentResidencyTaskCount, 0u));
}

TEST(GraphicsAllocationTest, givenResidentGraphicsAllocationWhenCheckIfResidencyTaskCountIsBelowHigherThanCurrentResidencyTaskCountThenReturnTrue) {
    MockGraphicsAllocation graphicsAllocation;
    auto currentResidencyTaskCount = 1u;
    graphicsAllocation.updateResidencyTaskCount(currentResidencyTaskCount, 0u);
    EXPECT_TRUE(graphicsAllocation.isResident(0u));
    EXPECT_TRUE(graphicsAllocation.isResidencyTaskCountBelow(currentResidencyTaskCount + 1u, 0u));
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsLinearStreamThenCpuAccessIsRequired) {
    EXPECT_TRUE(GraphicsAllocation::isCpuAccessRequired(GraphicsAllocation::AllocationType::LINEAR_STREAM));
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsKernelIsaThenCpuAccessIsNotRequired) {
    EXPECT_FALSE(GraphicsAllocation::isCpuAccessRequired(GraphicsAllocation::AllocationType::KERNEL_ISA));
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsInternalHeapThenCpuAccessIsRequired) {
    EXPECT_TRUE(GraphicsAllocation::isCpuAccessRequired(GraphicsAllocation::AllocationType::INTERNAL_HEAP));
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsTimestampPacketThenCpuAccessIsRequired) {
    EXPECT_TRUE(GraphicsAllocation::isCpuAccessRequired(GraphicsAllocation::AllocationType::TIMESTAMP_PACKET_TAG_BUFFER));
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsCommandBufferThenCpuAccessIsRequired) {
    EXPECT_TRUE(GraphicsAllocation::isCpuAccessRequired(GraphicsAllocation::AllocationType::COMMAND_BUFFER));
}

TEST(GraphicsAllocationTest, givenDefaultAllocationWhenGettingNumHandlesThenOneIsReturned) {
    MockGraphicsAllocation graphicsAllocation;
    EXPECT_EQ(1u, graphicsAllocation.getNumHandles());
}
