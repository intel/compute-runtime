/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/source/utilities/generic_pool_allocator.inl"
#include "shared/source/utilities/heap_allocator.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

using namespace NEO;

struct MockViewPoolTraits {
    static constexpr AllocationType allocationType = AllocationType::buffer;
    static constexpr size_t maxAllocationSize = MemoryConstants::pageSize2M;
    static constexpr size_t defaultPoolSize = 4 * MemoryConstants::pageSize2M;
    static constexpr size_t poolAlignment = MemoryConstants::pageSize2M;

    static AllocationProperties createAllocationProperties(Device *device, size_t poolSize) {
        return AllocationProperties{device->getRootDeviceIndex(),
                                    true,
                                    poolSize,
                                    allocationType,
                                    false,
                                    false,
                                    device->getDeviceBitfield()};
    }
};

template <typename Traits>
class MockGenericViewPool : public GenericViewPool<Traits> {
  public:
    using GenericViewPool<Traits>::chunkAllocator;
    using GenericViewPool<Traits>::mainStorage;

    MockGenericViewPool(Device *device, size_t storageSize) : GenericViewPool<Traits>(device, storageSize) {}
};

template <typename Traits>
class MockGenericViewPoolAllocator : public GenericViewPoolAllocator<Traits> {
  public:
    using GenericViewPoolAllocator<Traits>::bufferPools;

    MockGenericViewPoolAllocator(Device *device) : GenericViewPoolAllocator<Traits>(device) {}
};

using GenericViewPoolAllocatorTest = Test<DeviceFixture>;

TEST_F(GenericViewPoolAllocatorTest, givenAllocatorWhenAllocatingThenReturnsViewAllocationWithCorrectProperties) {
    MockGenericViewPoolAllocator<MockViewPoolTraits> allocator(pDevice);
    constexpr size_t requestSize = MemoryConstants::pageSize;

    auto allocation = allocator.allocate(requestSize);

    ASSERT_NE(nullptr, allocation);
    EXPECT_TRUE(allocation->isView());
    EXPECT_NE(nullptr, allocation->getParentAllocation());
    EXPECT_EQ(requestSize, allocation->getUnderlyingBufferSize());

    allocator.free(allocation);
}

TEST_F(GenericViewPoolAllocatorTest, givenAllocatorWhenRequestExceedsMaxAllocationSizeThenReturnsNull) {
    MockGenericViewPoolAllocator<MockViewPoolTraits> allocator(pDevice);
    constexpr size_t tooLargeSize = MockViewPoolTraits::maxAllocationSize + 1;

    auto allocation = allocator.allocate(tooLargeSize);

    EXPECT_EQ(nullptr, allocation);
}

TEST_F(GenericViewPoolAllocatorTest, givenEmptyAllocatorWhenAllocatingThenCreatesNewPool) {
    MockGenericViewPoolAllocator<MockViewPoolTraits> allocator(pDevice);

    EXPECT_EQ(0u, allocator.bufferPools.size());

    auto allocation = allocator.allocate(MemoryConstants::pageSize);

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(1u, allocator.bufferPools.size());

    allocator.free(allocation);
}

TEST_F(GenericViewPoolAllocatorTest, givenAllocatorWhenAllocatingMultipleTimesThenReusesSamePool) {
    MockGenericViewPoolAllocator<MockViewPoolTraits> allocator(pDevice);
    constexpr size_t requestSize = MemoryConstants::pageSize;

    auto allocation1 = allocator.allocate(requestSize);
    auto allocation2 = allocator.allocate(requestSize);

    ASSERT_NE(nullptr, allocation1);
    ASSERT_NE(nullptr, allocation2);
    EXPECT_EQ(allocation1->getParentAllocation(), allocation2->getParentAllocation());
    EXPECT_NE(allocation1->getGpuAddress(), allocation2->getGpuAddress());
    EXPECT_EQ(1u, allocator.bufferPools.size());

    allocator.free(allocation1);
    allocator.free(allocation2);
}

