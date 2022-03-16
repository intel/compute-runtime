/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_helper.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/memory_properties_helpers.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(MemoryManagerTest, givenSetUseSytemMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedThenNullptrIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.useSystemMemory = true;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}

TEST(MemoryManagerTest, givenAllowed32BitAndFroce32BitWhenGraphicsAllocationInDevicePoolIsAllocatedThenNullptrIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    memoryManager.setForce32BitAllocations(true);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allow32Bit = true;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}

TEST(AllocationFlagsTest, givenAllocateMemoryFlagWhenGetAllocationFlagsIsCalledThenAllocateFlagIsCorrectlySet) {
    HardwareInfo hwInfo(*defaultHwInfo);
    UltDeviceFactory deviceFactory{1, 0};
    auto pDevice = deviceFactory.rootDevices[0];
    MemoryProperties memoryProperties{};
    memoryProperties.pDevice = pDevice;
    auto allocationProperties = MemoryPropertiesHelper::getAllocationProperties(0, memoryProperties, true, 0, AllocationType::BUFFER, false, hwInfo, {}, true);
    EXPECT_TRUE(allocationProperties.flags.allocateMemory);

    auto allocationProperties2 = MemoryPropertiesHelper::getAllocationProperties(0, memoryProperties, false, 0, AllocationType::BUFFER, false, hwInfo, {}, true);
    EXPECT_FALSE(allocationProperties2.flags.allocateMemory);
}

TEST(UncacheableFlagsTest, givenUncachedResourceFlagWhenGetAllocationFlagsIsCalledThenUncacheableFlagIsCorrectlySet) {
    UltDeviceFactory deviceFactory{1, 0};
    auto pDevice = deviceFactory.rootDevices[0];

    MemoryProperties memoryProperties{};
    memoryProperties.pDevice = pDevice;
    memoryProperties.flags.locallyUncachedResource = true;
    auto allocationFlags = MemoryPropertiesHelper::getAllocationProperties(
        0, memoryProperties, false, 0, AllocationType::BUFFER, false, pDevice->getHardwareInfo(), {}, true);
    EXPECT_TRUE(allocationFlags.flags.uncacheable);

    memoryProperties.flags.locallyUncachedResource = false;
    auto allocationFlags2 = MemoryPropertiesHelper::getAllocationProperties(
        0, memoryProperties, false, 0, AllocationType::BUFFER, false, pDevice->getHardwareInfo(), {}, true);
    EXPECT_FALSE(allocationFlags2.flags.uncacheable);
}

TEST(AllocationFlagsTest, givenReadOnlyResourceFlagWhenGetAllocationFlagsIsCalledThenFlushL3FlagsAreCorrectlySet) {
    UltDeviceFactory deviceFactory{1, 2};
    auto pDevice = deviceFactory.rootDevices[0];

    MemoryProperties memoryProperties{};
    memoryProperties.pDevice = pDevice;
    memoryProperties.flags.readOnly = true;

    auto allocationFlags =
        MemoryPropertiesHelper::getAllocationProperties(
            0, memoryProperties, true, 0, AllocationType::BUFFER, false, pDevice->getHardwareInfo(), {}, false);
    EXPECT_FALSE(allocationFlags.flags.flushL3RequiredForRead);
    EXPECT_FALSE(allocationFlags.flags.flushL3RequiredForWrite);

    memoryProperties.flags.readOnly = false;
    auto allocationFlags2 = MemoryPropertiesHelper::getAllocationProperties(
        0, memoryProperties, true, 0, AllocationType::BUFFER, false, pDevice->getHardwareInfo(), {}, false);
    EXPECT_TRUE(allocationFlags2.flags.flushL3RequiredForRead);
    EXPECT_TRUE(allocationFlags2.flags.flushL3RequiredForWrite);
}

TEST(StorageInfoTest, whenStorageInfoIsCreatedWithDefaultConstructorThenReturnsOneHandle) {
    StorageInfo storageInfo;
    EXPECT_EQ(1u, storageInfo.getNumBanks());
}

