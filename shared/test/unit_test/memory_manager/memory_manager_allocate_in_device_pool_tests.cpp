/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_helper.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/memory_properties_helpers.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/memory_manager/local_memory_usage.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gfx_partition.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

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
    auto allocationProperties = MemoryPropertiesHelper::getAllocationProperties(0, memoryProperties, true, 0, AllocationType::buffer, false, hwInfo, {}, true);
    EXPECT_TRUE(allocationProperties.flags.allocateMemory);

    auto allocationProperties2 = MemoryPropertiesHelper::getAllocationProperties(0, memoryProperties, false, 0, AllocationType::buffer, false, hwInfo, {}, true);
    EXPECT_FALSE(allocationProperties2.flags.allocateMemory);
}

TEST(UncacheableFlagsTest, givenUncachedResourceFlagWhenGetAllocationFlagsIsCalledThenUncacheableFlagIsCorrectlySet) {
    UltDeviceFactory deviceFactory{1, 0};
    auto pDevice = deviceFactory.rootDevices[0];

    MemoryProperties memoryProperties{};
    memoryProperties.pDevice = pDevice;
    memoryProperties.flags.locallyUncachedResource = true;
    auto allocationFlags = MemoryPropertiesHelper::getAllocationProperties(
        0, memoryProperties, false, 0, AllocationType::buffer, false, pDevice->getHardwareInfo(), {}, true);
    EXPECT_TRUE(allocationFlags.flags.uncacheable);

    memoryProperties.flags.locallyUncachedResource = false;
    auto allocationFlags2 = MemoryPropertiesHelper::getAllocationProperties(
        0, memoryProperties, false, 0, AllocationType::buffer, false, pDevice->getHardwareInfo(), {}, true);
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
            0, memoryProperties, true, 0, AllocationType::buffer, false, pDevice->getHardwareInfo(), {}, false);
    EXPECT_FALSE(allocationFlags.flags.flushL3RequiredForRead);
    EXPECT_FALSE(allocationFlags.flags.flushL3RequiredForWrite);

    memoryProperties.flags.readOnly = false;
    auto allocationFlags2 = MemoryPropertiesHelper::getAllocationProperties(
        0, memoryProperties, true, 0, AllocationType::buffer, false, pDevice->getHardwareInfo(), {}, false);
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
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());

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
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(allocation);
}

HWTEST_F(MemoryManagerTests, givenDefaultHwInfoWhenAllocatingDebugAreaThenHeapInternalFrontWindowIsUsed) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MemoryManagerCreate<OsAgnosticMemoryManager> osAgnosticMemoryManager(false, true, executionEnvironment);

    NEO::AllocationProperties properties{0, true, MemoryConstants::pageSize64k,
                                         NEO::AllocationType::debugModuleArea,
                                         false,
                                         mockDeviceBitfield};
    properties.flags.use32BitFrontWindow = true;

    auto &gfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    auto systemMemoryPlacement = gfxCoreHelper.useSystemMemoryPlacementForISA(*defaultHwInfo);

    HeapIndex expectedHeap = HeapIndex::totalHeaps;
    HeapIndex baseHeap = HeapIndex::totalHeaps;
    if (systemMemoryPlacement) {
        expectedHeap = HeapIndex::heapInternalFrontWindow;
        baseHeap = HeapIndex::heapInternal;
    } else {
        expectedHeap = HeapIndex::heapInternalDeviceFrontWindow;
        baseHeap = HeapIndex::heapInternalDeviceMemory;
    }
    auto moduleDebugArea = osAgnosticMemoryManager.allocateGraphicsMemoryWithProperties(properties);
    auto gpuAddress = moduleDebugArea->getGpuAddress();
    auto gmmHelper = osAgnosticMemoryManager.getGmmHelper(moduleDebugArea->getRootDeviceIndex());

    EXPECT_LE(gmmHelper->canonize(osAgnosticMemoryManager.getGfxPartition(0)->getHeapBase(expectedHeap)), gpuAddress);
    EXPECT_GT(gmmHelper->canonize(osAgnosticMemoryManager.getGfxPartition(0)->getHeapLimit(expectedHeap)), gpuAddress);
    EXPECT_EQ(gmmHelper->canonize(osAgnosticMemoryManager.getGfxPartition(0)->getHeapBase(expectedHeap)), moduleDebugArea->getGpuBaseAddress());
    EXPECT_EQ(gmmHelper->canonize(osAgnosticMemoryManager.getGfxPartition(0)->getHeapBase(baseHeap)), moduleDebugArea->getGpuBaseAddress());

    osAgnosticMemoryManager.freeGraphicsMemory(moduleDebugArea);
}

