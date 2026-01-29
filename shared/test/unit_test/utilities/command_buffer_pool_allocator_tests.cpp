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
#include "shared/source/utilities/command_buffer_pool_allocator.h"
#include "shared/source/utilities/heap_allocator.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

using namespace NEO;

class MockCommandBufferPool : public CommandBufferPool {
  public:
    using CommandBufferPool::chunkAllocator;
    using CommandBufferPool::mainStorage;

    MockCommandBufferPool(Device *device, size_t storageSize) : CommandBufferPool(device, storageSize) {}
};

class MockCommandBufferPoolAllocator : public CommandBufferPoolAllocator {
  public:
    using CommandBufferPoolAllocator::bufferPools;

    MockCommandBufferPoolAllocator(Device *device) : CommandBufferPoolAllocator(device) {}
};

using CommandBufferPoolAllocatorTest = Test<DeviceFixture>;

TEST_F(CommandBufferPoolAllocatorTest, givenCommandBufferPoolAllocatorWhenAllocatingThenReturnsValidAllocation) {
    MockCommandBufferPoolAllocator allocator(pDevice);
    constexpr size_t requestSize = MemoryConstants::pageSize;

    auto allocation = allocator.allocateCommandBuffer(requestSize);

    ASSERT_NE(nullptr, allocation);
    EXPECT_TRUE(allocation->isView());
    EXPECT_NE(nullptr, allocation->getParentAllocation());
    EXPECT_EQ(requestSize, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(AllocationType::commandBuffer, allocation->getAllocationType());

    allocator.freeCommandBuffer(allocation);
}

TEST_F(CommandBufferPoolAllocatorTest, givenCommandBufferPoolAllocatorWhenAllocatingMultipleThenReusePool) {
    MockCommandBufferPoolAllocator allocator(pDevice);
    constexpr size_t requestSize = MemoryConstants::pageSize;

    auto allocation1 = allocator.allocateCommandBuffer(requestSize);
    auto allocation2 = allocator.allocateCommandBuffer(requestSize);

    ASSERT_NE(nullptr, allocation1);
    ASSERT_NE(nullptr, allocation2);
    EXPECT_EQ(allocation1->getParentAllocation(), allocation2->getParentAllocation());
    EXPECT_NE(allocation1->getGpuAddress(), allocation2->getGpuAddress());

    allocator.freeCommandBuffer(allocation1);
    allocator.freeCommandBuffer(allocation2);
}

TEST_F(CommandBufferPoolAllocatorTest, givenCommandBufferPoolAllocatorWhenFreedThenPoolCanStillBeUsed) {
    MockCommandBufferPoolAllocator allocator(pDevice);
    constexpr size_t requestSize = MemoryConstants::pageSize;

    auto allocation1 = allocator.allocateCommandBuffer(requestSize);
    ASSERT_NE(nullptr, allocation1);
    auto parentAllocation = allocation1->getParentAllocation();

    allocator.freeCommandBuffer(allocation1);

    auto allocation2 = allocator.allocateCommandBuffer(requestSize);
    ASSERT_NE(nullptr, allocation2);
    EXPECT_EQ(parentAllocation, allocation2->getParentAllocation());
    EXPECT_EQ(1u, allocator.bufferPools.size());

    allocator.freeCommandBuffer(allocation2);
}

TEST_F(CommandBufferPoolAllocatorTest, givenCommandBufferPoolAllocatorWhenRequestTooLargeThenReturnsNull) {
    MockCommandBufferPoolAllocator allocator(pDevice);
    constexpr size_t tooLargeSize = MemoryConstants::pageSize2M + 1;

    auto allocation = allocator.allocateCommandBuffer(tooLargeSize);

    EXPECT_EQ(nullptr, allocation);
}

TEST_F(CommandBufferPoolAllocatorTest, givenCommandBufferPoolAllocatorWhenFreeingNullThenDoesNotCrash) {
    MockCommandBufferPoolAllocator allocator(pDevice);

    allocator.freeCommandBuffer(nullptr);
}

TEST_F(CommandBufferPoolAllocatorTest, givenCommandBufferPoolAllocatorWhenFreeingNonViewAllocationThenDoesNotFree) {
    MockCommandBufferPoolAllocator allocator(pDevice);

    AllocationProperties allocProperties = {pDevice->getRootDeviceIndex(),
                                            true,
                                            MemoryConstants::pageSize,
                                            AllocationType::commandBuffer,
                                            false,
                                            false,
                                            pDevice->getDeviceBitfield()};

    auto nonViewAllocation = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(allocProperties);
    ASSERT_NE(nullptr, nonViewAllocation);
    EXPECT_FALSE(nonViewAllocation->isView());

    allocator.freeCommandBuffer(nonViewAllocation);
    pDevice->getMemoryManager()->freeGraphicsMemory(nonViewAllocation);
}

TEST_F(CommandBufferPoolAllocatorTest, givenCommandBufferPoolWhenCreatedThenPoolSizeIsAlignedTo2MB) {
    MockCommandBufferPoolAllocator allocator(pDevice);

    auto allocation = allocator.allocateCommandBuffer(MemoryConstants::pageSize);
    ASSERT_NE(nullptr, allocation);

    ASSERT_GE(allocator.bufferPools.size(), 1u);
    auto *pool = static_cast<MockCommandBufferPool *>(&allocator.bufferPools[0]);

    auto poolSize = pool->mainStorage->getUnderlyingBufferSize();

    EXPECT_TRUE(isAligned(poolSize, MemoryConstants::pageSize2M));

    allocator.freeCommandBuffer(allocation);
}

TEST_F(CommandBufferPoolAllocatorTest, givenCommandBufferPoolWhenPoolExhaustedThenNewPoolIsCreated) {
    MockCommandBufferPoolAllocator allocator(pDevice);

    // Create initial pool on first allocation
    auto firstAllocation = allocator.allocateCommandBuffer(MemoryConstants::pageSize);
    ASSERT_NE(nullptr, firstAllocation);
    ASSERT_EQ(1u, allocator.bufferPools.size());

    auto *pool = static_cast<MockCommandBufferPool *>(&allocator.bufferPools[0]);
    auto poolSize = pool->mainStorage->getUnderlyingBufferSize();

    constexpr size_t largeRequestSize = MemoryConstants::pageSize2M / 2 + MemoryConstants::pageSize;
    size_t numAllocationsToExhaust = poolSize / largeRequestSize + 1;

    std::vector<GraphicsAllocation *> allocations;
    allocations.push_back(firstAllocation);

    for (size_t i = 1; i < numAllocationsToExhaust + 1; i++) {
        auto allocation = allocator.allocateCommandBuffer(largeRequestSize);
        if (allocation) {
            allocations.push_back(allocation);
        }
    }

    EXPECT_GT(allocator.bufferPools.size(), 1u);

    for (auto alloc : allocations) {
        allocator.freeCommandBuffer(alloc);
    }
}

TEST_F(CommandBufferPoolAllocatorTest, givenCommandBufferPoolWhenIsPoolBufferCalledThenReturnsTrueForPoolAllocations) {
    MockCommandBufferPoolAllocator allocator(pDevice);

    auto poolAllocation = allocator.allocateCommandBuffer(MemoryConstants::pageSize);
    ASSERT_NE(nullptr, poolAllocation);

    EXPECT_TRUE(allocator.isPoolBuffer(poolAllocation->getParentAllocation()));

    AllocationProperties allocProperties = {pDevice->getRootDeviceIndex(),
                                            true,
                                            MemoryConstants::pageSize,
                                            AllocationType::commandBuffer,
                                            false,
                                            false,
                                            pDevice->getDeviceBitfield()};

    auto nonPoolAllocation = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(allocProperties);
    EXPECT_FALSE(allocator.isPoolBuffer(nonPoolAllocation));

    allocator.freeCommandBuffer(poolAllocation);
    pDevice->getMemoryManager()->freeGraphicsMemory(nonPoolAllocation);
}

TEST_F(CommandBufferPoolAllocatorTest, givenDeviceWhenGettingCommandBufferPoolAllocatorThenReturnsValidAllocator) {
    auto &allocator = pDevice->getCommandBufferPoolAllocator();

    auto allocation = allocator.allocateCommandBuffer(MemoryConstants::pageSize);
    ASSERT_NE(nullptr, allocation);
    EXPECT_TRUE(allocation->isView());

    allocator.freeCommandBuffer(allocation);
}

TEST_F(CommandBufferPoolAllocatorTest, givenCommandBufferPoolAllocatorWhenPoolExhaustedAndChunksFreedThenDrainMakesSpaceAvailable) {
    MockCommandBufferPoolAllocator allocator(pDevice);

    auto firstAllocation = allocator.allocateCommandBuffer(MemoryConstants::pageSize);
    ASSERT_NE(nullptr, firstAllocation);
    ASSERT_EQ(1u, allocator.bufferPools.size());

    auto *pool = static_cast<MockCommandBufferPool *>(&allocator.bufferPools[0]);
    auto poolSize = pool->mainStorage->getUnderlyingBufferSize();

    // Use large chunks to quickly exhaust the pool
    constexpr size_t largeChunkSize = MemoryConstants::pageSize2M / 2 + MemoryConstants::pageSize;

    std::vector<GraphicsAllocation *> allocations;
    allocations.push_back(firstAllocation);

    // Fill the pool until we can't allocate largeChunkSize anymore
    size_t maxAllocations = poolSize / largeChunkSize;
    for (size_t i = 1; i < maxAllocations; i++) {
        auto allocation = allocator.allocateCommandBuffer(largeChunkSize);
        if (allocation && allocation->getParentAllocation() == pool->mainStorage.get()) {
            allocations.push_back(allocation);
        } else if (allocation) {
            // Allocated from new pool - pool is exhausted
            allocator.freeCommandBuffer(allocation);
            break;
        }
    }

    size_t poolCountBeforeFree = allocator.bufferPools.size();

    // Free one allocation - this goes to freed chunks, not immediately available
    if (allocations.size() > 1) {
        allocator.freeCommandBuffer(allocations.back());
        allocations.pop_back();
    }

    // Next allocation should trigger drain() and reuse the freed space
    auto newAllocation = allocator.allocateCommandBuffer(largeChunkSize);
    ASSERT_NE(nullptr, newAllocation);

    // Should not have created additional pools if drain() worked
    EXPECT_EQ(poolCountBeforeFree, allocator.bufferPools.size());

    allocator.freeCommandBuffer(newAllocation);
    for (auto alloc : allocations) {
        allocator.freeCommandBuffer(alloc);
    }
}

class MockMemoryManagerTrackRemoveDownloadAllocation : public OsAgnosticMemoryManager {
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

TEST_F(CommandBufferPoolAllocatorTest, givenCommandBufferPoolAllocatorWhenFreeingViewAllocationThenRemovesFromDownloadAllocationsInCsr) {
    auto mockMemoryManager = std::make_unique<MockMemoryManagerTrackRemoveDownloadAllocation>(*pDevice->getExecutionEnvironment());
    auto *trackingMemoryManager = mockMemoryManager.get();

    std::unique_ptr<MemoryManager> originalMemoryManager;
    originalMemoryManager.reset(pDevice->executionEnvironment->memoryManager.release());
    pDevice->executionEnvironment->memoryManager = std::move(mockMemoryManager);

    {
        MockCommandBufferPoolAllocator allocator(pDevice);
        constexpr size_t requestSize = MemoryConstants::pageSize;

        auto allocation = allocator.allocateCommandBuffer(requestSize);
        ASSERT_NE(nullptr, allocation);
        EXPECT_TRUE(allocation->isView());

        EXPECT_EQ(0u, trackingMemoryManager->removeAllocationFromDownloadAllocationsInCsrCalled);

        auto allocationPtr = allocation;
        allocator.freeCommandBuffer(allocation);

        EXPECT_EQ(1u, trackingMemoryManager->removeAllocationFromDownloadAllocationsInCsrCalled);
        EXPECT_EQ(allocationPtr, trackingMemoryManager->lastRemovedAllocation);
    }

    pDevice->executionEnvironment->memoryManager = std::move(originalMemoryManager);
}