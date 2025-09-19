/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/device/root_device.h"
#include "shared/source/device/sub_device.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/local_memory_usage.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/mock_release_helper.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

struct MultiDeviceStorageInfoTest : public ::testing::Test {
    void SetUp() override {
        ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
        debugManager.flags.CreateMultipleSubDevices.set(numDevices);
        debugManager.flags.EnableLocalMemory.set(true);
        memoryManager = static_cast<MockMemoryManager *>(factory.rootDevices[0]->getMemoryManager());
    }
    const uint32_t numDevices = 4u;
    const DeviceBitfield allTilesMask{maxNBitValue(4u)};
    const uint32_t tileIndex = 2u;
    const DeviceBitfield singleTileMask{static_cast<uint32_t>(1u << tileIndex)};
    DebugManagerStateRestore restorer;
    UltDeviceFactory factory{1, numDevices};
    VariableBackup<UltHwConfig> backup{&ultHwConfig};
    MockMemoryManager *memoryManager;
};

TEST_F(MultiDeviceStorageInfoTest, givenEnabledFlagForForceMultiTileAllocPlacementWhenCreatingStorageInfoForAllocationThenAllMemoryBanksAreOnAndPageTableClonningIsNotRequired) {
    DebugManagerStateRestore restorer;

    AllocationType allocTypes[] = {AllocationType::kernelIsa, AllocationType::kernelIsaInternal, AllocationType::constantSurface};

    for (uint32_t i = 0; i < 2; i++) {
        debugManager.flags.ForceMultiTileAllocPlacement.set(1ull << (static_cast<uint64_t>(allocTypes[i]) - 1));
        AllocationProperties properties{mockRootDeviceIndex, false, 0u, allocTypes[i], false, false, singleTileMask};
        auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
        EXPECT_FALSE(storageInfo.cloningOfPageTables);
        EXPECT_EQ(allTilesMask, storageInfo.memoryBanks);
        EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
        EXPECT_TRUE(storageInfo.tileInstanced);

        properties.flags.multiOsContextCapable = true;
        auto storageInfo2 = memoryManager->createStorageInfoFromProperties(properties);
        EXPECT_FALSE(storageInfo2.cloningOfPageTables);
        EXPECT_EQ(allTilesMask, storageInfo2.memoryBanks);
        EXPECT_EQ(allTilesMask, storageInfo2.pageTablesVisibility);
        EXPECT_TRUE(storageInfo2.tileInstanced);
    }
}

TEST_F(MultiDeviceStorageInfoTest, givenDefaultFlagForForceMultiTileAllocPlacementWhenCreatingStorageInfoForAllocationThenSingleMemoryBanksIsOnAndPageTableClonningIsRequired) {

    AllocationType allocTypes[] = {AllocationType::kernelIsa, AllocationType::kernelIsaInternal, AllocationType::constantSurface};

    for (uint32_t i = 0; i < 2; i++) {
        AllocationProperties properties{mockRootDeviceIndex, false, 0u, allocTypes[i], false, false, singleTileMask};

        auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
        EXPECT_TRUE(storageInfo.cloningOfPageTables);
        EXPECT_EQ(singleTileMask, storageInfo.memoryBanks);
        EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
        EXPECT_FALSE(storageInfo.tileInstanced);

        properties.flags.multiOsContextCapable = true;
        auto storageInfo2 = memoryManager->createStorageInfoFromProperties(properties);
        EXPECT_TRUE(storageInfo2.cloningOfPageTables);
        EXPECT_EQ(singleTileMask, storageInfo2.memoryBanks);
        EXPECT_EQ(allTilesMask, storageInfo2.pageTablesVisibility);
        EXPECT_FALSE(storageInfo2.tileInstanced);
    }
}

TEST_F(MultiDeviceStorageInfoTest, givenEnabledFlagForForceSingleTileAllocPlacementWhenCreatingStorageInfoForAllocationThenSingleMemoryBanksIsOnAndPageTableClonningIsRequired) {
    DebugManagerStateRestore restorer;

    AllocationType allocTypes[] = {AllocationType::kernelIsa, AllocationType::kernelIsaInternal, AllocationType::constantSurface};

    for (uint32_t i = 0; i < 2; i++) {
        debugManager.flags.ForceSingleTileAllocPlacement.set(1ull << (static_cast<uint64_t>(allocTypes[i]) - 1));
        AllocationProperties properties{mockRootDeviceIndex, false, 0u, allocTypes[i], false, false, singleTileMask};
        auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
        EXPECT_TRUE(storageInfo.cloningOfPageTables);
        EXPECT_EQ(singleTileMask, storageInfo.memoryBanks.to_ulong());
        EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
        EXPECT_FALSE(storageInfo.tileInstanced);

        properties.flags.multiOsContextCapable = true;
        auto storageInfo2 = memoryManager->createStorageInfoFromProperties(properties);
        EXPECT_TRUE(storageInfo2.cloningOfPageTables);
        EXPECT_EQ(singleTileMask, storageInfo2.memoryBanks.to_ulong());
        EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
        EXPECT_FALSE(storageInfo2.tileInstanced);
    }
}