HWTEST2_F(MemoryManagerTests, givenEnabledLocalMemoryWhenAllocatingDebugAreaThenHeapInternalDeviceFrontWindowIsUsed, MatchAny) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrLocalMemory = true;

    MockExecutionEnvironment executionEnvironment(&hwInfo);
    MemoryManagerCreate<OsAgnosticMemoryManager> osAgnosticMemoryManager(false, true, executionEnvironment);

    NEO::AllocationProperties properties{0, true, MemoryConstants::pageSize64k,
                                         NEO::AllocationType::debugModuleArea,
                                         false,
                                         mockDeviceBitfield};
    properties.flags.use32BitFrontWindow = true;

    auto &gfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    auto systemMemoryPlacement = gfxCoreHelper.useSystemMemoryPlacementForISA(hwInfo);
    EXPECT_FALSE(systemMemoryPlacement);

    HeapIndex expectedHeap = HeapIndex::totalHeaps;
    HeapIndex baseHeap = HeapIndex::totalHeaps;

    expectedHeap = HeapIndex::heapInternalDeviceFrontWindow;
    baseHeap = HeapIndex::heapInternalDeviceMemory;

    auto moduleDebugArea = osAgnosticMemoryManager.allocateGraphicsMemoryWithProperties(properties);
    auto gpuAddress = moduleDebugArea->getGpuAddress();
    auto gmmHelper = osAgnosticMemoryManager.getGmmHelper(moduleDebugArea->getRootDeviceIndex());

    EXPECT_LE(gmmHelper->canonize(osAgnosticMemoryManager.getGfxPartition(0)->getHeapBase(expectedHeap)), gpuAddress);
    EXPECT_GT(gmmHelper->canonize(osAgnosticMemoryManager.getGfxPartition(0)->getHeapLimit(expectedHeap)), gpuAddress);
    EXPECT_EQ(gmmHelper->canonize(osAgnosticMemoryManager.getGfxPartition(0)->getHeapBase(expectedHeap)), moduleDebugArea->getGpuBaseAddress());
    EXPECT_EQ(gmmHelper->canonize(osAgnosticMemoryManager.getGfxPartition(0)->getHeapBase(baseHeap)), moduleDebugArea->getGpuBaseAddress());

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
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(BaseMemoryManagerTest, givenDebugVariableSetWhenCompressedBufferIsCreatedThenCreateCompressedGmm) {
    DebugManagerStateRestore restore;
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, true, executionEnvironment);

    AllocationProperties allocPropertiesBuffer(mockRootDeviceIndex, 1, AllocationType::buffer, mockDeviceBitfield);
    AllocationProperties allocPropertiesBufferCompressed(mockRootDeviceIndex, 1, AllocationType::buffer, mockDeviceBitfield);
    allocPropertiesBufferCompressed.flags.preferCompressed = true;

    debugManager.flags.RenderCompressedBuffersEnabled.set(1);
    auto allocationBuffer = memoryManager.allocateGraphicsMemoryInPreferredPool(allocPropertiesBuffer, nullptr);
    auto allocationBufferCompressed = memoryManager.allocateGraphicsMemoryInPreferredPool(allocPropertiesBufferCompressed, nullptr);
    EXPECT_EQ(nullptr, allocationBuffer->getDefaultGmm());
    EXPECT_NE(nullptr, allocationBufferCompressed->getDefaultGmm());
    EXPECT_TRUE(allocationBufferCompressed->getDefaultGmm()->isCompressionEnabled());
    memoryManager.freeGraphicsMemory(allocationBuffer);
    memoryManager.freeGraphicsMemory(allocationBufferCompressed);

    debugManager.flags.RenderCompressedBuffersEnabled.set(0);
    allocationBuffer = memoryManager.allocateGraphicsMemoryInPreferredPool(allocPropertiesBuffer, nullptr);
    allocationBufferCompressed = memoryManager.allocateGraphicsMemoryInPreferredPool(allocPropertiesBufferCompressed, nullptr);
    EXPECT_EQ(nullptr, allocationBuffer->getDefaultGmm());
    EXPECT_EQ(nullptr, allocationBufferCompressed->getDefaultGmm());
    memoryManager.freeGraphicsMemory(allocationBuffer);
    memoryManager.freeGraphicsMemory(allocationBufferCompressed);
}

