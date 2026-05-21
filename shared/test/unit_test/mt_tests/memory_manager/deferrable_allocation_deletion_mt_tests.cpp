/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/deferrable_allocation_deletion.h"
#include "shared/source/memory_manager/deferred_deleter.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

struct DeferredDeleterPublic : DeferredDeleter {
  public:
    using DeferredDeleter::doWorkInBackground;
    using DeferredDeleter::queue;
    using DeferredDeleter::queueMutex;
    std::atomic_bool shouldStopReached = false;
    std::atomic_bool allowExit = false;
    bool shouldStop() override {
        if (allowExit) {
            EXPECT_TRUE(queue.peekIsEmpty());
        }
        shouldStopReached = allowExit.load();
        return allowExit || DeferredDeleter::shouldStop();
    }
};

struct DeferrableAllocationDeletionMtTest : ::testing::Test {
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
    volatile TagAddressType *hwTag = nullptr;
};

TEST_F(DeferrableAllocationDeletionMtTest, givenDeferrableAllocationWhenApplyThenWaitForEachTaskCount) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    allocation->updateTaskCount(1u, defaultOsContextId);
    *hwTag = 0u;
    asyncDeleter->deferDeletion(new DeferrableAllocationDeletion(*memoryManager, *allocation));
    while (!asyncDeleter->queue.peekIsEmpty()) { // wait for async thread to get allocation from queue
        std::this_thread::yield();
    }

    EXPECT_EQ(0u, memoryManager->freeGraphicsMemoryCalled);
    EXPECT_TRUE(allocation->isUsedByOsContext(defaultOsContextId));

    // let async thread exit
    asyncDeleter->allowExit = true;

    *hwTag = 1u; // allow to destroy allocation
    while (!asyncDeleter->shouldStopReached) {
        std::this_thread::yield();
    }
    EXPECT_EQ(1u, memoryManager->freeGraphicsMemoryCalled);
}

HWTEST_F(DeferrableAllocationDeletionMtTest, givenDeferrableAllocationDeletionWhenTaskCountAlreadyFlushedThenDoNotProgrammTagUpdate) {
    std::atomic_bool applyCalled{false};
    struct DeferrableAllocationDeletionApplyCall : public DeferrableAllocationDeletion {
        DeferrableAllocationDeletionApplyCall(MemoryManager &mm, GraphicsAllocation &a, std::atomic_bool &flag)
            : DeferrableAllocationDeletion(mm, a), applyCalled(flag) {}
        bool apply() override {
            applyCalled = true;
            return DeferrableAllocationDeletion::apply();
        }
        std::atomic_bool &applyCalled;
    };
    auto &nonDefaultCommandStreamReceiver = static_cast<UltCommandStreamReceiver<FamilyType> &>(*device->commandStreamReceivers[1]);
    auto nonDefaultOsContextId = nonDefaultCommandStreamReceiver.getOsContext().getContextId();
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    *hwTag = 0u;
    *nonDefaultCommandStreamReceiver.getTagAddress() = 0u;
    nonDefaultCommandStreamReceiver.setLatestFlushedTaskCount(2u);
    static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getDefaultEngine().commandStreamReceiver)->setLatestFlushedTaskCount(2u);
    nonDefaultCommandStreamReceiver.taskCount = 2u;
    static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getDefaultEngine().commandStreamReceiver)->taskCount = 2u;
    allocation->updateTaskCount(1u, nonDefaultOsContextId);
    allocation->updateTaskCount(1u, defaultOsContextId);
    auto used = nonDefaultCommandStreamReceiver.getCS(0u).getUsed();
    EXPECT_FALSE(nonDefaultCommandStreamReceiver.flushBatchedSubmissionsCalled);
    auto usedDefault = device->getDefaultEngine().commandStreamReceiver->getCS(0u).getUsed();
    EXPECT_FALSE(static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getDefaultEngine().commandStreamReceiver)->flushBatchedSubmissionsCalled);
    EXPECT_FALSE(applyCalled);
    asyncDeleter->deferDeletion(new DeferrableAllocationDeletionApplyCall(*memoryManager, *allocation, applyCalled));
    while (!applyCalled) {
        std::this_thread::yield();
    }
    *hwTag = 2u;
    *nonDefaultCommandStreamReceiver.getTagAddress() = 2u;
    EXPECT_FALSE(nonDefaultCommandStreamReceiver.flushBatchedSubmissionsCalled);
    EXPECT_EQ(used, nonDefaultCommandStreamReceiver.getCS(0u).getUsed());
    EXPECT_FALSE(static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getDefaultEngine().commandStreamReceiver)->flushBatchedSubmissionsCalled);
    EXPECT_EQ(usedDefault, device->getDefaultEngine().commandStreamReceiver->getCS(0u).getUsed());
    asyncDeleter->allowExit = true;
    *hwTag = 2u;
    *nonDefaultCommandStreamReceiver.getTagAddress() = 2u;
    while (memoryManager->freeGraphicsMemoryCalled == 0u) { // wait for deletion to complete before applyCalled goes out of scope
        std::this_thread::yield();
    }
}

