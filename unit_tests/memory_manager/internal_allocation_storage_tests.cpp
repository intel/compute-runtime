/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/internal_allocation_storage.h"
#include "unit_tests/fixtures/memory_allocator_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/utilities/containers_tests_helpers.h"
#include "test.h"

struct InternalAllocationStorageTest : public MemoryAllocatorFixture,
                                       public ::testing::Test {
    using MemoryAllocatorFixture::TearDown;
    void SetUp() override {
        MemoryAllocatorFixture::SetUp();
        csr->initializeTagAllocation();
        storage = csr->getInternalAllocationStorage();
    }
    InternalAllocationStorage *storage;
};

TEST_F(InternalAllocationStorageTest, givenDebugFlagThatDisablesAllocationReuseWhenStoreReusableAllocationIsCalledThenAllocationIsReleased) {
    DebugManagerStateRestore stateRestorer;
    DebugManager.flags.DisableResourceRecycling.set(true);
    void *host_ptr = (void *)0x1234;

    auto allocation = memoryManager->allocateGraphicsMemory(1, host_ptr);

    storage->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);
    EXPECT_NE(allocation, csr->getAllocationsForReuse().peekHead());
    EXPECT_TRUE(csr->getAllocationsForReuse().peekIsEmpty());
}

TEST_F(InternalAllocationStorageTest, whenCleanAllocationListThenRemoveOnlyCompletedAllocations) {
    void *host_ptr = (void *)0x1234;

    auto allocation = memoryManager->allocateGraphicsMemory(1, host_ptr);
    auto allocation2 = memoryManager->allocateGraphicsMemory(1, host_ptr);
    auto allocation3 = memoryManager->allocateGraphicsMemory(1, host_ptr);

    allocation->updateTaskCount(10, 0);
    allocation2->updateTaskCount(5, 0);
    allocation3->updateTaskCount(15, 0);

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
    void *host_ptr = (void *)0x1234;
    auto allocation = memoryManager->allocateGraphicsMemory(1, host_ptr);

    storage->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION, 2u);

    auto *hwTag = csr->getTagAddress();

    *hwTag = 1u;
    auto newAllocation = storage->obtainReusableAllocation(1, false);
    EXPECT_EQ(nullptr, newAllocation);
    storage->cleanAllocationList(2u, REUSABLE_ALLOCATION);
}

TEST_F(InternalAllocationStorageTest, whenObtainAllocationFromEmptyReuseListThenReturnNullptr) {
    auto allocation2 = storage->obtainReusableAllocation(1, false);
    EXPECT_EQ(nullptr, allocation2);
}

TEST_F(InternalAllocationStorageTest, whenCompletedAllocationIsStoredAsReusableAndThenCanBeObtained) {
    void *host_ptr = (void *)0x1234;
    auto allocation = memoryManager->allocateGraphicsMemory(1, host_ptr);
    EXPECT_NE(nullptr, allocation);

    storage->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION, 2u);
    EXPECT_FALSE(csr->getAllocationsForReuse().peekIsEmpty());

    auto *hwTag = csr->getTagAddress();

    *hwTag = 2u;
    auto reusedAllocation = storage->obtainReusableAllocation(1, false).release();

    EXPECT_EQ(allocation, reusedAllocation);
    EXPECT_TRUE(csr->getAllocationsForReuse().peekIsEmpty());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(InternalAllocationStorageTest, whenNotUsedAllocationIsStoredAsReusableAndThenCanBeObtained) {
    void *host_ptr = (void *)0x1234;
    auto allocation = memoryManager->allocateGraphicsMemory(1, host_ptr);
    EXPECT_NE(nullptr, allocation);
    EXPECT_FALSE(allocation->peekWasUsed());
    EXPECT_EQ(0u, csr->peekTaskCount());
    *csr->getTagAddress() = 0; // initial hw tag for dll

    storage->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);
    EXPECT_EQ(0u, allocation->getTaskCount(0u));
    EXPECT_FALSE(csr->getAllocationsForReuse().peekIsEmpty());

    auto reusedAllocation = storage->obtainReusableAllocation(1, false).release();

    EXPECT_EQ(allocation, reusedAllocation);
    EXPECT_TRUE(csr->getAllocationsForReuse().peekIsEmpty());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(InternalAllocationStorageTest, whenObtainAllocationFromMidlleOfReusableListThenItIsDetachedFromLinkedList) {
    auto &reusableAllocations = csr->getAllocationsForReuse();
    EXPECT_TRUE(reusableAllocations.peekIsEmpty());

    auto allocation = memoryManager->allocateGraphicsMemory(1);
    auto allocation2 = memoryManager->allocateGraphicsMemory(10000);
    auto allocation3 = memoryManager->allocateGraphicsMemory(1);

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

    auto reusableAllocation = storage->obtainReusableAllocation(10000, false).release();
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
    storage->cleanAllocationList(ObjectNotUsed, REUSABLE_ALLOCATION);
}

TEST_F(InternalAllocationStorageTest, givenNonInternalAllocationWhenItIsPutOnReusableListWhenInternalAllocationIsRequestedThenNullIsReturned) {
    EXPECT_TRUE(csr->getAllocationsForReuse().peekIsEmpty());

    auto allocation = memoryManager->allocateGraphicsMemory(4096);
    storage->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);

    EXPECT_FALSE(csr->getAllocationsForReuse().peekIsEmpty());

    auto internalAllocation = storage->obtainReusableAllocation(1, true);
    EXPECT_EQ(nullptr, internalAllocation);
}

TEST_F(InternalAllocationStorageTest, givenInternalAllocationWhenItIsPutOnReusableListWhenNonInternalAllocationIsRequestedThenNullIsReturned) {
    EXPECT_TRUE(csr->getAllocationsForReuse().peekIsEmpty());

    auto allocation = memoryManager->allocateGraphicsMemory(4096);
    allocation->is32BitAllocation = true;

    storage->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);

    EXPECT_FALSE(csr->getAllocationsForReuse().peekIsEmpty());

    auto internalAllocation = storage->obtainReusableAllocation(1, false);
    EXPECT_EQ(nullptr, internalAllocation);
}

TEST_F(InternalAllocationStorageTest, givenInternalAllocationWhenItIsPutOnReusableListWhenInternalAllocationIsRequestedThenItIsReturned) {
    EXPECT_TRUE(csr->getAllocationsForReuse().peekIsEmpty());

    auto allocation = memoryManager->allocateGraphicsMemory(4096);
    allocation->is32BitAllocation = true;

    storage->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);

    EXPECT_FALSE(csr->getAllocationsForReuse().peekIsEmpty());

    auto internalAllocation = storage->obtainReusableAllocation(1, true);
    EXPECT_EQ(allocation, internalAllocation.get());
    internalAllocation.release();
    memoryManager->freeGraphicsMemory(allocation);
}