TEST(BaseMemoryManagerTest, givenCalltoAllocatePhysicalGraphicsDeviceMemoryThenPhysicalAllocationReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, true, executionEnvironment);

    AllocationProperties allocPropertiesBuffer(mockRootDeviceIndex, 1, AllocationType::buffer, mockDeviceBitfield);
    allocPropertiesBuffer.flags.isUSMDeviceAllocation = true;

    auto allocationBuffer = memoryManager.allocatePhysicalGraphicsMemory(allocPropertiesBuffer);
    EXPECT_NE(nullptr, allocationBuffer);
    memoryManager.freeGraphicsMemory(allocationBuffer);
}

TEST(BaseMemoryManagerTest, givenCalltoAllocatePhysicalGraphicsHostMemoryThenPhysicalAllocationReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, true, executionEnvironment);

    AllocationProperties allocPropertiesBuffer(mockRootDeviceIndex, 1, AllocationType::buffer, mockDeviceBitfield);
    allocPropertiesBuffer.flags.isUSMDeviceAllocation = false;

    auto allocationBuffer = memoryManager.allocatePhysicalGraphicsMemory(allocPropertiesBuffer);
    EXPECT_NE(nullptr, allocationBuffer);
    memoryManager.freeGraphicsMemory(allocationBuffer);
}

class MockMemoryManagerLocalMemory : public OsAgnosticMemoryManager {
  public:
    using OsAgnosticMemoryManager::localMemorySupported;
    using OsAgnosticMemoryManager::mapPhysicalDeviceMemoryToVirtualMemory;
    using OsAgnosticMemoryManager::unMapPhysicalDeviceMemoryFromVirtualMemory;
    MockMemoryManagerLocalMemory(ExecutionEnvironment &executionEnvironment) : OsAgnosticMemoryManager(executionEnvironment) {}
};

TEST(BaseMemoryManagerTest, givenCalltoAllocatePhysicalGraphicsMemoryWithoutLocalMemoryThenPhysicalAllocationReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MemoryManagerCreate<MockMemoryManagerLocalMemory> memoryManager(false, true, executionEnvironment);
    memoryManager.localMemorySupported[0] = 0;

    AllocationProperties allocPropertiesBuffer(mockRootDeviceIndex, 1, AllocationType::buffer, mockDeviceBitfield);
    allocPropertiesBuffer.flags.isUSMDeviceAllocation = true;

    auto allocationBuffer = memoryManager.allocatePhysicalGraphicsMemory(allocPropertiesBuffer);
    EXPECT_NE(nullptr, allocationBuffer);
    memoryManager.freeGraphicsMemory(allocationBuffer);
}

class MockMemoryManagerNoLocalMemoryFail : public OsAgnosticMemoryManager {
  public:
    using OsAgnosticMemoryManager::localMemorySupported;
    MockMemoryManagerNoLocalMemoryFail(ExecutionEnvironment &executionEnvironment) : OsAgnosticMemoryManager(executionEnvironment) {}
    void *allocateSystemMemory(size_t size, size_t alignment) override {
        return nullptr;
    }
};

TEST(BaseMemoryManagerTest, givenCalltoAllocatePhysicalGraphicsDeviceMemoryWithoutLocalMemoryThenNullptrReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MemoryManagerCreate<MockMemoryManagerNoLocalMemoryFail> memoryManager(false, true, executionEnvironment);
    memoryManager.localMemorySupported[0] = 0;

    AllocationProperties allocPropertiesBuffer(mockRootDeviceIndex, 1, AllocationType::buffer, mockDeviceBitfield);
    allocPropertiesBuffer.flags.isUSMDeviceAllocation = true;

    auto allocationBuffer = memoryManager.allocatePhysicalGraphicsMemory(allocPropertiesBuffer);
    EXPECT_EQ(nullptr, allocationBuffer);
}

