/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/fixtures/memory_allocator_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/utilities/containers_tests_helpers.h"

struct InternalAllocationStorageTest : public MemoryAllocatorFixture,
                                       public ::testing::Test {
    using MemoryAllocatorFixture::TearDown;
    void SetUp() override {
        MemoryAllocatorFixture::SetUp();
        storage = csr->getInternalAllocationStorage();
    }
    InternalAllocationStorage *storage;
};

TEST_F(InternalAllocationStorageTest, givenDebugFlagThatDisablesAllocationReuseWhenStoreReusableAllocationIsCalledThenAllocationIsReleased) {
    DebugManagerStateRestore stateRestorer;
    DebugManager.flags.DisableResourceRecycling.set(true);

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});

    storage->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);
    EXPECT_NE(allocation, csr->getAllocationsForReuse().peekHead());
    EXPECT_TRUE(csr->getAllocationsForReuse().peekIsEmpty());
}

TEST_F(InternalAllocationStorageTest, whenCleanAllocationListThenRemoveOnlyCompletedAllocations) {

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    auto allocation2 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    auto allocation3 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});

    allocation->updateTaskCount(10, csr->getOsContext().getContextId());
    allocation2->updateTaskCount(5, csr->getOsContext().getContextId());
    allocation3->updateTaskCount(15, csr->getOsContext().getContextId());

    storage->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), TEMPORARY_ALLOCATION);
    storage->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation2), TEMPORARY_ALLOCATION);
    storage->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation3), TEMPORARY_ALLOCATION);

    //head point to alloc 2, tail points to alloc3
    EXPECT_TRUE(csr->getTemporaryAllocations().peekContains(*allocation));
    EXPECT_TRUE(csr->getTemporaryAllocations().peekContains(*allocation2));
    EXPECT_TRUE(csr->getTemporaryAllocations().peekContains(*allocation3));
    EXPECT_EQ(-1, verifyDListOrder(csr->getTemporaryAllocations().peekHead(), allocation, allocation2, allocation3));

    //now remove element form the middle
    storage->cleanAllocationList(6, TEMPORARY_ALLOCATION);
    EXPECT_TRUE(csr->getTemporaryAllocations().peekContains(*allocation));
    EXPECT_FALSE(csr->getTemporaryAllocations().peekContains(*allocation2));
    EXPECT_TRUE(csr->getTemporaryAllocations().peekContains(*allocation3));
    EXPECT_EQ(-1, verifyDListOrder(csr->getTemporaryAllocations().peekHead(), allocation, allocation3));

    //now remove head
    storage->cleanAllocationList(11, TEMPORARY_ALLOCATION);
    EXPECT_FALSE(csr->getTemporaryAllocations().peekContains(*allocation));
    EXPECT_FALSE(csr->getTemporaryAllocations().peekContains(*allocation2));
    EXPECT_TRUE(csr->getTemporaryAllocations().peekContains(*allocation3));

    //now remove tail
    storage->cleanAllocationList(16, TEMPORARY_ALLOCATION);
    EXPECT_TRUE(csr->getTemporaryAllocations().peekIsEmpty());
}

TEST_F(InternalAllocationStorageTest, whenAllocationIsStoredAsReusableButIsStillUsedThenCannotBeObtained) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(AllocationProperties{0, MemoryConstants::pageSize, AllocationType::BUFFER, mockDeviceBitfield});

    storage->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION, 2u);

    auto *hwTag = csr->getTagAddress();

    *hwTag = 1u;
    auto newAllocation = storage->obtainReusableAllocation(1, AllocationType::BUFFER);
    EXPECT_EQ(nullptr, newAllocation);
    storage->cleanAllocationList(2u, REUSABLE_ALLOCATION);
}

TEST_F(InternalAllocationStorageTest, whenAllocationIsStoredAsTemporaryAndIsStillUsedThenCanBeObtained) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(AllocationProperties{0, MemoryConstants::pageSize, AllocationType::BUFFER, mockDeviceBitfield});

    storage->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(allocation), TEMPORARY_ALLOCATION, 2u);

    auto *hwTag = csr->getTagAddress();

    *hwTag = 1u;
    auto newAllocation = storage->obtainTemporaryAllocationWithPtr(1, allocation->getUnderlyingBuffer(), AllocationType::BUFFER);
    EXPECT_EQ(allocation, newAllocation.get());
    EXPECT_TRUE(csr->getTemporaryAllocations().peekIsEmpty());
    memoryManager->freeGraphicsMemory(newAllocation.release());
}

