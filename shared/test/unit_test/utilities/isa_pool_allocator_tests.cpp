/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

using namespace NEO;

using IsaPoolAllocatorTest = Test<DeviceFixture>;

void verifySharedIsaAllocation(const SharedIsaAllocation *sharedAllocation, size_t expectedOffset, size_t expectedSize) {
    ASSERT_NE(nullptr, sharedAllocation);
    EXPECT_NE(nullptr, sharedAllocation->getGraphicsAllocation());
    EXPECT_EQ(expectedOffset, sharedAllocation->getOffset());
    EXPECT_EQ(expectedSize, sharedAllocation->getSize());
}

TEST_F(IsaPoolAllocatorTest, givenIsaPoolAllocatorWhenNoAllocationsThenCreateNewAllocationPerIsaType) {
    auto &isaAllocator = pDevice->getIsaPoolAllocator();
    constexpr size_t requestAllocationSize = MemoryConstants::pageSize;

    // Allocate for user and builtin and verify that these are different allocations
    auto userAllocation = isaAllocator.requestGraphicsAllocationForIsa(false, requestAllocationSize);
    verifySharedIsaAllocation(userAllocation, 0ul, requestAllocationSize);
    EXPECT_EQ(AllocationType::kernelIsa, userAllocation->getGraphicsAllocation()->getAllocationType());

    auto builtinAllocation = isaAllocator.requestGraphicsAllocationForIsa(true, requestAllocationSize);
    verifySharedIsaAllocation(builtinAllocation, 0ul, requestAllocationSize);
    EXPECT_EQ(AllocationType::kernelIsaInternal, builtinAllocation->getGraphicsAllocation()->getAllocationType());

    EXPECT_NE(userAllocation->obtainSharedAllocationLock().mutex(), builtinAllocation->obtainSharedAllocationLock().mutex());
    EXPECT_NE(userAllocation->getGraphicsAllocation(), builtinAllocation->getGraphicsAllocation());
    isaAllocator.freeSharedIsaAllocation(userAllocation);
    isaAllocator.freeSharedIsaAllocation(builtinAllocation);
}

TEST_F(IsaPoolAllocatorTest, givenIsaPoolAllocatorWhenAllocationsExistThenReuseAllocation) {
    auto &isaAllocator = pDevice->getIsaPoolAllocator();

    constexpr size_t requestAllocationSize = MemoryConstants::pageSize;

    auto allocation = isaAllocator.requestGraphicsAllocationForIsa(false, requestAllocationSize);
    verifySharedIsaAllocation(allocation, 0ul, requestAllocationSize);

    auto allocationSize = allocation->getGraphicsAllocation()->getUnderlyingBufferSize();
    auto numOfSharedAllocations = allocationSize / requestAllocationSize;

    // Perform requests until allocation is full
    for (auto i = 1u; i < numOfSharedAllocations; i++) {
        auto tempSharedAllocation = isaAllocator.requestGraphicsAllocationForIsa(false, requestAllocationSize);
        verifySharedIsaAllocation(tempSharedAllocation, requestAllocationSize * (i), requestAllocationSize);
        EXPECT_EQ(allocation->getGraphicsAllocation(), tempSharedAllocation->getGraphicsAllocation());
        isaAllocator.freeSharedIsaAllocation(tempSharedAllocation);
    }

    // Verify that draining freed chunks is correct and allocation can be reused
    auto newAllocation = isaAllocator.requestGraphicsAllocationForIsa(false, requestAllocationSize);
    verifySharedIsaAllocation(newAllocation, requestAllocationSize, requestAllocationSize);
    EXPECT_EQ(allocation->getGraphicsAllocation(), newAllocation->getGraphicsAllocation());

    isaAllocator.freeSharedIsaAllocation(newAllocation);
    isaAllocator.freeSharedIsaAllocation(allocation);
}

TEST_F(IsaPoolAllocatorTest, givenIsaPoolAllocatorWhenNoPoolAvailableThenCreateNewPool) {
    auto &isaAllocator = pDevice->getIsaPoolAllocator();

    constexpr size_t sharedIsaAllocationSize = MemoryConstants::pageSize2M * 2;
    constexpr size_t requestAllocationSize = sharedIsaAllocationSize / 2 + MemoryConstants::pageSize;

    constexpr size_t numOfAllocations = 5;
    SharedIsaAllocation *sharedAllocations[numOfAllocations];
    for (auto i = 0u; i < numOfAllocations; i++) {
        sharedAllocations[i] = isaAllocator.requestGraphicsAllocationForIsa(false, requestAllocationSize);
        verifySharedIsaAllocation(sharedAllocations[i], 0, requestAllocationSize);
    }

    auto ga = sharedAllocations[0]->getGraphicsAllocation();
    for (auto i = 1u; i < numOfAllocations; i++) {
        EXPECT_NE(ga, sharedAllocations[i]->getGraphicsAllocation());
        isaAllocator.freeSharedIsaAllocation(sharedAllocations[i]);
    }
    isaAllocator.freeSharedIsaAllocation(sharedAllocations[0]);
}

TEST_F(IsaPoolAllocatorTest, givenIsaPoolAllocatorWhenRequestForLargeAllocationThenAllocatedSuccessfully) {
    constexpr size_t sharedIsaAllocationSize = MemoryConstants::pageSize2M * 2;
    constexpr size_t requestAllocationSize = sharedIsaAllocationSize * 2;

    auto &isaAllocator = pDevice->getIsaPoolAllocator();
    auto allocation = isaAllocator.requestGraphicsAllocationForIsa(false, requestAllocationSize);
    verifySharedIsaAllocation(allocation, 0, requestAllocationSize);
    isaAllocator.freeSharedIsaAllocation(allocation);
}