TEST(BaseMemoryManagerTest, givenCalltoAllocatePhysicalGraphicsDeviceMemoryWithLocalMemoryThenNullptrReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MemoryManagerCreate<MockMemoryManagerNoLocalMemoryFail> memoryManager(false, true, executionEnvironment);
    memoryManager.localMemorySupported[0] = 1;

    AllocationProperties allocPropertiesBuffer(mockRootDeviceIndex, 1, AllocationType::buffer, mockDeviceBitfield);
    allocPropertiesBuffer.flags.isUSMDeviceAllocation = true;

    auto allocationBuffer = memoryManager.allocatePhysicalGraphicsMemory(allocPropertiesBuffer);
    EXPECT_EQ(nullptr, allocationBuffer);
}

TEST(BaseMemoryManagerTest, givenCalltoAllocatePhysicalGraphicsHostMemoryThenNullptrReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MemoryManagerCreate<MockMemoryManagerNoLocalMemoryFail> memoryManager(false, true, executionEnvironment);

    AllocationProperties allocPropertiesBuffer(mockRootDeviceIndex, 1, AllocationType::bufferHostMemory, mockDeviceBitfield);
    allocPropertiesBuffer.flags.isUSMHostAllocation = true;

    auto allocationBuffer = memoryManager.allocatePhysicalGraphicsMemory(allocPropertiesBuffer);
    EXPECT_EQ(nullptr, allocationBuffer);
}

class MockAgnosticMemoryManager : public OsAgnosticMemoryManager {
  public:
    using OsAgnosticMemoryManager::mapPhysicalDeviceMemoryToVirtualMemory;
    using OsAgnosticMemoryManager::unMapPhysicalDeviceMemoryFromVirtualMemory;
    MockAgnosticMemoryManager(ExecutionEnvironment &executionEnvironment) : OsAgnosticMemoryManager(executionEnvironment) {}
};

TEST(BaseMemoryManagerTest, givenCalltoMapAndUnMapThenVirtialAddressSetUnSetOnPhysicalMemoryReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MemoryManagerCreate<MockAgnosticMemoryManager> memoryManager(false, true, executionEnvironment);

    AllocationProperties allocPropertiesBuffer(mockRootDeviceIndex, 1, AllocationType::buffer, mockDeviceBitfield);
    allocPropertiesBuffer.flags.isUSMDeviceAllocation = true;

    auto allocationBuffer = memoryManager.allocatePhysicalGraphicsMemory(allocPropertiesBuffer);
    EXPECT_NE(nullptr, allocationBuffer);
    uint64_t gpuAddress = 0x1234;
    size_t size = 4096;
    EXPECT_TRUE(memoryManager.mapPhysicalDeviceMemoryToVirtualMemory(allocationBuffer, gpuAddress, size));
    EXPECT_EQ(gpuAddress, allocationBuffer->getGpuAddress());

    EXPECT_TRUE(memoryManager.unMapPhysicalDeviceMemoryFromVirtualMemory(allocationBuffer, gpuAddress, size, nullptr, 0u));
    EXPECT_NE(gpuAddress, allocationBuffer->getGpuAddress());
    memoryManager.freeGraphicsMemory(allocationBuffer);
}

class FailingMemoryManager : public OsAgnosticMemoryManager {
  public:
    using OsAgnosticMemoryManager::localMemorySupported;
    FailingMemoryManager(ExecutionEnvironment &executionEnvironment) : OsAgnosticMemoryManager(executionEnvironment) {}

    GraphicsAllocation *allocatePhysicalLocalDeviceMemory(const AllocationData &allocationData, AllocationStatus &status) override {
        if (failAllocate) {
            return nullptr;
        }
        return OsAgnosticMemoryManager::allocatePhysicalLocalDeviceMemory(allocationData, status);
    };
    AllocationStatus registerLocalMemAlloc(GraphicsAllocation *allocation, uint32_t rootDeviceIndex) override { return AllocationStatus::Error; };