class MockMemoryManagerBaseImplementationOfDevicePool : public MemoryManagerCreate<OsAgnosticMemoryManager> {
  public:
    using MemoryManagerCreate<OsAgnosticMemoryManager>::MemoryManagerCreate;

    GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) override {
        if (failInAllocate) {
            return nullptr;
        }
        return OsAgnosticMemoryManager::allocateGraphicsMemoryInDevicePool(allocationData, status);
    }

    GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) override {
        if (failInAllocate) {
            return nullptr;
        }
        return OsAgnosticMemoryManager::allocateGraphicsMemoryWithAlignment(allocationData);
    }

    void *allocateSystemMemory(size_t size, size_t alignment) override {
        if (failInAllocate) {
            return nullptr;
        }
        return OsAgnosticMemoryManager::allocateSystemMemory(size, alignment);
    }
    bool failInAllocate = false;
};

using MemoryManagerTests = ::testing::Test;

TEST(MemoryManagerTest, givenNotSetUseSystemMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedThenLocalMemoryAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenNotSetUseSystemMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedThenLocalMemoryAllocationHasCorrectStorageInfoAndFlushL3IsSet) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.storageInfo.memoryBanks = 0x1;
    allocData.storageInfo.pageTablesVisibility = 0x2;
    allocData.storageInfo.cloningOfPageTables = false;
    allocData.flags.flushL3 = true;

    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);

    EXPECT_EQ(allocData.storageInfo.memoryBanks, allocation->storageInfo.memoryBanks);
    EXPECT_EQ(allocData.storageInfo.pageTablesVisibility, allocation->storageInfo.pageTablesVisibility);
    EXPECT_FALSE(allocation->storageInfo.cloningOfPageTables);
    EXPECT_TRUE(allocation->isFlushL3Required());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenEnabledLocalMemoryAndUseSytemMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedThenNullptrIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.useSystemMemory = true;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}

TEST(MemoryManagerTest, givenEnabledLocalMemoryAndAllowed32BitAndForce32BitWhenGraphicsAllocationInDevicePoolIsAllocatedThenNullptrIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);
    memoryManager.setForce32BitAllocations(true);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allow32Bit = true;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}

TEST(MemoryManagerTest, givenEnabledLocalMemoryAndAllowed32BitWhen32BitIsNotForcedThenGraphicsAllocationInDevicePoolReturnsLocalMemoryAllocation) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);
    memoryManager.setForce32BitAllocations(false);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allow32Bit = true;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenEnabledLocalMemoryWhenAllocate32BitFailsThenGraphicsAllocationInDevicePoolReturnsError) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.type = AllocationType::KERNEL_ISA; // HEAP_INTERNAL will be used
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;

    memoryManager.failAllocate32Bit = true;

    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);

    memoryManager.freeGraphicsMemory(allocation);
}

HWTEST_F(MemoryManagerTests, givenEnabledLocalMemoryWhenAllocatingDebugAreaThenHeapInternalDeviceFrontWindowIsUsed) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MemoryManagerCreate<OsAgnosticMemoryManager> osAgnosticMemoryManager(false, true, executionEnvironment);

    NEO::AllocationProperties properties{0, true, MemoryConstants::pageSize64k,
                                         NEO::AllocationType::DEBUG_MODULE_AREA,
                                         false,
                                         mockDeviceBitfield};
    properties.flags.use32BitFrontWindow = true;

    auto &hwHelper = HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);
    auto systemMemoryPlacement = hwHelper.useSystemMemoryPlacementForISA(*defaultHwInfo);

    HeapIndex expectedHeap = HeapIndex::TOTAL_HEAPS;
    HeapIndex baseHeap = HeapIndex::TOTAL_HEAPS;
    if (systemMemoryPlacement) {
        expectedHeap = HeapIndex::HEAP_INTERNAL_FRONT_WINDOW;
        baseHeap = HeapIndex::HEAP_INTERNAL;
    } else {
        expectedHeap = HeapIndex::HEAP_INTERNAL_DEVICE_FRONT_WINDOW;
        baseHeap = HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY;
    }
    auto moduleDebugArea = osAgnosticMemoryManager.allocateGraphicsMemoryWithProperties(properties);
    auto gpuAddress = moduleDebugArea->getGpuAddress();
    EXPECT_LE(GmmHelper::canonize(osAgnosticMemoryManager.getGfxPartition(0)->getHeapBase(expectedHeap)), gpuAddress);
    EXPECT_GT(GmmHelper::canonize(osAgnosticMemoryManager.getGfxPartition(0)->getHeapLimit(expectedHeap)), gpuAddress);
    EXPECT_EQ(GmmHelper::canonize(osAgnosticMemoryManager.getGfxPartition(0)->getHeapBase(expectedHeap)), moduleDebugArea->getGpuBaseAddress());
    EXPECT_EQ(GmmHelper::canonize(osAgnosticMemoryManager.getGfxPartition(0)->getHeapBase(baseHeap)), moduleDebugArea->getGpuBaseAddress());

    osAgnosticMemoryManager.freeGraphicsMemory(moduleDebugArea);
}

