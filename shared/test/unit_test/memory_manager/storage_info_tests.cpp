/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/device/root_device.h"
#include "shared/source/device/sub_device.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

struct MultiDeviceStorageInfoTest : public ::testing::Test {
    void SetUp() override {
        ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
        DebugManager.flags.CreateMultipleSubDevices.set(numDevices);
        DebugManager.flags.EnableLocalMemory.set(true);
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

TEST_F(MultiDeviceStorageInfoTest, givenEnabledFlagForMultiTileIsaPlacementWhenCreatingStorageInfoForKernelIsaThenAllMemoryBanksAreOnAndPageTableClonningIsNotRequired) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.MultiTileIsaPlacement.set(1);

    AllocationType isaTypes[] = {AllocationType::KERNEL_ISA, AllocationType::KERNEL_ISA_INTERNAL};

    for (uint32_t i = 0; i < 2; i++) {
        AllocationProperties properties{mockRootDeviceIndex, false, 0u, isaTypes[i], false, false, singleTileMask};
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

TEST_F(MultiDeviceStorageInfoTest, givenDefaultFlagForMultiTileIsaPlacementWhenCreatingStorageInfoForKernelIsaThenSingleMemoryBanksIsOnAndPageTableClonningIsRequired) {

    AllocationType isaTypes[] = {AllocationType::KERNEL_ISA, AllocationType::KERNEL_ISA_INTERNAL};

    for (uint32_t i = 0; i < 2; i++) {
        AllocationProperties properties{mockRootDeviceIndex, false, 0u, isaTypes[i], false, false, singleTileMask};

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

TEST_F(MultiDeviceStorageInfoTest, givenDisabledFlagForMultiTileIsaPlacementWhenCreatingStorageInfoForKernelIsaThenSingleMemoryBanksIsOnAndPageTableClonningIsRequired) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.MultiTileIsaPlacement.set(0);

    AllocationType isaTypes[] = {AllocationType::KERNEL_ISA, AllocationType::KERNEL_ISA_INTERNAL};

    for (uint32_t i = 0; i < 2; i++) {
        AllocationProperties properties{mockRootDeviceIndex, false, 0u, isaTypes[i], false, false, singleTileMask};
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
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::PRIVATE_SURFACE, false, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_FALSE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(singleTileMask, storageInfo.memoryBanks);
    EXPECT_EQ(singleTileMask, storageInfo.pageTablesVisibility);
    EXPECT_FALSE(storageInfo.tileInstanced);
    EXPECT_EQ(1u, storageInfo.getNumBanks());
}

TEST_F(MultiDeviceStorageInfoTest, whenCreatingStorageInfoForPrivateSurfaceThenAllMemoryBanksAreOnAndPageTableClonningIsNotRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::PRIVATE_SURFACE, false, false, allTilesMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_FALSE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(allTilesMask, storageInfo.memoryBanks);
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
    EXPECT_TRUE(storageInfo.tileInstanced);
    EXPECT_EQ(4u, storageInfo.getNumBanks());
}

TEST_F(MultiDeviceStorageInfoTest, givenMultiTileCsrWhenCreatingStorageInfoForInternalHeapThenSingleMemoryBankIsOnAndPageTableClonningIsRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::INTERNAL_HEAP, true, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(singleTileMask, storageInfo.memoryBanks);
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
}

TEST_F(MultiDeviceStorageInfoTest, givenSingleTileCsrWhenCreatingStorageInfoForInternalHeapThenSingleMemoryBankIsOnAndPageTableClonningIsNotRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::INTERNAL_HEAP, false, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_FALSE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(singleTileMask, storageInfo.memoryBanks);
    EXPECT_EQ(singleTileMask, storageInfo.pageTablesVisibility);
    EXPECT_FALSE(storageInfo.tileInstanced);
}

TEST_F(MultiDeviceStorageInfoTest, givenMultiTileCsrWhenCreatingStorageInfoForLinearStreamThenSingleMemoryBankIsOnAndPageTableClonningIsRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::LINEAR_STREAM, true, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(singleTileMask, storageInfo.memoryBanks);
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
}

TEST_F(MultiDeviceStorageInfoTest, givenSingleTileCsrWhenCreatingStorageInfoForLinearStreamThenSingleMemoryBankIsOnAndPageTableClonningIsNotRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::LINEAR_STREAM, false, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_FALSE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(singleTileMask, storageInfo.memoryBanks);
    EXPECT_EQ(singleTileMask, storageInfo.pageTablesVisibility);
    EXPECT_FALSE(storageInfo.tileInstanced);
}

TEST_F(MultiDeviceStorageInfoTest, givenMultiTileCsrWhenCreatingStorageInfoForCommandBufferThenFirstAvailableMemoryBankIsOnAndPageTableClonningIsRequired) {
    const DeviceBitfield firstTileMask{static_cast<uint32_t>(1u)};
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::COMMAND_BUFFER, true, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(firstTileMask, storageInfo.memoryBanks);
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
}

TEST_F(MultiDeviceStorageInfoTest, givenSingleTileCsrWhenCreatingStorageInfoForCommandBufferThenSingleMemoryBankIsOnAndPageTableClonningIsNotRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::COMMAND_BUFFER, false, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_FALSE(storageInfo.cloningOfPageTables);
    EXPECT_FALSE(storageInfo.tileInstanced);
    EXPECT_EQ(singleTileMask, storageInfo.memoryBanks);
    EXPECT_EQ(singleTileMask, storageInfo.pageTablesVisibility);
}

TEST_F(MultiDeviceStorageInfoTest, givenMultiTileCsrWhenCreatingStorageInfoForScratchSpaceThenAllMemoryBankAreOnAndPageTableClonningIsNotRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::SCRATCH_SURFACE, true, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_FALSE(storageInfo.cloningOfPageTables);
    EXPECT_TRUE(storageInfo.tileInstanced);
    EXPECT_EQ(allTilesMask, storageInfo.memoryBanks);
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
}

TEST_F(MultiDeviceStorageInfoTest, givenSingleTileCsrWhenCreatingStorageInfoForScratchSpaceThenSingleMemoryBankIsOnAndPageTableClonningIsRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::SCRATCH_SURFACE, false, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(singleTileMask, storageInfo.memoryBanks);
    EXPECT_EQ(singleTileMask, storageInfo.pageTablesVisibility);
}

TEST_F(MultiDeviceStorageInfoTest, givenMultiTileCsrWhenCreatingStorageInfoForPreemptionAllocationThenAllMemoryBankAreOnAndPageTableClonningIsNotRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::PREEMPTION, true, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_FALSE(storageInfo.cloningOfPageTables);
    EXPECT_TRUE(storageInfo.tileInstanced);
    EXPECT_EQ(allTilesMask, storageInfo.memoryBanks);
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
}

TEST_F(MultiDeviceStorageInfoTest, givenSingleTileCsrWhenCreatingStorageInfoForPreemptionAllocationThenSingleMemoryBankIsOnAndPageTableClonningIsRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::PREEMPTION, false, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(singleTileMask, storageInfo.memoryBanks);
    EXPECT_EQ(singleTileMask, storageInfo.pageTablesVisibility);
}

TEST_F(MultiDeviceStorageInfoTest, whenCreatingStorageInfoForWorkPartitionSurfaceThenAllMemoryBankAreOnAndPageTableClonningIsNotRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, 0u, AllocationType::WORK_PARTITION_SURFACE, true, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_FALSE(storageInfo.cloningOfPageTables);
    EXPECT_TRUE(storageInfo.tileInstanced);
    EXPECT_EQ(allTilesMask, storageInfo.memoryBanks);
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
}

HWTEST_F(MultiDeviceStorageInfoTest, givenSingleTileCsrWhenAllocatingCsrSpecificAllocationsThenStoreThemInSystemMemory) {
    auto commandStreamReceiver = static_cast<UltCommandStreamReceiver<FamilyType> *>(factory.rootDevices[0]->getSubDevice(tileIndex)->getDefaultEngine().commandStreamReceiver);
    auto &heap = commandStreamReceiver->getIndirectHeap(IndirectHeap::Type::INDIRECT_OBJECT, MemoryConstants::pageSize64k);
    auto heapAllocation = heap.getGraphicsAllocation();
    if (commandStreamReceiver->canUse4GbHeaps) {
        EXPECT_EQ(AllocationType::INTERNAL_HEAP, heapAllocation->getAllocationType());
    } else {
        EXPECT_EQ(AllocationType::LINEAR_STREAM, heapAllocation->getAllocationType());
    }

    commandStreamReceiver->ensureCommandBufferAllocation(heap, heap.getAvailableSpace() + 1, 0u);
    auto commandBufferAllocation = heap.getGraphicsAllocation();
    EXPECT_EQ(AllocationType::COMMAND_BUFFER, commandBufferAllocation->getAllocationType());
    EXPECT_NE(heapAllocation, commandBufferAllocation);
    EXPECT_NE(commandBufferAllocation->getMemoryPool(), MemoryPool::LocalMemory);
}

TEST_F(MultiDeviceStorageInfoTest, whenCreatingStorageInfoForBufferCompressedThenAllMemoryBanksAreOnAndPageTableClonningIsRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::BUFFER, true, allTilesMask};
    properties.flags.preferCompressed = true;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(allTilesMask, storageInfo.memoryBanks);
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
    EXPECT_TRUE(storageInfo.multiStorage);
    EXPECT_EQ(storageInfo.colouringGranularity, MemoryConstants::pageSize64k);
    EXPECT_EQ(storageInfo.colouringPolicy, ColouringPolicy::DeviceCountBased);
}

TEST_F(MultiDeviceStorageInfoTest, whenCreatingStorageInfoForBufferThenAllMemoryBanksAreOnAndPageTableClonningIsRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::BUFFER, true, allTilesMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(allTilesMask, storageInfo.memoryBanks);
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
    EXPECT_TRUE(storageInfo.multiStorage);
    EXPECT_EQ(storageInfo.colouringGranularity, MemoryConstants::pageSize64k);
    EXPECT_EQ(storageInfo.colouringPolicy, ColouringPolicy::DeviceCountBased);
}

TEST_F(MultiDeviceStorageInfoTest, whenCreatingStorageInfoForSVMGPUThenAllMemoryBanksAreOnAndPageTableClonningIsRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::SVM_GPU, true, allTilesMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(allTilesMask, storageInfo.memoryBanks);
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
    EXPECT_TRUE(storageInfo.multiStorage);
    EXPECT_EQ(storageInfo.colouringGranularity, MemoryConstants::pageSize64k);
    EXPECT_EQ(storageInfo.colouringPolicy, ColouringPolicy::DeviceCountBased);
}

TEST_F(MultiDeviceStorageInfoTest, givenMultiStorageGranularityWhenCreatingStorageInfoThenProperGranularityIsSet) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.MultiStorageGranularity.set(128);
    DebugManager.flags.MultiStoragePolicy.set(1);

    AllocationProperties properties{mockRootDeviceIndex, false, 10 * MemoryConstants::pageSize64k, AllocationType::SVM_GPU, true, allTilesMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(allTilesMask, storageInfo.memoryBanks);
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
    EXPECT_TRUE(storageInfo.multiStorage);
    EXPECT_EQ(storageInfo.colouringGranularity, MemoryConstants::kiloByte * 128);
    EXPECT_EQ(storageInfo.colouringPolicy, ColouringPolicy::ChunkSizeBased);
}

TEST_F(MultiDeviceStorageInfoTest, givenTwoPagesAllocationSizeWhenCreatingStorageInfoForBufferThenSingleMemoryBankIsOnAndPageTableClonningIsRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, 2 * MemoryConstants::pageSize64k, AllocationType::BUFFER, true, allTilesMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(1lu, storageInfo.memoryBanks.to_ulong());
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
    EXPECT_FALSE(storageInfo.multiStorage);
}

TEST_F(MultiDeviceStorageInfoTest, givenSpecifiedDeviceIndexWhenCreatingStorageInfoForBufferThenSingleMemoryBankIsOnAndPageTableClonningIsRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::BUFFER, true, false, singleTileMask};
    properties.multiStorageResource = true;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(singleTileMask, storageInfo.memoryBanks);
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
    EXPECT_FALSE(storageInfo.multiStorage);
}

TEST_F(MultiDeviceStorageInfoTest, givenResourceColouringNotSupportedWhenCreatingStorageInfoForBufferThenSingleMemoryBankIsOnAndPageTableClonningIsRequired) {
    memoryManager->supportsMultiStorageResources = false;
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::BUFFER, true, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(1lu, storageInfo.memoryBanks.count());
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
    EXPECT_FALSE(storageInfo.multiStorage);
}

TEST_F(MultiDeviceStorageInfoTest, givenNonMultiStorageResourceWhenCreatingStorageInfoForBufferThenSingleMemoryBankIsOnAndPageTableClonningIsRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::BUFFER, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_EQ(1lu, storageInfo.memoryBanks.count());
    EXPECT_EQ(allTilesMask, storageInfo.pageTablesVisibility);
    EXPECT_FALSE(storageInfo.multiStorage);
}

TEST_F(MultiDeviceStorageInfoTest, whenCreatingStorageInfoForBufferThenLocalOnlyFlagIsRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::BUFFER, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.localOnlyRequired);
}

TEST_F(MultiDeviceStorageInfoTest, whenCreatingStorageInfoForBufferCompressedThenLocalOnlyFlagIsRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::BUFFER, false, singleTileMask};
    properties.flags.preferCompressed = true;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.localOnlyRequired);
}