    bool failAllocate = false;
};

TEST(BaseMemoryManagerTest, givenCalltoAllocatePhysicalGraphicsMemoryWithFailedRegisterLocalMemAllocThenNullptrReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MemoryManagerCreate<FailingMemoryManager> memoryManager(false, true, executionEnvironment);
    memoryManager.localMemorySupported[0] = 1;

    AllocationProperties allocPropertiesBuffer(mockRootDeviceIndex, 1, AllocationType::buffer, mockDeviceBitfield);
    allocPropertiesBuffer.flags.isUSMDeviceAllocation = true;

    auto allocationBuffer = memoryManager.allocatePhysicalGraphicsMemory(allocPropertiesBuffer);
    EXPECT_EQ(nullptr, allocationBuffer);
}

TEST(BaseMemoryManagerTest, givenCalltoAllocatePhysicalGraphicsMemoryWithFailedLocalMemAllocThenNullptrReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MemoryManagerCreate<FailingMemoryManager> memoryManager(false, true, executionEnvironment);
    memoryManager.localMemorySupported[0] = 1;
    memoryManager.failAllocate = true;

    AllocationProperties allocPropertiesBuffer(mockRootDeviceIndex, 1, AllocationType::buffer, mockDeviceBitfield);
    allocPropertiesBuffer.flags.isUSMDeviceAllocation = true;

    auto allocationBuffer = memoryManager.allocatePhysicalGraphicsMemory(allocPropertiesBuffer);
    EXPECT_EQ(nullptr, allocationBuffer);
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
    allocData.type = AllocationType::svmGpu;
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
    allocData.type = AllocationType::svmGpu;
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
    auto allocationFlags = MemoryPropertiesHelper::getAllocationProperties(0, memoryProperties, true, 0, AllocationType::buffer, false, pDevice->getHardwareInfo(), {}, false);
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
        0, memoryProperties, true, 0, AllocationType::buffer, false, pDevice->getHardwareInfo(), {}, false);
    EXPECT_FALSE(allocationFlags.flags.flushL3RequiredForRead);
    EXPECT_FALSE(allocationFlags.flags.flushL3RequiredForWrite);
}

TEST(MemoryAllocationTest, givenAubDumpForceAllToLocalMemoryWhenMemoryAllocationIsCreatedThenItHasLocalMemoryPool) {
    DebugManagerStateRestore debugRestorer;
    debugManager.flags.AUBDumpForceAllToLocalMemory.set(true);

    MemoryAllocation allocation(mockRootDeviceIndex, 1u /*num gmms*/, AllocationType::unknown, nullptr, reinterpret_cast<void *>(0x1000), 0x1000,
                                0x1000, 0, MemoryPool::system4KBPages, false, false, MemoryManager::maxOsContextCount);
    EXPECT_EQ(MemoryPool::localMemory, allocation.getMemoryPool());
}

TEST(MemoryAllocationTest, givenAubDumpForceAllToLocalMemoryWhenMemoryAllocationIsOverriddenThenItHasLocalMemoryPool) {
    DebugManagerStateRestore debugRestorer;
    debugManager.flags.AUBDumpForceAllToLocalMemory.set(true);

    MemoryAllocation allocation(mockRootDeviceIndex, 1u /*num gmms*/, AllocationType::unknown, nullptr, reinterpret_cast<void *>(0x1000), 0x1000,
                                0x1000, 0, MemoryPool::system4KBPages, false, false, MemoryManager::maxOsContextCount);
    allocation.overrideMemoryPool(MemoryPool::system64KBPages);
    EXPECT_EQ(MemoryPool::localMemory, allocation.getMemoryPool());
}