TEST(BaseMemoryManagerTest, givenMemoryManagerWithForced32BitsWhenSystemMemoryIsNotSetAnd32BitNotAllowedThenAllocateInDevicePoolReturnsLocalMemory) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManagerBaseImplementationOfDevicePool memoryManager(false, true, executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;

    memoryManager.setForce32BitAllocations(true);

    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(BaseMemoryManagerTest, givenDebugVariableSetWhenCompressedBufferIsCreatedThenCreateCompressedGmm) {
    DebugManagerStateRestore restore;
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, true, executionEnvironment);

    AllocationProperties allocPropertiesBuffer(mockRootDeviceIndex, 1, AllocationType::BUFFER, mockDeviceBitfield);
    AllocationProperties allocPropertiesBufferCompressed(mockRootDeviceIndex, 1, AllocationType::BUFFER, mockDeviceBitfield);
    allocPropertiesBufferCompressed.flags.preferCompressed = true;

    DebugManager.flags.RenderCompressedBuffersEnabled.set(1);
    auto allocationBuffer = memoryManager.allocateGraphicsMemoryInPreferredPool(allocPropertiesBuffer, nullptr);
    auto allocationBufferCompressed = memoryManager.allocateGraphicsMemoryInPreferredPool(allocPropertiesBufferCompressed, nullptr);
    EXPECT_EQ(nullptr, allocationBuffer->getDefaultGmm());
    EXPECT_NE(nullptr, allocationBufferCompressed->getDefaultGmm());
    EXPECT_TRUE(allocationBufferCompressed->getDefaultGmm()->isCompressionEnabled);
    memoryManager.freeGraphicsMemory(allocationBuffer);
    memoryManager.freeGraphicsMemory(allocationBufferCompressed);

    DebugManager.flags.RenderCompressedBuffersEnabled.set(0);
    allocationBuffer = memoryManager.allocateGraphicsMemoryInPreferredPool(allocPropertiesBuffer, nullptr);
    allocationBufferCompressed = memoryManager.allocateGraphicsMemoryInPreferredPool(allocPropertiesBufferCompressed, nullptr);
    EXPECT_EQ(nullptr, allocationBuffer->getDefaultGmm());
    EXPECT_EQ(nullptr, allocationBufferCompressed->getDefaultGmm());
    memoryManager.freeGraphicsMemory(allocationBuffer);
    memoryManager.freeGraphicsMemory(allocationBufferCompressed);
}

