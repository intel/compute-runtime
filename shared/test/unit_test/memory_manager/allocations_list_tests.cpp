/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/memory_manager/allocations_list.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/fixtures/memory_allocator_fixture.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

struct AllocationsListFixtureTest : public MemoryAllocatorFixture,
                                    public ::testing::Test {
    void SetUp() override {
        MemoryAllocatorFixture::setUp();
    }
    void TearDown() override {
        MemoryAllocatorFixture::tearDown();
    }
};

TEST(AllocationsListTest, givenDefaultConstructedAllocationsListWhenQueriedThenIsEmpty) {
    AllocationsList allocations;
    EXPECT_TRUE(allocations.peekIsEmpty());
    EXPECT_EQ(nullptr, allocations.peekHead());
    EXPECT_EQ(nullptr, allocations.peekTail());
    EXPECT_TRUE(allocations.peekAllocations().empty());
}

TEST(AllocationsListTest, givenAllocationsListWithUsageWhenQueriedThenIsEmpty) {
    AllocationsList reusableList(REUSABLE_ALLOCATION);
    AllocationsList temporaryList(TEMPORARY_ALLOCATION);
    AllocationsList deferredList(DEFERRED_DEALLOCATION);
    EXPECT_TRUE(reusableList.peekIsEmpty());
    EXPECT_TRUE(temporaryList.peekIsEmpty());
    EXPECT_TRUE(deferredList.peekIsEmpty());
}

TEST(AllocationsListTest, givenEmptyAllocationsListWhenPushTailOneCalledThenAllocationIsAddedAtTail) {
    AllocationsList allocations;
    MockGraphicsAllocation firstAllocation;
    MockGraphicsAllocation secondAllocation;

    allocations.pushTailOne(firstAllocation);
    EXPECT_FALSE(allocations.peekIsEmpty());
    EXPECT_EQ(&firstAllocation, allocations.peekHead());
    EXPECT_EQ(&firstAllocation, allocations.peekTail());

    allocations.pushTailOne(secondAllocation);
    EXPECT_EQ(&firstAllocation, allocations.peekHead());
    EXPECT_EQ(&secondAllocation, allocations.peekTail());

    auto snapshot = allocations.peekAllocations();
    ASSERT_EQ(2u, snapshot.size());
    EXPECT_EQ(&firstAllocation, snapshot[0]);
    EXPECT_EQ(&secondAllocation, snapshot[1]);

    allocations.removeMatching([](GraphicsAllocation *) { return true; }, [](GraphicsAllocation *) {});
}

TEST(AllocationsListTest, givenAllocationsListWhenPushFrontOneCalledThenAllocationIsAddedAtHead) {
    AllocationsList allocations;
    MockGraphicsAllocation firstAllocation;
    MockGraphicsAllocation secondAllocation;

    allocations.pushFrontOne(firstAllocation);
    EXPECT_FALSE(allocations.peekIsEmpty());
    EXPECT_EQ(&firstAllocation, allocations.peekHead());
    EXPECT_EQ(&firstAllocation, allocations.peekTail());

    allocations.pushFrontOne(secondAllocation);
    EXPECT_EQ(&secondAllocation, allocations.peekHead());
    EXPECT_EQ(&firstAllocation, allocations.peekTail());

    auto snapshot = allocations.peekAllocations();
    ASSERT_EQ(2u, snapshot.size());
    EXPECT_EQ(&secondAllocation, snapshot[0]);
    EXPECT_EQ(&firstAllocation, snapshot[1]);

    allocations.removeMatching([](GraphicsAllocation *) { return true; }, [](GraphicsAllocation *) {});
}

TEST(AllocationsListTest, givenAllocationsListWhenPeekContainsCalledThenReportsMembership) {
    AllocationsList allocations;
    MockGraphicsAllocation storedAllocation;
    MockGraphicsAllocation otherAllocation;

    EXPECT_FALSE(allocations.peekContains(storedAllocation));
    allocations.pushTailOne(storedAllocation);
    EXPECT_TRUE(allocations.peekContains(storedAllocation));
    EXPECT_FALSE(allocations.peekContains(otherAllocation));

    allocations.removeMatching([](GraphicsAllocation *) { return true; }, [](GraphicsAllocation *) {});
}