TEST(MemoryManagerTest, givenDisabledLocalMemoryWhenAllocateInternalAllocationInDevicePoolThen32BitAllocationIsCreatedInNonDevicePool) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);

    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::internalHeap, mockDeviceBitfield}, nullptr);
    EXPECT_NE(nullptr, allocation);
    EXPECT_TRUE(allocation->is32BitAllocation());
    EXPECT_NE(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_FALSE(memoryManager.allocationInDevicePoolCreated);
    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenOsAgnosticMemoryManagerWhenGetLocalMemoryIsCalledThenSizeOfLocalMemoryIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);

    auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo();
    auto releaseHelper = executionEnvironment.rootDeviceEnvironments[0]->getReleaseHelper();

    auto subDevicesCount = GfxCoreHelper::getSubDevicesCount(hwInfo);
    uint32_t deviceMask = static_cast<uint32_t>(maxNBitValue(subDevicesCount));

    EXPECT_EQ(AubHelper::getPerTileLocalMemorySize(hwInfo, releaseHelper) * subDevicesCount, memoryManager.getLocalMemorySize(0u, deviceMask));
}

HWTEST_F(MemoryManagerTests, givenEnabledLocalMemoryWhenAllocatingKernelIsaThenLocalMemoryPoolIsUsed) {
    auto hwInfo = *defaultHwInfo;
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(true);
    hwInfo.featureTable.flags.ftrLocalMemory = true;

    MockExecutionEnvironment executionEnvironment(&hwInfo);
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::kernelIsa, mockDeviceBitfield}, nullptr);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_TRUE(memoryManager.allocationInDevicePoolCreated);

    memoryManager.freeGraphicsMemory(allocation);

    allocation = memoryManager.allocateGraphicsMemoryInPreferredPool({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::kernelIsaInternal, mockDeviceBitfield}, nullptr);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_TRUE(memoryManager.allocationInDevicePoolCreated);

    memoryManager.freeGraphicsMemory(allocation);
}

HWTEST_F(MemoryManagerTests, givenEnabledLocalMemoryWhenAllocateKernelIsaInDevicePoolThenLocalMemoryPoolIsUsed) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::internalHeap, mockDeviceBitfield}, nullptr);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_TRUE(memoryManager.allocationInDevicePoolCreated);

    memoryManager.freeGraphicsMemory(allocation);
}

HWTEST_F(MemoryManagerTests, givenEnabledLocalMemoryWhenLinearStreamIsAllocatedInDevicePoolThenLocalMemoryPoolIsUsed) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::linearStream, mockDeviceBitfield}, nullptr);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
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

    AllocationProperties allocProperties(mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield);
    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool(allocProperties, nullptr);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_EQ(allocation->getUnderlyingBufferSize(), memoryManager.getLocalMemoryUsageBankSelector(allocation->getAllocationType(), allocation->getRootDeviceIndex())->getOccupiedMemorySizeForBank(0));

    memoryManager.freeGraphicsMemory(allocation);
    EXPECT_EQ(0u, memoryManager.getLocalMemoryUsageBankSelector(AllocationType::externalHostPtr, mockRootDeviceIndex)->getOccupiedMemorySizeForBank(0));
}

TEST(MemoryManagerTest, givenSetUseSystemMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedThenlocalMemoryUsageIsNotUpdated) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    AllocationProperties allocProperties(mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::externalHostPtr, mockDeviceBitfield);
    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool(allocProperties, nullptr);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(0u, memoryManager.getLocalMemoryUsageBankSelector(allocation->getAllocationType(), allocation->getRootDeviceIndex())->getOccupiedMemorySizeForBank(0));

    memoryManager.freeGraphicsMemory(allocation);
    EXPECT_EQ(0u, memoryManager.getLocalMemoryUsageBankSelector(AllocationType::externalHostPtr, mockRootDeviceIndex)->getOccupiedMemorySizeForBank(0));
}