TEST_F(MultiDeviceStorageInfoTest, whenCreatingStorageInfoForSvmGpuThenLocalOnlyFlagIsRequired) {
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::SVM_GPU, false, singleTileMask};
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.localOnlyRequired);
}

TEST_F(MultiDeviceStorageInfoTest, whenCreatingStorageInfoForShareableSvmGpuThenLocalOnlyFlagIsRequiredAndIsNotLocable) {
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::SVM_GPU, false, singleTileMask};
    properties.flags.shareable = 1u;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_TRUE(storageInfo.localOnlyRequired);
    EXPECT_FALSE(storageInfo.isLockable);
}

TEST_F(MultiDeviceStorageInfoTest, givenReadOnlyBufferToBeCopiedAcrossTilesWhenCreatingStorageInfoThenCorrectValuesAreSet) {
    AllocationProperties properties{mockRootDeviceIndex, false, 1u, AllocationType::BUFFER, false, singleTileMask};
    properties.flags.readOnlyMultiStorage = true;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_EQ(allTilesMask, storageInfo.memoryBanks);
    EXPECT_FALSE(storageInfo.cloningOfPageTables);
    EXPECT_TRUE(storageInfo.readOnlyMultiStorage);
    EXPECT_TRUE(storageInfo.tileInstanced);
}