TEST_F(GenericViewPoolAllocatorTest, givenAllocatorWhenPoolExhaustedThenCreatesNewPool) {
    MockGenericViewPoolAllocator<MockViewPoolTraits> allocator(pDevice);

    auto firstAllocation = allocator.allocate(MemoryConstants::pageSize);
    ASSERT_NE(nullptr, firstAllocation);

    auto *pool = static_cast<MockGenericViewPool<MockViewPoolTraits> *>(&allocator.bufferPools[0]);
    auto poolSize = pool->mainStorage->getUnderlyingBufferSize();
    auto firstPoolParent = firstAllocation->getParentAllocation();

    std::vector<GraphicsAllocation *> allocations;
    allocations.push_back(firstAllocation);

    constexpr size_t largeRequestSize = MemoryConstants::pageSize2M / 2 + MemoryConstants::pageSize;
    size_t numAllocationsToExhaust = poolSize / largeRequestSize + 2;

    for (size_t i = 0; i < numAllocationsToExhaust; i++) {
        auto allocation = allocator.allocate(largeRequestSize);
        if (allocation) {
            allocations.push_back(allocation);
        }
    }

    EXPECT_GT(allocator.bufferPools.size(), 1u);

    bool foundAllocationFromSecondPool = false;
    for (auto alloc : allocations) {
        if (alloc->getParentAllocation() != firstPoolParent) {
            foundAllocationFromSecondPool = true;
            break;
        }
    }
    EXPECT_TRUE(foundAllocationFromSecondPool);

    for (auto alloc : allocations) {
        allocator.free(alloc);
    }
}

TEST_F(GenericViewPoolAllocatorTest, givenAllocatorWhenFreeingNullThenDoesNotCrash) {
    MockGenericViewPoolAllocator<MockViewPoolTraits> allocator(pDevice);

    allocator.free(nullptr);
}

TEST_F(GenericViewPoolAllocatorTest, givenAllocatorWhenFreeingNonViewAllocationThenDoesNotFreeIt) {
    MockGenericViewPoolAllocator<MockViewPoolTraits> allocator(pDevice);

    AllocationProperties allocProperties = {pDevice->getRootDeviceIndex(),
                                            true,
                                            MemoryConstants::pageSize,
                                            MockViewPoolTraits::allocationType,
                                            false,
                                            false,
                                            pDevice->getDeviceBitfield()};

    auto nonViewAllocation = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(allocProperties);
    ASSERT_NE(nullptr, nonViewAllocation);
    EXPECT_FALSE(nonViewAllocation->isView());

    allocator.free(nonViewAllocation);

    // Allocator does no-op on non-view allocation
    pDevice->getMemoryManager()->freeGraphicsMemory(nonViewAllocation); // NOLINT(clang-analyzer-cplusplus.NewDelete)
}

TEST_F(GenericViewPoolAllocatorTest, givenAllocatorWhenFreeingAllocationThenSpaceCanBeReused) {
    MockGenericViewPoolAllocator<MockViewPoolTraits> allocator(pDevice);
    constexpr size_t requestSize = MemoryConstants::pageSize;

    auto allocation1 = allocator.allocate(requestSize);
    ASSERT_NE(nullptr, allocation1);
    auto parentAllocation = allocation1->getParentAllocation();

    allocator.free(allocation1);

    auto allocation2 = allocator.allocate(requestSize);
    ASSERT_NE(nullptr, allocation2);
    EXPECT_EQ(parentAllocation, allocation2->getParentAllocation());
    EXPECT_EQ(1u, allocator.bufferPools.size());

    allocator.free(allocation2);
}

TEST_F(GenericViewPoolAllocatorTest, givenAllocatorWhenPoolExhaustedAndChunksFreedThenDrainReclaimsSpace) {
    MockGenericViewPoolAllocator<MockViewPoolTraits> allocator(pDevice);

    auto firstAllocation = allocator.allocate(MemoryConstants::pageSize);
    ASSERT_NE(nullptr, firstAllocation);

    auto *pool = static_cast<MockGenericViewPool<MockViewPoolTraits> *>(&allocator.bufferPools[0]);
    auto poolSize = pool->mainStorage->getUnderlyingBufferSize();

    constexpr size_t largeChunkSize = MemoryConstants::pageSize2M / 2 + MemoryConstants::pageSize;

    std::vector<GraphicsAllocation *> allocations;
    allocations.push_back(firstAllocation);

    size_t maxAllocations = poolSize / largeChunkSize;
    for (size_t i = 1; i < maxAllocations; i++) {
        auto allocation = allocator.allocate(largeChunkSize);
        if (allocation && allocation->getParentAllocation() == pool->mainStorage.get()) {
            allocations.push_back(allocation);
        } else if (allocation) {
            allocator.free(allocation);
            break;
        }
    }

    size_t poolCountBeforeFree = allocator.bufferPools.size();

    if (allocations.size() > 1) {
        allocator.free(allocations.back());
        allocations.pop_back();
    }

    auto newAllocation = allocator.allocate(largeChunkSize);
    ASSERT_NE(nullptr, newAllocation);

    EXPECT_EQ(poolCountBeforeFree, allocator.bufferPools.size());

    allocator.free(newAllocation);
    for (auto alloc : allocations) {
        allocator.free(alloc);
    }
}