TEST(MemoryManagerTest, givenInternalAllocationTypeWhenIsAllocatedInDevicePoolThenIntenalUsageBankSelectorIsUpdated) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    AllocationProperties allocProperties(mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::semaphoreBuffer, mockDeviceBitfield);
    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool(allocProperties, nullptr);

    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(0u, memoryManager.externalLocalMemoryUsageBankSelector[allocation->getRootDeviceIndex()]->getOccupiedMemorySizeForBank(0));

    if (allocation->getMemoryPool() == MemoryPool::localMemory) {
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

    AllocationProperties allocProperties(mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield);
    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool(allocProperties, nullptr);

    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(0u, memoryManager.internalLocalMemoryUsageBankSelector[allocation->getRootDeviceIndex()]->getOccupiedMemorySizeForBank(0));

    if (allocation->getMemoryPool() == MemoryPool::localMemory) {
        EXPECT_EQ(allocation->getUnderlyingBufferSize(), memoryManager.externalLocalMemoryUsageBankSelector[allocation->getRootDeviceIndex()]->getOccupiedMemorySizeForBank(0));
        EXPECT_EQ(memoryManager.getLocalMemoryUsageBankSelector(allocation->getAllocationType(), allocation->getRootDeviceIndex()), memoryManager.externalLocalMemoryUsageBankSelector[allocation->getRootDeviceIndex()].get());
    }

    memoryManager.freeGraphicsMemory(allocation);

    EXPECT_EQ(0u, memoryManager.externalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->getOccupiedMemorySizeForBank(0));
    EXPECT_EQ(0u, memoryManager.internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->getOccupiedMemorySizeForBank(0));
}

struct MemoryManagerDirectSubmissionImplicitScalingTest : public ::testing::Test {