TEST(BaseMemoryManagerTest, givenMemoryManagerWithForced32BitsWhenSystemMemoryIsNotSetAnd32BitAllowedThenAllocateInDevicePoolReturnsRetryInNonDevicePool) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManagerBaseImplementationOfDevicePool memoryManager(false, true, executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.allow32Bit = true;

    memoryManager.setForce32BitAllocations(true);

    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(BaseMemoryManagerTest, givenMemoryManagerWhenAllocateFailsThenAllocateInDevicePoolReturnsError) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManagerBaseImplementationOfDevicePool memoryManager(false, true, executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.allow32Bit = true;

    memoryManager.failInAllocate = true;
    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(BaseMemoryManagerTest, givenSvmGpuAllocationTypeWhenAllocateSystemMemoryFailsThenReturnNull) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());

    if (!executionEnvironment.rootDeviceEnvironments[0]->isFullRangeSvm()) {
        return;
    }

    MockMemoryManagerBaseImplementationOfDevicePool memoryManager(false, true, executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;

    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::SVM_GPU;
    allocData.hostPtr = reinterpret_cast<void *>(0x1000);

    memoryManager.failInAllocate = true;
    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(BaseMemoryManagerTest, givenSvmGpuAllocationTypeWhenAllocationSucceedThenReturnGpuAddressAsHostPtrAndCpuAllocation) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());

    if (!executionEnvironment.rootDeviceEnvironments[0]->isFullRangeSvm()) {
        return;
    }

    MockMemoryManagerBaseImplementationOfDevicePool memoryManager(false, true, executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;

    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::SVM_GPU;
    allocData.hostPtr = reinterpret_cast<void *>(0x1000);

    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(reinterpret_cast<uint64_t>(allocData.hostPtr), allocation->getGpuAddress());
    EXPECT_NE(reinterpret_cast<uint64_t>(allocation->getUnderlyingBuffer()), allocation->getGpuAddress());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryAllocationTest, givenMultiTileVisiblityWhenAskedForFlagsThenL3NeedsToBeFlushed) {
    UltDeviceFactory deviceFactory{1, 2};
    auto pDevice = deviceFactory.rootDevices[0];
    MemoryProperties memoryProperties{};
    memoryProperties.pDevice = pDevice;
    auto allocationFlags = MemoryPropertiesHelper::getAllocationProperties(0, memoryProperties, true, 0, AllocationType::BUFFER, false, pDevice->getHardwareInfo(), {}, false);
    EXPECT_TRUE(allocationFlags.flags.flushL3RequiredForRead);
    EXPECT_TRUE(allocationFlags.flags.flushL3RequiredForWrite);
}

TEST(MemoryAllocationTest, givenMultiTileVisiblityAndUncachedWhenAskedForFlagsThenL3DoesNotNeedToBeFlushed) {
    UltDeviceFactory deviceFactory{1, 2};
    auto pDevice = deviceFactory.rootDevices[0];
    MemoryProperties memoryProperties{};
    memoryProperties.pDevice = pDevice;
    memoryProperties.flags.locallyUncachedResource = true;

    auto allocationFlags = MemoryPropertiesHelper::getAllocationProperties(
        0, memoryProperties, true, 0, AllocationType::BUFFER, false, pDevice->getHardwareInfo(), {}, false);
    EXPECT_FALSE(allocationFlags.flags.flushL3RequiredForRead);
    EXPECT_FALSE(allocationFlags.flags.flushL3RequiredForWrite);
}

TEST(MemoryAllocationTest, givenAubDumpForceAllToLocalMemoryWhenMemoryAllocationIsCreatedThenItHasLocalMemoryPool) {
    DebugManagerStateRestore debugRestorer;
    DebugManager.flags.AUBDumpForceAllToLocalMemory.set(true);

    MemoryAllocation allocation(mockRootDeviceIndex, AllocationType::UNKNOWN, nullptr, reinterpret_cast<void *>(0x1000), 0x1000,
                                0x1000, 0, MemoryPool::System4KBPages, false, false, MemoryManager::maxOsContextCount);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation.getMemoryPool());
}