TEST_F(InternalAllocationStorageTest, givenTemporaryAllocationWhenAllocationIsObtainedThenItsTaskCountIsSetToNotReady) {
    const uint32_t initialTaskCount = 37u;
    const uint32_t contextId = csr->getOsContext().getContextId();

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(AllocationProperties{0, MemoryConstants::pageSize, AllocationType::BUFFER, mockDeviceBitfield});
    storage->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(allocation), TEMPORARY_ALLOCATION, initialTaskCount);
    ASSERT_EQ(initialTaskCount, allocation->getTaskCount(contextId));

    auto newAllocation = storage->obtainTemporaryAllocationWithPtr(1, allocation->getUnderlyingBuffer(), AllocationType::BUFFER);
    EXPECT_EQ(allocation, newAllocation.get());
    EXPECT_EQ(CompletionStamp::notReady, allocation->getTaskCount(contextId));
    memoryManager->freeGraphicsMemory(newAllocation.release());
}

TEST_F(InternalAllocationStorageTest, whenObtainAllocationFromEmptyReuseListThenReturnNullptr) {
    auto allocation2 = storage->obtainReusableAllocation(1, AllocationType::BUFFER);
    EXPECT_EQ(nullptr, allocation2);
}

TEST_F(InternalAllocationStorageTest, whenCompletedAllocationIsStoredAsReusableAndThenCanBeObtained) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(AllocationProperties{0, MemoryConstants::pageSize, AllocationType::BUFFER, mockDeviceBitfield});
    EXPECT_NE(nullptr, allocation);

    storage->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION, 2u);
    EXPECT_FALSE(csr->getAllocationsForReuse().peekIsEmpty());

    auto *hwTag = csr->getTagAddress();

    *hwTag = 2u;
    auto reusedAllocation = storage->obtainReusableAllocation(1, AllocationType::BUFFER).release();

    EXPECT_EQ(allocation, reusedAllocation);
    EXPECT_TRUE(csr->getAllocationsForReuse().peekIsEmpty());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(InternalAllocationStorageTest, whenNotUsedAllocationIsStoredAsReusableAndThenCanBeObtained) {

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(AllocationProperties{0, MemoryConstants::pageSize, AllocationType::BUFFER, mockDeviceBitfield});
    EXPECT_NE(nullptr, allocation);
    EXPECT_FALSE(allocation->isUsed());
    EXPECT_EQ(0u, csr->peekTaskCount());
    *csr->getTagAddress() = 0; // initial hw tag for dll

    storage->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);
    EXPECT_EQ(0u, allocation->getTaskCount(csr->getOsContext().getContextId()));
    EXPECT_FALSE(csr->getAllocationsForReuse().peekIsEmpty());

    auto reusedAllocation = storage->obtainReusableAllocation(1, AllocationType::BUFFER).release();

    EXPECT_EQ(allocation, reusedAllocation);
    EXPECT_TRUE(csr->getAllocationsForReuse().peekIsEmpty());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(InternalAllocationStorageTest, whenObtainAllocationFromMidlleOfReusableListThenItIsDetachedFromLinkedList) {
    auto &reusableAllocations = csr->getAllocationsForReuse();
    EXPECT_TRUE(reusableAllocations.peekIsEmpty());

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(AllocationProperties{0, 1, AllocationType::BUFFER, mockDeviceBitfield});
    auto allocation2 = memoryManager->allocateGraphicsMemoryWithProperties(AllocationProperties{0, 10000, AllocationType::BUFFER, mockDeviceBitfield});
    auto allocation3 = memoryManager->allocateGraphicsMemoryWithProperties(AllocationProperties{0, 1, AllocationType::BUFFER, mockDeviceBitfield});

    EXPECT_TRUE(reusableAllocations.peekIsEmpty());
    EXPECT_EQ(nullptr, allocation2->next);
    EXPECT_EQ(nullptr, allocation2->prev);

    storage->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);

    EXPECT_TRUE(reusableAllocations.peekContains(*allocation));
    EXPECT_FALSE(reusableAllocations.peekContains(*allocation2));
    EXPECT_FALSE(reusableAllocations.peekContains(*allocation3));
    EXPECT_EQ(nullptr, allocation2->next);
    EXPECT_EQ(nullptr, allocation2->prev);

    storage->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation2), REUSABLE_ALLOCATION);

    EXPECT_TRUE(reusableAllocations.peekContains(*allocation));
    EXPECT_TRUE(reusableAllocations.peekContains(*allocation2));
    EXPECT_FALSE(reusableAllocations.peekContains(*allocation3));
    EXPECT_EQ(nullptr, allocation2->next);
    EXPECT_EQ(allocation, allocation2->prev);

    storage->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation3), REUSABLE_ALLOCATION);

    EXPECT_TRUE(reusableAllocations.peekContains(*allocation));
    EXPECT_TRUE(reusableAllocations.peekContains(*allocation2));
    EXPECT_TRUE(reusableAllocations.peekContains(*allocation3));
    EXPECT_EQ(allocation3, allocation2->next);
    EXPECT_EQ(allocation, allocation2->prev);

    auto reusableAllocation = storage->obtainReusableAllocation(10000, AllocationType::BUFFER).release();
    EXPECT_EQ(reusableAllocation, allocation2);
    EXPECT_EQ(nullptr, allocation2->next);
    EXPECT_EQ(nullptr, allocation2->prev);

    EXPECT_EQ(nullptr, reusableAllocation->next);
    EXPECT_EQ(nullptr, reusableAllocation->prev);

    EXPECT_FALSE(reusableAllocations.peekContains(*reusableAllocation));
    EXPECT_TRUE(reusableAllocations.peekContains(*allocation));
    EXPECT_FALSE(reusableAllocations.peekContains(*allocation2));
    EXPECT_TRUE(reusableAllocations.peekContains(*allocation3));

    memoryManager->freeGraphicsMemory(allocation2);
    allocation->updateTaskCount(0u, csr->getOsContext().getContextId());
    allocation3->updateTaskCount(0u, csr->getOsContext().getContextId());
}

