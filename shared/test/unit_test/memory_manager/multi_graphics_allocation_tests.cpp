/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/migration_sync_data.h"
#include "shared/source/memory_manager/multi_graphics_allocation.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_migration_sync_data.h"
#include "shared/test/common/mocks/mock_multi_graphics_allocation.h"
#include "shared/test/common/mocks/ult_device_factory.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(MultiGraphicsAllocationTest, whenCreatingMultiGraphicsAllocationThenTheAllocationIsObtainableAsADefault) {
    GraphicsAllocation graphicsAllocation(1, // rootDeviceIndex
                                          AllocationType::BUFFER,
                                          nullptr, 0, 0, MemoryPool::System4KBPages, 0);

    MockMultiGraphicsAllocation multiGraphicsAllocation(1);
    EXPECT_EQ(2u, multiGraphicsAllocation.graphicsAllocations.size());

    multiGraphicsAllocation.addAllocation(&graphicsAllocation);

    EXPECT_EQ(&graphicsAllocation, multiGraphicsAllocation.getDefaultGraphicsAllocation());

    EXPECT_EQ(&graphicsAllocation, multiGraphicsAllocation.getGraphicsAllocation(graphicsAllocation.getRootDeviceIndex()));
    EXPECT_EQ(nullptr, multiGraphicsAllocation.getGraphicsAllocation(0));
}

TEST(MultiGraphicsAllocationTest, givenMultiGraphicsAllocationWhenAddingMultipleGraphicsAllocationsThenTheyAreObtainableByRootDeviceIndex) {
    GraphicsAllocation graphicsAllocation0(0, // rootDeviceIndex
                                           AllocationType::BUFFER,
                                           nullptr, 0, 0, MemoryPool::System4KBPages, 0);
    GraphicsAllocation graphicsAllocation1(1, // rootDeviceIndex
                                           AllocationType::BUFFER,
                                           nullptr, 0, 0, MemoryPool::System4KBPages, 0);

    MockMultiGraphicsAllocation multiGraphicsAllocation(1);

    EXPECT_EQ(2u, multiGraphicsAllocation.graphicsAllocations.size());
    multiGraphicsAllocation.addAllocation(&graphicsAllocation0);
    multiGraphicsAllocation.addAllocation(&graphicsAllocation1);

    EXPECT_EQ(&graphicsAllocation0, multiGraphicsAllocation.getGraphicsAllocation(graphicsAllocation0.getRootDeviceIndex()));
    EXPECT_EQ(&graphicsAllocation1, multiGraphicsAllocation.getGraphicsAllocation(graphicsAllocation1.getRootDeviceIndex()));
}

TEST(MultiGraphicsAllocationTest, givenMultiGraphicsAllocationWhenGettingAllocationTypeThenReturnAllocationTypeFromDefaultAllocation) {
    auto expectedAllocationType = AllocationType::BUFFER;
    GraphicsAllocation graphicsAllocation(1, // rootDeviceIndex
                                          expectedAllocationType,
                                          nullptr, 0, 0, MemoryPool::System4KBPages, 0);

    MockMultiGraphicsAllocation multiGraphicsAllocation(1);
    multiGraphicsAllocation.addAllocation(&graphicsAllocation);

    EXPECT_EQ(multiGraphicsAllocation.getDefaultGraphicsAllocation()->getAllocationType(), multiGraphicsAllocation.getAllocationType());
    EXPECT_EQ(expectedAllocationType, multiGraphicsAllocation.getAllocationType());
}

TEST(MultiGraphicsAllocationTest, givenMultiGraphicsAllocationWhenGettingCoherencyStatusThenReturnCoherencyStatusFromDefaultAllocation) {
    auto expectedAllocationType = AllocationType::BUFFER;
    GraphicsAllocation graphicsAllocation(1, // rootDeviceIndex
                                          expectedAllocationType,
                                          nullptr, 0, 0, MemoryPool::System4KBPages, 0);

    MockMultiGraphicsAllocation multiGraphicsAllocation(1);
    multiGraphicsAllocation.addAllocation(&graphicsAllocation);

    graphicsAllocation.setCoherent(true);
    EXPECT_TRUE(multiGraphicsAllocation.isCoherent());

    graphicsAllocation.setCoherent(false);
    EXPECT_FALSE(multiGraphicsAllocation.isCoherent());
}