TEST(MemoryAllocationTest, givenAubDumpForceAllToLocalMemoryWhenMemoryAllocationIsOverridenThenItHasLocalMemoryPool) {
    DebugManagerStateRestore debugRestorer;
    DebugManager.flags.AUBDumpForceAllToLocalMemory.set(true);

    MemoryAllocation allocation(mockRootDeviceIndex, AllocationType::UNKNOWN, nullptr, reinterpret_cast<void *>(0x1000), 0x1000,
                                0x1000, 0, MemoryPool::System4KBPages, false, false, MemoryManager::maxOsContextCount);
    allocation.overrideMemoryPool(MemoryPool::System64KBPages);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation.getMemoryPool());
}

HWTEST_F(MemoryManagerTests, givenEnabledLocalMemoryWhenAllocateInternalAllocationInDevicePoolThen32BitAllocationIsCreated) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::INTERNAL_HEAP, mockDeviceBitfield}, nullptr);
    EXPECT_NE(nullptr, allocation);
    EXPECT_TRUE(allocation->is32BitAllocation());
    EXPECT_TRUE(memoryManager.allocationInDevicePoolCreated);

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenDisabledLocalMemoryWhenAllocateInternalAllocationInDevicePoolThen32BitAllocationIsCreatedInNonDevicePool) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);

    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::INTERNAL_HEAP, mockDeviceBitfield}, nullptr);
    EXPECT_NE(nullptr, allocation);
    EXPECT_TRUE(allocation->is32BitAllocation());
    EXPECT_NE(MemoryPool::LocalMemory, allocation->getMemoryPool());
    EXPECT_FALSE(memoryManager.allocationInDevicePoolCreated);
    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenOsAgnosticMemoryManagerWhenGetLocalMemoryIsCalledThenSizeOfLocalMemoryIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);

    auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo();

    auto subDevicesCount = HwHelper::getSubDevicesCount(hwInfo);
    uint32_t deviceMask = static_cast<uint32_t>(maxNBitValue(subDevicesCount));

    EXPECT_EQ(AubHelper::getPerTileLocalMemorySize(hwInfo) * subDevicesCount, memoryManager.getLocalMemorySize(0u, deviceMask));
}

HWTEST_F(MemoryManagerTests, givenEnabledLocalMemoryWhenAllocatingKernelIsaThenLocalMemoryPoolIsUsed) {
    auto hwInfo = *defaultHwInfo;
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableLocalMemory.set(true);
    hwInfo.featureTable.flags.ftrLocalMemory = true;

    MockExecutionEnvironment executionEnvironment(&hwInfo);
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::KERNEL_ISA, mockDeviceBitfield}, nullptr);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
    EXPECT_TRUE(memoryManager.allocationInDevicePoolCreated);

    memoryManager.freeGraphicsMemory(allocation);

    allocation = memoryManager.allocateGraphicsMemoryInPreferredPool({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::KERNEL_ISA_INTERNAL, mockDeviceBitfield}, nullptr);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
    EXPECT_TRUE(memoryManager.allocationInDevicePoolCreated);

    memoryManager.freeGraphicsMemory(allocation);
}

HWTEST_F(MemoryManagerTests, givenEnabledLocalMemoryWhenAllocateKernelIsaInDevicePoolThenLocalMemoryPoolIsUsed) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::INTERNAL_HEAP, mockDeviceBitfield}, nullptr);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
    EXPECT_TRUE(memoryManager.allocationInDevicePoolCreated);

    memoryManager.freeGraphicsMemory(allocation);
}

HWTEST_F(MemoryManagerTests, givenEnabledLocalMemoryWhenLinearStreamIsAllocatedInDevicePoolThenLocalMemoryPoolIsUsed) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::LINEAR_STREAM, mockDeviceBitfield}, nullptr);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
    EXPECT_TRUE(memoryManager.allocationInDevicePoolCreated);

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(StorageInfoTest, whenMemoryBanksAreSetToZeroThenOneHandleIsReturned) {
    StorageInfo storageInfo;
    storageInfo.memoryBanks = 0u;
    EXPECT_EQ(1u, storageInfo.getNumBanks());
}