    void SetUp() override {
        debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
        executionEnvironment = std::make_unique<MockExecutionEnvironment>(defaultHwInfo.get());
        auto allTilesMask = executionEnvironment->rootDeviceEnvironments[mockRootDeviceIndex]->deviceAffinityMask.getGenericSubDevicesMask();

        allocationProperties = std::make_unique<AllocationProperties>(mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::unknown, allTilesMask);
        allocationProperties->flags.multiOsContextCapable = true;

        constexpr auto enableLocalMemory = true;
        memoryManager = std::make_unique<MockMemoryManager>(false, enableLocalMemory, *executionEnvironment);

        memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->reserveOnBanks(1u, MemoryConstants::pageSize2M);
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

    allocationProperties->allocationType = AllocationType::commandBuffer;
    auto allocation = memoryManager->allocateGraphicsMemoryInPreferredPool(*allocationProperties, nullptr);

    EXPECT_NE(nullptr, allocation);

    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_EQ(firstTileMask, allocation->storageInfo.getMemoryBanks());
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST2_F(MemoryManagerDirectSubmissionImplicitScalingTest, givenRingBufferTypeWhenIsAllocatedForMultiOsContextThenMemoryIsPlacedOnFirstAvailableMemoryBankInLocalMemory, IsXeHpcCore) {

    allocationProperties->allocationType = AllocationType::ringBuffer;
    auto allocation = memoryManager->allocateGraphicsMemoryInPreferredPool(*allocationProperties, nullptr);

    EXPECT_NE(nullptr, allocation);

    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_EQ(firstTileMask, allocation->storageInfo.getMemoryBanks());
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST2_F(MemoryManagerDirectSubmissionImplicitScalingTest, givenSemaphoreBufferTypeWhenIsAllocatedForMultiOsContextThenMemoryIsPlacedOnFirstAvailableMemoryBankInLocalMemory, IsXeHpcCore) {

    allocationProperties->allocationType = AllocationType::semaphoreBuffer;
    auto allocation = memoryManager->allocateGraphicsMemoryInPreferredPool(*allocationProperties, nullptr);

    EXPECT_NE(nullptr, allocation);

    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_EQ(firstTileMask, allocation->storageInfo.getMemoryBanks());
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST2_F(MemoryManagerDirectSubmissionImplicitScalingTest, givenDirectSubmissionForceLocalMemoryStorageDisabledWhenAllocatingMemoryForRingOrSemaphoreBufferThenAllocateInSystemMemory, IsXeHpcCore) {
    debugManager.flags.DirectSubmissionForceLocalMemoryStorageMode.set(0);
    for (auto &multiTile : ::testing::Bool()) {
        for (auto &allocationType : {AllocationType::ringBuffer, AllocationType::semaphoreBuffer}) {
            allocationProperties->allocationType = allocationType;
            allocationProperties->flags.multiOsContextCapable = multiTile;
            auto allocation = memoryManager->allocateGraphicsMemoryInPreferredPool(*allocationProperties, nullptr);

            EXPECT_NE(nullptr, allocation);

            EXPECT_NE(MemoryPool::localMemory, allocation->getMemoryPool());
            EXPECT_NE(firstTileMask, allocation->storageInfo.getMemoryBanks());

            memoryManager->freeGraphicsMemory(allocation);
        }
    }
}

HWTEST2_F(MemoryManagerDirectSubmissionImplicitScalingTest, givenDirectSubmissionForceLocalMemoryStorageEnabledForMultiTileEsWhenAllocatingMemoryForCommandOrRingOrSemaphoreBufferThenFirstBankIsSelectedOnlyForMultiTileEngines, IsXeHpcCore) {
    debugManager.flags.DirectSubmissionForceLocalMemoryStorageMode.set(1);
    for (auto &multiTile : ::testing::Bool()) {
        for (auto &allocationType : {AllocationType::commandBuffer, AllocationType::ringBuffer, AllocationType::semaphoreBuffer}) {
            allocationProperties->allocationType = allocationType;
            allocationProperties->flags.multiOsContextCapable = multiTile;
            auto allocation = memoryManager->allocateGraphicsMemoryInPreferredPool(*allocationProperties, nullptr);

            EXPECT_NE(nullptr, allocation);

            if (multiTile) {
                EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
                EXPECT_EQ(firstTileMask, allocation->storageInfo.getMemoryBanks());
            } else if (allocationType != AllocationType::commandBuffer) {
                EXPECT_NE(firstTileMask, allocation->storageInfo.getMemoryBanks());
            }
            memoryManager->freeGraphicsMemory(allocation);
        }
    }
}

TEST(MemoryManagerTest, givenDebugVariableToToggleGpuVaBitsWhenAllocatingResourceInHeapExtendedThenSpecificBitIsToggled) {
    if (defaultHwInfo->capabilityTable.gpuAddressSpace < maxNBitValue(57)) {
        GTEST_SKIP();
    }
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(true, true, executionEnvironment);

    auto mockGfxPartition = std::make_unique<MockGfxPartition>();
    mockGfxPartition->initHeap(HeapIndex::heapExtended, maxNBitValue(56) + 1, MemoryConstants::teraByte, MemoryConstants::pageSize64k);
    memoryManager.gfxPartitions[0] = std::move(mockGfxPartition);

    DebugManagerStateRestore restorer;
    EXPECT_EQ(4u, static_cast<uint32_t>(AllocationType::constantSurface));
    EXPECT_EQ(7u, static_cast<uint32_t>(AllocationType::globalSurface));
    debugManager.flags.ToggleBitIn57GpuVa.set("4:55,7:32");

    auto status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.size = static_cast<size_t>(MemoryConstants::kiloByte);
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = mockRootDeviceIndex;

    {
        allocData.type = AllocationType::buffer;
        auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
        ASSERT_NE(nullptr, allocation);

        auto gpuVA = allocation->getGpuAddress();

        EXPECT_TRUE(isBitSet(gpuVA, 56));
        EXPECT_FALSE(isBitSet(gpuVA, 55));
        EXPECT_TRUE(isBitSet(gpuVA, 32));

        memoryManager.freeGraphicsMemory(allocation);
    }
    {
        allocData.type = AllocationType::constantSurface;
        auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
        ASSERT_NE(nullptr, allocation);

        auto gpuVA = allocation->getGpuAddress();

        EXPECT_TRUE(isBitSet(gpuVA, 56));
        EXPECT_TRUE(isBitSet(gpuVA, 55));
        EXPECT_TRUE(isBitSet(gpuVA, 32));

        memoryManager.freeGraphicsMemory(allocation);
    }

    {
        allocData.type = AllocationType::globalSurface;
        auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
        ASSERT_NE(nullptr, allocation);

        auto gpuVA = allocation->getGpuAddress();

        EXPECT_TRUE(isBitSet(gpuVA, 56));
        EXPECT_FALSE(isBitSet(gpuVA, 55));
        EXPECT_FALSE(isBitSet(gpuVA, 32));

        memoryManager.freeGraphicsMemory(allocation);
    }
}