TEST(AllocationsListTest, givenAllocationStoredInAllocationsListWhenRemoveOneCalledThenAllocationIsRemovedAndReturned) {
    AllocationsList allocations;
    auto storedAllocation = std::make_unique<MockGraphicsAllocation>();
    auto storedAllocationRawPtr = storedAllocation.get();

    allocations.pushTailOne(*storedAllocation.release());
    EXPECT_TRUE(allocations.peekContains(*storedAllocationRawPtr));

    auto removed = allocations.removeOne(*storedAllocationRawPtr);
    EXPECT_EQ(storedAllocationRawPtr, removed.get());
    EXPECT_FALSE(allocations.peekContains(*storedAllocationRawPtr));
    EXPECT_TRUE(allocations.peekIsEmpty());
}

TEST(AllocationsListTest, givenAllocationNotInListWhenRemoveOneCalledThenReturnsNullptr) {
    AllocationsList allocations;
    MockGraphicsAllocation absentAllocation;

    auto removed = allocations.removeOne(absentAllocation);
    EXPECT_EQ(nullptr, removed.get());
}

TEST(AllocationsListTest, givenAllocationsListWithMultipleAllocationsWhenRemoveOneCalledThenOnlyTargetIsRemoved) {
    AllocationsList allocations;
    MockGraphicsAllocation firstAllocation;
    MockGraphicsAllocation middleAllocation;
    MockGraphicsAllocation lastAllocation;

    allocations.pushTailOne(firstAllocation);
    allocations.pushTailOne(middleAllocation);
    allocations.pushTailOne(lastAllocation);

    auto removed = allocations.removeOne(middleAllocation);
    EXPECT_EQ(&middleAllocation, removed.release());

    auto snapshot = allocations.peekAllocations();
    ASSERT_EQ(2u, snapshot.size());
    EXPECT_EQ(&firstAllocation, snapshot[0]);
    EXPECT_EQ(&lastAllocation, snapshot[1]);

    allocations.removeMatching([](GraphicsAllocation *) { return true; }, [](GraphicsAllocation *) {});
}

TEST(AllocationsListTest, givenAllocationsListWhenRemoveMatchingCalledThenMatchingAllocationsAreRemovedAndActionInvoked) {
    AllocationsList allocations;
    MockGraphicsAllocation firstAllocation;
    MockGraphicsAllocation secondAllocation;
    MockGraphicsAllocation thirdAllocation;

    allocations.pushTailOne(firstAllocation);
    allocations.pushTailOne(secondAllocation);
    allocations.pushTailOne(thirdAllocation);

    std::vector<GraphicsAllocation *> removedAllocations;
    allocations.removeMatching(
        [&](GraphicsAllocation *allocation) {
            return allocation == &secondAllocation;
        },
        [&](GraphicsAllocation *allocation) {
            removedAllocations.push_back(allocation);
        });

    ASSERT_EQ(1u, removedAllocations.size());
    EXPECT_EQ(&secondAllocation, removedAllocations[0]);

    auto snapshot = allocations.peekAllocations();
    ASSERT_EQ(2u, snapshot.size());
    EXPECT_EQ(&firstAllocation, snapshot[0]);
    EXPECT_EQ(&thirdAllocation, snapshot[1]);

    allocations.removeMatching([](GraphicsAllocation *) { return true; }, [](GraphicsAllocation *) {});
}

TEST(AllocationsListTest, givenAllocationsListWhenRemoveMatchingCalledWithFalsePredicateThenNothingRemoved) {
    AllocationsList allocations;
    MockGraphicsAllocation firstAllocation;
    MockGraphicsAllocation secondAllocation;

    allocations.pushTailOne(firstAllocation);
    allocations.pushTailOne(secondAllocation);

    uint32_t actionInvocations = 0u;
    allocations.removeMatching(
        [](GraphicsAllocation *) { return false; },
        [&](GraphicsAllocation *) { actionInvocations++; });

    EXPECT_EQ(0u, actionInvocations);
    EXPECT_EQ(2u, allocations.peekAllocations().size());

    allocations.removeMatching([](GraphicsAllocation *) { return true; }, [](GraphicsAllocation *) {});
}