TEST(StorageInfoTest, whenMemoryBanksAreNotSetToZeroThenNumberOfEnabledBitsIsReturnedAsNumberOfHandles) {
    StorageInfo storageInfo;
    storageInfo.memoryBanks = 0b001;
    EXPECT_EQ(1u, storageInfo.getNumBanks());
    storageInfo.memoryBanks = 0b010;
    EXPECT_EQ(1u, storageInfo.getNumBanks());
    storageInfo.memoryBanks = 0b100;
    EXPECT_EQ(1u, storageInfo.getNumBanks());
    storageInfo.memoryBanks = 0b011;
    EXPECT_EQ(2u, storageInfo.getNumBanks());
    storageInfo.memoryBanks = 0b101;
    EXPECT_EQ(2u, storageInfo.getNumBanks());
    storageInfo.memoryBanks = 0b110;
    EXPECT_EQ(2u, storageInfo.getNumBanks());
    storageInfo.memoryBanks = 0b111;
    EXPECT_EQ(3u, storageInfo.getNumBanks());
}

TEST(MemoryManagerTest, givenNotSetUseSystemMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedThenlocalMemoryUsageIsUpdated) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());

    MockMemoryManager memoryManager(false, true, executionEnvironment);

    AllocationProperties allocProperties(mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::BUFFER, mockDeviceBitfield);
    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool(allocProperties, nullptr);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
    EXPECT_EQ(allocation->getUnderlyingBufferSize(), memoryManager.getLocalMemoryUsageBankSelector(allocation->getAllocationType(), allocation->getRootDeviceIndex())->getOccupiedMemorySizeForBank(0));

    memoryManager.freeGraphicsMemory(allocation);
    EXPECT_EQ(0u, memoryManager.getLocalMemoryUsageBankSelector(AllocationType::EXTERNAL_HOST_PTR, mockRootDeviceIndex)->getOccupiedMemorySizeForBank(0));
}

TEST(MemoryManagerTest, givenSetUseSystemMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedThenlocalMemoryUsageIsNotUpdated) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    AllocationProperties allocProperties(mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::EXTERNAL_HOST_PTR, mockDeviceBitfield);
    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool(allocProperties, nullptr);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(0u, memoryManager.getLocalMemoryUsageBankSelector(allocation->getAllocationType(), allocation->getRootDeviceIndex())->getOccupiedMemorySizeForBank(0));

    memoryManager.freeGraphicsMemory(allocation);
    EXPECT_EQ(0u, memoryManager.getLocalMemoryUsageBankSelector(AllocationType::EXTERNAL_HOST_PTR, mockRootDeviceIndex)->getOccupiedMemorySizeForBank(0));
}

TEST(MemoryManagerTest, givenInternalAllocationTypeWhenIsAllocatedInDevicePoolThenIntenalUsageBankSelectorIsUpdated) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    AllocationProperties allocProperties(mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::SEMAPHORE_BUFFER, mockDeviceBitfield);
    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool(allocProperties, nullptr);

    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(0u, memoryManager.externalLocalMemoryUsageBankSelector[allocation->getRootDeviceIndex()]->getOccupiedMemorySizeForBank(0));

    if (allocation->getMemoryPool() == MemoryPool::LocalMemory) {
        EXPECT_EQ(allocation->getUnderlyingBufferSize(), memoryManager.internalLocalMemoryUsageBankSelector[allocation->getRootDeviceIndex()]->getOccupiedMemorySizeForBank(0));
        EXPECT_EQ(memoryManager.getLocalMemoryUsageBankSelector(allocation->getAllocationType(), allocation->getRootDeviceIndex()), memoryManager.internalLocalMemoryUsageBankSelector[allocation->getRootDeviceIndex()].get());
    }

    memoryManager.freeGraphicsMemory(allocation);

    EXPECT_EQ(0u, memoryManager.externalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->getOccupiedMemorySizeForBank(0));
    EXPECT_EQ(0u, memoryManager.internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->getOccupiedMemorySizeForBank(0));
}

