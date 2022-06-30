/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/memory_manager/deferrable_allocation_deletion.h"
#include "shared/source/memory_manager/deferred_deleter.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

struct DeferredDeleterPublic : DeferredDeleter {
  public:
    using DeferredDeleter::doWorkInBackground;
    using DeferredDeleter::queue;
    using DeferredDeleter::queueMutex;
    bool shouldStopReached = false;
    bool allowExit = false;
    bool shouldStop() override {
        if (allowExit) {
            EXPECT_TRUE(queue.peekIsEmpty());
        }
        shouldStopReached = allowExit;
        return allowExit;
    }
};

struct DeferrableAllocationDeletionTest : ::testing::Test {
    void SetUp() override {
        executionEnvironment = new MockExecutionEnvironment(defaultHwInfo.get(), false, 1u);
        executionEnvironment->incRefInternal();
        memoryManager = new MockMemoryManager(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
        device.reset(Device::create<MockDevice>(executionEnvironment, 0u));
        hwTag = device->getDefaultEngine().commandStreamReceiver->getTagAddress();
        defaultOsContextId = device->getDefaultEngine().osContext->getContextId();
        asyncDeleter = std::make_unique<DeferredDeleterPublic>();
        asyncDeleter->addClient();
    }
    void TearDown() override {
        asyncDeleter->allowExit = true;
        asyncDeleter->removeClient();
        executionEnvironment->decRefInternal();
    }
    ExecutionEnvironment *executionEnvironment;
    std::unique_ptr<DeferredDeleterPublic> asyncDeleter;
    MockMemoryManager *memoryManager = nullptr;
    std::unique_ptr<MockDevice> device;
    uint32_t defaultOsContextId = 0;
    volatile uint32_t *hwTag = nullptr;
};

TEST_F(DeferrableAllocationDeletionTest, givenDeferrableAllocationWhenApplyThenWaitForEachTaskCount) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    allocation->updateTaskCount(1u, defaultOsContextId);
    *hwTag = 0u;
    asyncDeleter->deferDeletion(new DeferrableAllocationDeletion(*memoryManager, *allocation));
    while (!asyncDeleter->queue.peekIsEmpty()) // wait for async thread to get allocation from queue
        std::this_thread::yield();

    EXPECT_EQ(0u, memoryManager->freeGraphicsMemoryCalled);
    EXPECT_TRUE(allocation->isUsedByOsContext(defaultOsContextId));

    // let async thread exit
    asyncDeleter->allowExit = true;

    *hwTag = 1u; // allow to destroy allocation
    while (!asyncDeleter->shouldStopReached)
        std::this_thread::yield();
    EXPECT_EQ(1u, memoryManager->freeGraphicsMemoryCalled);
}

HWTEST_F(DeferrableAllocationDeletionTest, givenAllocationUsedByTwoOsContextsWhenApplyDeletionThenWaitForBothContextsAndFlushNotReadyCsr) {
    auto &nonDefaultCommandStreamReceiver = static_cast<UltCommandStreamReceiver<FamilyType> &>(*device->commandStreamReceivers[1]);
    auto nonDefaultOsContextId = nonDefaultCommandStreamReceiver.getOsContext().getContextId();
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    *hwTag = 0u;
    *nonDefaultCommandStreamReceiver.getTagAddress() = 1u;
    allocation->updateTaskCount(1u, defaultOsContextId);
    allocation->updateTaskCount(1u, nonDefaultOsContextId);
    EXPECT_TRUE(allocation->isUsedByOsContext(defaultOsContextId));
    EXPECT_TRUE(allocation->isUsedByOsContext(nonDefaultOsContextId));
    EXPECT_EQ(0u, memoryManager->freeGraphicsMemoryCalled);
    EXPECT_FALSE(device->getUltCommandStreamReceiver<FamilyType>().flushBatchedSubmissionsCalled);
    EXPECT_FALSE(nonDefaultCommandStreamReceiver.flushBatchedSubmissionsCalled);
    asyncDeleter->deferDeletion(new DeferrableAllocationDeletion(*memoryManager, *allocation));
    while (allocation->isUsedByOsContext(nonDefaultOsContextId) && !device->getUltCommandStreamReceiver<FamilyType>().flushBatchedSubmissionsCalled) // wait for second context completion signal
        std::this_thread::yield();
    EXPECT_EQ(0u, memoryManager->freeGraphicsMemoryCalled);
    EXPECT_FALSE(nonDefaultCommandStreamReceiver.flushBatchedSubmissionsCalled);
    asyncDeleter->allowExit = true;
    *hwTag = 1u;
}

HWTEST_F(DeferrableAllocationDeletionTest, givenDeferrableAllocationDeletionWhenFlushedTaskIsGreaterThanAllocationTaskCountThenDoNotProgrammTagUpdate) {
    auto &nonDefaultCommandStreamReceiver = static_cast<UltCommandStreamReceiver<FamilyType> &>(*device->commandStreamReceivers[1]);
    auto nonDefaultOsContextId = nonDefaultCommandStreamReceiver.getOsContext().getContextId();
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    *hwTag = 0u;
    *nonDefaultCommandStreamReceiver.getTagAddress() = 1u;
    nonDefaultCommandStreamReceiver.setLatestFlushedTaskCount(4u);
    allocation->updateTaskCount(1u, nonDefaultOsContextId);
    allocation->updateTaskCount(1u, defaultOsContextId);
    auto used = nonDefaultCommandStreamReceiver.getCS(0u).getUsed();
    asyncDeleter->deferDeletion(new DeferrableAllocationDeletion(*memoryManager, *allocation));
    while (allocation->isUsedByOsContext(nonDefaultOsContextId) && !device->getUltCommandStreamReceiver<FamilyType>().flushBatchedSubmissionsCalled) // wait for second context completion signal
        std::this_thread::yield();
    EXPECT_FALSE(nonDefaultCommandStreamReceiver.flushBatchedSubmissionsCalled);
    EXPECT_EQ(used, nonDefaultCommandStreamReceiver.getCS(0u).getUsed());
    asyncDeleter->allowExit = true;
    *hwTag = 1u;
}

