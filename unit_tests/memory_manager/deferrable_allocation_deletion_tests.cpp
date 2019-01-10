/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/device/device.h"
#include "runtime/memory_manager/deferred_deleter.h"
#include "runtime/memory_manager/deferrable_allocation_deletion.h"
#include "runtime/os_interface/os_context.h"

#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "unit_tests/mocks/mock_allocation_properties.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include "test.h"

using namespace OCLRT;

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
        auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
        memoryManager = new MockMemoryManager(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
        device1.reset(Device::create<MockDevice>(nullptr, executionEnvironment.release(), 0u));
        hwTag = device1->getDefaultEngine().commandStreamReceiver->getTagAddress();
        device1ContextId = device1->getDefaultEngine().osContext->getContextId();
        asyncDeleter = std::make_unique<DeferredDeleterPublic>();
        asyncDeleter->addClient();
    }
    void TearDown() override {
        asyncDeleter->allowExit = true;
        asyncDeleter->removeClient();
    }
    std::unique_ptr<DeferredDeleterPublic> asyncDeleter;
    MockMemoryManager *memoryManager = nullptr;
    std::unique_ptr<MockDevice> device1;
    uint32_t device1ContextId = 0;
    volatile uint32_t *hwTag = nullptr;
};

TEST_F(DeferrableAllocationDeletionTest, givenDeferrableAllocationWhenApplyThenWaitForEachTaskCount) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    allocation->updateTaskCount(1u, device1ContextId);
    *hwTag = 0u;
    asyncDeleter->deferDeletion(new DeferrableAllocationDeletion(*memoryManager, *allocation));
    while (!asyncDeleter->queue.peekIsEmpty()) // wait for async thread to get allocation from queue
        std::this_thread::yield();

    EXPECT_EQ(0u, memoryManager->freeGraphicsMemoryCalled);
    EXPECT_TRUE(allocation->isUsedByOsContext(device1ContextId));

    // let async thread exit
    asyncDeleter->allowExit = true;

    *hwTag = 1u; // allow to destroy allocation
    while (!asyncDeleter->shouldStopReached)
        std::this_thread::yield();
    EXPECT_EQ(1u, memoryManager->freeGraphicsMemoryCalled);
}

HWTEST_F(DeferrableAllocationDeletionTest, givenAllocationUsedByTwoOsContextsWhenApplyDeletionThenWaitForBothContextsAndFlushNotReadyCsr) {
    std::unique_ptr<MockDevice> device2(Device::create<MockDevice>(nullptr, device1->getExecutionEnvironment(), 1u));
    auto device2ContextId = device2->getDefaultEngine().osContext->getContextId();
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    *hwTag = 0u;
    *device2->getDefaultEngine().commandStreamReceiver->getTagAddress() = 1u;
    allocation->updateTaskCount(1u, device1ContextId);
    allocation->updateTaskCount(1u, device2ContextId);
    EXPECT_TRUE(allocation->isUsedByOsContext(device1ContextId));
    EXPECT_TRUE(allocation->isUsedByOsContext(device2ContextId));
    EXPECT_EQ(0u, memoryManager->freeGraphicsMemoryCalled);
    EXPECT_FALSE(device1->getUltCommandStreamReceiver<FamilyType>().flushBatchedSubmissionsCalled);
    EXPECT_FALSE(device2->getUltCommandStreamReceiver<FamilyType>().flushBatchedSubmissionsCalled);
    asyncDeleter->deferDeletion(new DeferrableAllocationDeletion(*memoryManager, *allocation));
    while (allocation->isUsedByOsContext(device2ContextId)) // wait for second context completion signal
        std::this_thread::yield();
    EXPECT_EQ(0u, memoryManager->freeGraphicsMemoryCalled);
    EXPECT_TRUE(device1->getUltCommandStreamReceiver<FamilyType>().flushBatchedSubmissionsCalled);
    EXPECT_FALSE(device2->getUltCommandStreamReceiver<FamilyType>().flushBatchedSubmissionsCalled);
    asyncDeleter->allowExit = true;
    *hwTag = 1u;
}
TEST_F(DeferrableAllocationDeletionTest, givenNotUsedAllocationWhenApplyDeletionThenDontWait) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
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
    auto allocation1 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    auto allocation2 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    *hwTag = 1u;
    allocation1->updateTaskCount(2u, device1ContextId);
    allocation2->updateTaskCount(1u, device1ContextId);
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
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    *hwTag = 0u;
    allocation->updateTaskCount(1u, device1ContextId);
    EXPECT_EQ(0u, memoryManager->freeGraphicsMemoryCalled);
    DeferrableAllocationDeletion deletion{*memoryManager, *allocation};
    EXPECT_FALSE(deletion.apply());
    EXPECT_EQ(0u, memoryManager->freeGraphicsMemoryCalled);
    *hwTag = 1u; // complete allocation
    EXPECT_TRUE(deletion.apply());
    EXPECT_EQ(1u, memoryManager->freeGraphicsMemoryCalled);
}

TEST_F(DeferrableAllocationDeletionTest, givenNotUsedAllocationWhenDeletionIsAppliedThenReturnTrue) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    EXPECT_FALSE(allocation->isUsed());
    DeferrableAllocationDeletion deletion{*memoryManager, *allocation};
    EXPECT_TRUE(deletion.apply());
    EXPECT_EQ(1u, memoryManager->freeGraphicsMemoryCalled);
}