TEST_F(MultiDeviceStorageInfoTest, givenReadOnlyBufferToBeCopiedAcrossTilesWhenDebugVariableIsSetThenOnlyCertainBanksAreUsed) {
    DebugManagerStateRestore restorer;
    auto proposedTiles = allTilesMask;
    proposedTiles[1] = 0;

    DebugManager.flags.OverrideMultiStoragePlacement.set(proposedTiles.to_ulong());

    AllocationProperties properties{mockRootDeviceIndex, false, 64 * KB * 40, AllocationType::BUFFER, true, allTilesMask};

    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_EQ(proposedTiles, storageInfo.memoryBanks);
    EXPECT_TRUE(storageInfo.cloningOfPageTables);
    EXPECT_FALSE(storageInfo.tileInstanced);
    EXPECT_EQ(3u, storageInfo.getNumBanks());
}

TEST_F(MultiDeviceStorageInfoTest, givenLeastOccupiedBankAndOtherBitsEnabledInSubDeviceBitfieldWhenCreateStorageInfoThenTakeLeastOccupiedBankAsMemoryBank) {
    AllocationProperties properties{mockRootDeviceIndex, false, 1u, AllocationType::UNKNOWN, false, singleTileMask};
    auto leastOccupiedBank = memoryManager->getLocalMemoryUsageBankSelector(properties.allocationType, properties.rootDeviceIndex)->getLeastOccupiedBank(properties.subDevicesBitfield);
    properties.subDevicesBitfield.set(leastOccupiedBank);
    properties.subDevicesBitfield.set(leastOccupiedBank + 1);
    EXPECT_EQ(2u, properties.subDevicesBitfield.count());
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_EQ(1u, storageInfo.memoryBanks.count());
    EXPECT_TRUE(storageInfo.memoryBanks.test(leastOccupiedBank));
}