TEST(MemoryManagerTest, givenExternalAllocationTypeWhenIsAllocatedInDevicePoolThenIntenalUsageBankSelectorIsUpdated) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    AllocationProperties allocProperties(mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::BUFFER, mockDeviceBitfield);
    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool(allocProperties, nullptr);

    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(0u, memoryManager.internalLocalMemoryUsageBankSelector[allocation->getRootDeviceIndex()]->getOccupiedMemorySizeForBank(0));

    if (allocation->getMemoryPool() == MemoryPool::LocalMemory) {
        EXPECT_EQ(allocation->getUnderlyingBufferSize(), memoryManager.externalLocalMemoryUsageBankSelector[allocation->getRootDeviceIndex()]->getOccupiedMemorySizeForBank(0));
        EXPECT_EQ(memoryManager.getLocalMemoryUsageBankSelector(allocation->getAllocationType(), allocation->getRootDeviceIndex()), memoryManager.externalLocalMemoryUsageBankSelector[allocation->getRootDeviceIndex()].get());
    }

    memoryManager.freeGraphicsMemory(allocation);

    EXPECT_EQ(0u, memoryManager.externalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->getOccupiedMemorySizeForBank(0));
    EXPECT_EQ(0u, memoryManager.internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->getOccupiedMemorySizeForBank(0));
}

struct MemoryManagerDirectSubmissionImplicitScalingTest : public ::testing::Test {

    void SetUp() override {
        DebugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
        executionEnvironment = std::make_unique<MockExecutionEnvironment>(defaultHwInfo.get());
        auto allTilesMask = executionEnvironment->rootDeviceEnvironments[mockRootDeviceIndex]->deviceAffinityMask.getGenericSubDevicesMask();

        allocationProperties = std::make_unique<AllocationProperties>(mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::UNKNOWN, allTilesMask);
        allocationProperties->flags.multiOsContextCapable = true;

        constexpr auto enableLocalMemory = true;
        memoryManager = std::make_unique<MockMemoryManager>(false, enableLocalMemory, *executionEnvironment);

        memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->reserveOnBanks(1u, MemoryConstants::pageSize2Mb);
        EXPECT_NE(0u, memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->getLeastOccupiedBank(allTilesMask));
    }

    DebugManagerStateRestore restorer{};

    constexpr static DeviceBitfield firstTileMask{1u};
    constexpr static auto numSubDevices = 2u;
    std::unique_ptr<MockExecutionEnvironment> executionEnvironment{};
    std::unique_ptr<AllocationProperties> allocationProperties{};
    std::unique_ptr<MockMemoryManager> memoryManager{};
};

