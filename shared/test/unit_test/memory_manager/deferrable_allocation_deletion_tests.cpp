/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/deferrable_allocation_deletion.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

struct DeferrableAllocationDeletionTest : ::testing::Test {
    void SetUp() override {
        executionEnvironment = new MockExecutionEnvironment(defaultHwInfo.get(), false, 1u);
        executionEnvironment->incRefInternal();
        memoryManager = new MockMemoryManager(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
        device.reset(Device::create<MockDevice>(executionEnvironment, 0u));
        hwTag = device->getDefaultEngine().commandStreamReceiver->getTagAddress();
        defaultOsContextId = device->getDefaultEngine().osContext->getContextId();
    }
    void TearDown() override {
        executionEnvironment->decRefInternal();
    }
    ExecutionEnvironment *executionEnvironment;
    MockMemoryManager *memoryManager = nullptr;
    std::unique_ptr<MockDevice> device;
    uint32_t defaultOsContextId = 0;
    volatile TagAddressType *hwTag = nullptr;
};

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
    auto rootDeviceIndex = device->getRootDeviceIndex();
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize});
    allocation->updateTaskCount(2u, defaultOsContextId);
    EXPECT_TRUE(allocation->isUsed());
    DeferrableAllocationDeletion deletion{*memoryManager, *allocation};

    device.reset();
    for (const auto &rootDeviceEnvironment : executionEnvironment->rootDeviceEnvironments) {
        executionEnvironment->releaseRootDeviceEnvironmentResources(rootDeviceEnvironment.get());
    }
    executionEnvironment->rootDeviceEnvironments.clear();
    EXPECT_EQ(0u, memoryManager->getRegisteredEngines(rootDeviceIndex).size());
    EXPECT_TRUE(allocation->isUsed());

    memoryManager->freeGraphicsMemoryCalled = 0u;
    EXPECT_TRUE(deletion.apply());
    EXPECT_EQ(1u, memoryManager->freeGraphicsMemoryCalled);
}

HWTEST_F(DeferrableAllocationDeletionTest, givenMultiTileWhenTaskCompletedOnSingleTileThenDoNotFreeGraphicsAllocation) {
    auto csr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(device->getDefaultEngine().commandStreamReceiver);
    csr->setActivePartitions(2u);
    csr->immWritePostSyncWriteOffset = 32;
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

TEST_F(DeferrableAllocationDeletionTest, givenAllocationWithCompletedTaskCountWhenApplyThenFreeAndReturnTrue) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    allocation->updateTaskCount(1u, defaultOsContextId);
    *hwTag = 1u;

    DeferrableAllocationDeletion deletion{*memoryManager, *allocation};
    EXPECT_TRUE(deletion.apply());
    EXPECT_EQ(1u, memoryManager->freeGraphicsMemoryCalled);
}

HWTEST_F(DeferrableAllocationDeletionTest, givenAllocationWithPendingTaskCountNotYetFlushedWhenApplyThenUpdateTagFromWaitIsCalled) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    allocation->updateTaskCount(2u, defaultOsContextId);
    *hwTag = 0u;
    csr.setLatestFlushedTaskCount(1u); // flushed(1) < required(2), updateTagFromWait expected
    csr.flushBatchedSubmissionsCalled = false;

    DeferrableAllocationDeletion deletion{*memoryManager, *allocation};
    EXPECT_FALSE(deletion.apply());
    EXPECT_TRUE(csr.flushBatchedSubmissionsCalled);
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_F(DeferrableAllocationDeletionTest, givenAllocationWithPendingTaskCountAlreadyFlushedWhenApplyThenUpdateTagFromWaitIsNotCalled) {
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    allocation->updateTaskCount(2u, defaultOsContextId);
    *hwTag = 0u;
    csr.setLatestFlushedTaskCount(2u); // flushed(2) >= required(2), updateTagFromWait not expected
    csr.flushBatchedSubmissionsCalled = false;

    DeferrableAllocationDeletion deletion{*memoryManager, *allocation};
    EXPECT_FALSE(deletion.apply());
    EXPECT_FALSE(csr.flushBatchedSubmissionsCalled);
    memoryManager->freeGraphicsMemory(allocation);
}