TEST_F(MultiDeviceStorageInfoTest, whenCreatingStorageInfoForPrivateSurfaceWithOneTileThenOnlySingleBankIsUsed) {
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::privateSurface, false, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_FALSE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(singleTileMask, storageInfo.memoryBanks);
    EXPECT_EQ(singleTileMask, storageInfo.pageTablesVisibility);
    EXPECT_FALSE(storageInfo.tileInstanced);
    EXPECT_EQ(1u, storageInfo.getNumBanks());
}

TEST_F(MultiDeviceStorageInfoTest, whenCreatingStorageInfoForPrivateSurfaceThenAllMemoryBanksAreOnAndPageTableClonningIsNotRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::privateSurface, false, false, allTilesMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_FALSE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(allTilesMask, storageInfo.memoryBanks);
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
    EXPECT_TRUE(storageInfo.tileInstanced);
    EXPECT_EQ(4u, storageInfo.getNumBanks());
}

TEST_F(MultiDeviceStorageInfoTest, givenMultiTileCsrWhenCreatingStorageInfoForInternalHeapThenSingleMemoryBankIsOnAndPageTableClonningIsRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::internalHeap, true, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(singleTileMask, storageInfo.memoryBanks);
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
}

TEST_F(MultiDeviceStorageInfoTest, givenSingleTileCsrWhenCreatingStorageInfoForInternalHeapThenSingleMemoryBankIsOnAndPageTableClonningIsNotRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::internalHeap, false, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_FALSE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(singleTileMask, storageInfo.memoryBanks);
    EXPECT_EQ(singleTileMask, storageInfo.pageTablesVisibility);
    EXPECT_FALSE(storageInfo.tileInstanced);
}

TEST_F(MultiDeviceStorageInfoTest, givenMultiTileCsrWhenCreatingStorageInfoForLinearStreamThenSingleMemoryBankIsOnAndPageTableClonningIsRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::linearStream, true, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(singleTileMask, storageInfo.memoryBanks);
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
}

TEST_F(MultiDeviceStorageInfoTest, givenSingleTileCsrWhenCreatingStorageInfoForLinearStreamThenSingleMemoryBankIsOnAndPageTableClonningIsNotRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::linearStream, false, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_FALSE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(singleTileMask, storageInfo.memoryBanks);
    EXPECT_EQ(singleTileMask, storageInfo.pageTablesVisibility);
    EXPECT_FALSE(storageInfo.tileInstanced);
}

TEST_F(MultiDeviceStorageInfoTest, givenMultiTileCsrWhenCreatingStorageInfoForCommandBufferThenFirstAvailableMemoryBankIsOnAndPageTableClonningIsRequired) {
    const DeviceBitfield firstTileMask{static_cast<uint32_t>(1u)};
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::commandBuffer, true, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(firstTileMask, storageInfo.memoryBanks);
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
}

TEST_F(MultiDeviceStorageInfoTest, givenSingleTileCsrWhenCreatingStorageInfoForCommandBufferThenSingleMemoryBankIsOnAndPageTableClonningIsNotRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::commandBuffer, false, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_FALSE(storageInfo.cloningOfPageTables);
    EXPECT_FALSE(storageInfo.tileInstanced);
    EXPECT_EQ(singleTileMask, storageInfo.memoryBanks);
    EXPECT_EQ(singleTileMask, storageInfo.pageTablesVisibility);
}

TEST_F(MultiDeviceStorageInfoTest, givenMultiTileCsrWhenCreatingStorageInfoForScratchSpaceThenAllMemoryBankAreOnAndPageTableClonningIsNotRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::scratchSurface, true, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_FALSE(storageInfo.cloningOfPageTables);
    EXPECT_TRUE(storageInfo.tileInstanced);
    EXPECT_EQ(allTilesMask, storageInfo.memoryBanks);
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
}

TEST_F(MultiDeviceStorageInfoTest, givenSingleTileCsrWhenCreatingStorageInfoForScratchSpaceThenSingleMemoryBankIsOnAndPageTableClonningIsRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::scratchSurface, false, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(singleTileMask, storageInfo.memoryBanks);
    EXPECT_EQ(singleTileMask, storageInfo.pageTablesVisibility);
}

TEST_F(MultiDeviceStorageInfoTest, givenMultiTileCsrWhenCreatingStorageInfoForPreemptionAllocationThenAllMemoryBankAreOnAndPageTableClonningIsNotRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::preemption, true, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_FALSE(storageInfo.cloningOfPageTables);
    EXPECT_TRUE(storageInfo.tileInstanced);
    EXPECT_EQ(allTilesMask, storageInfo.memoryBanks);
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
}

TEST_F(MultiDeviceStorageInfoTest, givenSingleTileCsrWhenCreatingStorageInfoForPreemptionAllocationThenSingleMemoryBankIsOnAndPageTableClonningIsRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::preemption, false, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(singleTileMask, storageInfo.memoryBanks);
    EXPECT_EQ(singleTileMask, storageInfo.pageTablesVisibility);
}

TEST_F(MultiDeviceStorageInfoTest, givenMultiTileCsrWhenCreatingStorageInfoForDeferredTasksListAllocationThenAllMemoryBankAreOnAndPageTableClonningIsNotRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::deferredTasksList, true, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_FALSE(storageInfo.cloningOfPageTables);
    EXPECT_TRUE(storageInfo.tileInstanced);
    EXPECT_EQ(allTilesMask, storageInfo.memoryBanks);
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
}

