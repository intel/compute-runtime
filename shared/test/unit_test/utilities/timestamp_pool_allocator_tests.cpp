/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

using namespace NEO;

using TimestampPoolAllocatorTest = Test<DeviceFixture>;

namespace {
void verifySharedTimestampAllocation(const SharedTimestampAllocation *sharedAllocation,
                                     size_t expectedOffset,
                                     size_t expectedSize) {
    ASSERT_NE(nullptr, sharedAllocation);
    EXPECT_NE(nullptr, sharedAllocation->getGraphicsAllocation());
    EXPECT_EQ(expectedOffset, sharedAllocation->getOffset());
    EXPECT_EQ(expectedSize, sharedAllocation->getSize());
}
} // namespace

TEST_F(TimestampPoolAllocatorTest, givenTimestampPoolAllocatorWhenNoAllocationsThenCreateNewAllocation) {
    auto &timestampAllocator = pDevice->getDeviceTimestampPoolAllocator();
    constexpr size_t requestAllocationSize = MemoryConstants::pageSize;

    auto allocation = timestampAllocator.requestGraphicsAllocationForTimestamp(requestAllocationSize);
    verifySharedTimestampAllocation(allocation, 0ul, requestAllocationSize);
    EXPECT_EQ(AllocationType::gpuTimestampDeviceBuffer,
              allocation->getGraphicsAllocation()->getAllocationType());

    timestampAllocator.freeSharedTimestampAllocation(allocation);
}

TEST_F(TimestampPoolAllocatorTest, givenTimestampPoolAllocatorWhenAllocationsExistThenReuseAllocation) {
    auto &timestampAllocator = pDevice->getDeviceTimestampPoolAllocator();
    constexpr size_t requestAllocationSize = MemoryConstants::pageSize;

    auto allocation = timestampAllocator.requestGraphicsAllocationForTimestamp(requestAllocationSize);
    verifySharedTimestampAllocation(allocation, 0ul, requestAllocationSize);

    auto allocationSize = allocation->getGraphicsAllocation()->getUnderlyingBufferSize();
    auto numOfSharedAllocations = allocationSize / requestAllocationSize;

    // Perform requests until allocation is full
    for (auto i = 1u; i < numOfSharedAllocations; i++) {
        auto tempSharedAllocation = timestampAllocator.requestGraphicsAllocationForTimestamp(requestAllocationSize);
        verifySharedTimestampAllocation(tempSharedAllocation, requestAllocationSize * i, requestAllocationSize);
        EXPECT_EQ(allocation->getGraphicsAllocation(), tempSharedAllocation->getGraphicsAllocation());
        timestampAllocator.freeSharedTimestampAllocation(tempSharedAllocation);
    }

    // Verify that draining freed chunks is correct and allocation can be reused
    auto newAllocation = timestampAllocator.requestGraphicsAllocationForTimestamp(requestAllocationSize);
    verifySharedTimestampAllocation(newAllocation, requestAllocationSize, requestAllocationSize);
    EXPECT_EQ(allocation->getGraphicsAllocation(), newAllocation->getGraphicsAllocation());

    timestampAllocator.freeSharedTimestampAllocation(newAllocation);
    timestampAllocator.freeSharedTimestampAllocation(allocation);
}

TEST_F(TimestampPoolAllocatorTest, givenTimestampPoolAllocatorWhenPoolIsFullThenCreateNewPool) {
    // This test verifies that:
    // 1. First two allocations of size=poolSize/2 come from the same pool
    // 2. When pool becomes full (after two allocations), a new pool is created
    // 3. Third allocation comes from the new pool (different GraphicsAllocation)

    auto &timestampAllocator = pDevice->getDeviceTimestampPoolAllocator();

    // Request half of pool size to ensure exactly 2 allocations fit in one pool
    size_t requestAllocationSize = timestampAllocator.getDefaultPoolSize() / 2;

    // First allocation - should come from first pool
    auto allocation1 = timestampAllocator.requestGraphicsAllocationForTimestamp(requestAllocationSize);
    verifySharedTimestampAllocation(allocation1, 0, requestAllocationSize);

    // Second allocation - should come from first pool but with offset
    auto allocation2 = timestampAllocator.requestGraphicsAllocationForTimestamp(requestAllocationSize);
    verifySharedTimestampAllocation(allocation2, requestAllocationSize, requestAllocationSize);
    EXPECT_EQ(allocation1->getGraphicsAllocation(), allocation2->getGraphicsAllocation());

    // Third allocation - should create new pool because first one is full
    auto allocation3 = timestampAllocator.requestGraphicsAllocationForTimestamp(requestAllocationSize);
    verifySharedTimestampAllocation(allocation3, 0, requestAllocationSize);
    EXPECT_NE(allocation1->getGraphicsAllocation(), allocation3->getGraphicsAllocation());

    timestampAllocator.freeSharedTimestampAllocation(allocation1);
    timestampAllocator.freeSharedTimestampAllocation(allocation2);
    timestampAllocator.freeSharedTimestampAllocation(allocation3);
}

