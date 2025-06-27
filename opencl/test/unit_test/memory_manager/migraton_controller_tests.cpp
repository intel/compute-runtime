/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/memory_manager/migration_sync_data.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_migration_sync_data.h"

#include "opencl/source/memory_manager/migration_controller.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

using namespace NEO;

struct MigrationControllerTests : public ::testing::Test {
    void SetUp() override {
        pCsr0 = context.getDevice(0)->getDefaultEngine().commandStreamReceiver;
        pCsr1 = context.getDevice(1)->getDefaultEngine().commandStreamReceiver;
        memoryManager = static_cast<MockMemoryManager *>(context.getMemoryManager());
        for (auto &rootDeviceIndex : context.getRootDeviceIndices()) {

            memoryManager->localMemorySupported[rootDeviceIndex] = true;
        }
        auto &compilerProductHelper = context.getDevice(0)->getCompilerProductHelper();
        auto heapless = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);
        heaplessStateInit = compilerProductHelper.isHeaplessStateInitEnabled(heapless);
    }
    void TearDown() override {
    }
    MockDefaultContext context{true};
    CommandStreamReceiver *pCsr0 = nullptr;
    CommandStreamReceiver *pCsr1 = nullptr;
    MockMemoryManager *memoryManager = nullptr;
    bool heaplessStateInit = false;
};

HWTEST2_F(MigrationControllerTests, givenAllocationWithUndefinedLocationWhenHandleMigrationThenNoMigrationIsPerformedAndProperLocationIsSet, MatchAny) {
    std::unique_ptr<Image> pImage(Image1dHelperUlt<>::create(&context));
    EXPECT_TRUE(pImage->getMultiGraphicsAllocation().requiresMigrations());

    EXPECT_EQ(MigrationSyncData::locationUndefined, pImage->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());
    MigrationController::handleMigration(context, *pCsr0, pImage.get());
    EXPECT_EQ(0u, pImage->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());

    EXPECT_EQ(0u, memoryManager->lockResourceCalled);
    EXPECT_EQ(0u, memoryManager->unlockResourceCalled);
    EXPECT_EQ(heaplessStateInit ? 1u : 0u, pCsr0->peekLatestFlushedTaskCount());
}

HWTEST2_F(MigrationControllerTests, givenAllocationWithDefinedLocationWhenHandleMigrationToTheSameLocationThenDontMigrateMemory, MatchAny) {
    std::unique_ptr<Image> pImage(Image1dHelperUlt<>::create(&context));
    EXPECT_TRUE(pImage->getMultiGraphicsAllocation().requiresMigrations());

    pImage->getMultiGraphicsAllocation().getMigrationSyncData()->setCurrentLocation(1);
    EXPECT_EQ(1u, pImage->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());
    MigrationController::handleMigration(context, *pCsr1, pImage.get());
    EXPECT_EQ(1u, pImage->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());

    EXPECT_EQ(0u, memoryManager->lockResourceCalled);
    EXPECT_EQ(0u, memoryManager->unlockResourceCalled);
    EXPECT_EQ(heaplessStateInit ? 1u : 0u, pCsr1->peekLatestFlushedTaskCount());
}

HWTEST2_F(MigrationControllerTests, givenNotLockableImageAllocationWithDefinedLocationWhenHandleMigrationToDifferentLocationThenMigrateMemoryViaReadWriteImage, MatchAny) {
    REQUIRE_IMAGE_SUPPORT_OR_SKIP(&context);
    std::unique_ptr<Image> pImage(Image1dHelperUlt<>::create(&context));
    EXPECT_TRUE(pImage->getMultiGraphicsAllocation().requiresMigrations());

    auto srcAllocation = pImage->getMultiGraphicsAllocation().getGraphicsAllocation(0);
    auto dstAllocation = pImage->getMultiGraphicsAllocation().getGraphicsAllocation(1);

    srcAllocation->getDefaultGmm()->resourceParams.Flags.Info.NotLockable = 1;
    dstAllocation->getDefaultGmm()->resourceParams.Flags.Info.NotLockable = 1;

    EXPECT_FALSE(srcAllocation->isAllocationLockable());
    EXPECT_FALSE(dstAllocation->isAllocationLockable());

    const auto csr0LatestFlushedTaskCountBefore = pCsr0->peekLatestFlushedTaskCount();
    const auto csr1LatestFlushedTaskCountBefore = pCsr1->peekLatestFlushedTaskCount();

    pImage->getMultiGraphicsAllocation().getMigrationSyncData()->setCurrentLocation(0);
    EXPECT_EQ(0u, pImage->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());
    MigrationController::handleMigration(context, *pCsr1, pImage.get());
    EXPECT_EQ(1u, pImage->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());

    EXPECT_EQ(0u, memoryManager->lockResourceCalled);
    EXPECT_EQ(0u, memoryManager->unlockResourceCalled);
    EXPECT_EQ(csr0LatestFlushedTaskCountBefore + 1, pCsr0->peekLatestFlushedTaskCount());
    EXPECT_EQ(csr1LatestFlushedTaskCountBefore + 1, pCsr1->peekLatestFlushedTaskCount());
}