TEST_F(MultiDeviceStorageInfoTest, givenSingleTileCsrWhenCreatingStorageInfoForDeferredTasksListAllocationThenSingleMemoryBankIsOnAndPageTableClonningIsRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::deferredTasksList, false, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(singleTileMask, storageInfo.memoryBanks);
    EXPECT_EQ(singleTileMask, storageInfo.pageTablesVisibility);
}

TEST_F(MultiDeviceStorageInfoTest, whenCreatingStorageInfoForWorkPartitionSurfaceThenAllMemoryBankAreOnAndPageTableClonningIsNotRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::workPartitionSurface, true, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_FALSE(storageInfo.cloningOfPageTables);
    EXPECT_TRUE(storageInfo.tileInstanced);
    EXPECT_EQ(allTilesMask, storageInfo.memoryBanks);
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
}

HWTEST_F(MultiDeviceStorageInfoTest, givenSingleTileCsrWhenAllocatingCsrSpecificAllocationsThenStoreThemInSystemMemory) {
    auto commandStreamReceiver = static_cast<UltCommandStreamReceiver<FamilyType> *>(factory.rootDevices[0]->getSubDevice(tileIndex)->getDefaultEngine().commandStreamReceiver);
    auto &heap = commandStreamReceiver->getIndirectHeap(IndirectHeap::Type::indirectObject, MemoryConstants::pageSize64k);
    auto heapAllocation = heap.getGraphicsAllocation();
    if (commandStreamReceiver->canUse4GbHeaps()) {
        EXPECT_EQ(AllocationType::internalHeap, heapAllocation->getAllocationType());
    } else {
        EXPECT_EQ(AllocationType::linearStream, heapAllocation->getAllocationType());
    }

    commandStreamReceiver->ensureCommandBufferAllocation(heap, heap.getAvailableSpace() + 1, 0u);
    auto commandBufferAllocation = heap.getGraphicsAllocation();
    EXPECT_EQ(AllocationType::commandBuffer, commandBufferAllocation->getAllocationType());
    EXPECT_NE(heapAllocation, commandBufferAllocation);
    EXPECT_NE(commandBufferAllocation->getMemoryPool(), MemoryPool::localMemory);
}

TEST_F(MultiDeviceStorageInfoTest, whenCreatingStorageInfoForBufferCompressedThenAllMemoryBanksAreOnAndPageTableClonningIsRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::buffer, true, allTilesMask};
    properties.flags.preferCompressed = true;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(allTilesMask, storageInfo.memoryBanks);
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
    EXPECT_TRUE(storageInfo.multiStorage);
    EXPECT_EQ(storageInfo.colouringGranularity, MemoryConstants::pageSize64k);
    EXPECT_EQ(storageInfo.colouringPolicy, ColouringPolicy::deviceCountBased);
}

TEST_F(MultiDeviceStorageInfoTest, whenCreatingStorageInfoForBufferThenAllMemoryBanksAreOnAndPageTableClonningIsRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::buffer, true, allTilesMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(allTilesMask, storageInfo.memoryBanks);
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
    EXPECT_TRUE(storageInfo.multiStorage);
    EXPECT_EQ(storageInfo.colouringGranularity, MemoryConstants::pageSize64k);
    EXPECT_EQ(storageInfo.colouringPolicy, ColouringPolicy::deviceCountBased);
}

TEST_F(MultiDeviceStorageInfoTest, whenCreatingStorageInfoForSVMGPUThenAllMemoryBanksAreOnAndPageTableClonningIsRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::svmGpu, true, allTilesMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(allTilesMask, storageInfo.memoryBanks);
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
    EXPECT_TRUE(storageInfo.multiStorage);
    EXPECT_EQ(storageInfo.colouringGranularity, MemoryConstants::pageSize64k);
    EXPECT_EQ(storageInfo.colouringPolicy, ColouringPolicy::deviceCountBased);
}

TEST_F(MultiDeviceStorageInfoTest, givenMultiStorageGranularityWhenCreatingStorageInfoThenProperGranularityIsSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.MultiStorageGranularity.set(128);
    debugManager.flags.MultiStoragePolicy.set(1);

    AllocationProperties properties{mockRootDeviceIndex, false, 10 * MemoryConstants::pageSize64k, AllocationType::svmGpu, true, allTilesMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(allTilesMask, storageInfo.memoryBanks);
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
    EXPECT_TRUE(storageInfo.multiStorage);
    EXPECT_EQ(storageInfo.colouringGranularity, MemoryConstants::kiloByte * 128);
    EXPECT_EQ(storageInfo.colouringPolicy, ColouringPolicy::chunkSizeBased);
}

TEST_F(MultiDeviceStorageInfoTest, givenTwoPagesAllocationSizeWhenCreatingStorageInfoForBufferThenSingleMemoryBankIsOnAndPageTableClonningIsRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, 2 * MemoryConstants::pageSize64k, AllocationType::buffer, true, allTilesMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(1lu, storageInfo.memoryBanks.to_ulong());
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
    EXPECT_FALSE(storageInfo.multiStorage);
}

TEST_F(MultiDeviceStorageInfoTest, givenSpecifiedDeviceIndexWhenCreatingStorageInfoForBufferThenSingleMemoryBankIsOnAndPageTableClonningIsRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::buffer, true, false, singleTileMask};
    properties.multiStorageResource = true;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(singleTileMask, storageInfo.memoryBanks);
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
    EXPECT_FALSE(storageInfo.multiStorage);
}