TEST_F(TimestampPoolAllocatorTest, givenTimestampPoolAllocatorWhenRequestExceedsMaxSizeThenReturnNull) {
    auto &timestampAllocator = pDevice->getDeviceTimestampPoolAllocator();
    constexpr size_t requestAllocationSize = 3 * MemoryConstants::megaByte; // Larger than maxAllocationSize

    auto allocation = timestampAllocator.requestGraphicsAllocationForTimestamp(requestAllocationSize);
    EXPECT_EQ(nullptr, allocation);
}

TEST_F(TimestampPoolAllocatorTest, whenCheckingIsEnabledWithDifferentSettingsThenReturnsExpectedValue) {
    auto mockProductHelper = new MockProductHelper;
    pDevice->getRootDeviceEnvironmentRef().productHelper.reset(mockProductHelper);
    auto &timestampAllocator = pDevice->getDeviceTimestampPoolAllocator();

    {
        debugManager.flags.EnableTimestampPoolAllocator.set(0);
        mockProductHelper->is2MBLocalMemAlignmentEnabledResult = true;

        EXPECT_FALSE(timestampAllocator.isEnabled());
    }
    {
        debugManager.flags.EnableTimestampPoolAllocator.set(-1);
        mockProductHelper->is2MBLocalMemAlignmentEnabledResult = false;

        EXPECT_FALSE(timestampAllocator.isEnabled());
    }
    {
        debugManager.flags.EnableTimestampPoolAllocator.set(1);
        mockProductHelper->is2MBLocalMemAlignmentEnabledResult = false;

        EXPECT_TRUE(timestampAllocator.isEnabled());
    }
    {
        debugManager.flags.EnableTimestampPoolAllocator.set(-1);
        mockProductHelper->is2MBLocalMemAlignmentEnabledResult = true;

        EXPECT_FALSE(timestampAllocator.isEnabled());
    }
}

TEST_F(TimestampPoolAllocatorTest, givenTimestampPoolAllocatorWhenPoolSizeAlignmentRequestedThenReturnsAlignedSize) {
    auto &timestampAllocator = pDevice->getDeviceTimestampPoolAllocator();
    constexpr size_t requestAllocationSize = MemoryConstants::pageSize;

    auto allocation = timestampAllocator.requestGraphicsAllocationForTimestamp(requestAllocationSize);
    ASSERT_NE(nullptr, allocation);

    auto allocationSize = allocation->getGraphicsAllocation()->getUnderlyingBufferSize();
    EXPECT_EQ(0u, allocationSize % MemoryConstants::pageSize2M);

    timestampAllocator.freeSharedTimestampAllocation(allocation);
}

TEST_F(TimestampPoolAllocatorTest, givenFailingMemoryManagerWhenRequestingAllocationThenReturnNull) {
    auto &timestampAllocator = pDevice->getDeviceTimestampPoolAllocator();

    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->isMockHostMemoryManager = true;
    memoryManager->forceFailureInPrimaryAllocation = true;

    size_t requestAllocationSize = timestampAllocator.getDefaultPoolSize() / 2;
    auto allocation = timestampAllocator.requestGraphicsAllocationForTimestamp(requestAllocationSize);
    EXPECT_EQ(nullptr, allocation);

    if (allocation) {
        timestampAllocator.freeSharedTimestampAllocation(allocation);
    }
}