TEST(MultiGraphicsAllocationTest, WhenCreatingMultiGraphicsAllocationWithoutGraphicsAllocationThenNoDefaultAllocationIsReturned) {
    MockMultiGraphicsAllocation multiGraphicsAllocation(1);
    EXPECT_EQ(nullptr, multiGraphicsAllocation.getDefaultGraphicsAllocation());
}

TEST(MultiGraphicsAllocationTest, givenMultiGraphicsAllocationWhenRemovingGraphicsAllocationThenTheAllocationIsNoLongerAvailable) {
    uint32_t rootDeviceIndex = 1u;
    GraphicsAllocation graphicsAllocation(rootDeviceIndex,
                                          AllocationType::BUFFER,
                                          nullptr, 0, 0, MemoryPool::System4KBPages, 0);

    MockMultiGraphicsAllocation multiGraphicsAllocation(rootDeviceIndex);
    EXPECT_EQ(2u, multiGraphicsAllocation.graphicsAllocations.size());

    multiGraphicsAllocation.addAllocation(&graphicsAllocation);

    EXPECT_EQ(&graphicsAllocation, multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));

    multiGraphicsAllocation.removeAllocation(rootDeviceIndex);

    EXPECT_EQ(nullptr, multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));
}

struct MultiGraphicsAllocationTests : ::testing::Test {

    void SetUp() override {
        memoryManager = deviceFactory.rootDevices[0]->getMemoryManager();
    }
    void TearDown() override {
        for (auto &rootDeviceIndex : rootDeviceIndices) {
            memoryManager->freeGraphicsMemory(multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));
        }
    }

    UltDeviceFactory deviceFactory{2, 0};
    MockMultiGraphicsAllocation multiGraphicsAllocation{1};
    std::vector<uint32_t> rootDeviceIndices{{0u, 1u}};
    MemoryManager *memoryManager = nullptr;
};

TEST_F(MultiGraphicsAllocationTests, whenCreatingMultiGraphicsAllocationWithSharedStorageThenMigrationIsNotRequired) {

    AllocationProperties allocationProperties{0u,
                                              true, //allocateMemory
                                              MemoryConstants::pageSize,
                                              AllocationType::BUFFER_HOST_MEMORY,
                                              false, //multiOsContextCapable
                                              false, //isMultiStorageAllocationParam
                                              systemMemoryBitfield};

    auto ptr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, allocationProperties, multiGraphicsAllocation);
    EXPECT_NE(nullptr, ptr);

    EXPECT_EQ(2u, multiGraphicsAllocation.graphicsAllocations.size());

    EXPECT_NE(nullptr, multiGraphicsAllocation.getGraphicsAllocation(0)->getUnderlyingBuffer());
    EXPECT_EQ(multiGraphicsAllocation.getGraphicsAllocation(0)->getUnderlyingBuffer(), multiGraphicsAllocation.getGraphicsAllocation(1)->getUnderlyingBuffer());

    EXPECT_FALSE(multiGraphicsAllocation.requiresMigrations());
}

TEST_F(MultiGraphicsAllocationTests, whenCreatingMultiGraphicsAllocationWithExistingSystemMemoryThenMigrationIsNotRequired) {

    uint8_t hostPtr[MemoryConstants::pageSize]{};

    AllocationProperties allocationProperties{0u,
                                              false, //allocateMemory
                                              MemoryConstants::pageSize,
                                              AllocationType::BUFFER_HOST_MEMORY,
                                              false, //multiOsContextCapable
                                              false, //isMultiStorageAllocationParam
                                              systemMemoryBitfield};

    multiGraphicsAllocation.addAllocation(memoryManager->allocateGraphicsMemoryWithProperties(allocationProperties, hostPtr));
    EXPECT_FALSE(multiGraphicsAllocation.requiresMigrations());

    allocationProperties.rootDeviceIndex = 1u;
    multiGraphicsAllocation.addAllocation(memoryManager->allocateGraphicsMemoryWithProperties(allocationProperties, hostPtr));

    EXPECT_FALSE(multiGraphicsAllocation.requiresMigrations());

    multiGraphicsAllocation.setMultiStorage(false);
    EXPECT_FALSE(multiGraphicsAllocation.requiresMigrations());
}

