/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_pooling.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/mocks/mock_usm_memory_pool.h"

#include "gtest/gtest.h"

#include <functional>
#include <mutex>
#include <thread>

using namespace NEO;

namespace {
bool isLockableFromAnotherThread(std::mutex &mutex) {
    bool locked = false;
    std::thread([&mutex, &locked] {
        locked = mutex.try_lock();
        if (locked) {
            mutex.unlock();
        }
    }).join();
    return locked;
}

struct LockProbingMemoryManager : public MockMemoryManager {
    using MockMemoryManager::MockMemoryManager;

    void waitForEnginesCompletion(GraphicsAllocation &graphicsAllocation) override {
        if (onWaitForEnginesCompletion) {
            onWaitForEnginesCompletion();
        }
        MockMemoryManager::waitForEnginesCompletion(graphicsAllocation);
    }

    void captureEngineCompletionSnapshot(GraphicsAllocation &graphicsAllocation, EngineCompletionSnapshot &snapshot) override {
        if (onCaptureEngineCompletionSnapshot) {
            onCaptureEngineCompletionSnapshot();
        }
        MockMemoryManager::captureEngineCompletionSnapshot(graphicsAllocation, snapshot);
    }

    std::function<void()> onWaitForEnginesCompletion;
    std::function<void()> onCaptureEngineCompletionSnapshot;
};

struct LockProbingSvmAllocsManager : public MockSVMAllocsManager {
    using MockSVMAllocsManager::MockSVMAllocsManager;

    void *createHostUnifiedMemoryAllocation(size_t size, const UnifiedMemoryProperties &memoryProperties) override {
        if (onCreateHostAllocation) {
            onCreateHostAllocation();
        }
        return MockSVMAllocsManager::createHostUnifiedMemoryAllocation(size, memoryProperties);
    }

    void applyIndirectAccessTaskCountFloor(SvmAllocationData *allocationData) override {
        if (onApplyIndirectAccessTaskCountFloor) {
            onApplyIndirectAccessTaskCountFloor();
        }
        MockSVMAllocsManager::applyIndirectAccessTaskCountFloor(allocationData);
    }

    std::function<void()> onCreateHostAllocation;
    std::function<void()> onApplyIndirectAccessTaskCountFloor;
};

struct UsmPoolLockOrderMtTest : public ::testing::Test, public SVMMemoryAllocatorFixture<true, 1u> {
    void SetUp() override {
        SVMMemoryAllocatorFixture::setUp();
        lockProbingMemoryManager = std::make_unique<LockProbingMemoryManager>(false, executionEnvironment);
        lockProbingSvmManager = std::make_unique<LockProbingSvmAllocsManager>(lockProbingMemoryManager.get());
    }

    void TearDown() override {
        SVMMemoryAllocatorFixture::tearDown();
    }

    UnifiedMemoryProperties makeHostProperties(size_t alignment) {
        return UnifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, alignment, rootDeviceIndices, deviceBitfields);
    }

    std::unique_ptr<LockProbingMemoryManager> lockProbingMemoryManager;
    std::unique_ptr<LockProbingSvmAllocsManager> lockProbingSvmManager;
};
} // namespace

TEST_F(UsmPoolLockOrderMtTest, givenPoolsManagerWithoutPoolsWhenAllocatingThenNewPoolIsCreatedWithoutHoldingManagerLock) {
    MockUsmMemAllocPoolsManager poolsManager(InternalMemoryType::hostUnifiedMemory, rootDeviceIndices, deviceBitfields, nullptr, false);
    ASSERT_TRUE(poolsManager.initialize(lockProbingSvmManager.get()));

    uint32_t poolCreations = 0u;
    bool managerLockFreeDuringPoolCreation = true;
    lockProbingSvmManager->onCreateHostAllocation = [&] {
        ++poolCreations;
        managerLockFreeDuringPoolCreation &= isLockableFromAnotherThread(poolsManager.mtx);
    };

    const auto chunkSize = poolsManager.getPoolInfos()[1].maxServicedSize;
    auto memoryProperties = makeHostProperties(UsmMemAllocPool::chunkAlignment);
    auto ptr = poolsManager.createUnifiedMemoryAllocation(chunkSize, memoryProperties);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(1u, poolCreations);
    EXPECT_TRUE(managerLockFreeDuringPoolCreation);
    EXPECT_NE(nullptr, poolsManager.getPoolContainingAlloc(ptr).pool);

    lockProbingSvmManager->onCreateHostAllocation = nullptr;
    EXPECT_TRUE(poolsManager.freeSVMAlloc(ptr, FreePolicyType::none));
    poolsManager.cleanup();
}