TEST_F(MultiDeviceStorageInfoTest, givenResourceColouringNotSupportedWhenCreatingStorageInfoForBufferThenSingleMemoryBankIsOnAndPageTableClonningIsRequired) {
    memoryManager->supportsMultiStorageResources = false;
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::buffer, true, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(1lu, storageInfo.memoryBanks.count());
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
    EXPECT_FALSE(storageInfo.multiStorage);
}

TEST_F(MultiDeviceStorageInfoTest, givenNonMultiStorageResourceWhenCreatingStorageInfoForBufferThenSingleMemoryBankIsOnAndPageTableClonningIsRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::buffer, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(1lu, storageInfo.memoryBanks.count());
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
    EXPECT_FALSE(storageInfo.multiStorage);
}

TEST_F(MultiDeviceStorageInfoTest, whenCreatingStorageInfoForBufferThenLocalOnlyFlagIsRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::buffer, false, singleTileMask};
    auto releaseHelper = std::make_unique<MockReleaseHelper>();
    releaseHelper->isLocalOnlyAllowedResult = true;
    memoryManager->executionEnvironment.rootDeviceEnvironments[properties.rootDeviceIndex]->releaseHelper.reset(releaseHelper.release());
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.localOnlyRequired);
}

TEST_F(MultiDeviceStorageInfoTest, givenReleaseWhichDoesNotAllowLocalOnlyWhenCreatingStorageInfoForBufferThenLocalOnlyFlagIsNotRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::buffer, false, singleTileMask};
    auto releaseHelper = std::make_unique<MockReleaseHelper>();
    releaseHelper->isLocalOnlyAllowedResult = false;
    memoryManager->executionEnvironment.rootDeviceEnvironments[properties.rootDeviceIndex]->releaseHelper.reset(releaseHelper.release());
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_FALSE(storageInfo.localOnlyRequired);
}

TEST_F(MultiDeviceStorageInfoTest, whenCreatingStorageInfoForBufferCompressedThenLocalOnlyFlagIsRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::buffer, false, singleTileMask};
    properties.flags.preferCompressed = true;
    auto releaseHelper = std::make_unique<MockReleaseHelper>();
    releaseHelper->isLocalOnlyAllowedResult = true;
    memoryManager->executionEnvironment.rootDeviceEnvironments[properties.rootDeviceIndex]->releaseHelper.reset(releaseHelper.release());
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.localOnlyRequired);
}

TEST_F(MultiDeviceStorageInfoTest, givenReleaseWhichDoesNotAllowLocalOnlyWhenCreatingStorageInfoForBufferCompressedThenLocalOnlyFlagIsNotRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::buffer, false, singleTileMask};
    properties.flags.preferCompressed = true;
    auto releaseHelper = std::make_unique<MockReleaseHelper>();
    releaseHelper->isLocalOnlyAllowedResult = false;
    memoryManager->executionEnvironment.rootDeviceEnvironments[properties.rootDeviceIndex]->releaseHelper.reset(releaseHelper.release());
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_FALSE(storageInfo.localOnlyRequired);
}

TEST_F(MultiDeviceStorageInfoTest, whenCreatingStorageInfoForSvmGpuThenLocalOnlyFlagIsRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::svmGpu, false, singleTileMask};
    auto releaseHelper = std::make_unique<MockReleaseHelper>();
    releaseHelper->isLocalOnlyAllowedResult = true;
    memoryManager->executionEnvironment.rootDeviceEnvironments[properties.rootDeviceIndex]->releaseHelper.reset(releaseHelper.release());
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.localOnlyRequired);
}

TEST_F(MultiDeviceStorageInfoTest, givenReleaseWhichDoesNotAllowLocalOnlyWhenCreatingStorageInfoForSvmGpuThenLocalOnlyFlagIsNotRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::svmGpu, false, singleTileMask};
    auto releaseHelper = std::make_unique<MockReleaseHelper>();
    releaseHelper->isLocalOnlyAllowedResult = false;
    memoryManager->executionEnvironment.rootDeviceEnvironments[properties.rootDeviceIndex]->releaseHelper.reset(releaseHelper.release());
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_FALSE(storageInfo.localOnlyRequired);
}

TEST_F(MultiDeviceStorageInfoTest, whenCreatingStorageInfoForShareableSvmGpuThenLocalOnlyFlagIsRequiredAndIsNotLocable) {
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::svmGpu, false, singleTileMask};
    properties.flags.shareable = 1u;
    auto releaseHelper = std::make_unique<MockReleaseHelper>();
    releaseHelper->isLocalOnlyAllowedResult = true;
    memoryManager->executionEnvironment.rootDeviceEnvironments[properties.rootDeviceIndex]->releaseHelper.reset(releaseHelper.release());
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.localOnlyRequired);
    EXPECT_FALSE(storageInfo.isLockable);
}