HWTEST_F(DeferrableAllocationDeletionMtTest, givenAllocationUsedByTwoOsContextsWhenApplyDeletionThenWaitForBothContextsAndFlushNotReadyCsr) {
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
    while (allocation->isUsedByOsContext(nonDefaultOsContextId) && !device->getUltCommandStreamReceiver<FamilyType>().flushBatchedSubmissionsCalled) { // wait for second context completion signal
        std::this_thread::yield();
    }
    EXPECT_EQ(0u, memoryManager->freeGraphicsMemoryCalled);
    EXPECT_FALSE(nonDefaultCommandStreamReceiver.flushBatchedSubmissionsCalled);
    asyncDeleter->allowExit = true;
    *hwTag = 1u;
}

HWTEST_F(DeferrableAllocationDeletionMtTest, givenDeferrableAllocationDeletionWhenFlushedTaskIsGreaterThanAllocationTaskCountThenDoNotProgrammTagUpdate) {
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
    while (allocation->isUsedByOsContext(nonDefaultOsContextId) && !device->getUltCommandStreamReceiver<FamilyType>().flushBatchedSubmissionsCalled) { // wait for second context completion signal
        std::this_thread::yield();
    }
    EXPECT_FALSE(nonDefaultCommandStreamReceiver.flushBatchedSubmissionsCalled);
    EXPECT_EQ(used, nonDefaultCommandStreamReceiver.getCS(0u).getUsed());
    asyncDeleter->allowExit = true;
    *hwTag = 1u;
}

TEST_F(DeferrableAllocationDeletionMtTest, givenNotUsedAllocationWhenApplyDeletionThenDontWait) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    EXPECT_FALSE(allocation->isUsed());
    EXPECT_EQ(0u, memoryManager->freeGraphicsMemoryCalled);
    while (!asyncDeleter->doWorkInBackground) {
        std::this_thread::yield(); // wait for start async thread work
    }
    std::unique_lock<std::mutex> lock(asyncDeleter->queueMutex);
    asyncDeleter->allowExit = true;
    lock.unlock();
    asyncDeleter->deferDeletion(new DeferrableAllocationDeletion(*memoryManager, *allocation));
    while (!asyncDeleter->shouldStopReached) { // wait async thread job end
        std::this_thread::yield();
    }
    EXPECT_EQ(1u, memoryManager->freeGraphicsMemoryCalled);
}

TEST_F(DeferrableAllocationDeletionMtTest, givenTwoAllocationsUsedByOneOsContextsEnqueuedToAsyncDeleterWhenOneAllocationIsCompletedThenReleaseThatAllocation) {
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
    while (0u == memoryManager->freeGraphicsMemoryCalled) { // wait for delete second allocation
        std::this_thread::yield();
    }
    EXPECT_EQ(1u, memoryManager->freeGraphicsMemoryCalled);
    asyncDeleter->allowExit = true;
    *hwTag = 2u;
}