TEST_F(MultiGraphicsAllocationTests, whenCreatingMultiGraphicsAllocationWithSeparatedStorageThenMigrationIsRequired) {
    AllocationProperties allocationProperties{0u,
                                              true, //allocateMemory
                                              MemoryConstants::pageSize,
                                              AllocationType::BUFFER_HOST_MEMORY,
                                              false, //multiOsContextCapable
                                              false, //isMultiStorageAllocationParam
                                              systemMemoryBitfield};

    multiGraphicsAllocation.addAllocation(memoryManager->allocateGraphicsMemoryWithProperties(allocationProperties));
    EXPECT_FALSE(multiGraphicsAllocation.requiresMigrations());

    allocationProperties.rootDeviceIndex = 1u;
    multiGraphicsAllocation.addAllocation(memoryManager->allocateGraphicsMemoryWithProperties(allocationProperties));
    EXPECT_FALSE(multiGraphicsAllocation.requiresMigrations());

    multiGraphicsAllocation.setMultiStorage(true);
    EXPECT_TRUE(multiGraphicsAllocation.requiresMigrations());
}

TEST_F(MultiGraphicsAllocationTests, givenMultiGraphicsAllocationThatRequiresMigrationWhenCopyOrMoveMultiGraphicsAllocationThenTheCopyStillRequiresMigration) {
    AllocationProperties allocationProperties{0u,
                                              true, //allocateMemory
                                              MemoryConstants::pageSize,
                                              AllocationType::BUFFER_HOST_MEMORY,
                                              false, //multiOsContextCapable
                                              false, //isMultiStorageAllocationParam
                                              systemMemoryBitfield};

    multiGraphicsAllocation.addAllocation(memoryManager->allocateGraphicsMemoryWithProperties(allocationProperties));

    allocationProperties.rootDeviceIndex = 1u;
    multiGraphicsAllocation.addAllocation(memoryManager->allocateGraphicsMemoryWithProperties(allocationProperties));

    multiGraphicsAllocation.setMultiStorage(true);
    EXPECT_TRUE(multiGraphicsAllocation.requiresMigrations());
    EXPECT_EQ(1, multiGraphicsAllocation.migrationSyncData->getRefInternalCount());
    {

        auto copyMultiGraphicsAllocation(multiGraphicsAllocation);
        EXPECT_TRUE(copyMultiGraphicsAllocation.requiresMigrations());
        EXPECT_EQ(2, multiGraphicsAllocation.migrationSyncData->getRefInternalCount());

        auto movedMultiGraphicsAllocation(std::move(copyMultiGraphicsAllocation));
        EXPECT_TRUE(movedMultiGraphicsAllocation.requiresMigrations());
        EXPECT_EQ(2, multiGraphicsAllocation.migrationSyncData->getRefInternalCount());
    }
    EXPECT_EQ(1, multiGraphicsAllocation.migrationSyncData->getRefInternalCount());
}

struct MigrationSyncDataTests : public MultiGraphicsAllocationTests {
    void SetUp() override {
        MultiGraphicsAllocationTests::SetUp();
        AllocationProperties allocationProperties{0u,
                                                  true, //allocateMemory
                                                  MemoryConstants::pageSize,
                                                  AllocationType::BUFFER_HOST_MEMORY,
                                                  false, //multiOsContextCapable
                                                  false, //isMultiStorageAllocationParam
                                                  systemMemoryBitfield};

        multiGraphicsAllocation.addAllocation(memoryManager->allocateGraphicsMemoryWithProperties(allocationProperties));

        allocationProperties.rootDeviceIndex = 1u;
        multiGraphicsAllocation.addAllocation(memoryManager->allocateGraphicsMemoryWithProperties(allocationProperties));

        multiGraphicsAllocation.setMultiStorage(true);
        EXPECT_TRUE(multiGraphicsAllocation.requiresMigrations());

        migrationSyncData = multiGraphicsAllocation.getMigrationSyncData();
    }