TEST_F(GenericViewPoolAllocatorTest, givenAllocatorWhenIsPoolBufferCalledThenCorrectlyIdentifiesPoolAllocations) {
    MockGenericViewPoolAllocator<MockViewPoolTraits> allocator(pDevice);

    auto poolAllocation = allocator.allocate(MemoryConstants::pageSize);
    ASSERT_NE(nullptr, poolAllocation);

    EXPECT_TRUE(allocator.isPoolBuffer(poolAllocation->getParentAllocation()));

    AllocationProperties allocProperties = {pDevice->getRootDeviceIndex(),
                                            true,
                                            MemoryConstants::pageSize,
                                            MockViewPoolTraits::allocationType,
                                            false,
                                            false,
                                            pDevice->getDeviceBitfield()};

    auto nonPoolAllocation = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(allocProperties);
    EXPECT_FALSE(allocator.isPoolBuffer(nonPoolAllocation));

    allocator.free(poolAllocation);
    pDevice->getMemoryManager()->freeGraphicsMemory(nonPoolAllocation);
}

TEST_F(GenericViewPoolAllocatorTest, givenAllocatorWhenGetDefaultPoolSizeCalledThenReturnsTraitsValue) {
    MockGenericViewPoolAllocator<MockViewPoolTraits> allocator(pDevice);

    EXPECT_EQ(MockViewPoolTraits::defaultPoolSize, allocator.getDefaultPoolSize());
}

TEST_F(GenericViewPoolAllocatorTest, givenPoolWhenCreatedThenPoolSizeIsAlignedToPoolAlignment) {
    MockGenericViewPoolAllocator<MockViewPoolTraits> allocator(pDevice);

    auto allocation = allocator.allocate(MemoryConstants::pageSize);
    ASSERT_NE(nullptr, allocation);

    ASSERT_GE(allocator.bufferPools.size(), 1u);
    auto *pool = static_cast<MockGenericViewPool<MockViewPoolTraits> *>(&allocator.bufferPools[0]);

    auto poolSize = pool->mainStorage->getUnderlyingBufferSize();
    EXPECT_TRUE(isAligned(poolSize, MockViewPoolTraits::poolAlignment));

    allocator.free(allocation);
}

TEST_F(GenericViewPoolAllocatorTest, givenAllocatorWhenAllocatingDifferentSizesThenAllocationsHaveCorrectSizes) {
    MockGenericViewPoolAllocator<MockViewPoolTraits> allocator(pDevice);

    constexpr size_t size1 = MemoryConstants::pageSize;
    constexpr size_t size2 = MemoryConstants::pageSize * 2;
    constexpr size_t size3 = MemoryConstants::pageSize64k;

    auto allocation1 = allocator.allocate(size1);
    auto allocation2 = allocator.allocate(size2);
    auto allocation3 = allocator.allocate(size3);

    ASSERT_NE(nullptr, allocation1);
    ASSERT_NE(nullptr, allocation2);
    ASSERT_NE(nullptr, allocation3);

    EXPECT_EQ(size1, allocation1->getUnderlyingBufferSize());
    EXPECT_EQ(size2, allocation2->getUnderlyingBufferSize());
    EXPECT_EQ(size3, allocation3->getUnderlyingBufferSize());

    allocator.free(allocation1);
    allocator.free(allocation2);
    allocator.free(allocation3);
}