TEST_F(MultiDeviceStorageInfoTest, givenReleaseWhichDoesNotAllowLocalOnlyWhenCreatingStorageInfoForShareableSvmGpuThenLocalOnlyFlagIsNotRequiredAndIsNotLocable) {
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::svmGpu, false, singleTileMask};
    properties.flags.shareable = 1u;
    auto releaseHelper = std::make_unique<MockReleaseHelper>();
    releaseHelper->isLocalOnlyAllowedResult = false;
    memoryManager->executionEnvironment.rootDeviceEnvironments[properties.rootDeviceIndex]->releaseHelper.reset(releaseHelper.release());
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_FALSE(storageInfo.localOnlyRequired);
    EXPECT_FALSE(storageInfo.isLockable);
}

TEST_F(MultiDeviceStorageInfoTest, givenReadOnlyBufferToBeCopiedAcrossTilesWhenDebugVariableIsSetThenOnlyCertainBanksAreUsed) {
    DebugManagerStateRestore restorer;
    auto proposedTiles = allTilesMask;
    proposedTiles[1] = 0;

    debugManager.flags.OverrideMultiStoragePlacement.set(proposedTiles.to_ulong());

    AllocationProperties properties{mockRootDeviceIndex, false, 64 * MemoryConstants::kiloByte * 40, AllocationType::buffer, true, allTilesMask};

    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_EQ(proposedTiles, storageInfo.memoryBanks);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_FALSE(storageInfo.tileInstanced);
    EXPECT_EQ(3u, storageInfo.getNumBanks());
}

TEST_F(MultiDeviceStorageInfoTest, givenUnifiedSharedMemoryWhenMultiStoragePlacementIsOverriddenThenSpecifiedBanksAreUsed) {
    DebugManagerStateRestore restorer;
    auto proposedTiles = allTilesMask;
    proposedTiles[0] = 0;

    debugManager.flags.OverrideMultiStoragePlacement.set(proposedTiles.to_ulong());

    AllocationProperties properties{mockRootDeviceIndex, false, 512 * MemoryConstants::kiloByte, AllocationType::unifiedSharedMemory, true, allTilesMask};

    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);

    EXPECT_EQ(proposedTiles, storageInfo.memoryBanks);
}

TEST_F(MultiDeviceStorageInfoTest, givenUnifiedSharedMemoryOnMultiTileWhenKmdMigrationIsEnabledThenAllTilesAreUsed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);

    AllocationProperties properties{mockRootDeviceIndex, false, 512 * MemoryConstants::kiloByte, AllocationType::unifiedSharedMemory, true, allTilesMask};

    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);

    EXPECT_TRUE(properties.subDevicesBitfield.count() > 1);

    EXPECT_EQ(allTilesMask, storageInfo.memoryBanks);
}

TEST_F(MultiDeviceStorageInfoTest, givenLeastOccupiedBankAndOtherBitsEnabledInSubDeviceBitfieldWhenCreateStorageInfoThenTakeLeastOccupiedBankAsMemoryBank) {
    AllocationProperties properties{mockRootDeviceIndex, false, 1u, AllocationType::unknown, false, singleTileMask};
    auto leastOccupiedBank = memoryManager->getLocalMemoryUsageBankSelector(properties.allocationType, properties.rootDeviceIndex)->getLeastOccupiedBank(properties.subDevicesBitfield);
    properties.subDevicesBitfield.set(leastOccupiedBank);
    properties.subDevicesBitfield.set(leastOccupiedBank + 1);
    EXPECT_EQ(2u, properties.subDevicesBitfield.count());
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_EQ(1u, storageInfo.memoryBanks.count());
    EXPECT_TRUE(storageInfo.memoryBanks.test(leastOccupiedBank));
}

TEST_F(MultiDeviceStorageInfoTest, givenNoSubdeviceBitfieldWhenCreateStorageInfoForNonLockableAllocationThenReturnEmptyStorageInfo) {
    AllocationType allocationType = AllocationType::buffer;
    EXPECT_FALSE(GraphicsAllocation::isLockable(allocationType));
    AllocationProperties properties{mockRootDeviceIndex, false, 1u, allocationType, false, {}};
    StorageInfo emptyInfo{};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_EQ(storageInfo.getMemoryBanks(), emptyInfo.getMemoryBanks());
    EXPECT_FALSE(storageInfo.isLockable);
}

TEST_F(MultiDeviceStorageInfoTest, givenNoSubdeviceBitfieldWhenCreateStorageInfoForNonLockableAllocationThenReturnEmptyStorageInfoWithLockableFlag) {
    AllocationType allocationType = AllocationType::bufferHostMemory;
    EXPECT_TRUE(GraphicsAllocation::isLockable(allocationType));
    AllocationProperties properties{mockRootDeviceIndex, false, 1u, allocationType, false, {}};
    StorageInfo emptyInfo{};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_EQ(storageInfo.getMemoryBanks(), emptyInfo.getMemoryBanks());
    EXPECT_TRUE(storageInfo.isLockable);
}