TEST_F(MultiDeviceStorageInfoTest, givenNoSubdeviceBitfieldWhenCreateStorageInfoThenReturnEmptyStorageInfo) {
    AllocationProperties properties{mockRootDeviceIndex, false, 1u, AllocationType::UNKNOWN, false, {}};
    StorageInfo emptyInfo{};
    EXPECT_EQ(memoryManager->createStorageInfoFromProperties(properties).getMemoryBanks(), emptyInfo.getMemoryBanks());
}

TEST_F(MultiDeviceStorageInfoTest, givenGraphicsAllocationWithCpuAccessRequiredWhenCreatingStorageInfoThenSetCpuVisibleSegmentIsRequiredAndIsLockableFlagIsEnabled) {
    auto firstAllocationIdx = static_cast<int>(AllocationType::UNKNOWN);
    auto lastAllocationIdx = static_cast<int>(AllocationType::COUNT);

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
    auto firstAllocationIdx = static_cast<int>(AllocationType::UNKNOWN);
    auto lastAllocationIdx = static_cast<int>(AllocationType::COUNT);

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

TEST_F(MultiDeviceStorageInfoTest, givenGpuTimestampAllocationWhenUsingSingleTileDeviceThenExpectRegularAllocationStorageInfo) {
    AllocationProperties properties{mockRootDeviceIndex,
                                    false,
                                    1u,
                                    AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER,
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
                                    AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER,
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
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::SEMAPHORE_BUFFER, false, singleTileMask};
    properties.flags.multiOsContextCapable = false;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_EQ(singleTileMask, storageInfo.memoryBanks);
}

TEST_F(MultiDeviceStorageInfoTest, givenSingleTileWhenCreatingStorageInfoForCommandBufferThenProvidedMemoryBankIsSelected) {
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::COMMAND_BUFFER, false, singleTileMask};
    properties.flags.multiOsContextCapable = false;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_EQ(singleTileMask, storageInfo.memoryBanks);
}