TEST_F(DeferrableAllocationDeletionTest, givenNotUsedAllocationWhenApplyDeletionThenDontWait) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    EXPECT_FALSE(allocation->isUsed());
    EXPECT_EQ(0u, memoryManager->freeGraphicsMemoryCalled);
    while (!asyncDeleter->doWorkInBackground)
        std::this_thread::yield(); //wait for start async thread work
    std::unique_lock<std::mutex> lock(asyncDeleter->queueMutex);
    asyncDeleter->allowExit = true;
    lock.unlock();
    asyncDeleter->deferDeletion(new DeferrableAllocationDeletion(*memoryManager, *allocation));
    while (!asyncDeleter->shouldStopReached) // wait async thread job end
        std::this_thread::yield();
    EXPECT_EQ(1u, memoryManager->freeGraphicsMemoryCalled);
}

TEST_F(DeferrableAllocationDeletionTest, givenTwoAllocationsUsedByOneOsContextsEnqueuedToAsyncDeleterWhenOneAllocationIsCompletedThenReleaseThatAllocation) {
    auto allocation1 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    auto allocation2 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    *hwTag = 1u;
    allocation1->updateTaskCount(2u, defaultOsContextId);
    allocation2->updateTaskCount(1u, defaultOsContextId);
    EXPECT_EQ(0u, memoryManager->freeGraphicsMemoryCalled);
    EXPECT_TRUE(allocation1->isUsed());
    EXPECT_TRUE(allocation2->isUsed());
    asyncDeleter->deferDeletion(new DeferrableAllocationDeletion(*memoryManager, *allocation1));
    asyncDeleter->deferDeletion(new DeferrableAllocationDeletion(*memoryManager, *allocation2));
    while (0u == memoryManager->freeGraphicsMemoryCalled) // wait for delete second allocation
        std::this_thread::yield();
    EXPECT_EQ(1u, memoryManager->freeGraphicsMemoryCalled);
    asyncDeleter->allowExit = true;
    *hwTag = 2u;
}

TEST_F(DeferrableAllocationDeletionTest, givenNotCompletedAllocationWhenDeletionIsAppliedThenReturnFalse) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    *hwTag = 0u;
    allocation->updateTaskCount(1u, defaultOsContextId);
    EXPECT_EQ(0u, memoryManager->freeGraphicsMemoryCalled);
    DeferrableAllocationDeletion deletion{*memoryManager, *allocation};
    EXPECT_FALSE(deletion.apply());
    EXPECT_EQ(0u, memoryManager->freeGraphicsMemoryCalled);
    *hwTag = 1u; // complete allocation
    EXPECT_TRUE(deletion.apply());
    EXPECT_EQ(1u, memoryManager->freeGraphicsMemoryCalled);
}

TEST_F(DeferrableAllocationDeletionTest, givenNotUsedAllocationWhenDeletionIsAppliedThenReturnTrue) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    EXPECT_FALSE(allocation->isUsed());
    DeferrableAllocationDeletion deletion{*memoryManager, *allocation};
    EXPECT_TRUE(deletion.apply());
    EXPECT_EQ(1u, memoryManager->freeGraphicsMemoryCalled);
}

TEST_F(DeferrableAllocationDeletionTest, givenAllocationUsedByUnregisteredEngineWhenDeletionIsAppliedThenReturnTrue) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    allocation->updateTaskCount(2u, defaultOsContextId);
    EXPECT_TRUE(allocation->isUsed());
    DeferrableAllocationDeletion deletion{*memoryManager, *allocation};

    device.reset();
    executionEnvironment->rootDeviceEnvironments.clear();
    EXPECT_EQ(0u, memoryManager->registeredEngines.size());
    EXPECT_TRUE(allocation->isUsed());

    memoryManager->freeGraphicsMemoryCalled = 0u;
    EXPECT_TRUE(deletion.apply());
    EXPECT_EQ(1u, memoryManager->freeGraphicsMemoryCalled);
}

HWTEST_F(DeferrableAllocationDeletionTest, givenMultiTileWhenTaskCompletedOnSingleTileThenDoNotFreeGraphicsAllocation) {
    auto csr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(device->getDefaultEngine().commandStreamReceiver);
    csr->setActivePartitions(2u);
    csr->postSyncWriteOffset = 32;
    auto hwTagNextTile = ptrOffset(hwTag, 32);

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    allocation->updateTaskCount(1u, defaultOsContextId);

    EXPECT_EQ(0u, memoryManager->freeGraphicsMemoryCalled);
    DeferrableAllocationDeletion deletion{*memoryManager, *allocation};

    *hwTag = 0u;
    *hwTagNextTile = 0u;
    EXPECT_FALSE(deletion.apply());
    EXPECT_EQ(0u, memoryManager->freeGraphicsMemoryCalled);

    *hwTag = 1u;
    EXPECT_FALSE(deletion.apply());
    EXPECT_EQ(0u, memoryManager->freeGraphicsMemoryCalled);

    *hwTagNextTile = 1u;
    EXPECT_TRUE(deletion.apply());
    EXPECT_EQ(1u, memoryManager->freeGraphicsMemoryCalled);
}