TEST_F(MultiDeviceStorageInfoTest, givenGraphicsAllocationWithCpuAccessRequiredWhenCreatingStorageInfoThenSetCpuVisibleSegmentIsRequiredAndIsLockableFlagIsEnabled) {
    auto firstAllocationIdx = static_cast<int>(AllocationType::unknown);
    auto lastAllocationIdx = static_cast<int>(AllocationType::count);

    for (int allocationIdx = firstAllocationIdx; allocationIdx != lastAllocationIdx; allocationIdx++) {
        auto allocationType = static_cast<AllocationType>(allocationIdx);
        AllocationProperties properties{mockRootDeviceIndex, false, 1u, allocationType, false, singleTileMask};
        auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
        if (GraphicsAllocation::isCpuAccessRequired(properties.allocationType)) {
            EXPECT_TRUE(storageInfo.cpuVisibleSegment);
            EXPECT_TRUE(storageInfo.isLockable);
        } else {
            EXPECT_FALSE(storageInfo.cpuVisibleSegment);
        }
    }
}

TEST_F(MultiDeviceStorageInfoTest, givenGraphicsAllocationThatIsLockableWhenCreatingStorageInfoThenIsLockableFlagIsEnabled) {
    auto firstAllocationIdx = static_cast<int>(AllocationType::unknown);
    auto lastAllocationIdx = static_cast<int>(AllocationType::count);

    for (int allocationIdx = firstAllocationIdx; allocationIdx != lastAllocationIdx; allocationIdx++) {
        auto allocationType = static_cast<AllocationType>(allocationIdx);
        AllocationProperties properties{mockRootDeviceIndex, false, 1u, allocationType, false, singleTileMask};
        auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
        if (GraphicsAllocation::isLockable(properties.allocationType)) {
            EXPECT_TRUE(storageInfo.isLockable);
        } else {
            EXPECT_FALSE(storageInfo.isLockable);
        }
    }
}

TEST_F(MultiDeviceStorageInfoTest, givenAllocationTypeBufferWhenCreatingStorageInfoThenIsLockableFlagIsSetCorrectly) {
    AllocationProperties properties{mockRootDeviceIndex, false, 1u, AllocationType::buffer, false, singleTileMask};
    {
        properties.makeDeviceBufferLockable = false;
        auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
        EXPECT_FALSE(storageInfo.isLockable);
    }
    {
        properties.makeDeviceBufferLockable = true;
        auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
        EXPECT_TRUE(storageInfo.isLockable);
    }
    {
        properties.allocationType = AllocationType::image;
        auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
        EXPECT_FALSE(storageInfo.isLockable);
    }
}

TEST_F(MultiDeviceStorageInfoTest, givenGpuTimestampAllocationWhenUsingSingleTileDeviceThenExpectRegularAllocationStorageInfo) {
    AllocationProperties properties{mockRootDeviceIndex,
                                    false,
                                    1u,
                                    AllocationType::gpuTimestampDeviceBuffer,
                                    singleTileMask.count() > 1u,
                                    false,
                                    singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_EQ(singleTileMask, storageInfo.memoryBanks);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_FALSE(storageInfo.tileInstanced);
    EXPECT_EQ(singleTileMask, storageInfo.pageTablesVisibility);
}

TEST_F(MultiDeviceStorageInfoTest,
       givenGpuTimestampAllocationWhenUsingMultiTileDeviceThenExpectColoringAndCloningPagesAllocationStorageInfo) {
    AllocationProperties properties{mockRootDeviceIndex,
                                    false,
                                    1u,
                                    AllocationType::gpuTimestampDeviceBuffer,
                                    allTilesMask.count() > 1u,
                                    false,
                                    allTilesMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);

    auto leastOccupiedBank = memoryManager->getLocalMemoryUsageBankSelector(properties.allocationType, properties.rootDeviceIndex)->getLeastOccupiedBank(properties.subDevicesBitfield);
    DeviceBitfield allocationMask;
    allocationMask.set(leastOccupiedBank);

    EXPECT_EQ(allocationMask, storageInfo.memoryBanks);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_FALSE(storageInfo.tileInstanced);
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
}

TEST_F(MultiDeviceStorageInfoTest, givenSingleTileWhenCreatingStorageInfoForSemaphoreBufferThenProvidedMemoryBankIsSelected) {
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::semaphoreBuffer, false, singleTileMask};
    properties.flags.multiOsContextCapable = false;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_EQ(singleTileMask, storageInfo.memoryBanks);
}

TEST_F(MultiDeviceStorageInfoTest, givenSingleTileWhenCreatingStorageInfoForCommandBufferThenProvidedMemoryBankIsSelected) {
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::commandBuffer, false, singleTileMask};
    properties.flags.multiOsContextCapable = false;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_EQ(singleTileMask, storageInfo.memoryBanks);
}

TEST_F(MultiDeviceStorageInfoTest, givenSingleTileWhenCreatingStorageInfoForRingBufferThenProvidedMemoryBankIsSelected) {
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::ringBuffer, false, singleTileMask};
    properties.flags.multiOsContextCapable = false;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_EQ(singleTileMask, storageInfo.memoryBanks);
}