TEST_F(MultiDeviceStorageInfoTest, givenSingleTileWhenCreatingStorageInfoForRingBufferThenProvidedMemoryBankIsSelected) {
    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::RING_BUFFER, false, singleTileMask};
    properties.flags.multiOsContextCapable = false;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_EQ(singleTileMask, storageInfo.memoryBanks);
}

TEST_F(MultiDeviceStorageInfoTest, givenMultiTileWhenCreatingStorageInfoForSemaphoreBufferThenFirstBankIsSelectedEvenIfOtherTileIsLessOccupied) {
    constexpr uint32_t firstAvailableTileMask = 2u;

    memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->reserveOnBanks(firstAvailableTileMask, MemoryConstants::pageSize2Mb);
    EXPECT_NE(1u, memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->getLeastOccupiedBank(allTilesMask));

    AffinityMaskHelper affinityMaskHelper{false};
    affinityMaskHelper.enableGenericSubDevice(1);
    affinityMaskHelper.enableGenericSubDevice(2);
    affinityMaskHelper.enableGenericSubDevice(3);

    memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[mockRootDeviceIndex]->deviceAffinityMask = affinityMaskHelper;

    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::SEMAPHORE_BUFFER, false, affinityMaskHelper.getGenericSubDevicesMask()};
    properties.flags.multiOsContextCapable = true;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_EQ(DeviceBitfield{firstAvailableTileMask}, storageInfo.memoryBanks);
}

TEST_F(MultiDeviceStorageInfoTest, givenMultiTileWhenCreatingStorageInfoForCommandBufferThenFirstBankIsSelectedEvenIfOtherTileIsLessOccupied) {
    constexpr uint32_t firstAvailableTileMask = 2u;

    memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->reserveOnBanks(firstAvailableTileMask, MemoryConstants::pageSize2Mb);
    EXPECT_NE(1u, memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->getLeastOccupiedBank(allTilesMask));

    AffinityMaskHelper affinityMaskHelper{false};
    affinityMaskHelper.enableGenericSubDevice(1);
    affinityMaskHelper.enableGenericSubDevice(2);
    affinityMaskHelper.enableGenericSubDevice(3);

    memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[mockRootDeviceIndex]->deviceAffinityMask = affinityMaskHelper;

    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::COMMAND_BUFFER, false, affinityMaskHelper.getGenericSubDevicesMask()};
    properties.flags.multiOsContextCapable = true;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_EQ(DeviceBitfield{firstAvailableTileMask}, storageInfo.memoryBanks);
}

TEST_F(MultiDeviceStorageInfoTest, givenMultiTileWhenCreatingStorageInfoForRingBufferThenFirstBankIsSelectedEvenIfOtherTileIsLessOccupied) {
    constexpr uint32_t firstAvailableTileMask = 2u;

    memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->reserveOnBanks(firstAvailableTileMask, MemoryConstants::pageSize2Mb);
    EXPECT_NE(1u, memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->getLeastOccupiedBank(allTilesMask));

    AffinityMaskHelper affinityMaskHelper{false};
    affinityMaskHelper.enableGenericSubDevice(1);
    affinityMaskHelper.enableGenericSubDevice(2);
    affinityMaskHelper.enableGenericSubDevice(3);

    memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[mockRootDeviceIndex]->deviceAffinityMask = affinityMaskHelper;

    AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, AllocationType::RING_BUFFER, false, affinityMaskHelper.getGenericSubDevicesMask()};
    properties.flags.multiOsContextCapable = true;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
    EXPECT_EQ(DeviceBitfield{firstAvailableTileMask}, storageInfo.memoryBanks);
}