HWTEST2_F(MemoryManagerDirectSubmissionImplicitScalingTest, givenCommandBufferTypeWhenIsAllocatedForMultiOsContextThenMemoryIsPlacedOnFirstAvailableMemoryBankInLocalMemory, IsXeHpcCore) {

    allocationProperties->allocationType = AllocationType::COMMAND_BUFFER;
    auto allocation = memoryManager->allocateGraphicsMemoryInPreferredPool(*allocationProperties, nullptr);

    EXPECT_NE(nullptr, allocation);

    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
    EXPECT_EQ(firstTileMask, allocation->storageInfo.getMemoryBanks());
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST2_F(MemoryManagerDirectSubmissionImplicitScalingTest, givenRingBufferTypeWhenIsAllocatedForMultiOsContextThenMemoryIsPlacedOnFirstAvailableMemoryBankInLocalMemory, IsXeHpcCore) {

    allocationProperties->allocationType = AllocationType::RING_BUFFER;
    auto allocation = memoryManager->allocateGraphicsMemoryInPreferredPool(*allocationProperties, nullptr);

    EXPECT_NE(nullptr, allocation);

    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
    EXPECT_EQ(firstTileMask, allocation->storageInfo.getMemoryBanks());
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST2_F(MemoryManagerDirectSubmissionImplicitScalingTest, givenSemaphoreBufferTypeWhenIsAllocatedForMultiOsContextThenMemoryIsPlacedOnFirstAvailableMemoryBankInLocalMemory, IsXeHpcCore) {

    allocationProperties->allocationType = AllocationType::SEMAPHORE_BUFFER;
    auto allocation = memoryManager->allocateGraphicsMemoryInPreferredPool(*allocationProperties, nullptr);

    EXPECT_NE(nullptr, allocation);

    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
    EXPECT_EQ(firstTileMask, allocation->storageInfo.getMemoryBanks());
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST2_F(MemoryManagerDirectSubmissionImplicitScalingTest, givenDirectSubmissionForceLocalMemoryStorageDisabledWhenAllocatingMemoryForRingOrSemaphoreBufferThenAllocateInSystemMemory, IsXeHpcCore) {
    DebugManager.flags.DirectSubmissionForceLocalMemoryStorageMode.set(0);
    for (auto &multiTile : ::testing::Bool()) {
        for (auto &allocationType : {AllocationType::RING_BUFFER, AllocationType::SEMAPHORE_BUFFER}) {
            allocationProperties->allocationType = allocationType;
            allocationProperties->flags.multiOsContextCapable = multiTile;
            auto allocation = memoryManager->allocateGraphicsMemoryInPreferredPool(*allocationProperties, nullptr);

            EXPECT_NE(nullptr, allocation);

            EXPECT_NE(MemoryPool::LocalMemory, allocation->getMemoryPool());
            EXPECT_NE(firstTileMask, allocation->storageInfo.getMemoryBanks());

            memoryManager->freeGraphicsMemory(allocation);
        }
    }
}

HWTEST2_F(MemoryManagerDirectSubmissionImplicitScalingTest, givenDirectSubmissionForceLocalMemoryStorageEnabledForMultiTileEsWhenAllocatingMemoryForCommandOrRingOrSemaphoreBufferThenFirstBankIsSelectedOnlyForMultiTileEngines, IsXeHpcCore) {
    DebugManager.flags.DirectSubmissionForceLocalMemoryStorageMode.set(1);
    for (auto &multiTile : ::testing::Bool()) {
        for (auto &allocationType : {AllocationType::COMMAND_BUFFER, AllocationType::RING_BUFFER, AllocationType::SEMAPHORE_BUFFER}) {
            allocationProperties->allocationType = allocationType;
            allocationProperties->flags.multiOsContextCapable = multiTile;
            auto allocation = memoryManager->allocateGraphicsMemoryInPreferredPool(*allocationProperties, nullptr);

            EXPECT_NE(nullptr, allocation);

            if (multiTile) {
                EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
                EXPECT_EQ(firstTileMask, allocation->storageInfo.getMemoryBanks());
            } else {
                if (allocationType != AllocationType::COMMAND_BUFFER) {
                    EXPECT_NE(firstTileMask, allocation->storageInfo.getMemoryBanks());
                    EXPECT_NE(firstTileMask, allocation->storageInfo.getMemoryBanks());
                }
            }
            memoryManager->freeGraphicsMemory(allocation);
        }
    }
}

HWTEST2_F(MemoryManagerDirectSubmissionImplicitScalingTest, givenDirectSubmissionForceLocalMemoryStorageEnabledForAllEnginesWhenAllocatingMemoryForCommandOrRingOrSemaphoreBufferThenFirstBankIsSelected, IsXeHpcCore) {
    DebugManager.flags.DirectSubmissionForceLocalMemoryStorageMode.set(2);
    for (auto &multiTile : ::testing::Bool()) {
        for (auto &allocationType : {AllocationType::COMMAND_BUFFER, AllocationType::RING_BUFFER, AllocationType::SEMAPHORE_BUFFER}) {
            allocationProperties->allocationType = allocationType;
            allocationProperties->flags.multiOsContextCapable = multiTile;
            auto allocation = memoryManager->allocateGraphicsMemoryInPreferredPool(*allocationProperties, nullptr);

            EXPECT_NE(nullptr, allocation);

            EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
            EXPECT_EQ(firstTileMask, allocation->storageInfo.getMemoryBanks());

            memoryManager->freeGraphicsMemory(allocation);
        }
    }
}