TEST(AllocationsListTest, givenEmptyTargetWhenTransferAllAllocationsToCalledThenAllocationsAreMovedAndSourceIsEmpty) {
    AllocationsList source;
    AllocationsList target;
    MockGraphicsAllocation firstAllocation;
    MockGraphicsAllocation secondAllocation;

    source.pushTailOne(firstAllocation);
    source.pushTailOne(secondAllocation);

    source.transferAllAllocationsTo(target);

    EXPECT_TRUE(source.peekIsEmpty());
    auto snapshot = target.peekAllocations();
    ASSERT_EQ(2u, snapshot.size());
    EXPECT_EQ(&firstAllocation, snapshot[0]);
    EXPECT_EQ(&secondAllocation, snapshot[1]);

    target.removeMatching([](GraphicsAllocation *) { return true; }, [](GraphicsAllocation *) {});
}

TEST(AllocationsListTest, givenNonEmptyTargetWhenTransferAllAllocationsToCalledThenAllocationsAreAppendedAtTail) {
    AllocationsList source;
    AllocationsList target;
    MockGraphicsAllocation existingAllocation;
    MockGraphicsAllocation firstAllocation;
    MockGraphicsAllocation secondAllocation;

    target.pushTailOne(existingAllocation);
    source.pushTailOne(firstAllocation);
    source.pushTailOne(secondAllocation);

    source.transferAllAllocationsTo(target);

    EXPECT_TRUE(source.peekIsEmpty());
    auto snapshot = target.peekAllocations();
    ASSERT_EQ(3u, snapshot.size());
    EXPECT_EQ(&existingAllocation, snapshot[0]);
    EXPECT_EQ(&firstAllocation, snapshot[1]);
    EXPECT_EQ(&secondAllocation, snapshot[2]);

    target.removeMatching([](GraphicsAllocation *) { return true; }, [](GraphicsAllocation *) {});
}

TEST(AllocationsListTest, givenTransferAllAllocationsToCalledWithSelfThenContentIsUnchanged) {
    AllocationsList allocations;
    MockGraphicsAllocation firstAllocation;
    MockGraphicsAllocation secondAllocation;

    allocations.pushTailOne(firstAllocation);
    allocations.pushTailOne(secondAllocation);

    allocations.transferAllAllocationsTo(allocations);

    auto snapshot = allocations.peekAllocations();
    ASSERT_EQ(2u, snapshot.size());
    EXPECT_EQ(&firstAllocation, snapshot[0]);
    EXPECT_EQ(&secondAllocation, snapshot[1]);

    allocations.removeMatching([](GraphicsAllocation *) { return true; }, [](GraphicsAllocation *) {});
}

TEST_F(AllocationsListFixtureTest, givenAllocationStoredInListWhenFreeAllGraphicsAllocationsCalledThenAllocationIsFreedAndListIsEmpty) {
    AllocationsList allocations;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{mockRootDeviceIndex, MemoryConstants::pageSize});
    ASSERT_NE(nullptr, allocation);

    allocations.pushTailOne(*allocation);
    EXPECT_FALSE(allocations.peekIsEmpty());

    allocations.freeAllGraphicsAllocations(device.get());
    EXPECT_TRUE(allocations.peekIsEmpty());
}

TEST_F(AllocationsListFixtureTest, givenAllocationsListWhenDetachAllocationRequestedWithMatchingSizeAndNoCsrThenAllocationIsDetached) {
    AllocationsList allocations(REUSABLE_ALLOCATION);
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{mockRootDeviceIndex, MemoryConstants::pageSize});
    ASSERT_NE(nullptr, allocation);

    allocations.pushTailOne(*allocation);

    auto detached = allocations.detachAllocation(MemoryConstants::pageSize, nullptr, nullptr, allocation->getAllocationType());
    EXPECT_EQ(allocation, detached.get());
    EXPECT_TRUE(allocations.peekIsEmpty());

    memoryManager->freeGraphicsMemory(detached.release());
}

TEST_F(AllocationsListFixtureTest, givenAllocationsListWhenDetachAllocationRequestedWithDifferentTypeThenNullIsReturned) {
    AllocationsList allocations(REUSABLE_ALLOCATION);
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{mockRootDeviceIndex, MemoryConstants::pageSize});
    ASSERT_NE(nullptr, allocation);

    allocations.pushTailOne(*allocation);

    auto detached = allocations.detachAllocation(MemoryConstants::pageSize, nullptr, nullptr, AllocationType::commandBuffer);
    EXPECT_EQ(nullptr, detached.get());
    EXPECT_FALSE(allocations.peekIsEmpty());

    allocations.freeAllGraphicsAllocations(device.get());
}

