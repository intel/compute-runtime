/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/source/utilities/buffer_pool_allocator.inl"
#include "shared/source/utilities/heap_allocator.h"
#include "shared/source/utilities/linear_stream_pool_allocator.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

using namespace NEO;

class MockLinearStreamPool : public LinearStreamPool {
  public:
    using LinearStreamPool::chunkAllocator;
    using LinearStreamPool::mainStorage;

    MockLinearStreamPool(Device *device, size_t storageSize) : LinearStreamPool(device, storageSize) {}
};

class MockLinearStreamPoolAllocator : public LinearStreamPoolAllocator {
  public:
    using LinearStreamPoolAllocator::bufferPools;

    MockLinearStreamPoolAllocator(Device *device) : LinearStreamPoolAllocator(device) {}
};

using LinearStreamPoolAllocatorTest = Test<DeviceFixture>;

TEST_F(LinearStreamPoolAllocatorTest, givenLinearStreamPoolAllocatorWhenAllocatingThenReturnsValidAllocation) {
    MockLinearStreamPoolAllocator allocator(pDevice);
    constexpr size_t requestSize = MemoryConstants::pageSize;

    auto allocation = allocator.allocateLinearStream(requestSize);

    ASSERT_NE(nullptr, allocation);
    EXPECT_TRUE(allocation->isView());
    EXPECT_NE(nullptr, allocation->getParentAllocation());
    EXPECT_EQ(requestSize, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(AllocationType::linearStream, allocation->getAllocationType());

    allocator.freeLinearStream(allocation);
}

TEST_F(LinearStreamPoolAllocatorTest, givenLinearStreamPoolAllocatorWhenAllocatingMultipleThenReusePool) {
    MockLinearStreamPoolAllocator allocator(pDevice);
    constexpr size_t requestSize = MemoryConstants::pageSize;

    auto allocation1 = allocator.allocateLinearStream(requestSize);
    auto allocation2 = allocator.allocateLinearStream(requestSize);

    ASSERT_NE(nullptr, allocation1);
    ASSERT_NE(nullptr, allocation2);
    // Both views should share the same parent pool allocation
    // But have different GPU addresses, different offsets in pool
    EXPECT_EQ(allocation1->getParentAllocation(), allocation2->getParentAllocation());
    EXPECT_NE(allocation1->getGpuAddress(), allocation2->getGpuAddress());

    allocator.freeLinearStream(allocation1);
    allocator.freeLinearStream(allocation2);
}

TEST_F(LinearStreamPoolAllocatorTest, givenLinearStreamPoolAllocatorWhenFreedThenPoolCanStillBeUsed) {
    MockLinearStreamPoolAllocator allocator(pDevice);
    constexpr size_t requestSize = MemoryConstants::pageSize;

    auto allocation1 = allocator.allocateLinearStream(requestSize);
    ASSERT_NE(nullptr, allocation1);
    auto parentAllocation = allocation1->getParentAllocation();

    allocator.freeLinearStream(allocation1);

    // Allocate again, should reuse same pool parent
    auto allocation2 = allocator.allocateLinearStream(requestSize);
    ASSERT_NE(nullptr, allocation2);
    EXPECT_EQ(parentAllocation, allocation2->getParentAllocation());
    EXPECT_EQ(1u, allocator.bufferPools.size());

    allocator.freeLinearStream(allocation2);
}

TEST_F(LinearStreamPoolAllocatorTest, givenLinearStreamPoolAllocatorWhenRequestTooLargeThenReturnsNull) {
    MockLinearStreamPoolAllocator allocator(pDevice);
    constexpr size_t tooLargeSize = MemoryConstants::pageSize2M + 1;

    // Request size exceeds maxRequestedSize, allocation should fail
    auto allocation = allocator.allocateLinearStream(tooLargeSize);

    EXPECT_EQ(nullptr, allocation);
}

TEST_F(LinearStreamPoolAllocatorTest, givenLinearStreamPoolAllocatorWhenFreeingNullThenDoesNotCrash) {
    MockLinearStreamPoolAllocator allocator(pDevice);

    allocator.freeLinearStream(nullptr);
}

TEST_F(LinearStreamPoolAllocatorTest, givenLinearStreamPoolAllocatorWhenFreeingNonViewThenDoesNotCrash) {
    MockLinearStreamPoolAllocator allocator(pDevice);

    auto memoryManager = pDevice->getMemoryManager();
    AllocationProperties allocProperties = {pDevice->getRootDeviceIndex(),
                                            MemoryConstants::pageSize,
                                            AllocationType::buffer,
                                            pDevice->getDeviceBitfield()};
    auto nonViewAllocation = memoryManager->allocateGraphicsMemoryWithProperties(allocProperties);

    allocator.freeLinearStream(nonViewAllocation);
    memoryManager->freeGraphicsMemory(nonViewAllocation);
}

TEST_F(LinearStreamPoolAllocatorTest, givenLinearStreamPoolWhenCreatingWithDifferentSizesThenAllocationsHaveCorrectSizes) {
    MockLinearStreamPoolAllocator allocator(pDevice);

    constexpr size_t size1 = MemoryConstants::pageSize;
    constexpr size_t size2 = MemoryConstants::pageSize * 2;
    constexpr size_t size3 = MemoryConstants::pageSize64k;

    auto allocation1 = allocator.allocateLinearStream(size1);
    auto allocation2 = allocator.allocateLinearStream(size2);
    auto allocation3 = allocator.allocateLinearStream(size3);

    ASSERT_NE(nullptr, allocation1);
    ASSERT_NE(nullptr, allocation2);
    ASSERT_NE(nullptr, allocation3);

    EXPECT_EQ(size1, allocation1->getUnderlyingBufferSize());
    EXPECT_EQ(size2, allocation2->getUnderlyingBufferSize());
    EXPECT_EQ(size3, allocation3->getUnderlyingBufferSize());

    allocator.freeLinearStream(allocation1);
    allocator.freeLinearStream(allocation2);
    allocator.freeLinearStream(allocation3);
}

TEST_F(LinearStreamPoolAllocatorTest, givenLinearStreamPoolAllocatorWhenAllocatingFromEmptyThenCreatesNewPool) {
    MockLinearStreamPoolAllocator allocator(pDevice);

    EXPECT_EQ(0u, allocator.bufferPools.size());

    auto allocation = allocator.allocateLinearStream(MemoryConstants::pageSize);

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(1u, allocator.bufferPools.size());

    allocator.freeLinearStream(allocation);
}

TEST_F(LinearStreamPoolAllocatorTest, givenLinearStreamPoolAllocatorWhenPoolExhaustedThenCreatesNewPool) {
    MockLinearStreamPoolAllocator allocator(pDevice);

    std::vector<GraphicsAllocation *> allocations;
    size_t totalAllocated = 0;
    constexpr size_t requestSize = MemoryConstants::pageSize64k;

    // Keep allocating until pool is exhausted
    while (totalAllocated < MemoryConstants::pageSize2M * 2) {
        auto allocation = allocator.allocateLinearStream(requestSize);
        if (!allocation) {
            break;
        }
        allocations.push_back(allocation);
        totalAllocated += requestSize;
    }

    EXPECT_GT(allocations.size(), 0u);

    for (auto allocation : allocations) {
        allocator.freeLinearStream(allocation);
    }
}

TEST_F(LinearStreamPoolAllocatorTest, givenLinearStreamPoolWhenAllocatingViewThenParentAllocationTracksTaskCount) {
    MockLinearStreamPoolAllocator allocator(pDevice);

    auto allocation = allocator.allocateLinearStream(MemoryConstants::pageSize);
    ASSERT_NE(nullptr, allocation);
    ASSERT_TRUE(allocation->isView());

    auto parent = allocation->getParentAllocation();
    ASSERT_NE(nullptr, parent);

    // Update view's task count, should propagate to parent
    allocation->updateTaskCount(100u, 0u);

    EXPECT_EQ(100u, parent->getTaskCount(0u));

    allocator.freeLinearStream(allocation);
}

TEST_F(LinearStreamPoolAllocatorTest, givenLinearStreamPoolAllocatorWhenPoolExhaustedAndFreedThenDrainAllowsReuse) {
    MockLinearStreamPoolAllocator allocator(pDevice);
    constexpr size_t requestSize = MemoryConstants::pageSize64k;
    std::vector<GraphicsAllocation *> allocations;

    // Fill the first pool completely
    size_t totalAllocated = 0;
    constexpr size_t poolSize = MemoryConstants::pageSize2M * 2;
    while (totalAllocated < poolSize) {
        auto allocation = allocator.allocateLinearStream(requestSize);
        if (!allocation) {
            break;
        }
        allocations.push_back(allocation);
        totalAllocated += requestSize;
    }

    size_t initialPoolCount = allocator.bufferPools.size();
    EXPECT_GE(initialPoolCount, 1u);

    // Free half of the allocations to create freed space
    size_t freedCount = allocations.size() / 2;
    for (size_t i = 0; i < freedCount; i++) {
        allocator.freeLinearStream(allocations[i]);
    }

    // This allocation should trigger drain() to reclaim freed space
    auto reuseAllocation = allocator.allocateLinearStream(requestSize);
    ASSERT_NE(nullptr, reuseAllocation);

    // Verify we reused space from the same pool - didn't create new pool
    EXPECT_EQ(initialPoolCount, allocator.bufferPools.size());

    allocator.freeLinearStream(reuseAllocation);

    for (size_t i = freedCount; i < allocations.size(); i++) {
        allocator.freeLinearStream(allocations[i]);
    }
}

TEST_F(LinearStreamPoolAllocatorTest, givenLinearStreamPoolAllocatorWhenFirstPoolFullThenAllocatesFromSecondPool) {
    MockLinearStreamPoolAllocator allocator(pDevice);
    constexpr size_t requestSize = MemoryConstants::pageSize64k;
    std::vector<GraphicsAllocation *> allocations;

    // Fill the first pool completely, keep all allocations so pool stays full and cannot drain
    size_t totalAllocated = 0;
    constexpr size_t poolSize = MemoryConstants::pageSize2M * 2;
    while (totalAllocated < poolSize) {
        auto allocation = allocator.allocateLinearStream(requestSize);
        if (!allocation) {
            break;
        }
        allocations.push_back(allocation);
        totalAllocated += requestSize;
    }

    auto firstPoolSize = allocator.bufferPools.size();
    auto firstPoolParent = allocations[0]->getParentAllocation();

    // Allocate when first pool is full - should create second pool
    auto allocationFromSecondPool = allocator.allocateLinearStream(requestSize);
    ASSERT_NE(nullptr, allocationFromSecondPool);
    EXPECT_GT(allocator.bufferPools.size(), firstPoolSize);

    // Verify allocation is from different parent - different pool
    EXPECT_NE(firstPoolParent, allocationFromSecondPool->getParentAllocation());

    allocator.freeLinearStream(allocationFromSecondPool);

    for (auto allocation : allocations) {
        allocator.freeLinearStream(allocation);
    }
}

TEST_F(LinearStreamPoolAllocatorTest, givenLinearStreamPoolAllocatorWhenMultiplePoolsExistThenTryAllocateFindsSpaceInLaterPool) {
    MockLinearStreamPoolAllocator allocator(pDevice);
    constexpr size_t requestSize = MemoryConstants::pageSize64k;
    std::vector<GraphicsAllocation *> firstPoolAllocations;
    std::vector<GraphicsAllocation *> secondPoolAllocations;

    // Fill first pool, keep all allocations so pool stays full and cannot drain
    size_t totalAllocated = 0;
    constexpr size_t poolSize = MemoryConstants::pageSize2M * 2;
    while (totalAllocated < poolSize) {
        auto allocation = allocator.allocateLinearStream(requestSize);
        if (!allocation) {
            break;
        }
        firstPoolAllocations.push_back(allocation);
        totalAllocated += requestSize;
    }

    auto firstPoolParent = firstPoolAllocations[0]->getParentAllocation();

    // Create second pool by allocating when first is full
    auto firstFromSecondPool = allocator.allocateLinearStream(requestSize);
    ASSERT_NE(nullptr, firstFromSecondPool);
    secondPoolAllocations.push_back(firstFromSecondPool);

    auto secondPoolParent = firstFromSecondPool->getParentAllocation();
    EXPECT_NE(firstPoolParent, secondPoolParent);

    // Allocate more, tryAllocate() should skip full first pool and use second pool
    for (int i = 0; i < 5; i++) {
        auto allocation = allocator.allocateLinearStream(requestSize);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(secondPoolParent, allocation->getParentAllocation());
        secondPoolAllocations.push_back(allocation);
    }

    EXPECT_EQ(2u, allocator.bufferPools.size());

    for (auto allocation : firstPoolAllocations) {
        allocator.freeLinearStream(allocation);
    }
    for (auto allocation : secondPoolAllocations) {
        allocator.freeLinearStream(allocation);
    }
}

class MockMemoryManagerTrackRemoveDownloadAllocationLinearStream : public OsAgnosticMemoryManager {
  public:
    using OsAgnosticMemoryManager::OsAgnosticMemoryManager;

    void removeAllocationFromDownloadAllocationsInCsr(GraphicsAllocation *alloc) override {
        removeAllocationFromDownloadAllocationsInCsrCalled++;
        lastRemovedAllocation = alloc;
        OsAgnosticMemoryManager::removeAllocationFromDownloadAllocationsInCsr(alloc);
    }

    uint32_t removeAllocationFromDownloadAllocationsInCsrCalled = 0u;
    GraphicsAllocation *lastRemovedAllocation = nullptr;
};

TEST_F(LinearStreamPoolAllocatorTest, givenLinearStreamPoolAllocatorWhenFreeingViewAllocationThenRemovesFromDownloadAllocationsInCsr) {
    auto mockMemoryManager = std::make_unique<MockMemoryManagerTrackRemoveDownloadAllocationLinearStream>(*pDevice->getExecutionEnvironment());
    auto *trackingMemoryManager = mockMemoryManager.get();

    std::unique_ptr<MemoryManager> originalMemoryManager;
    originalMemoryManager.reset(pDevice->executionEnvironment->memoryManager.release());
    pDevice->executionEnvironment->memoryManager = std::move(mockMemoryManager);

    {
        MockLinearStreamPoolAllocator allocator(pDevice);
        constexpr size_t requestSize = MemoryConstants::pageSize;

        auto allocation = allocator.allocateLinearStream(requestSize);
        ASSERT_NE(nullptr, allocation);
        EXPECT_TRUE(allocation->isView());

        EXPECT_EQ(0u, trackingMemoryManager->removeAllocationFromDownloadAllocationsInCsrCalled);

        auto allocationPtr = allocation;
        allocator.freeLinearStream(allocation);

        EXPECT_EQ(1u, trackingMemoryManager->removeAllocationFromDownloadAllocationsInCsrCalled);
        EXPECT_EQ(allocationPtr, trackingMemoryManager->lastRemovedAllocation);
    }

    pDevice->executionEnvironment->memoryManager = std::move(originalMemoryManager);
}