TEST_F(UsmPoolLockOrderMtTest, givenPooledChunkWhenFreedBlockingThenIndirectAccessTaskCountFloorIsAppliedWithoutHoldingPoolLock) {
    MockUsmMemAllocPool pool;
    auto poolMemoryProperties = makeHostProperties(MemoryConstants::pageSize2M);
    ASSERT_TRUE(pool.initialize(lockProbingSvmManager.get(), poolMemoryProperties, 2 * MemoryConstants::megaByte, 0u, MemoryConstants::megaByte));

    auto chunkMemoryProperties = makeHostProperties(MemoryConstants::pageSize64k);
    auto ptr = pool.createUnifiedMemoryAllocation(MemoryConstants::kiloByte, chunkMemoryProperties);
    ASSERT_NE(nullptr, ptr);

    uint32_t floorApplications = 0u;
    bool poolLockFreeDuringFloor = true;
    lockProbingSvmManager->onApplyIndirectAccessTaskCountFloor = [&] {
        ++floorApplications;
        poolLockFreeDuringFloor &= isLockableFromAnotherThread(pool.getMutex());
    };

    EXPECT_TRUE(pool.freeSVMAlloc(ptr, FreePolicyType::blocking).freeSucceeded);
    EXPECT_EQ(1u, floorApplications);
    EXPECT_TRUE(poolLockFreeDuringFloor);

    lockProbingSvmManager->onApplyIndirectAccessTaskCountFloor = nullptr;
    pool.cleanup();
}

TEST_F(UsmPoolLockOrderMtTest, givenPoolWithDeferredChunkWhenCleaningUpThenIndirectAccessTaskCountFloorIsAppliedWithoutHoldingPoolLock) {
    MockUsmMemAllocPool pool;
    auto poolMemoryProperties = makeHostProperties(MemoryConstants::pageSize2M);
    ASSERT_TRUE(pool.initialize(lockProbingSvmManager.get(), poolMemoryProperties, 2 * MemoryConstants::megaByte, 0u, MemoryConstants::megaByte));
    pool.deferredFreeChunks.push_back(UsmMemAllocPool::DeferredFreeInfo{});

    uint32_t floorApplications = 0u;
    bool poolLockFreeDuringFloor = true;
    lockProbingSvmManager->onApplyIndirectAccessTaskCountFloor = [&] {
        ++floorApplications;
        poolLockFreeDuringFloor &= isLockableFromAnotherThread(pool.getMutex());
    };

    pool.cleanup();
    EXPECT_EQ(1u, floorApplications);
    EXPECT_TRUE(poolLockFreeDuringFloor);
}

TEST_F(UsmPoolLockOrderMtTest, givenPooledChunkWhenFreedBlockingThenEnginesCompletionIsAwaitedWithoutHoldingPoolLock) {
    MockUsmMemAllocPool pool;
    auto poolMemoryProperties = makeHostProperties(MemoryConstants::pageSize2M);
    ASSERT_TRUE(pool.initialize(lockProbingSvmManager.get(), poolMemoryProperties, 2 * MemoryConstants::megaByte, 0u, MemoryConstants::megaByte));

    auto chunkMemoryProperties = makeHostProperties(MemoryConstants::pageSize64k);
    auto ptr = pool.createUnifiedMemoryAllocation(MemoryConstants::kiloByte, chunkMemoryProperties);
    ASSERT_NE(nullptr, ptr);

    uint32_t waits = 0u;
    bool poolLockFreeDuringWait = true;
    bool chunkLiveDuringWait = true;
    lockProbingMemoryManager->onWaitForEnginesCompletion = [&] {
        ++waits;
        const bool poolLockFree = isLockableFromAnotherThread(pool.getMutex());
        poolLockFreeDuringWait &= poolLockFree;
        chunkLiveDuringWait &= poolLockFree && false == pool.isEmpty();
    };

    EXPECT_TRUE(pool.freeSVMAlloc(ptr, FreePolicyType::blocking).freeSucceeded);
    EXPECT_NE(0u, waits);
    EXPECT_TRUE(poolLockFreeDuringWait);
    EXPECT_TRUE(chunkLiveDuringWait);

    lockProbingMemoryManager->onWaitForEnginesCompletion = nullptr;
    pool.cleanup();
}

TEST_F(UsmPoolLockOrderMtTest, givenPooledChunkWhenFreedWithDeferThenEngineCompletionSnapshotIsCapturedWithoutHoldingPoolLock) {
    MockUsmMemAllocPool pool;
    auto poolMemoryProperties = makeHostProperties(MemoryConstants::pageSize2M);
    ASSERT_TRUE(pool.initialize(lockProbingSvmManager.get(), poolMemoryProperties, 2 * MemoryConstants::megaByte, 0u, MemoryConstants::megaByte));

    auto chunkMemoryProperties = makeHostProperties(MemoryConstants::pageSize64k);
    auto ptr = pool.createUnifiedMemoryAllocation(MemoryConstants::kiloByte, chunkMemoryProperties);
    ASSERT_NE(nullptr, ptr);

    uint32_t snapshotCaptures = 0u;
    bool poolLockFreeDuringSnapshotCapture = true;
    bool chunkLiveDuringSnapshotCapture = true;
    lockProbingMemoryManager->onCaptureEngineCompletionSnapshot = [&] {
        ++snapshotCaptures;
        const bool poolLockFree = isLockableFromAnotherThread(pool.getMutex());
        poolLockFreeDuringSnapshotCapture &= poolLockFree;
        chunkLiveDuringSnapshotCapture &= poolLockFree && false == pool.isEmpty();
    };

    EXPECT_TRUE(pool.freeSVMAlloc(ptr, FreePolicyType::defer).freeSucceeded);
    EXPECT_NE(0u, snapshotCaptures);
    EXPECT_TRUE(poolLockFreeDuringSnapshotCapture);
    EXPECT_TRUE(chunkLiveDuringSnapshotCapture);

    lockProbingMemoryManager->onCaptureEngineCompletionSnapshot = nullptr;
    pool.cleanup();
}