HWTEST2_F(MigrationControllerTests, givenNotLockableBufferAllocationWithDefinedLocationWhenHandleMigrationToDifferentLocationThenMigrateMemoryViaReadWriteBuffer, MatchAny) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    debugManager.flags.DoCpuCopyOnWriteBuffer.set(0);
    debugManager.flags.EnableBlitterForEnqueueOperations.set(0);
    std::unique_ptr<Buffer> pBuffer(BufferHelper<>::create(&context));
    const_cast<MultiGraphicsAllocation &>(pBuffer->getMultiGraphicsAllocation()).setMultiStorage(true);
    EXPECT_TRUE(pBuffer->getMultiGraphicsAllocation().requiresMigrations());

    auto srcAllocation = pBuffer->getMultiGraphicsAllocation().getGraphicsAllocation(0);
    auto dstAllocation = pBuffer->getMultiGraphicsAllocation().getGraphicsAllocation(1);

    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm0 = new Gmm(context.getDevice(0)->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    auto gmm1 = new Gmm(context.getDevice(1)->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    srcAllocation->setDefaultGmm(gmm0);
    dstAllocation->setDefaultGmm(gmm1);

    srcAllocation->getDefaultGmm()->resourceParams.Flags.Info.NotLockable = 1;
    dstAllocation->getDefaultGmm()->resourceParams.Flags.Info.NotLockable = 1;

    EXPECT_FALSE(srcAllocation->isAllocationLockable());
    EXPECT_FALSE(dstAllocation->isAllocationLockable());

    pBuffer->getMultiGraphicsAllocation().getMigrationSyncData()->setCurrentLocation(0);
    EXPECT_EQ(0u, pBuffer->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());
    MigrationController::handleMigration(context, *pCsr1, pBuffer.get());
    EXPECT_EQ(1u, pBuffer->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());

    EXPECT_EQ(0u, memoryManager->lockResourceCalled);
    EXPECT_EQ(0u, memoryManager->unlockResourceCalled);
    EXPECT_EQ(heaplessStateInit ? 2u : 1u, pCsr1->peekLatestFlushedTaskCount());
    EXPECT_EQ(heaplessStateInit ? 2u : 1u, pCsr0->peekLatestFlushedTaskCount());
}

HWTEST2_F(MigrationControllerTests, givenLockableBufferAllocationWithDefinedLocationWhenHandleMigrationToDifferentLocationThenMigrateMemoryViaLockMemory, MatchAny) {
    std::unique_ptr<Buffer> pBuffer(BufferHelper<>::create(&context));
    const_cast<MultiGraphicsAllocation &>(pBuffer->getMultiGraphicsAllocation()).setMultiStorage(true);
    EXPECT_TRUE(pBuffer->getMultiGraphicsAllocation().requiresMigrations());

    auto srcAllocation = pBuffer->getMultiGraphicsAllocation().getGraphicsAllocation(0);
    auto dstAllocation = pBuffer->getMultiGraphicsAllocation().getGraphicsAllocation(1);

    EXPECT_TRUE(srcAllocation->isAllocationLockable());
    EXPECT_TRUE(dstAllocation->isAllocationLockable());

    pBuffer->getMultiGraphicsAllocation().getMigrationSyncData()->setCurrentLocation(0);
    EXPECT_EQ(0u, pBuffer->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());
    MigrationController::handleMigration(context, *pCsr1, pBuffer.get());
    EXPECT_EQ(1u, pBuffer->getMultiGraphicsAllocation().getMigrationSyncData()->getCurrentLocation());

    EXPECT_EQ(2u, memoryManager->lockResourceCalled);
    EXPECT_EQ(2u, memoryManager->unlockResourceCalled);
    EXPECT_EQ(heaplessStateInit ? 1u : 0u, pCsr1->peekLatestFlushedTaskCount());
    EXPECT_EQ(heaplessStateInit ? 1u : 0u, pCsr0->peekLatestFlushedTaskCount());
}

HWTEST2_F(MigrationControllerTests, givenMultiGraphicsAllocationUsedInOneCsrWhenHandlingMigrationToOtherCsrOnTheSameRootDeviceThenDontWaitOnCpuForTheFirstCsrCompletion, MatchAny) {
    VariableBackup<decltype(MultiGraphicsAllocation::createMigrationSyncDataFunc)> createFuncBackup{&MultiGraphicsAllocation::createMigrationSyncDataFunc};
    MultiGraphicsAllocation::createMigrationSyncDataFunc = [](size_t size) -> MigrationSyncData * {
        return new MockMigrationSyncData(size);
    };

    std::unique_ptr<Image> pImage(Image1dHelperUlt<>::create(&context));

    ASSERT_TRUE(pImage->getMultiGraphicsAllocation().requiresMigrations());

    auto migrationSyncData = static_cast<MockMigrationSyncData *>(pImage->getMultiGraphicsAllocation().getMigrationSyncData());

    EXPECT_EQ(0u, migrationSyncData->waitOnCpuCalled);

    migrationSyncData->setCurrentLocation(0);
    EXPECT_EQ(0u, migrationSyncData->getCurrentLocation());
    MigrationController::handleMigration(context, *pCsr0, pImage.get());
    EXPECT_EQ(0u, migrationSyncData->getCurrentLocation());

    EXPECT_EQ(0u, memoryManager->lockResourceCalled);
    EXPECT_EQ(0u, memoryManager->unlockResourceCalled);
    EXPECT_EQ(heaplessStateInit ? 1u : 0u, pCsr1->peekLatestFlushedTaskCount());
    EXPECT_EQ(heaplessStateInit ? 1u : 0u, pCsr0->peekLatestFlushedTaskCount());
    EXPECT_EQ(0u, migrationSyncData->waitOnCpuCalled);
}

HWTEST2_F(MigrationControllerTests, givenMultiGraphicsAllocationUsedInOneCsrWhenHandlingMigrationToOtherCsrOnTheDifferentRootDevicesThenWaitOnCpuForTheFirstCsrCompletion, MatchAny) {
    VariableBackup<decltype(MultiGraphicsAllocation::createMigrationSyncDataFunc)> createFuncBackup{&MultiGraphicsAllocation::createMigrationSyncDataFunc};
    MultiGraphicsAllocation::createMigrationSyncDataFunc = [](size_t size) -> MigrationSyncData * {
        return new MockMigrationSyncData(size);
    };

    std::unique_ptr<Image> pImage(Image1dHelperUlt<>::create(&context));

    ASSERT_TRUE(pImage->getMultiGraphicsAllocation().requiresMigrations());

    pImage->getMultiGraphicsAllocation().getGraphicsAllocation(0)->getDefaultGmm()->resourceParams.Flags.Info.NotLockable = false;
    pImage->getMultiGraphicsAllocation().getGraphicsAllocation(1)->getDefaultGmm()->resourceParams.Flags.Info.NotLockable = false;

    auto migrationSyncData = static_cast<MockMigrationSyncData *>(pImage->getMultiGraphicsAllocation().getMigrationSyncData());

    EXPECT_EQ(0u, migrationSyncData->waitOnCpuCalled);

    migrationSyncData->setCurrentLocation(1);
    EXPECT_EQ(1u, migrationSyncData->getCurrentLocation());
    MigrationController::handleMigration(context, *pCsr0, pImage.get());
    EXPECT_EQ(0u, migrationSyncData->getCurrentLocation());

    EXPECT_EQ(2u, memoryManager->lockResourceCalled);
    EXPECT_EQ(2u, memoryManager->unlockResourceCalled);
    EXPECT_EQ(heaplessStateInit ? 1u : 0u, pCsr1->peekLatestFlushedTaskCount());
    EXPECT_EQ(heaplessStateInit ? 1u : 0u, pCsr0->peekLatestFlushedTaskCount());
    EXPECT_EQ(1u, migrationSyncData->waitOnCpuCalled);
}

HWTEST2_F(MigrationControllerTests, givenMultiGraphicsAllocationUsedInOneCsrWhenHandlingMigrationToTheSameCsrThenDontWaitOnCpu, MatchAny) {
    VariableBackup<decltype(MultiGraphicsAllocation::createMigrationSyncDataFunc)> createFuncBackup{&MultiGraphicsAllocation::createMigrationSyncDataFunc};
    MultiGraphicsAllocation::createMigrationSyncDataFunc = [](size_t size) -> MigrationSyncData * {
        return new MockMigrationSyncData(size);
    };

    std::unique_ptr<Image> pImage(Image1dHelperUlt<>::create(&context));

    ASSERT_TRUE(pImage->getMultiGraphicsAllocation().requiresMigrations());

    auto migrationSyncData = static_cast<MockMigrationSyncData *>(pImage->getMultiGraphicsAllocation().getMigrationSyncData());

    EXPECT_EQ(0u, migrationSyncData->waitOnCpuCalled);

    migrationSyncData->signalUsage(pCsr0->getTagAddress(), 0u);
    migrationSyncData->setCurrentLocation(0);
    EXPECT_EQ(0u, migrationSyncData->getCurrentLocation());
    MigrationController::handleMigration(context, *pCsr0, pImage.get());
    EXPECT_EQ(0u, migrationSyncData->getCurrentLocation());

    EXPECT_EQ(0u, memoryManager->lockResourceCalled);
    EXPECT_EQ(0u, memoryManager->unlockResourceCalled);
    EXPECT_EQ(heaplessStateInit ? 1u : 0u, pCsr1->peekLatestFlushedTaskCount());
    EXPECT_EQ(heaplessStateInit ? 1u : 0u, pCsr0->peekLatestFlushedTaskCount());
    EXPECT_EQ(0u, migrationSyncData->waitOnCpuCalled);
}

HWTEST2_F(MigrationControllerTests, whenHandleMigrationThenProperTagAddressAndTaskCountIsSet, MatchAny) {
    VariableBackup<decltype(MultiGraphicsAllocation::createMigrationSyncDataFunc)> createFuncBackup{&MultiGraphicsAllocation::createMigrationSyncDataFunc};
    MultiGraphicsAllocation::createMigrationSyncDataFunc = [](size_t size) -> MigrationSyncData * {
        return new MockMigrationSyncData(size);
    };

    std::unique_ptr<Image> pImage(Image1dHelperUlt<>::create(&context));

    ASSERT_TRUE(pImage->getMultiGraphicsAllocation().requiresMigrations());

    auto migrationSyncData = static_cast<MockMigrationSyncData *>(pImage->getMultiGraphicsAllocation().getMigrationSyncData());

    migrationSyncData->setCurrentLocation(0);
    MigrationController::handleMigration(context, *pCsr0, pImage.get());

    EXPECT_EQ(pCsr0->getTagAddress(), migrationSyncData->tagAddress);
    EXPECT_EQ(pCsr0->peekTaskCount() + 1, migrationSyncData->latestTaskCountUsed);
}

HWTEST2_F(MigrationControllerTests, givenWaitForTimestampsEnabledWhenHandleMigrationIsCalledThenSignalTaskCountBasedUsage, MatchAny) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableTimestampWaitForQueues.set(4);

    VariableBackup<decltype(MultiGraphicsAllocation::createMigrationSyncDataFunc)> createFuncBackup{&MultiGraphicsAllocation::createMigrationSyncDataFunc};
    MultiGraphicsAllocation::createMigrationSyncDataFunc = [](size_t size) -> MigrationSyncData * {
        return new MockMigrationSyncData(size);
    };

    std::unique_ptr<Buffer> pBuffer(BufferHelper<>::create(&context));
    const_cast<MultiGraphicsAllocation &>(pBuffer->getMultiGraphicsAllocation()).setMultiStorage(true);

    ASSERT_TRUE(pBuffer->getMultiGraphicsAllocation().requiresMigrations());

    auto migrationSyncData = static_cast<MockMigrationSyncData *>(pBuffer->getMultiGraphicsAllocation().getMigrationSyncData());

    MigrationController::handleMigration(context, *pCsr0, pBuffer.get());

    EXPECT_EQ(1u, migrationSyncData->signalUsageCalled);
}

HWTEST2_F(MigrationControllerTests, whenMemoryMigrationForMemoryObjectIsAlreadyInProgressThenDoEarlyReturn, MatchAny) {
    std::unique_ptr<Buffer> pBuffer(BufferHelper<>::create(&context));

    ASSERT_TRUE(pBuffer->getMultiGraphicsAllocation().requiresMigrations());

    auto migrationSyncData = static_cast<MockMigrationSyncData *>(pBuffer->getMultiGraphicsAllocation().getMigrationSyncData());

    migrationSyncData->startMigration();
    EXPECT_TRUE(migrationSyncData->isMigrationInProgress());

    MigrationController::migrateMemory(context, *memoryManager, pBuffer.get(), pCsr1->getRootDeviceIndex());

    EXPECT_TRUE(migrationSyncData->isMigrationInProgress());
}