TEST_F(InternalAllocationStorageTest, givenAllocationWhenItIsPutOnReusableListWhenOtherAllocationTypeIsRequestedThenNullIsReturned) {
    EXPECT_TRUE(csr->getAllocationsForReuse().peekIsEmpty());

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(AllocationProperties{0, MemoryConstants::pageSize, AllocationType::BUFFER, mockDeviceBitfield});
    storage->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);

    EXPECT_FALSE(csr->getAllocationsForReuse().peekIsEmpty());

    auto internalAllocation = storage->obtainReusableAllocation(1, AllocationType::INTERNAL_HEAP);
    EXPECT_EQ(nullptr, internalAllocation);
}

class WaitAtDeletionAllocation : public MockGraphicsAllocation {
  public:
    WaitAtDeletionAllocation(void *buffer, size_t sizeIn)
        : MockGraphicsAllocation(buffer, sizeIn) {
        inDestructor = false;
    }

    std::mutex mutex;
    std::atomic<bool> inDestructor;
    ~WaitAtDeletionAllocation() override {
        inDestructor = true;
        std::lock_guard<std::mutex> lock(mutex);
    }
};

TEST_F(InternalAllocationStorageTest, givenAllocationListWhenTwoThreadsCleanConcurrentlyThenBothThreadsCanAccessTheList) {
    auto allocation1 = new WaitAtDeletionAllocation(nullptr, 0);
    allocation1->updateTaskCount(1, csr->getOsContext().getContextId());
    storage->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation1), TEMPORARY_ALLOCATION);

    std::unique_lock<std::mutex> allocationDeletionLock(allocation1->mutex);

    auto allocation2 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    allocation2->updateTaskCount(2, csr->getOsContext().getContextId());
    storage->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation2), TEMPORARY_ALLOCATION);

    std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);

    std::thread thread1([&] {
        storage->cleanAllocationList(1, TEMPORARY_ALLOCATION);
    });

    std::thread thread2([&] {
        std::lock_guard<std::mutex> lock(mutex);
        storage->cleanAllocationList(2, TEMPORARY_ALLOCATION);
    });

    while (!allocation1->inDestructor)
        ;
    lock.unlock();
    allocationDeletionLock.unlock();

    thread1.join();
    thread2.join();

    EXPECT_TRUE(csr->getTemporaryAllocations().peekIsEmpty());
}

HWTEST_F(InternalAllocationStorageTest, givenMultipleActivePartitionsWhenDetachingReusableAllocationThenCheckTaskCountFinishedOnAllTiles) {
    auto ultCsr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(csr);
    csr->setActivePartitions(2u);
    ultCsr->postSyncWriteOffset = 32;

    auto tagAddress = csr->getTagAddress();
    *tagAddress = 0xFF;
    tagAddress = ptrOffset(tagAddress, 32);
    *tagAddress = 0x0;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});

    storage->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);
    EXPECT_EQ(allocation, csr->getAllocationsForReuse().peekHead());
    EXPECT_FALSE(csr->getAllocationsForReuse().peekIsEmpty());
    allocation->updateTaskCount(1u, csr->getOsContext().getContextId());

    std::unique_ptr<GraphicsAllocation> allocationReusable = csr->getAllocationsForReuse().detachAllocation(0, nullptr, csr, AllocationType::INTERNAL_HOST_MEMORY);
    EXPECT_EQ(nullptr, allocationReusable.get());

    *tagAddress = 0x1;
    allocationReusable = csr->getAllocationsForReuse().detachAllocation(0, nullptr, csr, AllocationType::INTERNAL_HOST_MEMORY);
    EXPECT_EQ(allocation, allocationReusable.get());

    memoryManager->freeGraphicsMemory(allocationReusable.release());
}