TEST_F(MultiDeviceStorageInfoTest, givenMultiTileWhenCreatingStorageInfoForSemaphoreBufferThenFirstBankIsSelectedEvenIfOtherTileIsLessOccupied) {
    constexpr uint32_t firstAvailableTileMask = 2u;

    memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->reserveOnBanks(firstAvailableTileMask, MemoryConstants::pageSize2M);
    EXPECT_NE(1u, memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->getLeastOccupiedBank(allTilesMask));

    AffinityMaskHelper affinityMaskHelper{false};
    affinityMaskHelper.enableGenericSubDevice(1);
    affinityMaskHelper.enableGenericSubDevice(2);
    affinityMaskHelper.enableGenericSubDevice(3);

    memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[mockRootDeviceIndex]->deviceAffinityMask = affinityMaskHelper;

    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::semaphoreBuffer, false, affinityMaskHelper.getGenericSubDevicesMask()};
    properties.flags.multiOsContextCapable = true;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_EQ(DeviceBitfield{firstAvailableTileMask}, storageInfo.memoryBanks);
}

TEST_F(MultiDeviceStorageInfoTest, givenMultiTileWhenCreatingStorageInfoForCommandBufferThenFirstBankIsSelectedEvenIfOtherTileIsLessOccupied) {
    constexpr uint32_t firstAvailableTileMask = 2u;

    memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->reserveOnBanks(firstAvailableTileMask, MemoryConstants::pageSize2M);
    EXPECT_NE(1u, memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->getLeastOccupiedBank(allTilesMask));

    AffinityMaskHelper affinityMaskHelper{false};
    affinityMaskHelper.enableGenericSubDevice(1);
    affinityMaskHelper.enableGenericSubDevice(2);
    affinityMaskHelper.enableGenericSubDevice(3);

    memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[mockRootDeviceIndex]->deviceAffinityMask = affinityMaskHelper;

    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::commandBuffer, false, affinityMaskHelper.getGenericSubDevicesMask()};
    properties.flags.multiOsContextCapable = true;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_EQ(DeviceBitfield{firstAvailableTileMask}, storageInfo.memoryBanks);
}

TEST_F(MultiDeviceStorageInfoTest, givenMultiTileWhenCreatingStorageInfoForRingBufferThenFirstBankIsSelectedEvenIfOtherTileIsLessOccupied) {
    constexpr uint32_t firstAvailableTileMask = 2u;

    memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->reserveOnBanks(firstAvailableTileMask, MemoryConstants::pageSize2M);
    EXPECT_NE(1u, memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->getLeastOccupiedBank(allTilesMask));

    AffinityMaskHelper affinityMaskHelper{false};
    affinityMaskHelper.enableGenericSubDevice(1);
    affinityMaskHelper.enableGenericSubDevice(2);
    affinityMaskHelper.enableGenericSubDevice(3);

    memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[mockRootDeviceIndex]->deviceAffinityMask = affinityMaskHelper;

    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::ringBuffer, false, affinityMaskHelper.getGenericSubDevicesMask()};
    properties.flags.multiOsContextCapable = true;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_EQ(DeviceBitfield{firstAvailableTileMask}, storageInfo.memoryBanks);
}

TEST_F(MultiDeviceStorageInfoTest, givenDirectSubmissionForceLocalMemoryStorageDisabledWhenCreatingStorageInfoForCommandRingOrSemaphoreBufferThenPreferredBankIsSelected) {
    DebugManagerStateRestore restorer;

    debugManager.flags.DirectSubmissionForceLocalMemoryStorageMode.set(0);
    constexpr uint32_t firstAvailableTileMask = 2u;

    memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->reserveOnBanks(firstAvailableTileMask, MemoryConstants::pageSize2M);
    EXPECT_NE(1u, memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->getLeastOccupiedBank(allTilesMask));

    AffinityMaskHelper affinityMaskHelper{false};
    affinityMaskHelper.enableGenericSubDevice(1);
    affinityMaskHelper.enableGenericSubDevice(2);
    affinityMaskHelper.enableGenericSubDevice(3);
    memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[mockRootDeviceIndex]->deviceAffinityMask = affinityMaskHelper;

    for (auto &multiTile : ::testing::Bool()) {
        for (auto &allocationType : {AllocationType::commandBuffer, AllocationType::ringBuffer, AllocationType::semaphoreBuffer}) {
            AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, allocationType, false, affinityMaskHelper.getGenericSubDevicesMask()};
            properties.flags.multiOsContextCapable = multiTile;
            auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
            EXPECT_NE(DeviceBitfield{firstAvailableTileMask}, storageInfo.memoryBanks);
        }
    }
}

TEST_F(MultiDeviceStorageInfoTest, givenDirectSubmissionForceLocalMemoryStorageEnabledForMultiTileEnginesWhenCreatingStorageInfoForCommandRingOrSemaphoreBufferThenFirstBankIsSelectedOnlyForMultiTileEngines) {
    DebugManagerStateRestore restorer;

    debugManager.flags.DirectSubmissionForceLocalMemoryStorageMode.set(1);
    constexpr uint32_t firstAvailableTileMask = 2u;

    memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->reserveOnBanks(firstAvailableTileMask, MemoryConstants::pageSize2M);
    EXPECT_NE(1u, memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->getLeastOccupiedBank(allTilesMask));

    AffinityMaskHelper affinityMaskHelper{false};
    affinityMaskHelper.enableGenericSubDevice(1);
    affinityMaskHelper.enableGenericSubDevice(2);
    affinityMaskHelper.enableGenericSubDevice(3);
    memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[mockRootDeviceIndex]->deviceAffinityMask = affinityMaskHelper;

    for (auto &multiTile : ::testing::Bool()) {
        for (auto &allocationType : {AllocationType::commandBuffer, AllocationType::ringBuffer, AllocationType::semaphoreBuffer}) {
            AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, allocationType, false, affinityMaskHelper.getGenericSubDevicesMask()};
            properties.flags.multiOsContextCapable = multiTile;
            auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
            if (multiTile) {
                EXPECT_EQ(DeviceBitfield{firstAvailableTileMask}, storageInfo.memoryBanks);
            } else {
                EXPECT_NE(DeviceBitfield{firstAvailableTileMask}, storageInfo.memoryBanks);
            }
        }
    }
}