    void TearDown() override {
        MultiGraphicsAllocationTests::TearDown();
    }

    MigrationSyncData *migrationSyncData = nullptr;
};

TEST_F(MigrationSyncDataTests, whenMigrationSyncDataExistsAndSetMultiStorageIsCalledThenReuseSameMigrationSyncData) {

    EXPECT_NE(nullptr, migrationSyncData);

    multiGraphicsAllocation.setMultiStorage(true);

    EXPECT_EQ(migrationSyncData, multiGraphicsAllocation.getMigrationSyncData());
}

TEST(MigrationSyncDataTest, givenEmptyMultiGraphicsAllocationWhenSetMultiStorageIsCalledThenAbortIsCalled) {
    MultiGraphicsAllocation multiGraphicsAllocation(1);
    EXPECT_EQ(nullptr, multiGraphicsAllocation.getDefaultGraphicsAllocation());
    EXPECT_THROW(multiGraphicsAllocation.setMultiStorage(true), std::exception);
}

TEST_F(MigrationSyncDataTests, whenMigrationIsNotStartedThenMigrationIsNotInProgress) {
    EXPECT_FALSE(migrationSyncData->isMigrationInProgress());

    migrationSyncData->startMigration();

    EXPECT_TRUE(migrationSyncData->isMigrationInProgress());
}

TEST_F(MigrationSyncDataTests, whenMigrationIsInProgressThenMultigraphicsAllocationDoesntRequireMigration) {
    EXPECT_TRUE(multiGraphicsAllocation.requiresMigrations());
    migrationSyncData->startMigration();

    EXPECT_TRUE(migrationSyncData->isMigrationInProgress());
    EXPECT_FALSE(multiGraphicsAllocation.requiresMigrations());
}

TEST_F(MigrationSyncDataTests, whenSetTargetLocationIsCalledThenProperLocationIsSetAndMigrationIsStopped) {
    migrationSyncData->startMigration();

    EXPECT_TRUE(migrationSyncData->isMigrationInProgress());

    migrationSyncData->setCurrentLocation(0u);
    EXPECT_FALSE(migrationSyncData->isMigrationInProgress());
    EXPECT_EQ(0u, migrationSyncData->getCurrentLocation());
}

TEST(MigrationSyncDataTest, whenWaitOnCpuIsCalledThenWaitForValueSpecifiedInSignalUsageMethod) {
    auto migrationSyncData = std::make_unique<MockMigrationSyncDataWithYield>(MemoryConstants::pageSize);
    uint32_t tagAddress = 0;

    migrationSyncData->signalUsage(&tagAddress, 2u);
    migrationSyncData->waitOnCpu();
    EXPECT_EQ(2u, tagAddress);
}

TEST(MigrationSyncDataTest, whenTaskCountIsHigherThanExpectedThenWaitOnCpuDoesntHang) {
    auto migrationSyncData = std::make_unique<MockMigrationSyncData>(MemoryConstants::pageSize);
    uint32_t tagAddress = 5u;

    migrationSyncData->signalUsage(&tagAddress, 2u);
    EXPECT_EQ(&tagAddress, migrationSyncData->tagAddress);
    EXPECT_EQ(2u, migrationSyncData->latestTaskCountUsed);

    migrationSyncData->waitOnCpu();
    EXPECT_EQ(5u, tagAddress);
}

TEST_F(MigrationSyncDataTests, givenNoSignaledUsageWhenWaitOnCpuIsCalledThenEarlyReturnAndDontCrash) {
    EXPECT_NO_THROW(migrationSyncData->waitOnCpu());
    migrationSyncData->signalUsage(nullptr, 2u);
    EXPECT_NO_THROW(migrationSyncData->waitOnCpu());
}

TEST_F(MigrationSyncDataTests, whenGetHostPtrMethodIsCalledThenAlignedPointerIsReturned) {
    auto hostPtr = reinterpret_cast<uintptr_t>(migrationSyncData->getHostPtr());

    EXPECT_TRUE(isAligned(hostPtr, MemoryConstants::pageSize));
}