TEST_F(AllocationsListFixtureTest, givenAllocationsListWhenDetachAllocationRequestedWithLargerSizeThenNullIsReturned) {
    AllocationsList allocations(REUSABLE_ALLOCATION);
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{mockRootDeviceIndex, MemoryConstants::pageSize});
    ASSERT_NE(nullptr, allocation);

    allocations.pushTailOne(*allocation);

    auto detached = allocations.detachAllocation(MemoryConstants::pageSize * 64, nullptr, nullptr, allocation->getAllocationType());
    EXPECT_EQ(nullptr, detached.get());
    EXPECT_FALSE(allocations.peekIsEmpty());

    allocations.freeAllGraphicsAllocations(device.get());
}

TEST_F(AllocationsListFixtureTest, givenAllocationsListWhenForceSystemMemoryDoesNotMatchThenAllocationIsNotDetached) {
    AllocationsList allocations(REUSABLE_ALLOCATION);
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{mockRootDeviceIndex, MemoryConstants::pageSize});
    ASSERT_NE(nullptr, allocation);
    allocation->storageInfo.systemMemoryForced = false;

    allocations.pushTailOne(*allocation);

    auto detached = allocations.detachAllocation(MemoryConstants::pageSize, nullptr, true /*forceSystemMemoryFlag*/, nullptr, allocation->getAllocationType());
    EXPECT_EQ(nullptr, detached.get());
    EXPECT_FALSE(allocations.peekIsEmpty());

    allocations.freeAllGraphicsAllocations(device.get());
}

TEST_F(AllocationsListFixtureTest, givenReusableListWithUsedAllocationWhenDetachAllocationRequestedWithCsrAndTaskNotReadyThenNullIsReturned) {
    AllocationsList allocations(REUSABLE_ALLOCATION);
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, allocation);

    allocation->updateTaskCount(10u, csr->getOsContext().getContextId());
    *csr->getTagAddress() = 0u;
    allocations.pushTailOne(*allocation);

    auto detached = allocations.detachAllocation(MemoryConstants::pageSize, nullptr, csr, allocation->getAllocationType());
    EXPECT_EQ(nullptr, detached.get());
    EXPECT_FALSE(allocations.peekIsEmpty());

    allocations.freeAllGraphicsAllocations(device.get());
}

TEST_F(AllocationsListFixtureTest, givenReusableListWithReadyAllocationWhenDetachAllocationRequestedWithCsrThenAllocationIsDetached) {
    AllocationsList allocations(REUSABLE_ALLOCATION);
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, allocation);

    allocation->updateTaskCount(1u, csr->getOsContext().getContextId());
    *csr->getTagAddress() = 100u;
    allocations.pushTailOne(*allocation);

    auto detached = allocations.detachAllocation(MemoryConstants::pageSize, nullptr, csr, allocation->getAllocationType());
    EXPECT_EQ(allocation, detached.get());
    EXPECT_TRUE(allocations.peekIsEmpty());

    memoryManager->freeGraphicsMemory(detached.release());
}

TEST_F(AllocationsListFixtureTest, givenTemporaryListWhenDetachAllocationRequestedWithCsrThenTaskCountIsSetToNotReady) {
    AllocationsList allocations(TEMPORARY_ALLOCATION);
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, allocation);

    allocations.pushTailOne(*allocation);

    auto detached = allocations.detachAllocation(MemoryConstants::pageSize, allocation->getUnderlyingBuffer(), csr, allocation->getAllocationType());
    EXPECT_EQ(allocation, detached.get());
    EXPECT_EQ(CompletionStamp::notReady, detached->getTaskCount(csr->getOsContext().getContextId()));

    memoryManager->freeGraphicsMemory(detached.release());
}

TEST_F(AllocationsListFixtureTest, givenAllocationsListWhenDetachAllocationRequestedWithDifferentRequiredPtrThenAllocationIsNotDetached) {
    AllocationsList allocations(REUSABLE_ALLOCATION);
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, allocation);

    allocation->updateTaskCount(1u, csr->getOsContext().getContextId());
    *csr->getTagAddress() = 100u;
    allocations.pushTailOne(*allocation);

    void *differentPtr = reinterpret_cast<void *>(static_cast<uintptr_t>(0xDEADBEEF));
    auto detached = allocations.detachAllocation(MemoryConstants::pageSize, differentPtr, csr, allocation->getAllocationType());
    EXPECT_EQ(nullptr, detached.get());
    EXPECT_FALSE(allocations.peekIsEmpty());

    allocations.freeAllGraphicsAllocations(device.get());
}