TEST_F(MultiDeviceStorageInfoTest, givenDirectSubmissionForceLocalMemoryStorageDisabledWhenCreatingStorageInfoForCommandRingOrSemaphoreBufferThenPreferredBankIsSelected) {
    DebugManagerStateRestore restorer;

    DebugManager.flags.DirectSubmissionForceLocalMemoryStorageMode.set(0);
    constexpr uint32_t firstAvailableTileMask = 2u;

    memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->reserveOnBanks(firstAvailableTileMask, MemoryConstants::pageSize2Mb);
    EXPECT_NE(1u, memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->getLeastOccupiedBank(allTilesMask));

    AffinityMaskHelper affinityMaskHelper{false};
    affinityMaskHelper.enableGenericSubDevice(1);
    affinityMaskHelper.enableGenericSubDevice(2);
    affinityMaskHelper.enableGenericSubDevice(3);
    memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[mockRootDeviceIndex]->deviceAffinityMask = affinityMaskHelper;

    for (auto &multiTile : ::testing::Bool()) {
        for (auto &allocationType : {AllocationType::COMMAND_BUFFER, AllocationType::RING_BUFFER, AllocationType::SEMAPHORE_BUFFER}) {
            AllocationProperties properties{mockRootDeviceIndex, false, numDevices * MemoryConstants::pageSize64k, allocationType, false, affinityMaskHelper.getGenericSubDevicesMask()};
            properties.flags.multiOsContextCapable = multiTile;
            auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);
            EXPECT_NE(DeviceBitfield{firstAvailableTileMask}, storageInfo.memoryBanks);
        }
    }
}

TEST_F(MultiDeviceStorageInfoTest, givenDirectSubmissionForceLocalMemoryStorageEnabledForMultiTileEnginesWhenCreatingStorageInfoForCommandRingOrSemaphoreBufferThenFirstBankIsSelectedOnlyForMultiTileEngines) {
    DebugManagerStateRestore restorer;

    DebugManager.flags.DirectSubmissionForceLocalMemoryStorageMode.set(1);
    constexpr uint32_t firstAvailableTileMask = 2u;

    memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->reserveOnBanks(firstAvailableTileMask, MemoryConstants::pageSize2Mb);
    EXPECT_NE(1u, memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->getLeastOccupiedBank(allTilesMask));

    AffinityMaskHelper affinityMaskHelper{false};
    affinityMaskHelper.enableGenericSubDevice(1);
    affinityMaskHelper.enableGenericSubDevice(2);
    affinityMaskHelper.enableGenericSubDevice(3);
    memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[mockRootDeviceIndex]->deviceAffinityMask = affinityMaskHelper;

    for (auto &multiTile : ::testing::Bool()) {
        for (auto &allocationType : {AllocationType::COMMAND_BUFFER, AllocationType::RING_BUFFER, AllocationType::SEMAPHORE_BUFFER}) {
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

    DebugManager.flags.DirectSubmissionForceLocalMemoryStorageMode.set(2);
    constexpr uint32_t firstAvailableTileMask = 2u;
    constexpr uint32_t leastOccupiedTileMask = 4u;

    memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->reserveOnBanks(firstAvailableTileMask, MemoryConstants::pageSize2Mb);
    EXPECT_NE(1u, memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->getLeastOccupiedBank(allTilesMask));

    AffinityMaskHelper affinityMaskHelper{false};
    affinityMaskHelper.enableGenericSubDevice(1);
    affinityMaskHelper.enableGenericSubDevice(2);
    affinityMaskHelper.enableGenericSubDevice(3);
    memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[mockRootDeviceIndex]->deviceAffinityMask = affinityMaskHelper;

    for (auto &multiTile : ::testing::Bool()) {
        for (auto &allocationType : {AllocationType::COMMAND_BUFFER, AllocationType::RING_BUFFER, AllocationType::SEMAPHORE_BUFFER}) {
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
