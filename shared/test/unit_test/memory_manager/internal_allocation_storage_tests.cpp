/*
 * Copyright (C) 2018-2025 Intel Corporation
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
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/utilities/containers_tests_helpers.h"

struct InternalAllocationStorageTest : public MemoryAllocatorFixture,
                                       public ::testing::Test {
    void SetUp() override {
        MemoryAllocatorFixture::setUp();
        storage = csr->getInternalAllocationStorage();
    }

    void TearDown() override {
        MemoryAllocatorFixture::tearDown();
    }
    InternalAllocationStorage *storage;
};

TEST_F(InternalAllocationStorageTest, givenDebugFlagThatDisablesAllocationReuseWhenStoreReusableAllocationIsCalledThenAllocationIsReleased) {
    DebugManagerStateRestore stateRestorer;
    debugManager.flags.DisableResourceRecycling.set(true);

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

    // head point to alloc 2, tail points to alloc3
    EXPECT_TRUE(csr->getTemporaryAllocations().peekContains(*allocation));
    EXPECT_TRUE(csr->getTemporaryAllocations().peekContains(*allocation2));
    EXPECT_TRUE(csr->getTemporaryAllocations().peekContains(*allocation3));
    EXPECT_EQ(-1, verifyDListOrder(csr->getTemporaryAllocations().peekHead(), allocation, allocation2, allocation3));

    // now remove element form the middle
    storage->cleanAllocationList(6, TEMPORARY_ALLOCATION);
    EXPECT_TRUE(csr->getTemporaryAllocations().peekContains(*allocation));
    EXPECT_FALSE(csr->getTemporaryAllocations().peekContains(*allocation2));
    EXPECT_TRUE(csr->getTemporaryAllocations().peekContains(*allocation3));
    EXPECT_EQ(-1, verifyDListOrder(csr->getTemporaryAllocations().peekHead(), allocation, allocation3));

    // now remove head
    storage->cleanAllocationList(11, TEMPORARY_ALLOCATION);
    EXPECT_FALSE(csr->getTemporaryAllocations().peekContains(*allocation));
    EXPECT_FALSE(csr->getTemporaryAllocations().peekContains(*allocation2));
    EXPECT_TRUE(csr->getTemporaryAllocations().peekContains(*allocation3));

    // now remove tail
    storage->cleanAllocationList(16, TEMPORARY_ALLOCATION);
    EXPECT_TRUE(csr->getTemporaryAllocations().peekIsEmpty());
}

TEST_F(InternalAllocationStorageTest, whenAllocationIsStoredAsReusableButIsStillUsedThenCannotBeObtained) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(AllocationProperties{0, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield});

    storage->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION, 2u);

    auto *hwTag = csr->getTagAddress();

    *hwTag = 1u;
    auto newAllocation = storage->obtainReusableAllocation(1, AllocationType::buffer);
    EXPECT_EQ(nullptr, newAllocation);
    storage->cleanAllocationList(2u, REUSABLE_ALLOCATION);
}

TEST_F(InternalAllocationStorageTest, whenGetDeferredAllocationsThenReturnDeferredAllocationsListFromInternalStorage) {
    EXPECT_EQ(&csr->getDeferredAllocations(), &csr->getInternalAllocationStorage()->getDeferredAllocations());
}

TEST_F(InternalAllocationStorageTest, whenAllocationIsStoredAsTemporaryAndIsStillUsedThenCanBeObtained) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(AllocationProperties{0, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield});

    storage->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(allocation), TEMPORARY_ALLOCATION, 2u);

    auto *hwTag = csr->getTagAddress();

    *hwTag = 1u;
    auto newAllocation = storage->obtainTemporaryAllocationWithPtr(1, allocation->getUnderlyingBuffer(), AllocationType::buffer);
    EXPECT_EQ(allocation, newAllocation.get());
    EXPECT_TRUE(csr->getTemporaryAllocations().peekIsEmpty());
    memoryManager->freeGraphicsMemory(newAllocation.release());
}

TEST_F(InternalAllocationStorageTest, givenTemporaryAllocationWhenAllocationIsObtainedThenItsTaskCountIsSetToNotReady) {
    const uint32_t initialTaskCount = 37u;
    const uint32_t contextId = csr->getOsContext().getContextId();

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(AllocationProperties{0, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield});
    storage->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(allocation), TEMPORARY_ALLOCATION, initialTaskCount);
    ASSERT_EQ(initialTaskCount, allocation->getTaskCount(contextId));

    auto newAllocation = storage->obtainTemporaryAllocationWithPtr(1, allocation->getUnderlyingBuffer(), AllocationType::buffer);
    EXPECT_EQ(allocation, newAllocation.get());
    EXPECT_EQ(CompletionStamp::notReady, allocation->getTaskCount(contextId));
    memoryManager->freeGraphicsMemory(newAllocation.release());
}

TEST_F(InternalAllocationStorageTest, whenObtainAllocationFromEmptyReuseListThenReturnNullptr) {
    auto allocation2 = storage->obtainReusableAllocation(1, AllocationType::buffer);
    EXPECT_EQ(nullptr, allocation2);
}

TEST_F(InternalAllocationStorageTest, whenCompletedAllocationIsStoredAsReusableAndThenCanBeObtained) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(AllocationProperties{0, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield});
    EXPECT_NE(nullptr, allocation);

    storage->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION, 2u);
    EXPECT_FALSE(csr->getAllocationsForReuse().peekIsEmpty());

    auto *hwTag = csr->getTagAddress();

    *hwTag = 2u;
    auto reusedAllocation = storage->obtainReusableAllocation(1, AllocationType::buffer).release();

    EXPECT_EQ(allocation, reusedAllocation);
    EXPECT_TRUE(csr->getAllocationsForReuse().peekIsEmpty());
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_F(InternalAllocationStorageTest, whenNotUsedAllocationIsStoredAsReusableAndThenCanBeObtained) {

    auto ultCsr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(csr);
    if (ultCsr->heaplessStateInitialized) {
        ultCsr->taskCount.store(0);
    }

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(AllocationProperties{0, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield});
    EXPECT_NE(nullptr, allocation);
    EXPECT_FALSE(allocation->isUsed());
    EXPECT_EQ(0u, csr->peekTaskCount());
    *csr->getTagAddress() = 0; // initial hw tag for dll

    storage->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);
    EXPECT_EQ(0u, allocation->getTaskCount(csr->getOsContext().getContextId()));
    EXPECT_FALSE(csr->getAllocationsForReuse().peekIsEmpty());

    auto reusedAllocation = storage->obtainReusableAllocation(1, AllocationType::buffer).release();

    EXPECT_EQ(allocation, reusedAllocation);
    EXPECT_TRUE(csr->getAllocationsForReuse().peekIsEmpty());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(InternalAllocationStorageTest, whenObtainAllocationFromMidlleOfReusableListThenItIsDetachedFromLinkedList) {
    auto &reusableAllocations = csr->getAllocationsForReuse();
    EXPECT_TRUE(reusableAllocations.peekIsEmpty());

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(AllocationProperties{0, 1, AllocationType::buffer, mockDeviceBitfield});
    auto allocation2 = memoryManager->allocateGraphicsMemoryWithProperties(AllocationProperties{0, 10000, AllocationType::buffer, mockDeviceBitfield});
    auto allocation3 = memoryManager->allocateGraphicsMemoryWithProperties(AllocationProperties{0, 1, AllocationType::buffer, mockDeviceBitfield});

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

    auto reusableAllocation = storage->obtainReusableAllocation(10000, AllocationType::buffer).release();
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

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(AllocationProperties{0, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield});
    storage->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);

    EXPECT_FALSE(csr->getAllocationsForReuse().peekIsEmpty());

    auto internalAllocation = storage->obtainReusableAllocation(1, AllocationType::internalHeap);
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
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));

    auto memoryManager = deviceFactory->rootDevices[0]->getMemoryManager();

    auto ultCsr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(deviceFactory->rootDevices[0]->getDefaultEngine().commandStreamReceiver);
    ultCsr->setActivePartitions(2);
    ultCsr->immWritePostSyncWriteOffset = 32;

    auto storage = ultCsr->getInternalAllocationStorage();

    auto tagAddress = ultCsr->getTagAddress();
    *tagAddress = 0xFF;
    tagAddress = ptrOffset(tagAddress, 32);
    *tagAddress = 0x0;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});

    storage->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);
    EXPECT_EQ(allocation, ultCsr->getAllocationsForReuse().peekHead());
    EXPECT_FALSE(ultCsr->getAllocationsForReuse().peekIsEmpty());
    allocation->updateTaskCount(1u, ultCsr->getOsContext().getContextId());

    std::unique_ptr<GraphicsAllocation> allocationReusable = ultCsr->getAllocationsForReuse().detachAllocation(0, nullptr, ultCsr, AllocationType::internalHostMemory);
    EXPECT_EQ(nullptr, allocationReusable.get());

    *tagAddress = 0x1;
    allocationReusable = ultCsr->getAllocationsForReuse().detachAllocation(0, nullptr, ultCsr, AllocationType::internalHostMemory);
    EXPECT_EQ(allocation, allocationReusable.get());

    memoryManager->freeGraphicsMemory(allocationReusable.release());
}

HWTEST_F(InternalAllocationStorageTest, givenSingleTempAllocationsListWhenStoringFromDifferentRootDeviceThenSelectCorrectly) {
    DebugManagerStateRestore restore;
    debugManager.flags.UseSingleListForTemporaryAllocations.set(1);

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(2, 1));

    auto memoryManager = deviceFactory->rootDevices[0]->getMemoryManager();

    auto rootCsr0 = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(deviceFactory->rootDevices[0]->getDefaultEngine().commandStreamReceiver);
    auto rootCsr1 = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(deviceFactory->rootDevices[1]->getDefaultEngine().commandStreamReceiver);

    auto allocation0 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootCsr0->getRootDeviceIndex(), MemoryConstants::pageSize});
    auto allocation1 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootCsr1->getRootDeviceIndex(), MemoryConstants::pageSize});

    memoryManager->storeTemporaryAllocation(std::unique_ptr<GraphicsAllocation>(allocation0), rootCsr0->getOsContext().getContextId(), 0);
    memoryManager->storeTemporaryAllocation(std::unique_ptr<GraphicsAllocation>(allocation1), rootCsr1->getOsContext().getContextId(), 0);

    std::unique_ptr<GraphicsAllocation> allocationReusable = memoryManager->obtainTemporaryAllocationWithPtr(rootCsr1, MemoryConstants::pageSize, allocation0->getUnderlyingBuffer(), allocation0->getAllocationType());
    EXPECT_EQ(nullptr, allocationReusable.get());

    allocationReusable = memoryManager->obtainTemporaryAllocationWithPtr(rootCsr0, MemoryConstants::pageSize, allocation0->getUnderlyingBuffer(), allocation0->getAllocationType());
    EXPECT_NE(nullptr, allocationReusable.get());

    memoryManager->freeGraphicsMemory(allocationReusable.release());

    allocationReusable = memoryManager->obtainTemporaryAllocationWithPtr(rootCsr0, MemoryConstants::pageSize, allocation1->getUnderlyingBuffer(), allocation1->getAllocationType());
    EXPECT_EQ(nullptr, allocationReusable.get());

    allocationReusable = memoryManager->obtainTemporaryAllocationWithPtr(rootCsr1, MemoryConstants::pageSize, allocation1->getUnderlyingBuffer(), allocation1->getAllocationType());
    EXPECT_NE(nullptr, allocationReusable.get());

    memoryManager->freeGraphicsMemory(allocationReusable.release());
}

HWTEST_F(InternalAllocationStorageTest, givenSingleTempAllocationsListWhenStoringFromDifferentTileThenSelectCorrectly) {
    DebugManagerStateRestore restore;
    debugManager.flags.UseSingleListForTemporaryAllocations.set(1);

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));

    auto memoryManager = deviceFactory->rootDevices[0]->getMemoryManager();

    auto csr0 = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(deviceFactory->rootDevices[0]->getDefaultEngine().commandStreamReceiver);
    auto csr1 = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(deviceFactory->subDevices[0]->getDefaultEngine().commandStreamReceiver);

    auto allocation0 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr0->getRootDeviceIndex(), MemoryConstants::pageSize});
    allocation0->storageInfo.subDeviceBitfield = csr0->deviceBitfield;
    auto allocation1 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr1->getRootDeviceIndex(), MemoryConstants::pageSize});
    allocation1->storageInfo.subDeviceBitfield = csr1->deviceBitfield;

    memoryManager->storeTemporaryAllocation(std::unique_ptr<GraphicsAllocation>(allocation0), csr0->getOsContext().getContextId(), 0);
    memoryManager->storeTemporaryAllocation(std::unique_ptr<GraphicsAllocation>(allocation1), csr1->getOsContext().getContextId(), 0);

    std::unique_ptr<GraphicsAllocation> allocationReusable = memoryManager->obtainTemporaryAllocationWithPtr(csr1, MemoryConstants::pageSize, allocation0->getUnderlyingBuffer(), allocation0->getAllocationType());
    EXPECT_EQ(nullptr, allocationReusable.get());

    allocationReusable = memoryManager->obtainTemporaryAllocationWithPtr(csr0, MemoryConstants::pageSize, allocation0->getUnderlyingBuffer(), allocation0->getAllocationType());
    EXPECT_NE(nullptr, allocationReusable.get());

    memoryManager->freeGraphicsMemory(allocationReusable.release());

    allocationReusable = memoryManager->obtainTemporaryAllocationWithPtr(csr0, MemoryConstants::pageSize, allocation1->getUnderlyingBuffer(), allocation1->getAllocationType());
    EXPECT_EQ(nullptr, allocationReusable.get());

    allocationReusable = memoryManager->obtainTemporaryAllocationWithPtr(csr1, MemoryConstants::pageSize, allocation1->getUnderlyingBuffer(), allocation1->getAllocationType());
    EXPECT_NE(nullptr, allocationReusable.get());

    memoryManager->freeGraphicsMemory(allocationReusable.release());
}

HWTEST_F(InternalAllocationStorageTest, givenSingleTempAllocationsListWhenStoringSysMemThenObtainCorrectly) {
    DebugManagerStateRestore restore;
    debugManager.flags.UseSingleListForTemporaryAllocations.set(1);

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));

    auto memoryManager = deviceFactory->rootDevices[0]->getMemoryManager();

    auto csr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(deviceFactory->subDevices[0]->getDefaultEngine().commandStreamReceiver);

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    allocation->storageInfo.subDeviceBitfield = 0;

    memoryManager->storeTemporaryAllocation(std::unique_ptr<GraphicsAllocation>(allocation), csr->getOsContext().getContextId(), 0);

    std::unique_ptr<GraphicsAllocation> allocationReusable = memoryManager->obtainTemporaryAllocationWithPtr(csr, MemoryConstants::pageSize, allocation->getUnderlyingBuffer(), allocation->getAllocationType());
    EXPECT_NE(nullptr, allocationReusable.get());

    memoryManager->freeGraphicsMemory(allocationReusable.release());
}

TEST_F(InternalAllocationStorageTest, givenInternalAllocationWhenTaskCountMetsExpectationAndItHasBeenAssignedThenAllocIsRemoved) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    uint32_t expectedTaskCount = 10u;
    *csr->getTagAddress() = expectedTaskCount;
    allocation->updateTaskCount(expectedTaskCount, csr->getOsContext().getContextId());
    allocation->hostPtrTaskCountAssignment = 0;
    storage->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), TEMPORARY_ALLOCATION);
    storage->cleanAllocationList(expectedTaskCount, TEMPORARY_ALLOCATION);
    EXPECT_TRUE(csr->getTemporaryAllocations().peekIsEmpty());
}

TEST_F(InternalAllocationStorageTest, givenInternalAllocationWhenTaskCountMetsExpectationAndItHasNotBeenAssignedThenAllocIsNotRemoved) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    uint32_t expectedTaskCount = 10u;
    *csr->getTagAddress() = expectedTaskCount;
    allocation->updateTaskCount(expectedTaskCount, csr->getOsContext().getContextId());
    allocation->hostPtrTaskCountAssignment = 1;
    storage->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), TEMPORARY_ALLOCATION);
    storage->cleanAllocationList(expectedTaskCount, TEMPORARY_ALLOCATION);
    EXPECT_FALSE(csr->getTemporaryAllocations().peekIsEmpty());
    allocation->hostPtrTaskCountAssignment = 0;
}