TEST_F(GenericViewPoolAllocatorTest, givenMultiplePoolsWhenAllocatingThenAllocatesFromPoolWithAvailableSpace) {
    MockGenericViewPoolAllocator<MockViewPoolTraits> allocator(pDevice);
    constexpr size_t requestSize = MemoryConstants::pageSize64k;
    std::vector<GraphicsAllocation *> firstPoolAllocations;

    auto firstAllocation = allocator.allocate(requestSize);
    ASSERT_NE(nullptr, firstAllocation);
    firstPoolAllocations.push_back(firstAllocation);

    auto *pool = static_cast<MockGenericViewPool<MockViewPoolTraits> *>(&allocator.bufferPools[0]);
    auto poolSize = pool->mainStorage->getUnderlyingBufferSize();
    auto firstPoolParent = firstAllocation->getParentAllocation();

    size_t totalAllocated = requestSize;
    while (totalAllocated < poolSize) {
        auto allocation = allocator.allocate(requestSize);
        if (!allocation || allocation->getParentAllocation() != firstPoolParent) {
            if (allocation) {
                allocator.free(allocation);
            }
            break;
        }
        firstPoolAllocations.push_back(allocation);
        totalAllocated += requestSize;
    }

    auto allocationFromSecondPool = allocator.allocate(requestSize);
    ASSERT_NE(nullptr, allocationFromSecondPool);
    EXPECT_GT(allocator.bufferPools.size(), 1u);

    auto secondPoolParent = allocationFromSecondPool->getParentAllocation();
    EXPECT_NE(firstPoolParent, secondPoolParent);

    auto anotherFromSecondPool = allocator.allocate(requestSize);
    ASSERT_NE(nullptr, anotherFromSecondPool);
    EXPECT_EQ(secondPoolParent, anotherFromSecondPool->getParentAllocation());

    allocator.free(allocationFromSecondPool);
    allocator.free(anotherFromSecondPool);
    for (auto allocation : firstPoolAllocations) {
        allocator.free(allocation);
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

TEST_F(GenericViewPoolAllocatorTest, givenAllocatorWhenFreeingViewAllocationThenRemovesFromDownloadAllocationsInCsr) {
    auto mockMemoryManager = std::make_unique<MockMemoryManagerTrackRemoveDownloadAllocation>(*pDevice->getExecutionEnvironment());
    auto *trackingMemoryManager = mockMemoryManager.get();

    std::unique_ptr<MemoryManager> originalMemoryManager;
    originalMemoryManager.reset(pDevice->executionEnvironment->memoryManager.release());
    pDevice->executionEnvironment->memoryManager = std::move(mockMemoryManager);

    {
        MockGenericViewPoolAllocator<MockViewPoolTraits> allocator(pDevice);
        constexpr size_t requestSize = MemoryConstants::pageSize;

        auto allocation = allocator.allocate(requestSize);
        ASSERT_NE(nullptr, allocation);
        EXPECT_TRUE(allocation->isView());

        EXPECT_EQ(0u, trackingMemoryManager->removeAllocationFromDownloadAllocationsInCsrCalled);

        auto allocationPtr = allocation;
        allocator.free(allocation);

        EXPECT_EQ(1u, trackingMemoryManager->removeAllocationFromDownloadAllocationsInCsrCalled);
        EXPECT_EQ(allocationPtr, trackingMemoryManager->lastRemovedAllocation);
    }

    pDevice->executionEnvironment->memoryManager = std::move(originalMemoryManager);
}

class MockMemoryManagerFailAllocation : public OsAgnosticMemoryManager {
  public:
    using OsAgnosticMemoryManager::OsAgnosticMemoryManager;

    GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) override {
        return nullptr;
    }
};

TEST_F(GenericViewPoolAllocatorTest, givenPoolWhenMainStorageAllocationFailsThenAllocateReturnsNull) {
    auto mockMemoryManager = std::make_unique<MockMemoryManagerFailAllocation>(*pDevice->getExecutionEnvironment());

    std::unique_ptr<MemoryManager> originalMemoryManager;
    originalMemoryManager.reset(pDevice->executionEnvironment->memoryManager.release());
    pDevice->executionEnvironment->memoryManager = std::move(mockMemoryManager);

    {
        MockGenericViewPoolAllocator<MockViewPoolTraits> allocator(pDevice);

        auto allocation = allocator.allocate(MemoryConstants::pageSize);

        EXPECT_EQ(nullptr, allocation);
    }

    pDevice->executionEnvironment->memoryManager = std::move(originalMemoryManager);
}