TEST_F(AllocationsListFixtureTest, givenTemporaryListWithExternalHostPtrAllocationWhenDetachWithOverlappingHostPtrThenPartialOverlapIsReported) {
    AllocationsList allocations(TEMPORARY_ALLOCATION);
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(AllocationProperties{0, MemoryConstants::pageSize, AllocationType::externalHostPtr, mockDeviceBitfield});
    ASSERT_NE(nullptr, allocation);
    allocation->setAllocationType(AllocationType::externalHostPtr);
    auto mockAllocation = static_cast<MockGraphicsAllocation *>(allocation);
    mockAllocation->cpuPtr = reinterpret_cast<void *>(0x1000);

    allocations.pushTailOne(*allocation);

    bool partialOverlapFound = false;
    void *overlappingPtr = reinterpret_cast<void *>(0x1800);
    auto detached = allocations.detachAllocation(MemoryConstants::pageSize * 2, overlappingPtr, csr, AllocationType::externalHostPtr, &partialOverlapFound);
    EXPECT_EQ(nullptr, detached.get());
    EXPECT_TRUE(partialOverlapFound);

    allocations.freeAllGraphicsAllocations(device.get());
}

TEST_F(AllocationsListFixtureTest, givenTemporaryListWithExternalHostPtrAllocationWhenDetachWithNonOverlappingHostPtrThenPartialOverlapIsNotReported) {
    AllocationsList allocations(TEMPORARY_ALLOCATION);
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(AllocationProperties{0, MemoryConstants::pageSize, AllocationType::externalHostPtr, mockDeviceBitfield});
    ASSERT_NE(nullptr, allocation);
    allocation->setAllocationType(AllocationType::externalHostPtr);
    auto mockAllocation = static_cast<MockGraphicsAllocation *>(allocation);
    mockAllocation->cpuPtr = reinterpret_cast<void *>(0x1000);

    allocations.pushTailOne(*allocation);

    bool partialOverlapFound = false;
    void *nonOverlappingPtr = reinterpret_cast<void *>(0x100000);
    auto detached = allocations.detachAllocation(MemoryConstants::pageSize * 2, nonOverlappingPtr, csr, AllocationType::externalHostPtr, &partialOverlapFound);
    EXPECT_EQ(nullptr, detached.get());
    EXPECT_FALSE(partialOverlapFound);

    allocations.freeAllGraphicsAllocations(device.get());
}

TEST_F(AllocationsListFixtureTest, givenAllocationsListWithMatchingTileBitfieldWhenDetachAllocationCalledThenAllocationIsDetached) {
    AllocationsList allocations(REUSABLE_ALLOCATION);
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, allocation);
    allocation->storageInfo.subDeviceBitfield = csr->getOsContext().getDeviceBitfield();
    allocation->updateTaskCount(1u, csr->getOsContext().getContextId());
    *csr->getTagAddress() = 100u;
    allocations.pushTailOne(*allocation);

    auto detached = allocations.detachAllocation(MemoryConstants::pageSize, nullptr, csr, allocation->getAllocationType());
    EXPECT_EQ(allocation, detached.get());

    memoryManager->freeGraphicsMemory(detached.release());
}

TEST_F(AllocationsListFixtureTest, givenAllocationsListWithMismatchedTileBitfieldWhenDetachAllocationCalledThenAllocationIsNotDetached) {
    AllocationsList allocations(REUSABLE_ALLOCATION);
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, allocation);
    DeviceBitfield differentBitfield(0b10);
    allocation->storageInfo.subDeviceBitfield = differentBitfield;
    allocation->updateTaskCount(1u, csr->getOsContext().getContextId());
    *csr->getTagAddress() = 100u;
    allocations.pushTailOne(*allocation);

    auto detached = allocations.detachAllocation(MemoryConstants::pageSize, nullptr, csr, allocation->getAllocationType());
    EXPECT_EQ(nullptr, detached.get());
    EXPECT_FALSE(allocations.peekIsEmpty());

    allocations.freeAllGraphicsAllocations(device.get());
}