TEST_F(MultiDeviceStorageInfoTest, givenDirectSubmissionForceLocalMemoryStorageEnabledForAllEnginesWhenCreatingStorageInfoForCommandRingOrSemaphoreBufferThenFirstBankIsSelectedOnlyForMultiTile) {
    DebugManagerStateRestore restorer;

    debugManager.flags.DirectSubmissionForceLocalMemoryStorageMode.set(2);
    constexpr uint32_t firstAvailableTileMask = 2u;
    constexpr uint32_t leastOccupiedTileMask = 4u;

    memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->reserveOnBanks(firstAvailableTileMask, MemoryConstants::pageSize2M);
    EXPECT_NE(1u, memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->getLeastOccupiedBank(allTilesMask));

    AffinityMaskHelper affinityMaskHelper{false};
    affinityMaskHelper.enableGenericSubDevice(1);
    affinityMaskHelper.enableGenericSubDevice(2);
    affinityMaskHelper.enableGenericSubDevice(3);
    memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[mockRootDeviceIndex]->deviceAffinityMask = affinityMaskHelper;

    for (auto &multiTile : ::testing::Bool()) {
        for (auto &allocationType : {AllocationType::commandBuffer, AllocationType::ringBuffer, AllocationType::semaphoreBuffer}) {
            AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, allocationType, false, affinityMaskHelper.getGenericSubDevicesMask()};
            properties.flags.multiOsContextCapable = multiTile;
            auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
            if (multiTile) {
                EXPECT_EQ(DeviceBitfield{firstAvailableTileMask}, storageInfo.memoryBanks);
            } else {
                EXPECT_EQ(DeviceBitfield{leastOccupiedTileMask}, storageInfo.memoryBanks);
            }
        }
    }
}

TEST_F(MultiDeviceStorageInfoTest, givenBufferOrSvmGpuAllocationWhenLocalOnlyFlagValueComputedThenProductHelperIsUsed) {
    constexpr bool preferCompressed{false};
    MockProductHelper productHelper{};
    MockReleaseHelper mockReleaseHelper{};

    EXPECT_EQ(0U, productHelper.getStorageInfoLocalOnlyFlagCalled);

    const auto isEnabledForRelease{[](ReleaseHelper *releaseHelper) { return (!releaseHelper || releaseHelper->isLocalOnlyAllowed()); }};

    for (const auto allocationType : std::array{AllocationType::buffer, AllocationType::svmGpu}) {
        productHelper.getStorageInfoLocalOnlyFlagResult = isEnabledForRelease(nullptr);
        EXPECT_EQ(memoryManager->getLocalOnlyRequired(allocationType, productHelper, nullptr, preferCompressed),
                  productHelper.getStorageInfoLocalOnlyFlagResult);

        for (const bool allowed : std::array{false, true}) {
            mockReleaseHelper.isLocalOnlyAllowedResult = allowed;
            productHelper.getStorageInfoLocalOnlyFlagResult = isEnabledForRelease(&mockReleaseHelper);
            EXPECT_EQ(memoryManager->getLocalOnlyRequired(allocationType, productHelper, &mockReleaseHelper, preferCompressed),
                      productHelper.getStorageInfoLocalOnlyFlagResult);
        }
    }
    EXPECT_EQ(6U, productHelper.getStorageInfoLocalOnlyFlagCalled);
}

TEST_F(MultiDeviceStorageInfoTest, givenNeitherBufferNorSvmGpuAllocationWhenLocalOnlyFlagValueComputedThenProductHelperIsNotUsed) {
    MockProductHelper productHelper{};
    MockReleaseHelper mockReleaseHelper{};

    EXPECT_EQ(0U, productHelper.getStorageInfoLocalOnlyFlagCalled);

    const auto allocationType{AllocationType::unknown};

    bool preferCompressed{true};
    for (const bool allowed : std::array{false, true}) {
        mockReleaseHelper.isLocalOnlyAllowedResult = allowed;
        EXPECT_EQ(memoryManager->getLocalOnlyRequired(allocationType, productHelper, &mockReleaseHelper, preferCompressed),
                  mockReleaseHelper.isLocalOnlyAllowedResult);
    }

    preferCompressed = false;
    for (const bool allowed : std::array{false, true}) {
        mockReleaseHelper.isLocalOnlyAllowedResult = allowed;
        EXPECT_EQ(memoryManager->getLocalOnlyRequired(allocationType, productHelper, &mockReleaseHelper, preferCompressed),
                  false);
    }
    EXPECT_EQ(0U, productHelper.getStorageInfoLocalOnlyFlagCalled);
}
