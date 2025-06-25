/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/windows/dxgi_wrapper.h"
#include "shared/source/os_interface/windows/wddm/um_km_data_translator.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"
#include "shared/source/utilities/heap_allocator.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/execution_environment_helper.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/libult/create_command_stream.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_deferred_deleter.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gfx_partition.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_client_context_base.h"
#include "shared/test/common/mocks/mock_gmm_page_table_mngr.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/mock_release_helper.h"
#include "shared/test/common/mocks/windows/mock_wddm_allocation.h"
#include "shared/test/common/os_interface/windows/mock_wddm_memory_manager.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include <memory>

using namespace NEO;

namespace NEO {
extern bool returnEmptyFilesVector;
}

class MockAllocateGraphicsMemoryWithAlignmentWddm : public MemoryManagerCreate<WddmMemoryManager> {
  public:
    using WddmMemoryManager::allocateGraphicsMemoryWithAlignment;
    using WddmMemoryManager::getPreferredAllocationMethod;

    MockAllocateGraphicsMemoryWithAlignmentWddm(ExecutionEnvironment &executionEnvironment) : MemoryManagerCreate(false, false, executionEnvironment) {}
    bool allocateSystemMemoryAndCreateGraphicsAllocationFromItCalled = false;
    bool allocateGraphicsMemoryUsingKmdAndMapItToCpuVACalled = false;
    bool callBaseAllocateGraphicsMemoryUsingKmdAndMapItToCpuVA = false;
    bool mapGpuVirtualAddressWithCpuPtr = false;

    GraphicsAllocation *allocateSystemMemoryAndCreateGraphicsAllocationFromIt(const AllocationData &allocationData) override {
        allocateSystemMemoryAndCreateGraphicsAllocationFromItCalled = true;

        return nullptr;
    }
    GraphicsAllocation *allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(const AllocationData &allocationData, bool allowLargePages) override {
        allocateGraphicsMemoryUsingKmdAndMapItToCpuVACalled = true;
        if (callBaseAllocateGraphicsMemoryUsingKmdAndMapItToCpuVA) {
            return WddmMemoryManager::allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(allocationData, allowLargePages);
        }

        return nullptr;
    }
    bool mapGpuVirtualAddress(WddmAllocation *graphicsAllocation, const void *requiredGpuPtr) override {
        if (requiredGpuPtr != nullptr) {
            mapGpuVirtualAddressWithCpuPtr = true;
        } else {
            mapGpuVirtualAddressWithCpuPtr = false;
        }

        return true;
    }
    size_t getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod allocationMethod) const override {
        return hugeGfxMemoryChunkSize;
    }
    size_t hugeGfxMemoryChunkSize = WddmMemoryManager::getHugeGfxMemoryChunkSize(preferredAllocationMethod);
};

class WddmMemoryManagerTests : public ::testing::Test {
  public:
    static constexpr uint32_t rootDeviceIndex{0U};

    std::unique_ptr<VariableBackup<bool>> returnEmptyFilesVectorBackup;

    MockAllocateGraphicsMemoryWithAlignmentWddm *memoryManager = nullptr;
    WddmMock *wddm = nullptr;
    ExecutionEnvironment *executionEnvironment = nullptr;

    void SetUp() override {
        returnEmptyFilesVectorBackup = std::make_unique<VariableBackup<bool>>(&NEO::returnEmptyFilesVector, true);
        HardwareInfo *hwInfo = nullptr;
        executionEnvironment = getExecutionEnvironmentImpl(hwInfo, 1);

        memoryManager = new MockAllocateGraphicsMemoryWithAlignmentWddm(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
        wddm = static_cast<WddmMock *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->as<Wddm>());
    }

    void TearDown() override {
        delete executionEnvironment;
    }
};

TEST_F(WddmMemoryManagerTests, GivenLocalMemoryAllocationModeReleaseKeyWhenWddmMemoryManagerConstructedThenUsmDeviceAllocationModeProperlySet) {
    DebugManagerStateRestore restorer;

    for (const int32_t releaseKeyVal : std::array{0, 1, 2}) {
        debugManager.flags.NEO_LOCAL_MEMORY_ALLOCATION_MODE.set(releaseKeyVal);
        WddmMemoryManager memoryManager{*executionEnvironment};
        EXPECT_EQ(memoryManager.usmDeviceAllocationMode, toLocalMemAllocationMode(releaseKeyVal));
        EXPECT_EQ(memoryManager.isLocalOnlyAllocationMode(), memoryManager.getGmmHelper(rootDeviceIndex)->isLocalOnlyAllocationMode());

        memoryManager.getGmmHelper(rootDeviceIndex)->setLocalOnlyAllocationMode(false);
    }
}

TEST_F(WddmMemoryManagerTests, GivenAllocDataWithSVMCPUSetWhenAllocateGraphicsMemoryWithAlignmentThenProperFunctionIsUsed) {
    AllocationProperties allocationProperties{0u, 0u, NEO::AllocationType::svmCpu, {}};
    NEO::AllocationData allocData = {};
    allocData.rootDeviceIndex = allocationProperties.rootDeviceIndex;
    allocData.type = allocationProperties.allocationType;
    allocData.forceKMDAllocation = true;
    allocData.makeGPUVaDifferentThanCPUPtr = true;
    allocData.allocationMethod = memoryManager->getPreferredAllocationMethod(allocationProperties);
    memoryManager->allocateGraphicsMemoryWithAlignment(allocData);

    if (allocData.allocationMethod == GfxMemoryAllocationMethod::allocateByKmd) {
        EXPECT_TRUE(memoryManager->allocateGraphicsMemoryUsingKmdAndMapItToCpuVACalled);
    } else {
        EXPECT_TRUE(memoryManager->allocateSystemMemoryAndCreateGraphicsAllocationFromItCalled);
    }
}

TEST_F(WddmMemoryManagerTests, GivenNotCompressedAndNotLockableAllocationTypeWhenAllocateUsingKmdAndMapToCpuVaThenSetResourceLockable) {
    NEO::AllocationData allocData = {};
    allocData.type = NEO::AllocationType::buffer;

    EXPECT_FALSE(GraphicsAllocation::isLockable(allocData.type));
    allocData.forceKMDAllocation = true;
    allocData.makeGPUVaDifferentThanCPUPtr = true;

    memoryManager->callBaseAllocateGraphicsMemoryUsingKmdAndMapItToCpuVA = true;
    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(allocData, false);

    EXPECT_NE(nullptr, graphicsAllocation);

    EXPECT_TRUE(graphicsAllocation->isAllocationLockable());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(WddmMemoryManagerTests, GivenoverrideAllocationCpuCacheableWhenAllocateUsingKmdAndMapToCpuVaThenoverrideAllocationCpuCacheable) {
    NEO::AllocationData allocData = {};
    allocData.type = NEO::AllocationType::commandBuffer;
    auto mockProductHelper = std::make_unique<MockProductHelper>();
    mockProductHelper->overrideAllocationCpuCacheableResult = true;
    executionEnvironment->rootDeviceEnvironments[0]->productHelper.reset(mockProductHelper.release());

    memoryManager->callBaseAllocateGraphicsMemoryUsingKmdAndMapItToCpuVA = true;
    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(allocData, false);

    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_TRUE(graphicsAllocation->getDefaultGmm()->resourceParams.Flags.Info.Cacheable);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(WddmMemoryManagerTests, GivenCompressedAndNotLockableAllocationTypeWhenAllocateUsingKmdAndMapToCpuVaThenSetResourceNotLockable) {
    NEO::AllocationData allocData = {};
    allocData.type = NEO::AllocationType::buffer;

    EXPECT_FALSE(GraphicsAllocation::isLockable(allocData.type));
    allocData.forceKMDAllocation = true;
    allocData.makeGPUVaDifferentThanCPUPtr = true;
    allocData.flags.preferCompressed = true;

    memoryManager->callBaseAllocateGraphicsMemoryUsingKmdAndMapItToCpuVA = true;
    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(allocData, false);

    EXPECT_NE(nullptr, graphicsAllocation);

    EXPECT_FALSE(graphicsAllocation->isAllocationLockable());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(WddmMemoryManagerTests, givenPageSize64kAlignmentWhenAllocateUsingKmdAndMapToCpuVaWithLargePagesThenAllocationIsAlignedTo64Kb) {
    AllocationData allocationData;
    allocationData.size = 5;
    allocationData.type = AllocationType::fillPattern;
    allocationData.alignment = MemoryConstants::pageSize64k;

    const size_t expectedAllignedAllocSize = alignUp(allocationData.size, MemoryConstants::pageSize64k);

    memoryManager->callBaseAllocateGraphicsMemoryUsingKmdAndMapItToCpuVA = true;
    auto allocation = memoryManager->allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(allocationData, true);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(expectedAllignedAllocSize, allocation->getUnderlyingBufferSize());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerTests, givenPageSize64kAlignmentWhenAllocateUsingKmdAndMapToCpuVaWithoutLargePagesThenAllocationIsAlignedTo4Kb) {
    AllocationData allocationData;
    allocationData.size = 5;
    allocationData.type = AllocationType::fillPattern;
    allocationData.alignment = MemoryConstants::pageSize64k;

    const size_t expectedAllignedAllocSize = alignUp(allocationData.size, MemoryConstants::pageSize);

    memoryManager->callBaseAllocateGraphicsMemoryUsingKmdAndMapItToCpuVA = true;
    auto allocation = memoryManager->allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(allocationData, false);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(expectedAllignedAllocSize, allocation->getUnderlyingBufferSize());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerTests, givenPageSizeAlignmentWhenAllocateUsingKmdAndMapToCpuVaWithLargePagesThenAllocationIsAlignedTo4Kb) {
    AllocationData allocationData;
    allocationData.size = 5;
    allocationData.type = AllocationType::fillPattern;
    allocationData.alignment = MemoryConstants::pageSize;

    const size_t expectedAllignedAllocSize = alignUp(allocationData.size, MemoryConstants::pageSize);

    memoryManager->callBaseAllocateGraphicsMemoryUsingKmdAndMapItToCpuVA = true;
    auto allocation = memoryManager->allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(allocationData, true);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(expectedAllignedAllocSize, allocation->getUnderlyingBufferSize());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerTests, givenWddmMemoryManagerWithoutLocalMemoryWhenGettingGlobalMemoryPercentThenCorrectValueIsReturned) {
    MockWddmMemoryManager memoryManager(true, false, *executionEnvironment);
    uint32_t rootDeviceIndex = 0u;
    EXPECT_EQ(0.94, memoryManager.getPercentOfGlobalMemoryAvailable(rootDeviceIndex));
}

TEST_F(WddmMemoryManagerTests, givenWddmMemoryManagerWithLocalMemoryWhenGettingGlobalMemoryPercentThenCorrectValueIsReturned) {
    MockWddmMemoryManager memoryManager(true, true, *executionEnvironment);
    uint32_t rootDeviceIndex = 0u;
    EXPECT_EQ(0.98, memoryManager.getPercentOfGlobalMemoryAvailable(rootDeviceIndex));
}

TEST_F(WddmMemoryManagerTests, givenAllocateGraphicsMemory64kbWhen32bitThenAddressIsProper) {
    if constexpr (is64bit) {
        GTEST_SKIP();
    }
    MockWddmMemoryManager memoryManager(false, false, *executionEnvironment);
    AllocationData allocationData;
    allocationData.size = 4096u;
    auto allocation = memoryManager.allocateGraphicsMemory64kb(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_LT(allocation->getGpuAddress(), MemoryConstants::max32BitAddress);

    memoryManager.freeGraphicsMemory(allocation);
}

class MockAllocateGraphicsMemoryUsingKmdAndMapItToCpuVAWddm : public MemoryManagerCreate<WddmMemoryManager> {
  public:
    using WddmMemoryManager::adjustGpuPtrToHostAddressSpace;
    using WddmMemoryManager::allocateGraphicsMemoryUsingKmdAndMapItToCpuVA;
    using WddmMemoryManager::allocateMemoryByKMD;
    using WddmMemoryManager::getPreferredAllocationMethod;
    using WddmMemoryManager::mapGpuVirtualAddress;
    MockAllocateGraphicsMemoryUsingKmdAndMapItToCpuVAWddm(ExecutionEnvironment &executionEnvironment) : MemoryManagerCreate(false, false, executionEnvironment) {}

    bool mapGpuVirtualAddressWithCpuPtr = false;

    bool mapGpuVirtualAddress(WddmAllocation *graphicsAllocation, const void *requiredGpuPtr) override {
        if (requiredGpuPtr != nullptr) {
            mapGpuVirtualAddressWithCpuPtr = true;
        } else {
            mapGpuVirtualAddressWithCpuPtr = false;
        }

        return true;
    }
    NTSTATUS createInternalNTHandle(D3DKMT_HANDLE *resourceHandle, HANDLE *ntHandle, uint32_t rootDeviceIndex) override {
        if (failCreateInternalNTHandle) {
            return 1;
        } else {
            return WddmMemoryManager::createInternalNTHandle(resourceHandle, ntHandle, rootDeviceIndex);
        }
    }
    bool failCreateInternalNTHandle = false;
};

class WddmMemoryManagerAllocPathTests : public ::testing::Test {
  public:
    std::unique_ptr<VariableBackup<bool>> returnEmptyFilesVectorBackup;
    MockAllocateGraphicsMemoryUsingKmdAndMapItToCpuVAWddm *memoryManager = nullptr;
    WddmMock *wddm = nullptr;
    ExecutionEnvironment *executionEnvironment = nullptr;

    void SetUp() override {
        returnEmptyFilesVectorBackup = std::make_unique<VariableBackup<bool>>(&NEO::returnEmptyFilesVector, true);
        HardwareInfo *hwInfo = nullptr;
        executionEnvironment = getExecutionEnvironmentImpl(hwInfo, 1);

        memoryManager = new MockAllocateGraphicsMemoryUsingKmdAndMapItToCpuVAWddm(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
        wddm = static_cast<WddmMock *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Wddm>());
    }

    void TearDown() override {
        delete executionEnvironment;
    }
};

TEST_F(WddmMemoryManagerAllocPathTests, givenAllocateGraphicsMemoryUsingKmdAndMapItToCpuVAWhenPreferedAllocationMethodThenProperArgumentsAreSet) {
    {
        AllocationProperties allocationProperties{0u, 0u, NEO::AllocationType::svmCpu, {}};
        NEO::AllocationData allocData = {};
        allocData.rootDeviceIndex = allocationProperties.rootDeviceIndex;
        allocData.type = allocationProperties.allocationType;
        allocData.forceKMDAllocation = true;
        allocData.makeGPUVaDifferentThanCPUPtr = true;
        allocData.allocationMethod = memoryManager->getPreferredAllocationMethod(allocationProperties);
        auto graphicsAllocation = memoryManager->allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(allocData, false);

        if (allocData.allocationMethod == GfxMemoryAllocationMethod::allocateByKmd && is64bit) {
            EXPECT_FALSE(memoryManager->mapGpuVirtualAddressWithCpuPtr);
        } else {
            EXPECT_TRUE(memoryManager->mapGpuVirtualAddressWithCpuPtr);
        }

        memoryManager->freeGraphicsMemory(graphicsAllocation);
    }
    {
        NEO::AllocationData allocData = {};
        allocData.type = NEO::AllocationType::externalHostPtr;
        auto graphicsAllocation = memoryManager->allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(allocData, false);

        if (executionEnvironment->rootDeviceEnvironments[allocData.rootDeviceIndex]->getHardwareInfo()->capabilityTable.gpuAddressSpace >= MemoryConstants::max64BitAppAddress) {
            EXPECT_TRUE(memoryManager->mapGpuVirtualAddressWithCpuPtr);
        } else {
            EXPECT_FALSE(memoryManager->mapGpuVirtualAddressWithCpuPtr);
        }

        memoryManager->freeGraphicsMemory(graphicsAllocation);
    }
}

TEST_F(WddmMemoryManagerAllocPathTests, GivenValidAllocationThenCreateInternalHandleSucceeds) {
    NEO::AllocationData allocData = {};
    allocData.type = NEO::AllocationType::svmCpu;
    allocData.forceKMDAllocation = true;
    allocData.makeGPUVaDifferentThanCPUPtr = true;
    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(allocData, false);

    uint64_t handle = 0;
    EXPECT_EQ(0, graphicsAllocation->createInternalHandle(memoryManager, 0u, handle));

    memoryManager->closeInternalHandle(handle, 0u, graphicsAllocation);

    EXPECT_EQ(0, graphicsAllocation->createInternalHandle(memoryManager, 0u, handle));

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(WddmMemoryManagerAllocPathTests, GivenNoAllocationThenCreateInternalHandleSucceeds) {
    NEO::AllocationData allocData = {};
    allocData.type = NEO::AllocationType::svmCpu;
    allocData.forceKMDAllocation = true;
    allocData.makeGPUVaDifferentThanCPUPtr = true;
    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(allocData, false);

    uint64_t handle = 0;
    EXPECT_EQ(0, graphicsAllocation->createInternalHandle(memoryManager, 0u, handle));

    memoryManager->closeInternalHandle(handle, 0u, nullptr);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(WddmMemoryManagerAllocPathTests, GivenValidAllocationThenCreateInternalHandleSucceedsAfterMultipleCallsToCreate) {
    NEO::AllocationData allocData = {};
    allocData.type = NEO::AllocationType::svmCpu;
    allocData.forceKMDAllocation = true;
    allocData.makeGPUVaDifferentThanCPUPtr = true;
    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(allocData, false);

    uint64_t handle = 0;
    EXPECT_EQ(0, graphicsAllocation->createInternalHandle(memoryManager, 0u, handle));

    EXPECT_EQ(0, graphicsAllocation->createInternalHandle(memoryManager, 0u, handle));

    memoryManager->closeInternalHandle(handle, 0u, graphicsAllocation);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(WddmMemoryManagerAllocPathTests, GivenValidAllocationWithFailingCreateInternalHandleThenErrorReturned) {
    NEO::AllocationData allocData = {};
    allocData.type = NEO::AllocationType::svmCpu;
    allocData.forceKMDAllocation = true;
    allocData.makeGPUVaDifferentThanCPUPtr = true;
    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(allocData, false);

    uint64_t handle = 0;
    EXPECT_EQ(0, graphicsAllocation->createInternalHandle(memoryManager, 0u, handle));

    memoryManager->closeInternalHandle(handle, 0u, graphicsAllocation);

    memoryManager->failCreateInternalNTHandle = true;
    EXPECT_EQ(1, graphicsAllocation->createInternalHandle(memoryManager, 0u, handle));

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(WddmMemoryManagerTests, GivenAllocationWhenAllocationIsFreeThenFreeToGmmClientContextCalled) {
    NEO::AllocationData allocData = {};
    allocData.type = NEO::AllocationType::buffer;

    allocData.forceKMDAllocation = true;
    allocData.makeGPUVaDifferentThanCPUPtr = true;

    memoryManager->callBaseAllocateGraphicsMemoryUsingKmdAndMapItToCpuVA = true;
    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(allocData, false);
    auto gmmHelper = executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper();

    memoryManager->freeGraphicsMemory(graphicsAllocation);
    EXPECT_GT(reinterpret_cast<MockGmmClientContextBase *>(gmmHelper->getClientContext())->freeGpuVirtualAddressCalled, 0u);
}

TEST_F(WddmMemoryManagerTests, givenAllocateGraphicsMemoryUsingKmdAndMapItToCpuVAWhenCreatingHostAllocationThenGpuAndCpuAddressesAreEqual) {
    memoryManager->hugeGfxMemoryChunkSize = MemoryConstants::pageSize64k;
    NEO::AllocationData allocData = {};
    allocData.type = NEO::AllocationType::bufferHostMemory;
    allocData.size = 2ULL * MemoryConstants::pageSize64k;
    allocData.forceKMDAllocation = false;
    allocData.makeGPUVaDifferentThanCPUPtr = false;
    memoryManager->callBaseAllocateGraphicsMemoryUsingKmdAndMapItToCpuVA = true;
    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(allocData, true);
    auto gmmHelper = executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper();
    EXPECT_EQ(graphicsAllocation->getGpuAddress(), reinterpret_cast<uintptr_t>(graphicsAllocation->getUnderlyingBuffer()));

    memoryManager->freeGraphicsMemory(graphicsAllocation);
    EXPECT_GT(reinterpret_cast<MockGmmClientContextBase *>(gmmHelper->getClientContext())->freeGpuVirtualAddressCalled, 0u);
}

TEST_F(WddmMemoryManagerAllocPathTests, givenAllocateGraphicsMemoryUsingKmdAndMapItToCpuVAWhen32bitThenProperAddressSet) {
    if constexpr (is64bit) {
        GTEST_SKIP();
    }
    NEO::AllocationData allocData = {};
    allocData.type = NEO::AllocationType::buffer;

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(allocData, false);

    EXPECT_NE(nullptr, graphicsAllocation);
    EXPECT_LT(graphicsAllocation->getGpuAddress(), MemoryConstants::max32BitAddress);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(WddmMemoryManagerAllocPathTests, givenAllocateMemoryByKMDWhen32bitAndIsStatelessAccessRequiredThenProperAddressSet) {
    if constexpr (is64bit) {
        GTEST_SKIP();
    }
    NEO::AllocationData allocData = {};
    allocData.type = NEO::AllocationType::constantSurface;

    auto graphicsAllocation = memoryManager->allocateMemoryByKMD(allocData);

    EXPECT_NE(nullptr, graphicsAllocation);
    EXPECT_LT(graphicsAllocation->getGpuAddress(), MemoryConstants::max32BitAddress);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

class MockWddmReserveValidAddressRange : public WddmMock {
  public:
    MockWddmReserveValidAddressRange(RootDeviceEnvironment &rootDeviceEnvironment) : WddmMock(rootDeviceEnvironment){};
    bool reserveValidAddressRange(size_t size, void *&reservedMem) override {
        reserveValidAddressRangeResult.called++;
        reservedMem = dummyAddress;
        return true;
    }
    void *dummyAddress = reinterpret_cast<void *>(0x43211111);
};

TEST_F(WddmMemoryManagerAllocPathTests, givenLocalMemoryWhen32bitAndCallAdjustGpuPtrToHostAddressSpaceThenProperAlignmentIsApplied) {
    if constexpr (is64bit) {
        GTEST_SKIP();
    }
    auto mockWddm = std::make_unique<MockWddmReserveValidAddressRange>(*executionEnvironment->rootDeviceEnvironments[0].get());
    auto addressWithoutAligment = reinterpret_cast<uint64_t>(mockWddm->dummyAddress);
    uint32_t rootDeviceIndex = 0u;
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::move(mockWddm));
    size_t size = 10;

    auto wddmAllocation = std::make_unique<WddmAllocation>(rootDeviceIndex,
                                                           1u, // numGmms
                                                           NEO::AllocationType::buffer, nullptr, 0,
                                                           size, nullptr, MemoryPool::localMemory,
                                                           0u, // shareable
                                                           0u);
    void *addressPtr;
    memoryManager->adjustGpuPtrToHostAddressSpace(*wddmAllocation.get(), addressPtr);

    EXPECT_NE(nullptr, addressPtr);
    auto address = reinterpret_cast<uint64_t>(addressPtr);
    uint64_t alignmentMask = 2 * MemoryConstants::megaByte - 1;
    EXPECT_FALSE(address & alignmentMask);
    EXPECT_NE(addressWithoutAligment, address);
}

TEST_F(WddmMemoryManagerAllocPathTests, givenSystemMemoryWhen32bitAndCallAdjustGpuPtrToHostAddressSpaceThenThereIsNoExtraAlignment) {
    if constexpr (is64bit) {
        GTEST_SKIP();
    }
    auto mockWddm = std::make_unique<MockWddmReserveValidAddressRange>(*executionEnvironment->rootDeviceEnvironments[0].get());
    auto expectedAddress = reinterpret_cast<uint64_t>(mockWddm->dummyAddress);
    uint32_t rootDeviceIndex = 0u;
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::move(mockWddm));
    size_t size = 10;

    auto wddmAllocation = std::make_unique<WddmAllocation>(rootDeviceIndex,
                                                           1u, // numGmms
                                                           NEO::AllocationType::buffer, nullptr, 0,
                                                           size, nullptr, MemoryPool::system64KBPages,
                                                           0u, // shareable
                                                           0u);
    void *addressPtr;
    memoryManager->adjustGpuPtrToHostAddressSpace(*wddmAllocation.get(), addressPtr);

    EXPECT_NE(nullptr, addressPtr);
    auto address = reinterpret_cast<uint64_t>(addressPtr);

    EXPECT_EQ(expectedAddress, address);
}

TEST_F(WddmMemoryManagerTests, givenTypeWhenCallIsStatelessAccessRequiredThenProperValueIsReturned) {

    auto wddmMemoryManager = std::make_unique<MockWddmMemoryManager>(*executionEnvironment);

    for (auto type : {AllocationType::buffer,
                      AllocationType::sharedBuffer,
                      AllocationType::scratchSurface,
                      AllocationType::linearStream,
                      AllocationType::privateSurface,
                      AllocationType::constantSurface,
                      AllocationType::globalSurface,
                      AllocationType::printfSurface}) {
        EXPECT_TRUE(wddmMemoryManager->isStatelessAccessRequired(type));
    }
    for (auto type : {AllocationType::bufferHostMemory,
                      AllocationType::commandBuffer,
                      AllocationType::externalHostPtr,
                      AllocationType::fillPattern,
                      AllocationType::image,
                      AllocationType::indirectObjectHeap,
                      AllocationType::instructionHeap,
                      AllocationType::internalHeap,
                      AllocationType::internalHostMemory,
                      AllocationType::kernelArgsBuffer,
                      AllocationType::kernelIsa,
                      AllocationType::kernelIsaInternal,
                      AllocationType::mapAllocation,
                      AllocationType::mcs,
                      AllocationType::pipe,
                      AllocationType::preemption,
                      AllocationType::profilingTagBuffer,
                      AllocationType::sharedImage,
                      AllocationType::sharedResourceCopy,
                      AllocationType::surfaceStateHeap,
                      AllocationType::svmCpu,
                      AllocationType::svmGpu,
                      AllocationType::svmZeroCopy,
                      AllocationType::syncBuffer,
                      AllocationType::tagBuffer,
                      AllocationType::globalFence,
                      AllocationType::timestampPacketTagBuffer,
                      AllocationType::writeCombined,
                      AllocationType::ringBuffer,
                      AllocationType::semaphoreBuffer,
                      AllocationType::debugContextSaveArea,
                      AllocationType::debugSbaTrackingBuffer,
                      AllocationType::debugModuleArea,
                      AllocationType::unifiedSharedMemory,
                      AllocationType::workPartitionSurface,
                      AllocationType::gpuTimestampDeviceBuffer,
                      AllocationType::swTagBuffer,
                      AllocationType::deferredTasksList,
                      AllocationType::assertBuffer,
                      AllocationType::syncDispatchToken}) {
        EXPECT_FALSE(wddmMemoryManager->isStatelessAccessRequired(type));
    }
}

TEST_F(WddmMemoryManagerTests, givenForcePreferredAllocationMethodFlagSetWhenGettingPreferredAllocationMethodThenValueFlagIsReturned) {
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getProductHelper();
    for (auto i = 0; i < static_cast<int>(AllocationType::count); i++) {
        AllocationProperties allocationProperties{0u, 0u, static_cast<AllocationType>(i), {}};
        if (productHelper.getPreferredAllocationMethod(allocationProperties.allocationType)) {
            EXPECT_EQ(productHelper.getPreferredAllocationMethod(allocationProperties.allocationType), memoryManager->getPreferredAllocationMethod(allocationProperties));
        } else {
            EXPECT_EQ(preferredAllocationMethod, memoryManager->getPreferredAllocationMethod(allocationProperties));
        }

        DebugManagerStateRestore restorer;
        for (const auto &allocationMethod : {GfxMemoryAllocationMethod::useUmdSystemPtr, GfxMemoryAllocationMethod::allocateByKmd}) {
            debugManager.flags.ForcePreferredAllocationMethod.set(static_cast<int32_t>(allocationMethod));
            EXPECT_EQ(allocationMethod, memoryManager->getPreferredAllocationMethod(allocationProperties));
        }
    }
}

class WddmMemoryManagerSimpleTest : public ::testing::Test {
  public:
    void SetUp() override {
        auto osEnvironment = new OsEnvironmentWin();
        gdi = new MockGdi();
        osEnvironment->gdi.reset(gdi);
        executionEnvironment.osEnvironment.reset(osEnvironment);

        executionEnvironment.prepareRootDeviceEnvironments(2u);
        for (auto i = 0u; i < executionEnvironment.rootDeviceEnvironments.size(); i++) {
            executionEnvironment.rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
            executionEnvironment.rootDeviceEnvironments[i]->initGmm();
            auto wddmTemp = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment.rootDeviceEnvironments[i]));
            constexpr uint64_t heap32Base = (is32bit) ? 0x1000 : 0x800000000000;
            wddmTemp->setHeap32(heap32Base, 1000 * MemoryConstants::pageSize - 1);
            wddmTemp->init();
            wddm = wddmTemp;
        }
        rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
        wddm = static_cast<WddmMock *>(rootDeviceEnvironment->osInterface->getDriverModel()->as<Wddm>());
        rootDeviceEnvironment->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
        executionEnvironment.initializeMemoryManager();

        memoryManager = std::make_unique<MockWddmMemoryManager>(executionEnvironment);
        csr.reset(createCommandStream(executionEnvironment, 0u, 1));
        auto hwInfo = rootDeviceEnvironment->getHardwareInfo();
        auto &gfxCoreHelper = rootDeviceEnvironment->getHelper<GfxCoreHelper>();
        osContext = memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*rootDeviceEnvironment)[0],
                                                                                                                      PreemptionHelper::getDefaultPreemptionMode(*hwInfo)));
        osContext->ensureContextInitialized(false);

        osContext->incRefInternal();
        mockTemporaryResources = reinterpret_cast<MockWddmResidentAllocationsContainer *>(wddm->getTemporaryResourcesContainer());
    }

    void TearDown() override {
        osContext->decRefInternal();
    }

    void testAlignment(uint32_t allocationSize, uint32_t expectedAlignment) {
        const bool enable64kbPages = false;
        const bool localMemoryEnabled = true;
        memoryManager = std::make_unique<MockWddmMemoryManager>(enable64kbPages, localMemoryEnabled, executionEnvironment);

        MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
        AllocationData allocData;
        allocData.allFlags = 0;
        allocData.size = allocationSize;
        allocData.flags.allocateMemory = true;
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);

        EXPECT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
        EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
        EXPECT_EQ(0u, allocation->getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly);
        EXPECT_EQ(alignUp(allocationSize, expectedAlignment), allocation->getUnderlyingBufferSize());
        EXPECT_EQ(expectedAlignment, allocation->getDefaultGmm()->resourceParams.BaseAlignment);

        memoryManager->freeGraphicsMemory(allocation);
    }

    template <bool using32Bit>
    void givenLocalMemoryAllocationAndRequestedSizeIsHugeThenResultAllocationIsSplitted() {
        if constexpr (using32Bit) {
            GTEST_SKIP();
        } else {

            DebugManagerStateRestore dbgRestore;
            wddm->init();
            wddm->mapGpuVaStatus = true;
            VariableBackup<bool> restorer{&wddm->callBaseMapGpuVa, false};

            memoryManager = std::make_unique<MockWddmMemoryManager>(false, true, executionEnvironment);
            AllocationData allocData;
            allocData.allFlags = 0;
            allocData.size = static_cast<size_t>(MemoryConstants::gigaByte * 13);
            allocData.flags.allocateMemory = true;

            MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
            auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
            EXPECT_NE(nullptr, allocation);
            EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
            EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
            EXPECT_EQ(4u, allocation->getNumGmms());
            EXPECT_EQ(4u, wddm->createAllocationResult.called);

            uint64_t totalSizeFromGmms = 0u;
            for (uint32_t gmmId = 0u; gmmId < allocation->getNumGmms(); ++gmmId) {
                Gmm *gmm = allocation->getGmm(gmmId);
                EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.NonLocalOnly);
                EXPECT_EQ(2 * MemoryConstants::megaByte, gmm->resourceParams.BaseAlignment);
                EXPECT_TRUE(isAligned(gmm->resourceParams.BaseWidth64, gmm->resourceParams.BaseAlignment));

                totalSizeFromGmms += gmm->resourceParams.BaseWidth64;
            }
            EXPECT_EQ(static_cast<uint64_t>(allocData.size), totalSizeFromGmms);

            memoryManager->freeGraphicsMemory(allocation);
        }
    }

    DebugManagerStateRestore restore{};
    MockExecutionEnvironment executionEnvironment{};

    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
    std::unique_ptr<MockWddmMemoryManager> memoryManager;
    std::unique_ptr<CommandStreamReceiver> csr;

    WddmMock *wddm = nullptr;
    MockWddmResidentAllocationsContainer *mockTemporaryResources;
    OsContext *osContext = nullptr;
    MockGdi *gdi = nullptr;
};

TEST_F(WddmMemoryManagerSimpleTest, givenAllocateGraphicsMemoryWithPropertiesCalledWithDebugSurfaceTypeThenDebugSurfaceIsCreatedAndZerod) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForcePreferredAllocationMethod.set(static_cast<int32_t>(GfxMemoryAllocationMethod::useUmdSystemPtr));
    AllocationProperties debugSurfaceProperties{0, true, MemoryConstants::pageSize, AllocationType::debugContextSaveArea, false, false, 0b1011};
    auto debugSurface = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(debugSurfaceProperties));
    EXPECT_NE(nullptr, debugSurface);

    auto mem = debugSurface->getUnderlyingBuffer();
    auto size = debugSurface->getUnderlyingBufferSize();

    EXPECT_TRUE(memoryZeroed(mem, size));
    memoryManager->freeGraphicsMemory(debugSurface);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWhenAllocateGraphicsMemoryIsCalledThenMemoryPoolIsSystem4KBPages) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForcePreferredAllocationMethod.set(static_cast<int32_t>(GfxMemoryAllocationMethod::useUmdSystemPtr));
    memoryManager.reset(new MockWddmMemoryManager(false, false, executionEnvironment));
    if (memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::system4KBPages, allocation->getMemoryPool());
    EXPECT_EQ(executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->featureTable.flags.ftrLocalMemory,
              allocation->getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWhenAllocateGraphicsMemoryIsCalledThenAllocationSizeIsRegistered) {
    memoryManager.reset(new MockWddmMemoryManager(false, true, executionEnvironment));

    const auto allocationSize = MemoryConstants::pageSize;
    auto allocationProperties = MockAllocationProperties{csr->getRootDeviceIndex(), allocationSize};
    allocationProperties.allocationType = AllocationType::buffer;
    EXPECT_EQ(0u, memoryManager->getUsedLocalMemorySize(csr->getRootDeviceIndex()));
    EXPECT_EQ(0u, memoryManager->getUsedSystemMemorySize());
    auto localAllocation = memoryManager->allocateGraphicsMemoryWithProperties(allocationProperties);
    EXPECT_NE(nullptr, localAllocation);
    EXPECT_EQ(localAllocation->getUnderlyingBufferSize(), memoryManager->getUsedLocalMemorySize(csr->getRootDeviceIndex()));
    EXPECT_EQ(0u, memoryManager->getUsedSystemMemorySize());

    allocationProperties.allocationType = AllocationType::bufferHostMemory;
    auto systemAllocation = memoryManager->allocateGraphicsMemoryWithProperties(allocationProperties);
    EXPECT_NE(nullptr, systemAllocation);
    EXPECT_EQ(localAllocation->getUnderlyingBufferSize(), memoryManager->getUsedLocalMemorySize(csr->getRootDeviceIndex()));
    EXPECT_EQ(systemAllocation->getUnderlyingBufferSize(), memoryManager->getUsedSystemMemorySize());

    memoryManager->freeGraphicsMemory(localAllocation);
    EXPECT_EQ(0u, memoryManager->getUsedLocalMemorySize(csr->getRootDeviceIndex()));
    EXPECT_EQ(systemAllocation->getUnderlyingBufferSize(), memoryManager->getUsedSystemMemorySize());

    memoryManager->freeGraphicsMemory(systemAllocation);
    EXPECT_EQ(0u, memoryManager->getUsedLocalMemorySize(csr->getRootDeviceIndex()));
    EXPECT_EQ(0u, memoryManager->getUsedSystemMemorySize());
}

class MockCreateWddmAllocationMemoryManager : public MockWddmMemoryManager {
  public:
    MockCreateWddmAllocationMemoryManager(NEO::ExecutionEnvironment &execEnv) : MockWddmMemoryManager(execEnv) {}
    bool createWddmAllocation(WddmAllocation *allocation, void *requiredGpuPtr) override {
        return false;
    }
    bool createPhysicalAllocation(WddmAllocation *allocation) override {
        return false;
    }
};

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWhenAllocateGraphicsMemoryFailedThenNullptrFromAllocateMemoryByKMDIsReturned) {
    memoryManager.reset(new MockCreateWddmAllocationMemoryManager(executionEnvironment));
    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize;
    auto allocation = memoryManager->allocateMemoryByKMD(allocationData);
    EXPECT_EQ(nullptr, allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWhenAllocateGraphicsMemoryFailedThenNullptrFromAllocatePhysicalDeviceMemoryIsReturned) {
    memoryManager.reset(new MockCreateWddmAllocationMemoryManager(executionEnvironment));
    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize;
    MemoryManager::AllocationStatus status;
    auto allocation = memoryManager->allocatePhysicalDeviceMemory(allocationData, status);
    EXPECT_EQ(nullptr, allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWhenAllocateGraphicsMemoryFailedThenNullptrFromAllocatePhysicalLocalDeviceMemoryIsReturned) {
    memoryManager.reset(new MockCreateWddmAllocationMemoryManager(executionEnvironment));
    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize;
    MemoryManager::AllocationStatus status;
    auto allocation = memoryManager->allocatePhysicalLocalDeviceMemory(allocationData, status);
    EXPECT_EQ(nullptr, allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWith64KBPagesEnabledWhenAllocateGraphicsMemory64kbIsCalledThenMemoryPoolIsSystem64KBPages) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, executionEnvironment));
    AllocationData allocationData;
    allocationData.size = 4096u;
    auto allocation = memoryManager->allocateGraphicsMemory64kb(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::system64KBPages, allocation->getMemoryPool());

    EXPECT_EQ(executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->featureTable.flags.ftrLocalMemory,
              allocation->getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly);

    memoryManager->freeGraphicsMemory(allocation);
}

auto compareStorageInfo = [](const StorageInfo &left, const StorageInfo &right) -> void {
    EXPECT_EQ(left.memoryBanks, right.memoryBanks);
    EXPECT_EQ(left.pageTablesVisibility, right.pageTablesVisibility);
    EXPECT_EQ(left.subDeviceBitfield, right.subDeviceBitfield);
    EXPECT_EQ(left.cloningOfPageTables, right.cloningOfPageTables);
    EXPECT_EQ(left.tileInstanced, right.tileInstanced);
    EXPECT_EQ(left.multiStorage, right.multiStorage);
    EXPECT_EQ(left.colouringPolicy, right.colouringPolicy);
    EXPECT_EQ(left.colouringGranularity, right.colouringGranularity);
    EXPECT_EQ(left.cpuVisibleSegment, right.cpuVisibleSegment);
    EXPECT_EQ(left.isLockable, right.isLockable);
    EXPECT_EQ(left.localOnlyRequired, right.localOnlyRequired);
    EXPECT_EQ(left.systemMemoryPlacement, right.systemMemoryPlacement);
    EXPECT_EQ(left.systemMemoryForced, right.systemMemoryForced);
    EXPECT_EQ(0, memcmp(left.resourceTag, right.resourceTag, AppResourceDefines::maxStrLen + 1));
    EXPECT_EQ(left.isChunked, right.isChunked);
    EXPECT_EQ(left.numOfChunks, right.numOfChunks);
};

TEST_F(WddmMemoryManagerSimpleTest, givenAllocationDataWithStorageInfoWhenAllocateGraphicsMemory64kbThenStorageInfoInAllocationIsSetCorrectly) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, executionEnvironment));
    AllocationData allocationData{};
    allocationData.type = AllocationType::buffer;
    allocationData.storageInfo = {};
    allocationData.storageInfo.isLockable = true;

    auto allocation = memoryManager->allocateGraphicsMemory64kb(allocationData);
    EXPECT_NE(nullptr, allocation);
    compareStorageInfo(allocationData.storageInfo, allocation->storageInfo);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenAllocationDataWithFlagsWhenAllocateGraphicsMemory64kbThenAllocationFlagFlushL3RequiredIsSetCorrectly) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForcePreferredAllocationMethod.set(static_cast<int32_t>(GfxMemoryAllocationMethod::useUmdSystemPtr));
    class MockGraphicsAllocation : public GraphicsAllocation {
      public:
        using GraphicsAllocation::allocationInfo;
    };
    memoryManager.reset(new MockWddmMemoryManager(false, false, executionEnvironment));
    AllocationData allocationData;
    allocationData.flags.flushL3 = true;
    auto allocation = static_cast<MockGraphicsAllocation *>(memoryManager->allocateGraphicsMemory64kb(allocationData));
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(allocationData.flags.flushL3, allocation->allocationInfo.flags.flushL3Required);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenAllocationWhenCallingSetLockedMemoryThenFlagIsSetCorrectly) {
    class MockGraphicsAllocation : public GraphicsAllocation {
      public:
        using GraphicsAllocation::allocationInfo;
    };
    memoryManager.reset(new MockWddmMemoryManager(false, false, executionEnvironment));
    AllocationData allocationData;
    auto allocation = static_cast<MockGraphicsAllocation *>(memoryManager->allocateGraphicsMemory64kb(allocationData));
    EXPECT_NE(nullptr, allocation);
    EXPECT_FALSE(allocation->allocationInfo.flags.lockedMemory);
    allocation->setLockedMemory(true);
    EXPECT_TRUE(allocation->allocationInfo.flags.lockedMemory);
    EXPECT_TRUE(allocation->isLockedMemory());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWhenAllocateGraphicsMemoryWithPtrIsCalledThenMemoryPoolIsSystem4KBPages) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, executionEnvironment));
    if (memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = 4096u;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, size}, ptr);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::system4KBPages, allocation->getMemoryPool());
    for (size_t i = 0; i < allocation->fragmentsStorage.fragmentCount; i++) {
        EXPECT_EQ(1u, static_cast<OsHandleWin *>(allocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage)->gmm->resourceParams.Flags.Info.NonLocalOnly);
    }
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWhenAllocate32BitGraphicsMemoryWithPtrIsCalledThenMemoryPoolIsSystem4KBPagesWith32BitGpuAddressing) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, executionEnvironment));
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = MemoryConstants::pageSize;

    auto allocation = memoryManager->allocate32BitGraphicsMemory(csr->getRootDeviceIndex(), size, ptr, AllocationType::buffer);

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::system4KBPagesWith32BitGpuAddressing, allocation->getMemoryPool());
    EXPECT_EQ(executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->featureTable.flags.ftrLocalMemory,
              allocation->getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWith64KBPagesDisabledWhenAllocateGraphicsMemoryForSVMThen4KBGraphicsAllocationIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForcePreferredAllocationMethod.set(static_cast<int32_t>(GfxMemoryAllocationMethod::useUmdSystemPtr));
    memoryManager.reset(new MockWddmMemoryManager(false, false, executionEnvironment));
    if (memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    auto size = MemoryConstants::pageSize;

    auto svmAllocation = memoryManager->allocateGraphicsMemoryWithProperties({csr->getRootDeviceIndex(), size, AllocationType::svmZeroCopy, mockDeviceBitfield});
    EXPECT_NE(nullptr, svmAllocation);
    EXPECT_EQ(MemoryPool::system4KBPages, svmAllocation->getMemoryPool());
    EXPECT_EQ(executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->featureTable.flags.ftrLocalMemory,
              svmAllocation->getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly);
    memoryManager->freeGraphicsMemory(svmAllocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWith64KBPagesEnabledWhenAllocateGraphicsMemoryForSVMThenMemoryPoolIsSystem64KBPages) {
    memoryManager.reset(new MockWddmMemoryManager(true, false, executionEnvironment));
    if (memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    auto size = MemoryConstants::pageSize;

    auto svmAllocation = memoryManager->allocateGraphicsMemoryWithProperties({csr->getRootDeviceIndex(), size, AllocationType::svmZeroCopy, mockDeviceBitfield});
    EXPECT_NE(nullptr, svmAllocation);
    EXPECT_EQ(MemoryPool::system64KBPages, svmAllocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(svmAllocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWhenCreateAllocationFromHandleIsCalledThenMemoryPoolIsSystemCpuInaccessible) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, executionEnvironment));
    MockWddmMemoryManager::OsHandleData osHandleData{1u};
    gdi->getQueryResourceInfoArgOut().NumAllocations = 1;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), nullptr, 0, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));

    D3DDDI_OPENALLOCATIONINFO allocationInfo;
    allocationInfo.pPrivateDriverData = gmm->gmmResourceInfo->peekHandle();
    allocationInfo.hAllocation = ALLOCATION_HANDLE;
    allocationInfo.PrivateDriverDataSize = sizeof(GMM_RESOURCE_INFO);

    VariableBackup<D3DDDI_OPENALLOCATIONINFO *> openResourceBackup(&gdi->getOpenResourceArgOut().pOpenAllocationInfo, &allocationInfo);

    AllocationProperties properties(0, false, 0, AllocationType::sharedBuffer, false, false, 0);
    auto allocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::systemCpuInaccessible, allocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenSharedHandleWhenCreateGraphicsAllocationFromMultipleSharedHandlesIsCalledThenNullptrIsReturned) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, executionEnvironment));
    auto handle = 1u;
    gdi->getQueryResourceInfoArgOut().NumAllocations = 1;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), nullptr, 0, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));

    D3DDDI_OPENALLOCATIONINFO allocationInfo;
    allocationInfo.pPrivateDriverData = gmm->gmmResourceInfo->peekHandle();
    allocationInfo.hAllocation = ALLOCATION_HANDLE;
    allocationInfo.PrivateDriverDataSize = sizeof(GMM_RESOURCE_INFO);

    VariableBackup<D3DDDI_OPENALLOCATIONINFO *> openResourceBackup(&gdi->getOpenResourceArgOut().pOpenAllocationInfo, &allocationInfo);

    AllocationProperties properties(0, false, 0, AllocationType::sharedBuffer, false, false, 0);
    std::vector<osHandle> handles{handle};
    auto allocation = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, true, nullptr);
    EXPECT_EQ(nullptr, allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenAllocationPropertiesWhenCreateAllocationFromHandleIsCalledThenCorrectAllocationTypeIsSet) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, executionEnvironment));
    MockMemoryManager::OsHandleData osHandleData{1u};
    gdi->getQueryResourceInfoArgOut().NumAllocations = 1;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), nullptr, 0, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));

    D3DDDI_OPENALLOCATIONINFO allocationInfo;
    allocationInfo.pPrivateDriverData = gmm->gmmResourceInfo->peekHandle();
    allocationInfo.hAllocation = ALLOCATION_HANDLE;
    allocationInfo.PrivateDriverDataSize = sizeof(GMM_RESOURCE_INFO);

    VariableBackup<D3DDDI_OPENALLOCATIONINFO *> openResourceBackup(&gdi->getOpenResourceArgOut().pOpenAllocationInfo, &allocationInfo);

    AllocationProperties propertiesBuffer(0, false, 0, AllocationType::sharedBuffer, false, false, 0);
    AllocationProperties propertiesImage(0, false, 0, AllocationType::sharedImage, false, false, 0);

    AllocationProperties *propertiesArray[2] = {&propertiesBuffer, &propertiesImage};

    for (auto properties : propertiesArray) {
        auto allocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, *properties, false, false, true, nullptr);
        EXPECT_NE(nullptr, allocation);
        EXPECT_EQ(properties->allocationType, allocation->getAllocationType());
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(WddmMemoryManagerSimpleTest, whenCreateAllocationFromHandleAndMapCallFailsThenFreeGraphicsMemoryIsCalled) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, executionEnvironment));
    MockWddmMemoryManager::OsHandleData osHandleData{1u};
    gdi->getQueryResourceInfoArgOut().NumAllocations = 1;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::make_unique<Gmm>(rootDeviceEnvironment->getGmmHelper(), nullptr, 0, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, StorageInfo{}, gmmRequirements);

    D3DDDI_OPENALLOCATIONINFO allocationInfo;
    allocationInfo.pPrivateDriverData = gmm->gmmResourceInfo->peekHandle();
    allocationInfo.hAllocation = ALLOCATION_HANDLE;
    allocationInfo.PrivateDriverDataSize = sizeof(GMM_RESOURCE_INFO);
    wddm->mapGpuVaStatus = false;
    wddm->callBaseMapGpuVa = false;

    VariableBackup<D3DDDI_OPENALLOCATIONINFO *> openResourceBackup(&gdi->getOpenResourceArgOut().pOpenAllocationInfo, &allocationInfo);
    EXPECT_EQ(0u, memoryManager->freeGraphicsMemoryImplCalled);

    AllocationProperties properties(0, false, 0, AllocationType::sharedBuffer, false, false, 0);

    auto allocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(1u, memoryManager->freeGraphicsMemoryImplCalled);
}

TEST_F(WddmMemoryManagerSimpleTest, givenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledWhenNotAlignedPtrIsPassedAndFlushL3RequiredThenSetCorrectGmmResource) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, executionEnvironment));
    auto size = 13u;
    auto hostPtr = reinterpret_cast<const void *>(0x10001);

    AllocationData allocationData;
    allocationData.size = size;
    allocationData.hostPtr = hostPtr;
    allocationData.flags.flushL3 = true;
    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(hostPtr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(1u, allocation->getAllocationOffset());

    const auto &productHelper = rootDeviceEnvironment->getHelper<ProductHelper>();
    auto expectedUsage = GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER;
    if (productHelper.isMisalignedUserPtr2WayCoherent()) {
        expectedUsage = GMM_RESOURCE_USAGE_HW_CONTEXT;
    }
    EXPECT_EQ(expectedUsage, allocation->getGmm(0)->resourceParams.Usage);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledWhenNotAlignedPtrIsPassedAndFlushL3NotRequiredThenSetCorrectGmmResource) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, executionEnvironment));
    auto size = 13u;
    auto hostPtr = reinterpret_cast<const void *>(0x10001);

    AllocationData allocationData;
    allocationData.size = size;
    allocationData.hostPtr = hostPtr;
    allocationData.flags.flushL3 = false;
    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);
    EXPECT_NE(nullptr, allocation);

    EXPECT_EQ(GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER, allocation->getGmm(0)->resourceParams.Usage);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledWhenNotAlignedPtrIsPassedAndImportedAllocationIsFalseThenAlignedGraphicsAllocationIsFreed) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, executionEnvironment));
    auto size = 13u;
    auto hostPtr = reinterpret_cast<const void *>(0x10001);

    AllocationData allocationData;
    allocationData.size = size;
    allocationData.hostPtr = hostPtr;
    allocationData.flags.flushL3 = true;
    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(hostPtr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(1u, allocation->getAllocationOffset());
    memoryManager->freeGraphicsMemoryImpl(allocation, false);
}

TEST_F(WddmMemoryManagerSimpleTest, GivenShareableEnabledAndSmallSizeWhenAskedToCreateGrahicsAllocationThenValidAllocationIsReturned) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, executionEnvironment));
    memoryManager->hugeGfxMemoryChunkSize = MemoryConstants::pageSize64k;
    AllocationData allocationData;
    allocationData.size = 4096u;
    allocationData.flags.shareable = true;
    auto allocation = memoryManager->allocateMemoryByKMD(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_FALSE(memoryManager->allocateHugeGraphicsMemoryCalled);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWhenRenderCompressBufferEnabledAndPrefferCompressionIsTrueThenGmmHasCompressionEnabled) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, executionEnvironment));
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.RenderCompressedBuffersEnabled.set(1);
    memoryManager->hugeGfxMemoryChunkSize = MemoryConstants::pageSize64k;
    AllocationData allocationData;
    allocationData.size = 4096u;
    allocationData.flags.preferCompressed = true;
    auto allocation = memoryManager->allocateMemoryByKMD(allocationData);
    EXPECT_TRUE(allocation->getDefaultGmm()->isCompressionEnabled());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWhenRenderCompressBufferEnabledAndPrefferCompressionIsFalseThenGmmHasCompressionDisabled) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, executionEnvironment));
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.RenderCompressedBuffersEnabled.set(1);
    memoryManager->hugeGfxMemoryChunkSize = MemoryConstants::pageSize64k;
    AllocationData allocationData;
    allocationData.size = 4096u;
    allocationData.flags.preferCompressed = false;
    auto allocation = memoryManager->allocateMemoryByKMD(allocationData);
    EXPECT_FALSE(allocation->getDefaultGmm()->isCompressionEnabled());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, GivenShareableEnabledAndHugeSizeWhenAskedToCreateGrahicsAllocationThenValidAllocationIsReturned) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, executionEnvironment));
    memoryManager->hugeGfxMemoryChunkSize = MemoryConstants::pageSize64k;
    AllocationData allocationData;
    allocationData.size = 2ULL * MemoryConstants::pageSize64k;
    allocationData.flags.shareable = true;
    auto allocation = memoryManager->allocateMemoryByKMD(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_TRUE(memoryManager->allocateHugeGraphicsMemoryCalled);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, GivenShareableEnabledAndSmallSizeWhenAskedToCreatePhysicalGraphicsAllocationThenValidAllocationIsReturned) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, executionEnvironment));
    memoryManager->hugeGfxMemoryChunkSize = MemoryConstants::pageSize64k;
    AllocationData allocationData;
    allocationData.size = 4096u;
    allocationData.flags.shareable = true;
    MemoryManager::AllocationStatus status;
    auto allocation = memoryManager->allocatePhysicalDeviceMemory(allocationData, status);
    EXPECT_NE(nullptr, allocation);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, GivenShareableEnabledAndHugeSizeWhenAskedToCreatePhysicalLocalGraphicsAllocationThenValidAllocationIsReturned) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, executionEnvironment));
    memoryManager->hugeGfxMemoryChunkSize = MemoryConstants::pageSize64k;
    AllocationData allocationData;
    allocationData.size = 2ULL * MemoryConstants::pageSize64k;
    allocationData.flags.shareable = true;
    MemoryManager::AllocationStatus status;
    auto allocation = memoryManager->allocatePhysicalLocalDeviceMemory(allocationData, status);
    EXPECT_NE(nullptr, allocation);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, GivenPhysicalDeviceMemoryAndVirtualMemoryThenMapSucceeds) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, executionEnvironment));
    memoryManager->hugeGfxMemoryChunkSize = MemoryConstants::pageSize64k;
    AllocationData allocationData;
    allocationData.size = 2ULL * MemoryConstants::pageSize64k;
    uint64_t gpuRange = 0x1234;
    MemoryManager::AllocationStatus status;
    auto allocation = memoryManager->allocatePhysicalLocalDeviceMemory(allocationData, status);
    EXPECT_NE(nullptr, allocation);
    auto res = memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(allocation, gpuRange, allocationData.size);
    EXPECT_TRUE(res);
    memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(allocation, gpuRange, allocationData.size, osContext, 0u);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, GivenPhysicalHostMemoryAndVirtualMemoryThenMapFails) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, executionEnvironment));
    AllocationData allocationData;
    allocationData.allFlags = 0;
    allocationData.size = MemoryConstants::pageSize;
    allocationData.flags.allocateMemory = true;
    allocationData.flags.useSystemMemory = true;
    allocationData.flags.isUSMHostAllocation = true;
    uint64_t gpuRange = 0x1234;
    MemoryManager::AllocationStatus status;
    auto allocation = memoryManager->allocatePhysicalHostMemory(allocationData, status);
    EXPECT_EQ(nullptr, allocation);

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    rootDeviceIndices.pushUnique(1);
    MultiGraphicsAllocation multiGraphicsAllocations{2};
    auto res = memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocations, allocation, gpuRange, allocationData.size);
    EXPECT_FALSE(res);
    memoryManager->unMapPhysicalHostMemoryFromVirtualMemory(multiGraphicsAllocations, allocation, gpuRange, allocationData.size);
}

TEST_F(WddmMemoryManagerSimpleTest, givenZeroFenceValueOnSingleEngineRegisteredWhenHandleFenceCompletionIsCalledThenDoNotWaitOnCpu) {
    ASSERT_EQ(1u, memoryManager->getRegisteredEngines(0).size());

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties({0, 32, AllocationType::buffer, mockDeviceBitfield}));
    allocation->getResidencyData().updateCompletionData(0u, 0u);

    memoryManager->handleFenceCompletion(allocation);
    EXPECT_EQ(0u, wddm->waitFromCpuResult.called);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenNonZeroFenceValueOnSingleEngineRegisteredWhenHandleFenceCompletionIsCalledThenWaitOnCpuOnce) {
    ASSERT_EQ(1u, memoryManager->getRegisteredEngines(0).size());

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties({0, 32, AllocationType::buffer, mockDeviceBitfield}));
    auto fence = &static_cast<OsContextWin *>(memoryManager->getRegisteredEngines(0)[0].osContext)->getResidencyController().getMonitoredFence();
    allocation->getResidencyData().updateCompletionData(129u, 0u);

    memoryManager->handleFenceCompletion(allocation);
    EXPECT_EQ(1u, wddm->waitFromCpuResult.called);
    EXPECT_EQ(129u, wddm->waitFromCpuResult.uint64ParamPassed);
    EXPECT_EQ(fence, wddm->waitFromCpuResult.monitoredFence);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenNonZeroFenceValuesOnMultipleEnginesRegisteredWhenHandleFenceCompletionIsCalledThenWaitOnCpuForEachEngineFromRootDevice) {
    const uint32_t rootDeviceIndex = 1;
    DeviceBitfield deviceBitfield(2);
    std::unique_ptr<CommandStreamReceiver> csr(createCommandStream(executionEnvironment, rootDeviceIndex, deviceBitfield));

    auto wddm2 = static_cast<WddmMock *>(executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->as<Wddm>());
    wddm2->callBaseWaitFromCpu = false;
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm2);

    auto hwInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
    auto &gfxCoreHelper = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHelper<GfxCoreHelper>();
    OsContext *osContext = memoryManager->createAndRegisterOsContext(csr.get(),
                                                                     EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment.rootDeviceEnvironments[rootDeviceIndex])[1],
                                                                                                                  PreemptionHelper::getDefaultPreemptionMode(*hwInfo), deviceBitfield));
    osContext->ensureContextInitialized(false);
    ASSERT_EQ(1u, memoryManager->getRegisteredEngines(rootDeviceIndex).size());

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties({0u, 32, AllocationType::buffer, mockDeviceBitfield}));
    auto lastEngineFence = &static_cast<OsContextWin *>(memoryManager->getRegisteredEngines(0)[0].osContext)->getResidencyController().getMonitoredFence();
    allocation->getResidencyData().updateCompletionData(129u, 0u);
    allocation->getResidencyData().updateCompletionData(152u, 1u);

    memoryManager->handleFenceCompletion(allocation);
    EXPECT_EQ(1u, wddm->waitFromCpuResult.called);
    EXPECT_EQ(0u, wddm2->waitFromCpuResult.called);
    EXPECT_EQ(129u, wddm->waitFromCpuResult.uint64ParamPassed);
    EXPECT_EQ(lastEngineFence, wddm->waitFromCpuResult.monitoredFence);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenNonZeroFenceValueOnSomeOfMultipleEnginesRegisteredWhenHandleFenceCompletionIsCalledThenWaitOnCpuForTheseEngines) {
    const uint32_t rootDeviceIndex = 1;

    DeviceBitfield deviceBitfield(2);
    std::unique_ptr<CommandStreamReceiver> csr(createCommandStream(executionEnvironment, rootDeviceIndex, deviceBitfield));
    std::unique_ptr<CommandStreamReceiver> csr2(createCommandStream(executionEnvironment, rootDeviceIndex, deviceBitfield));

    auto wddm2 = static_cast<WddmMock *>(executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->as<Wddm>());
    wddm2->callBaseWaitFromCpu = false;
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm2);
    auto &gfxCoreHelper = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHelper<GfxCoreHelper>();
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
    memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment.rootDeviceEnvironments[rootDeviceIndex])[1],
                                                                                                      PreemptionHelper::getDefaultPreemptionMode(*hwInfo), deviceBitfield));
    memoryManager->createAndRegisterOsContext(csr2.get(), EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment.rootDeviceEnvironments[rootDeviceIndex])[1],
                                                                                                       PreemptionHelper::getDefaultPreemptionMode(*hwInfo), deviceBitfield));
    ASSERT_EQ(2u, memoryManager->getRegisteredEngines(rootDeviceIndex).size());

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties({1u, 32, AllocationType::buffer, mockDeviceBitfield}));
    auto lastEngineFence = &static_cast<OsContextWin *>(memoryManager->getRegisteredEngines(1)[0].osContext)->getResidencyController().getMonitoredFence();
    allocation->getResidencyData().updateCompletionData(129u, 1u);
    allocation->getResidencyData().updateCompletionData(0, 2u);

    memoryManager->handleFenceCompletion(allocation);
    EXPECT_EQ(1u, wddm2->waitFromCpuResult.called);
    EXPECT_EQ(129u, wddm2->waitFromCpuResult.uint64ParamPassed);
    EXPECT_EQ(lastEngineFence, wddm2->waitFromCpuResult.monitoredFence);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenWddmMemoryManagerWhenSelectAlignmentAndHeapCalledThenCorrectHeapReturned) {
    HeapIndex heap = HeapIndex::heapStandard;
    auto alignment = memoryManager->selectAlignmentAndHeap(MemoryConstants::pageSize64k, &heap);
    EXPECT_EQ(heap, HeapIndex::heapStandard64KB);
    EXPECT_EQ(MemoryConstants::pageSize64k, alignment);
}

TEST_F(WddmMemoryManagerSimpleTest, givenWddmMemoryManagerWhenGpuAddressIsReservedAndFreedThenAddressRangeIsNonZero) {
    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    uint32_t rootDeviceIndexReserved = 1;
    HeapIndex heap = HeapIndex::heapStandard;
    auto alignment = memoryManager->selectAlignmentAndHeap(MemoryConstants::pageSize64k, &heap);
    EXPECT_EQ(heap, HeapIndex::heapStandard64KB);
    EXPECT_EQ(MemoryConstants::pageSize64k, alignment);
    auto addressRange = memoryManager->reserveGpuAddressOnHeap(0ull, MemoryConstants::pageSize64k, rootDeviceIndices, &rootDeviceIndexReserved, heap, alignment);
    auto gmmHelper = memoryManager->getGmmHelper(0);
    EXPECT_EQ(0u, rootDeviceIndexReserved);
    EXPECT_NE(0u, gmmHelper->decanonize(addressRange.address));
    EXPECT_EQ(MemoryConstants::pageSize64k, addressRange.size);

    memoryManager->freeGpuAddress(addressRange, 0);

    addressRange = memoryManager->reserveGpuAddress(0ull, MemoryConstants::pageSize64k, rootDeviceIndices, &rootDeviceIndexReserved);
    EXPECT_EQ(0u, rootDeviceIndexReserved);
    EXPECT_NE(0u, gmmHelper->decanonize(addressRange.address));
    EXPECT_EQ(MemoryConstants::pageSize64k, addressRange.size);

    memoryManager->freeGpuAddress(addressRange, 0);
}

TEST_F(WddmMemoryManagerSimpleTest, givenWddmMemoryManagerWhenGpuAddressIsReservedAndFreedThenRequestingTheSameAddressButCanonizedReturnedRequestedAddress) {
    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    uint32_t rootDeviceIndexReserved = 1;
    HeapIndex heap = HeapIndex::heapStandard;
    auto alignment = memoryManager->selectAlignmentAndHeap(MemoryConstants::pageSize64k, &heap);
    EXPECT_EQ(heap, HeapIndex::heapStandard64KB);
    EXPECT_EQ(MemoryConstants::pageSize64k, alignment);
    auto addressRange = memoryManager->reserveGpuAddressOnHeap(0ull, MemoryConstants::pageSize64k, rootDeviceIndices, &rootDeviceIndexReserved, heap, alignment);
    auto gmmHelper = memoryManager->getGmmHelper(0);
    EXPECT_EQ(0u, rootDeviceIndexReserved);
    EXPECT_NE(0u, gmmHelper->decanonize(addressRange.address));
    EXPECT_EQ(MemoryConstants::pageSize64k, addressRange.size);

    const auto addressRangeOriginal = addressRange;
    memoryManager->freeGpuAddress(addressRange, 0);

    addressRange = memoryManager->reserveGpuAddressOnHeap(gmmHelper->canonize(addressRangeOriginal.address), MemoryConstants::pageSize64k, rootDeviceIndices, &rootDeviceIndexReserved, heap, alignment);
    EXPECT_EQ(0u, rootDeviceIndexReserved);
    EXPECT_EQ(addressRangeOriginal.address, addressRange.address);
    EXPECT_EQ(MemoryConstants::pageSize64k, addressRange.size);

    memoryManager->freeGpuAddress(addressRange, 0);
}

TEST_F(WddmMemoryManagerSimpleTest, givenWddmMemoryManagerWhenCpuAddressIsReservedAndFreedThenAddressRangeIsNonZero) {
    auto addressRange = memoryManager->reserveCpuAddress(0, 1234);
    EXPECT_NE(0u, addressRange.address);
    EXPECT_EQ(1234u, addressRange.size);
    memoryManager->freeCpuAddress(addressRange);
}

TEST_F(WddmMemoryManagerSimpleTest, givenWddmMemoryManagerWhenAllocatingWithGpuVaThenNullptrIsReturned) {
    AllocationData allocationData;

    allocationData.size = 0x1000;
    allocationData.gpuAddress = 0x2000;
    allocationData.osContext = osContext;

    auto allocation = memoryManager->allocateGraphicsMemoryWithGpuVa(allocationData);
    EXPECT_EQ(nullptr, allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, whenAllocationCreatedFromSharedHandleIsDestroyedThenDestroyAllocationFromGdiIsNotInvoked) {
    gdi->getQueryResourceInfoArgOut().NumAllocations = 1;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), nullptr, 0, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));

    D3DDDI_OPENALLOCATIONINFO allocationInfo;
    allocationInfo.pPrivateDriverData = gmm->gmmResourceInfo->peekHandle();
    allocationInfo.hAllocation = ALLOCATION_HANDLE;
    allocationInfo.PrivateDriverDataSize = sizeof(GMM_RESOURCE_INFO);

    VariableBackup<D3DDDI_OPENALLOCATIONINFO *> openResourceBackup(&gdi->getOpenResourceArgOut().pOpenAllocationInfo, &allocationInfo);

    AllocationProperties properties(0, false, 0, AllocationType::sharedBuffer, false, false, 0);
    MockWddmMemoryManager::OsHandleData osHandleData{1u};

    auto allocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    EXPECT_NE(nullptr, allocation);

    memoryManager->setDeferredDeleter(nullptr);
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(1u, memoryManager->freeGraphicsMemoryImplCalled);

    gdi->getDestroyArg().AllocationCount = 7;
    auto destroyArg = gdi->getDestroyArg();
    EXPECT_EQ(7u, destroyArg.AllocationCount);
    gdi->getDestroyArg().AllocationCount = 0;
}
TEST_F(WddmMemoryManagerSimpleTest, whenDestroyingLockedAllocationIfDeviceRequiresMakeResidentPriorToLockThenCallEvictDoNotCallOtherwise) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForcePreferredAllocationMethod.set(static_cast<int32_t>(GfxMemoryAllocationMethod::useUmdSystemPtr));
    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize}));
    memoryManager->lockResource(allocation);
    auto makeResidentPriorToLockRequired = memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[0u]->getHelper<GfxCoreHelper>().makeResidentBeforeLockNeeded(allocation->needsMakeResidentBeforeLock());
    EXPECT_EQ(makeResidentPriorToLockRequired, allocation->needsMakeResidentBeforeLock());
    memoryManager->freeGraphicsMemory(allocation);
    if (makeResidentPriorToLockRequired) {
        EXPECT_EQ(1u, mockTemporaryResources->removeResourceResult.called);
    } else {
        EXPECT_EQ(0u, mockTemporaryResources->removeResourceResult.called);
    }
    EXPECT_EQ(0u, mockTemporaryResources->evictResourceResult.called);
}

TEST_F(WddmMemoryManagerSimpleTest, givenLocalMemoryKernelIsaWithMemoryCopiedWhenDestroyingAllocationIfDeviceRequiresMakeResidentPriorToLockThenRemoveFromTemporaryResources) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);
    char data{};
    AllocationProperties properties{0, true, sizeof(data), AllocationType::kernelIsa, false, false, 0};
    properties.subDevicesBitfield.set(0);
    memoryManager->localMemorySupported[properties.rootDeviceIndex] = true;
    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(properties));
    ASSERT_NE(nullptr, allocation);
    memoryManager->copyMemoryToAllocation(allocation, 0, &data, sizeof(data));

    auto makeResidentPriorToLockRequired = memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[0u]->getHelper<GfxCoreHelper>().makeResidentBeforeLockNeeded(allocation->needsMakeResidentBeforeLock());
    EXPECT_EQ(makeResidentPriorToLockRequired, allocation->needsMakeResidentBeforeLock());
    EXPECT_EQ(makeResidentPriorToLockRequired, allocation->isExplicitlyMadeResident());
    memoryManager->freeGraphicsMemory(allocation);
    if (makeResidentPriorToLockRequired) {
        EXPECT_EQ(1u, mockTemporaryResources->removeResourceResult.called);
        EXPECT_EQ(1u, mockTemporaryResources->evictResourceResult.called);
    } else {
        EXPECT_EQ(0u, mockTemporaryResources->removeResourceResult.called);
        EXPECT_EQ(0u, mockTemporaryResources->evictResourceResult.called);
    }
}
TEST_F(WddmMemoryManagerSimpleTest, whenDestroyingNotLockedAllocationThatDoesntNeedMakeResidentBeforeLockThenDontEvictAllocationFromWddmTemporaryResources) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForcePreferredAllocationMethod.set(static_cast<int32_t>(GfxMemoryAllocationMethod::useUmdSystemPtr));
    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize}));
    EXPECT_FALSE(allocation->isLocked());
    EXPECT_FALSE(allocation->needsMakeResidentBeforeLock());
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(0u, mockTemporaryResources->evictResourceResult.called);
}
TEST_F(WddmMemoryManagerSimpleTest, whenDestroyingLockedAllocationThatNeedsMakeResidentBeforeLockThenRemoveTemporaryResource) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForcePreferredAllocationMethod.set(static_cast<int32_t>(GfxMemoryAllocationMethod::useUmdSystemPtr));
    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize}));
    allocation->setMakeResidentBeforeLockRequired(true);
    memoryManager->lockResource(allocation);
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(1u, mockTemporaryResources->removeResourceResult.called);
}
TEST_F(WddmMemoryManagerSimpleTest, whenDestroyingNotLockedAllocationThatNeedsMakeResidentBeforeLockThenDontEvictAllocationFromWddmTemporaryResources) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForcePreferredAllocationMethod.set(static_cast<int32_t>(GfxMemoryAllocationMethod::useUmdSystemPtr));
    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize}));
    allocation->setMakeResidentBeforeLockRequired(true);
    EXPECT_FALSE(allocation->isLocked());
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(0u, mockTemporaryResources->evictResourceResult.called);
}
TEST_F(WddmMemoryManagerSimpleTest, whenDestroyingAllocationWithReservedGpuVirtualAddressThenReleaseTheAddress) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForcePreferredAllocationMethod.set(static_cast<int32_t>(GfxMemoryAllocationMethod::useUmdSystemPtr));
    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize}));
    uint64_t gpuAddress = 0x123;
    uint64_t sizeForFree = 0x1234;
    allocation->setReservedGpuVirtualAddress(gpuAddress);
    allocation->setReservedSizeForGpuVirtualAddress(sizeForFree);
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(1u, wddm->freeGpuVirtualAddressResult.called);
    EXPECT_EQ(gpuAddress, wddm->freeGpuVirtualAddressResult.uint64ParamPassed);
    EXPECT_EQ(sizeForFree, wddm->freeGpuVirtualAddressResult.sizePassed);
}

TEST_F(WddmMemoryManagerSimpleTest, givenAllocationWithReservedGpuVirtualAddressWhenMapCallFailsDuringCreateWddmAllocationThenReleasePreferredAddress) {
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper(), 1);
    allocation.setAllocationType(AllocationType::kernelIsa);
    uint64_t gpuAddress = 0x123;
    uint64_t sizeForFree = 0x1234;
    allocation.setReservedGpuVirtualAddress(gpuAddress);
    allocation.setReservedSizeForGpuVirtualAddress(sizeForFree);

    wddm->callBaseMapGpuVa = false;
    wddm->mapGpuVaStatus = false;

    memoryManager->createWddmAllocation(&allocation, nullptr);
    EXPECT_EQ(1u, wddm->freeGpuVirtualAddressResult.called);
    EXPECT_EQ(gpuAddress, wddm->freeGpuVirtualAddressResult.uint64ParamPassed);
    EXPECT_EQ(sizeForFree, wddm->freeGpuVirtualAddressResult.sizePassed);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMultiHandleAllocationAndPreferredGpuVaIsSpecifiedWhenCreateAllocationIsCalledThenAllocationHasProperGpuAddressAndHeapSvmIsUsed) {
    if (memoryManager->isLimitedRange(0)) {
        GTEST_SKIP();
    }

    uint32_t numGmms = 10;
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper(), numGmms);
    allocation.setAllocationType(AllocationType::buffer);
    allocation.storageInfo.multiStorage = true;

    wddm->callBaseMapGpuVa = true;

    uint64_t gpuPreferredVa = 0x20000ull;

    memoryManager->createWddmAllocation(&allocation, reinterpret_cast<void *>(gpuPreferredVa));
    EXPECT_EQ(gpuPreferredVa, allocation.getGpuAddress());
    EXPECT_EQ(numGmms, wddm->mapGpuVirtualAddressResult.called);

    auto gmmSize = allocation.getDefaultGmm()->gmmResourceInfo->getSizeAllocation();
    auto lastRequiredAddress = (numGmms - 1) * gmmSize + gpuPreferredVa;
    EXPECT_EQ(lastRequiredAddress, wddm->mapGpuVirtualAddressResult.uint64ParamPassed);
    EXPECT_GT(lastRequiredAddress, memoryManager->getGfxPartition(0)->getHeapMinimalAddress(HeapIndex::heapSvm));
    EXPECT_LT(lastRequiredAddress, memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapSvm));

    wddm->destroyAllocation(&allocation, nullptr);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMultiHandleAllocationWhenCreatePhysicalAllocationIsCalledThenAllocationSuccess) {
    if (memoryManager->isLimitedRange(0)) {
        GTEST_SKIP();
    }

    uint32_t numGmms = 10;
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper(), numGmms);
    allocation.setAllocationType(AllocationType::buffer);
    allocation.storageInfo.multiStorage = true;

    wddm->callBaseMapGpuVa = true;

    memoryManager->createPhysicalAllocation(&allocation);
    EXPECT_EQ(0ull, allocation.getGpuAddress());
    EXPECT_EQ(0u, wddm->mapGpuVirtualAddressResult.called);
    wddm->destroyAllocation(&allocation, nullptr);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMultiHandleAllocationWhenCreatePhysicalAllocationIsCalledThenFailureReturned) {
    if (memoryManager->isLimitedRange(0)) {
        GTEST_SKIP();
    }

    uint32_t numGmms = 10;
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper(), numGmms);
    allocation.setAllocationType(AllocationType::buffer);
    allocation.storageInfo.multiStorage = true;

    wddm->callBaseMapGpuVa = true;

    wddm->failCreateAllocation = true;
    auto ret = memoryManager->createPhysicalAllocation(&allocation);
    EXPECT_FALSE(ret);
}

TEST_F(WddmMemoryManagerSimpleTest, givenSvmCpuAllocationWhenSizeAndAlignmentProvidedThenAllocateMemoryReserveGpuVa) {
    if (memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }

    size_t size = 2 * MemoryConstants::megaByte;
    MockAllocationProperties properties{csr->getRootDeviceIndex(), true, size, AllocationType::svmCpu, mockDeviceBitfield};
    properties.alignment = size;
    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(properties));
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(allocation->getUnderlyingBuffer(), allocation->getDriverAllocatedCpuPtr());
    // limited platforms will not use heap HeapIndex::heapSvm
    if (executionEnvironment.rootDeviceEnvironments[allocation->getRootDeviceIndex()]->isFullRangeSvm()) {
        EXPECT_EQ(alignUp(allocation->getReservedAddressPtr(), size), reinterpret_cast<void *>(allocation->getGpuAddress()));
    }
    EXPECT_EQ((2 * size), allocation->getReservedAddressSize());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenWriteCombinedAllocationThenCpuAddressIsEqualToGpuAddress) {
    if (is32bit) {
        GTEST_SKIP();
    }
    memoryManager.reset(new MockWddmMemoryManager(true, true, executionEnvironment));
    size_t size = 2 * MemoryConstants::megaByte;
    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties({0, size, AllocationType::writeCombined, mockDeviceBitfield}));
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_NE(nullptr, reinterpret_cast<void *>(allocation->getGpuAddress()));

    if (executionEnvironment.rootDeviceEnvironments[allocation->getRootDeviceIndex()]->isFullRangeSvm()) {
        EXPECT_EQ(allocation->getUnderlyingBuffer(), reinterpret_cast<void *>(allocation->getGpuAddress()));
    }

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenDebugVariableWhenCreatingWddmMemoryManagerThenSetSupportForMultiStorageResources) {
    DebugManagerStateRestore restore;
    EXPECT_TRUE(memoryManager->supportsMultiStorageResources);

    {
        debugManager.flags.EnableMultiStorageResources.set(0);
        MockWddmMemoryManager memoryManager(true, true, executionEnvironment);
        EXPECT_FALSE(memoryManager.supportsMultiStorageResources);
    }

    {
        debugManager.flags.EnableMultiStorageResources.set(1);
        MockWddmMemoryManager memoryManager(true, true, executionEnvironment);
        EXPECT_TRUE(memoryManager.supportsMultiStorageResources);
    }
}

TEST_F(WddmMemoryManagerSimpleTest, givenBufferHostMemoryAllocationAndLimitedRangeAnd32BitThenAllocationGoesToExternalHeap) {
    if (executionEnvironment.rootDeviceEnvironments[0]->isFullRangeSvm() || !is32bit) {
        GTEST_SKIP();
    }

    memoryManager.reset(new MockWddmMemoryManager(true, true, executionEnvironment));
    size_t size = 2 * MemoryConstants::megaByte;
    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties({0, size, AllocationType::bufferHostMemory, mockDeviceBitfield}));
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    uint64_t gpuAddress = allocation->getGpuAddress();
    EXPECT_NE(0ULL, gpuAddress);
    EXPECT_EQ(0ULL, gpuAddress & 0xffFFffF000000000);

    auto gmmHelper = rootDeviceEnvironment->getGmmHelper();
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapExternal)), gpuAddress);
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapExternal)), gpuAddress);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenIsaTypeAnd32BitFrontWindowWhenFrontWindowMemoryAllocatedAndFreedThenFrontWindowHeapAllocatorIsUsed) {
    size_t size = 1 * MemoryConstants::kiloByte + 512;

    auto gfxPartition = new MockGfxPartition();
    gfxPartition->callHeapAllocate = false;

    memoryManager->gfxPartitions[0].reset(gfxPartition);
    AllocationProperties props{0, size, AllocationType::kernelIsa, mockDeviceBitfield};
    props.flags.use32BitFrontWindow = 1;

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(props));
    ASSERT_NE(nullptr, allocation);

    EXPECT_LE(size, allocation->getUnderlyingBufferSize());
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_TRUE(allocation->isAllocInFrontWindowPool());

    uint64_t gpuAddress = allocation->getGpuAddress();
    EXPECT_NE(0ULL, gpuAddress);
    EXPECT_EQ(gfxPartition->mockGpuVa, gpuAddress);

    gfxPartition->heapFreePtr = 0;
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(gpuAddress, gfxPartition->heapFreePtr);
}

HWTEST2_F(WddmMemoryManagerSimpleTest, givenLocalMemoryIsaTypeAnd32BitFrontWindowWhenFrontWindowMemoryAllocatedAndFreedThenFrontWindowHeapAllocatorIsUsed, MatchAny) {
    DebugManagerStateRestore restore{};
    debugManager.flags.ForceLocalMemoryAccessMode.set(0);

    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrLocalMemory = true;
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hwInfo);
    executionEnvironment.rootDeviceEnvironments[0]->initGmm();

    const auto localMemoryEnabled = true;
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, localMemoryEnabled, executionEnvironment);

    size_t size = 1 * MemoryConstants::kiloByte + 512;

    auto gfxPartition = new MockGfxPartition();
    gfxPartition->callHeapAllocate = false;

    memoryManager->gfxPartitions[0].reset(gfxPartition);
    AllocationProperties props{0, size, AllocationType::kernelIsa, mockDeviceBitfield};
    props.flags.use32BitFrontWindow = 1;

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(props));
    ASSERT_NE(nullptr, allocation);

    EXPECT_LE(alignUp(size, MemoryConstants::pageSize64k), allocation->getUnderlyingBufferSize());
    EXPECT_TRUE(allocation->isAllocInFrontWindowPool());

    uint64_t gpuAddress = allocation->getGpuAddress();
    EXPECT_NE(0ULL, gpuAddress);
    EXPECT_EQ(gfxPartition->mockGpuVa, gpuAddress);

    gfxPartition->heapFreePtr = 0;
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(gpuAddress, gfxPartition->heapFreePtr);
}

TEST_F(WddmMemoryManagerSimpleTest, givenDebugModuleAreaTypeWhenCreatingAllocationThen32BitAllocationWithFrontWindowGpuVaIsReturned) {
    const auto size = MemoryConstants::pageSize64k;

    NEO::AllocationProperties properties{0, true, size,
                                         NEO::AllocationType::debugModuleArea,
                                         false,
                                         mockDeviceBitfield};

    auto moduleDebugArea = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    EXPECT_NE(nullptr, moduleDebugArea);
    EXPECT_NE(nullptr, moduleDebugArea->getUnderlyingBuffer());
    EXPECT_GE(moduleDebugArea->getUnderlyingBufferSize(), size);

    auto address64bit = moduleDebugArea->getGpuAddressToPatch();
    EXPECT_LT(address64bit, MemoryConstants::max32BitAddress);
    EXPECT_TRUE(moduleDebugArea->is32BitAllocation());

    auto gmmHelper = rootDeviceEnvironment->getGmmHelper();
    auto frontWindowBase = gmmHelper->canonize(memoryManager->getGfxPartition(moduleDebugArea->getRootDeviceIndex())->getHeapBase(memoryManager->selectInternalHeap(moduleDebugArea->isAllocatedInLocalMemoryPool())));
    EXPECT_EQ(frontWindowBase, moduleDebugArea->getGpuBaseAddress());
    EXPECT_EQ(frontWindowBase, moduleDebugArea->getGpuAddress());

    memoryManager->freeGraphicsMemory(moduleDebugArea);
}

HWTEST2_F(WddmMemoryManagerSimpleTest, givenEnabledLocalMemoryWhenAllocatingDebugAreaThenHeapInternalDeviceFrontWindowIsUsed, MatchAny) {
    DebugManagerStateRestore restore{};
    debugManager.flags.ForceLocalMemoryAccessMode.set(0);
    const auto size = MemoryConstants::pageSize64k;
    const auto localMemoryEnabled = true;

    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrLocalMemory = true;
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hwInfo);
    executionEnvironment.rootDeviceEnvironments[0]->initGmm();

    memoryManager = std::make_unique<MockWddmMemoryManager>(false, localMemoryEnabled, executionEnvironment);
    NEO::AllocationProperties properties{0, true, size,
                                         NEO::AllocationType::debugModuleArea,
                                         false,
                                         mockDeviceBitfield};

    auto moduleDebugArea = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    EXPECT_NE(nullptr, moduleDebugArea);
    EXPECT_NE(nullptr, moduleDebugArea->getUnderlyingBuffer());
    EXPECT_GE(moduleDebugArea->getUnderlyingBufferSize(), size);

    auto address64bit = moduleDebugArea->getGpuAddressToPatch();
    EXPECT_LT(address64bit, MemoryConstants::max32BitAddress);
    EXPECT_TRUE(moduleDebugArea->isAllocatedInLocalMemoryPool());

    auto gmmHelper = rootDeviceEnvironment->getGmmHelper();
    auto frontWindowBase = gmmHelper->canonize(memoryManager->getGfxPartition(moduleDebugArea->getRootDeviceIndex())->getHeapBase(memoryManager->selectInternalHeap(moduleDebugArea->isAllocatedInLocalMemoryPool())));
    EXPECT_EQ(frontWindowBase, moduleDebugArea->getGpuBaseAddress());
    EXPECT_EQ(frontWindowBase, moduleDebugArea->getGpuAddress());

    memoryManager->freeGraphicsMemory(moduleDebugArea);
}

TEST_F(WddmMemoryManagerSimpleTest, givenEnabledLocalMemoryWhenAllocatingMemoryInFrontWindowThenCorrectHeapIsUsedForGpuVa) {
    DebugManagerStateRestore restore{};
    debugManager.flags.ForceLocalMemoryAccessMode.set(0);
    const auto size = MemoryConstants::pageSize64k;
    const auto localMemoryEnabled = true;

    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrLocalMemory = true;
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hwInfo);
    executionEnvironment.rootDeviceEnvironments[0]->initGmm();

    memoryManager = std::make_unique<MockWddmMemoryManager>(false, localMemoryEnabled, executionEnvironment);
    NEO::AllocationProperties properties{0, true, size,
                                         NEO::AllocationType::buffer,
                                         false,
                                         mockDeviceBitfield};
    properties.flags.use32BitFrontWindow = true;

    auto buffer = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    EXPECT_NE(nullptr, buffer);
    EXPECT_GE(buffer->getUnderlyingBufferSize(), size);

    auto address64bit = buffer->getGpuAddressToPatch();
    EXPECT_NE(0u, address64bit);
    EXPECT_TRUE(buffer->isAllocatedInLocalMemoryPool());

    auto gmmHelper = rootDeviceEnvironment->getGmmHelper();
    auto heapBase = gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapStandard64KB));
    auto heapLimit = gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapStandard64KB));

    EXPECT_NE(0u, buffer->getGpuAddress());

    const auto &productHelper = rootDeviceEnvironment->getHelper<ProductHelper>();

    if (!productHelper.overrideGfxPartitionLayoutForWsl() && is64bit) {
        EXPECT_LE(heapBase, buffer->getGpuAddress());
        EXPECT_GT(heapLimit, buffer->getGpuAddress());
    }

    memoryManager->freeGraphicsMemory(buffer);
}

TEST_F(WddmMemoryManagerSimpleTest, whenWddmMemoryManagerIsCreatedThenAlignmentSelectorHasExpectedAlignments) {
    std::vector<AlignmentSelector::CandidateAlignment> expectedAlignments = {
        {MemoryConstants::pageSize2M, false, 0.1f, HeapIndex::totalHeaps},
        {MemoryConstants::pageSize64k, true, AlignmentSelector::anyWastage, HeapIndex::totalHeaps},
    };

    MockWddmMemoryManager memoryManager(true, true, executionEnvironment);
    EXPECT_EQ(expectedAlignments, memoryManager.alignmentSelector.peekCandidateAlignments());
}

TEST_F(WddmMemoryManagerSimpleTest, given2MbPagesDisabledWhenWddmMemoryManagerIsCreatedThenAlignmentSelectorHasExpectedAlignments) {
    DebugManagerStateRestore restore{};
    debugManager.flags.AlignLocalMemoryVaTo2MB.set(0);

    std::vector<AlignmentSelector::CandidateAlignment> expectedAlignments = {
        {MemoryConstants::pageSize64k, true, AlignmentSelector::anyWastage, HeapIndex::totalHeaps},
    };

    MockWddmMemoryManager memoryManager(true, true, executionEnvironment);
    EXPECT_EQ(expectedAlignments, memoryManager.alignmentSelector.peekCandidateAlignments());
}

TEST_F(WddmMemoryManagerSimpleTest, givenCustomAlignmentWhenWddmMemoryManagerIsCreatedThenAlignmentSelectorHasExpectedAlignments) {
    DebugManagerStateRestore restore{};

    {
        debugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(MemoryConstants::megaByte);
        std::vector<AlignmentSelector::CandidateAlignment> expectedAlignments = {
            {MemoryConstants::pageSize2M, false, 0.1f, HeapIndex::totalHeaps},
            {MemoryConstants::megaByte, false, AlignmentSelector::anyWastage, HeapIndex::totalHeaps},
            {MemoryConstants::pageSize64k, true, AlignmentSelector::anyWastage, HeapIndex::totalHeaps},
        };
        MockWddmMemoryManager memoryManager(true, true, executionEnvironment);
        EXPECT_EQ(expectedAlignments, memoryManager.alignmentSelector.peekCandidateAlignments());
    }

    {
        debugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(2 * MemoryConstants::pageSize2M);
        std::vector<AlignmentSelector::CandidateAlignment> expectedAlignments = {
            {2 * MemoryConstants::pageSize2M, false, AlignmentSelector::anyWastage, HeapIndex::totalHeaps},
            {MemoryConstants::pageSize2M, false, 0.1f, HeapIndex::totalHeaps},
            {MemoryConstants::pageSize64k, true, AlignmentSelector::anyWastage, HeapIndex::totalHeaps},
        };
        MockWddmMemoryManager memoryManager(true, true, executionEnvironment);
        EXPECT_EQ(expectedAlignments, memoryManager.alignmentSelector.peekCandidateAlignments());
    }
}

TEST_F(WddmMemoryManagerSimpleTest, whenAlignmentRequirementExceedsPageSizeThenAllocateGraphicsMemoryFromSystemPtr) {
    struct MockWddmMemoryManagerAllocateWithAlignment : MockWddmMemoryManager {
        using MockWddmMemoryManager::MockWddmMemoryManager;

        GraphicsAllocation *allocateSystemMemoryAndCreateGraphicsAllocationFromIt(const AllocationData &allocationData) override {
            ++callCount.allocateSystemMemoryAndCreateGraphicsAllocationFromIt;
            return nullptr;
        }
        GraphicsAllocation *allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(const AllocationData &allocationData, bool allowLargePages) override {
            ++callCount.allocateGraphicsMemoryUsingKmdAndMapItToCpuVA;
            return nullptr;
        }

        struct {
            int allocateSystemMemoryAndCreateGraphicsAllocationFromIt = 0;
            int allocateGraphicsMemoryUsingKmdAndMapItToCpuVA = 0;
        } callCount;
    };

    MockWddmMemoryManagerAllocateWithAlignment memoryManager(true, true, executionEnvironment);

    AllocationProperties allocationProperties{0u, 1024, NEO::AllocationType::buffer, {}};
    NEO::AllocationData allocData = {};
    allocData.rootDeviceIndex = allocationProperties.rootDeviceIndex;
    allocData.type = allocationProperties.allocationType;
    allocData.size = allocationProperties.size;
    allocData.allocationMethod = memoryManager.getPreferredAllocationMethod(allocationProperties);
    allocData.alignment = MemoryConstants::pageSize64k * 4;
    memoryManager.allocateGraphicsMemoryWithAlignment(allocData);
    EXPECT_EQ(1, memoryManager.callCount.allocateSystemMemoryAndCreateGraphicsAllocationFromIt);
    EXPECT_EQ(0, memoryManager.callCount.allocateGraphicsMemoryUsingKmdAndMapItToCpuVA);

    memoryManager.callCount.allocateSystemMemoryAndCreateGraphicsAllocationFromIt = 0;
    memoryManager.callCount.allocateGraphicsMemoryUsingKmdAndMapItToCpuVA = 0;

    allocData.size = 1024;
    allocData.alignment = MemoryConstants::pageSize;
    memoryManager.allocateGraphicsMemoryWithAlignment(allocData);
    if (allocData.allocationMethod == GfxMemoryAllocationMethod::allocateByKmd) {
        EXPECT_EQ(0, memoryManager.callCount.allocateSystemMemoryAndCreateGraphicsAllocationFromIt);
        EXPECT_EQ(1, memoryManager.callCount.allocateGraphicsMemoryUsingKmdAndMapItToCpuVA);
    } else {
        EXPECT_EQ(1, memoryManager.callCount.allocateSystemMemoryAndCreateGraphicsAllocationFromIt);
        EXPECT_EQ(0, memoryManager.callCount.allocateGraphicsMemoryUsingKmdAndMapItToCpuVA);
    }
}

TEST_F(WddmMemoryManagerSimpleTest, givenUseSystemMemorySetToTrueWhenAllocateInDevicePoolIsCalledThenNullptrIsReturned) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, executionEnvironment));
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.useSystemMemory = true;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}

TEST_F(WddmMemoryManagerSimpleTest, givenNotSetUseSystemMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedThenLocalMemoryAllocationIsReturned) {
    const bool localMemoryEnabled = true;

    memoryManager = std::make_unique<MockWddmMemoryManager>(false, localMemoryEnabled, executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_EQ(0u, allocation->getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenShareableAllocationWhenAllocateInDevicePoolThenMemoryIsNotLocableAndLocalOnlyIsSet) {
    const bool localMemoryEnabled = true;

    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrLocalMemory = true;
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hwInfo);
    executionEnvironment.rootDeviceEnvironments[0]->initGmm();

    memoryManager = std::make_unique<MockWddmMemoryManager>(false, localMemoryEnabled, executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.type = AllocationType::svmGpu;
    allocData.storageInfo.localOnlyRequired = true;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.shareable = true;
    allocData.storageInfo.memoryBanks = 2;
    allocData.storageInfo.systemMemoryPlacement = false;
    allocData.alignment = MemoryConstants::pageSize64k;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_EQ(0u, allocation->getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly);
    EXPECT_TRUE(isAligned<MemoryConstants::pageSize64k>(allocation->getUnderlyingBufferSize()));
    uint64_t handle = 0;
    allocation->peekInternalHandle(memoryManager.get(), handle);
    EXPECT_NE(handle, 0u);

    EXPECT_EQ(1u, allocation->getDefaultGmm()->resourceParams.Flags.Info.LocalOnly);
    EXPECT_EQ(1u, allocation->getDefaultGmm()->resourceParams.Flags.Info.NotLockable);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenShareableAllocationWhenAllocateGraphicsMemoryInPreferredPoolThenMemoryIsNotLockableAndLocalOnlyIsSetCorrectly) {
    const bool localMemoryEnabled = true;

    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrLocalMemory = true;
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hwInfo);
    executionEnvironment.rootDeviceEnvironments[0]->initGmm();
    auto &productHelper = executionEnvironment.rootDeviceEnvironments[0]->getProductHelper();

    memoryManager = std::make_unique<MockWddmMemoryManager>(false, localMemoryEnabled, executionEnvironment);
    auto releaseHelper = std::make_unique<MockReleaseHelper>();
    releaseHelper->isLocalOnlyAllowedResult = true;
    memoryManager->executionEnvironment.rootDeviceEnvironments[mockRootDeviceIndex]->releaseHelper.reset(releaseHelper.release());
    AllocationProperties properties{mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::svmGpu, mockDeviceBitfield};
    properties.allFlags = 0;
    properties.size = MemoryConstants::pageSize;
    properties.flags.allocateMemory = true;
    properties.flags.shareable = true;

    auto allocation = memoryManager->allocateGraphicsMemoryInPreferredPool(properties, nullptr);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_EQ(0u, allocation->getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly);
    uint64_t handle = 0;
    allocation->peekInternalHandle(memoryManager.get(), handle);
    EXPECT_NE(handle, 0u);

    EXPECT_EQ(!productHelper.useLocalPreferredForCacheableBuffers(), allocation->getDefaultGmm()->resourceParams.Flags.Info.LocalOnly);
    EXPECT_EQ(1u, allocation->getDefaultGmm()->resourceParams.Flags.Info.NotLockable);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenCustomAlignmentAndAllocationAsBigAsTheAlignmentWhenAllocationInDevicePoolIsCreatedThenUseCustomAlignment) {
    const uint32_t customAlignment = 4 * MemoryConstants::pageSize64k;
    const uint32_t expectedAlignment = customAlignment;
    const uint32_t size = 4 * MemoryConstants::pageSize64k;
    debugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(customAlignment);
    testAlignment(size, expectedAlignment);
    testAlignment(size + 1, expectedAlignment);
}

TEST_F(WddmMemoryManagerSimpleTest, givenCustomAlignmentAndAllocationNotAsBigAsTheAlignmentWhenAllocationInDevicePoolIsCreatedThenDoNotUseCustomAlignment) {
    const uint32_t customAlignment = 4 * MemoryConstants::pageSize64k;
    const uint32_t expectedAlignment = MemoryConstants::pageSize64k;
    const uint32_t size = 3 * MemoryConstants::pageSize64k;
    debugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(customAlignment);
    testAlignment(size, expectedAlignment);
}

TEST_F(WddmMemoryManagerSimpleTest, givenCustomAlignmentBiggerThan2MbAndAllocationBiggerThanCustomAlignmentWhenAllocationInDevicePoolIsCreatedThenUseCustomAlignment) {
    const uint32_t customAlignment = 4 * MemoryConstants::megaByte;
    const uint32_t expectedAlignment = customAlignment;
    const uint32_t size = 4 * MemoryConstants::megaByte;
    debugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(customAlignment);
    testAlignment(size, expectedAlignment);
    testAlignment(size + 1, expectedAlignment);
}

TEST_F(WddmMemoryManagerSimpleTest, givenCustomAlignmentBiggerThan2MbAndAllocationLessThanCustomAlignmentWhenAllocationInDevicePoolIsCreatedThenDoNotUseCustomAlignment) {
    const uint32_t customAlignment = 4 * MemoryConstants::megaByte;
    const uint32_t expectedAlignment = 2 * MemoryConstants::megaByte;
    const uint32_t size = 4 * MemoryConstants::megaByte - 1;
    debugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(customAlignment);
    testAlignment(size, expectedAlignment);
}

TEST_F(WddmMemoryManagerSimpleTest, givenForced2MBSizeAlignmentWhenAllocationInDevicePoolIsCreatedThenUseProperAlignment) {
    debugManager.flags.ExperimentalAlignLocalMemorySizeTo2MB.set(true);

    uint32_t size = 1;
    uint32_t expectedAlignment = MemoryConstants::pageSize2M;
    testAlignment(size, expectedAlignment);

    size = 1 + MemoryConstants::pageSize2M;
    testAlignment(size, expectedAlignment);
}

TEST_F(WddmMemoryManagerSimpleTest, givenAllocationLessThen2MbWhenAllocationInDevicePoolIsCreatedThenUse64KbAlignment) {
    const uint32_t expectedAlignment = MemoryConstants::pageSize64k;
    const uint32_t size = 2 * MemoryConstants::megaByte - 1;
    testAlignment(size, expectedAlignment);
}

TEST_F(WddmMemoryManagerSimpleTest, givenTooMuchMemoryWastedOn2MbAlignmentWhenAllocationInDevicePoolIsCreatedThenUse64kbAlignment) {
    const float threshold = 0.1f;

    {
        const uint32_t alignedSize = 4 * MemoryConstants::megaByte;
        const uint32_t maxAmountOfWastedMemory = static_cast<uint32_t>(alignedSize * threshold);
        testAlignment(alignedSize, MemoryConstants::pageSize2M);
        testAlignment(alignedSize - maxAmountOfWastedMemory + 1, MemoryConstants::pageSize2M);
        testAlignment(alignedSize - maxAmountOfWastedMemory - 1, MemoryConstants::pageSize64k);
    }

    {
        const uint32_t alignedSize = 8 * MemoryConstants::megaByte;
        const uint32_t maxAmountOfWastedMemory = static_cast<uint32_t>(alignedSize * threshold);
        testAlignment(alignedSize, MemoryConstants::pageSize2M);
        testAlignment(alignedSize - maxAmountOfWastedMemory + 1, MemoryConstants::pageSize2M);
        testAlignment(alignedSize - maxAmountOfWastedMemory - 1, MemoryConstants::pageSize64k);
    }
}

TEST_F(WddmMemoryManagerSimpleTest, givenBigAllocationWastingMaximumPossibleAmountOfMemorytWhenAllocationInDevicePoolIsCreatedThenStillUse2MbAlignment) {
    const uint32_t size = 200 * MemoryConstants::megaByte + 1; // almost entire 2MB page will be wasted
    testAlignment(size, MemoryConstants::pageSize2M);
}

TEST_F(WddmMemoryManagerSimpleTest, givenAtLeast2MbAllocationWhenAllocationInDevicePoolIsCreatedThenUse2MbAlignment) {
    const uint32_t size = 2 * MemoryConstants::megaByte;

    {
        debugManager.flags.AlignLocalMemoryVaTo2MB.set(-1);
        const uint32_t expectedAlignment = 2 * MemoryConstants::megaByte;
        testAlignment(size, expectedAlignment);
        testAlignment(2 * size, expectedAlignment);
    }
    {
        debugManager.flags.AlignLocalMemoryVaTo2MB.set(0);
        const uint32_t expectedAlignment = MemoryConstants::pageSize64k;
        testAlignment(size, expectedAlignment);
        testAlignment(2 * size, expectedAlignment);
    }
    {
        debugManager.flags.AlignLocalMemoryVaTo2MB.set(1);
        const uint32_t expectedAlignment = 2 * MemoryConstants::megaByte;
        testAlignment(size, expectedAlignment);
        testAlignment(2 * size, expectedAlignment);
    }
}

HWTEST_F(WddmMemoryManagerSimpleTest, givenLinearStreamWhenItIsAllocatedThenItIsInLocalMemoryHasCpuPointerAndHasStandardHeap64kbAsGpuAddress) {
    if (rootDeviceEnvironment->getHelper<ProductHelper>().overrideGfxPartitionLayoutForWsl()) {
        GTEST_SKIP();
    }
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, true, executionEnvironment);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, 4096u, AllocationType::linearStream, mockDeviceBitfield});

    ASSERT_NE(nullptr, graphicsAllocation);
    EXPECT_EQ(MemoryPool::localMemory, graphicsAllocation->getMemoryPool());
    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_TRUE(graphicsAllocation->isLocked());
    auto gpuAddress = graphicsAllocation->getGpuAddress();
    auto gpuAddressEnd = gpuAddress + 4096u;
    auto &partition = wddm->getGfxPartition();

    if (is64bit) {
        auto gmmHelper = memoryManager->getGmmHelper(graphicsAllocation->getRootDeviceIndex());
        if (executionEnvironment.rootDeviceEnvironments[graphicsAllocation->getRootDeviceIndex()]->isFullRangeSvm()) {
            EXPECT_GE(gpuAddress, gmmHelper->canonize(partition.Standard64KB.Base));
            EXPECT_LE(gpuAddressEnd, gmmHelper->canonize(partition.Standard64KB.Limit));
        } else {
            EXPECT_GE(gpuAddress, gmmHelper->canonize(partition.Standard.Base));
            EXPECT_LE(gpuAddressEnd, gmmHelper->canonize(partition.Standard.Limit));
        }
    } else {
        if (executionEnvironment.rootDeviceEnvironments[graphicsAllocation->getRootDeviceIndex()]->isFullRangeSvm()) {
            EXPECT_GE(gpuAddress, 0ull);
            EXPECT_LE(gpuAddress, UINT32_MAX);

            EXPECT_GE(gpuAddressEnd, 0ull);
            EXPECT_LE(gpuAddressEnd, UINT32_MAX);
        }
    }

    EXPECT_EQ(graphicsAllocation->getAllocationType(), AllocationType::linearStream);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenNotSetUseSystemMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedThenLocalMemoryAllocationHasCorrectStorageInfoAndFlushL3IsSet) {
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, true, executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.storageInfo.memoryBanks = 0x1;
    allocData.storageInfo.pageTablesVisibility = 0x2;
    allocData.storageInfo.cloningOfPageTables = false;
    allocData.flags.flushL3 = true;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);

    EXPECT_EQ(allocData.storageInfo.memoryBanks, allocation->storageInfo.memoryBanks);
    EXPECT_EQ(allocData.storageInfo.pageTablesVisibility, allocation->storageInfo.pageTablesVisibility);
    EXPECT_FALSE(allocation->storageInfo.cloningOfPageTables);
    EXPECT_TRUE(allocation->isFlushL3Required());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenEnabledLocalMemoryAndUseSytemMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedThenNullptrIsReturned) {
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, true, executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.useSystemMemory = true;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}

TEST_F(WddmMemoryManagerSimpleTest, givenEnabledLocalMemoryAndAllowed32BitAndForce32BitWhenGraphicsAllocationInDevicePoolIsAllocatedThenNullptrIsReturned) {
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, true, executionEnvironment);
    memoryManager->setForce32BitAllocations(true);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allow32Bit = true;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}

TEST_F(WddmMemoryManagerSimpleTest, givenEnabledLocalMemoryAndAllowed32BitWhen32BitIsNotForcedThenGraphicsAllocationInDevicePoolReturnsLocalMemoryAllocation) {
    const bool localMemoryEnabled = true;

    memoryManager = std::make_unique<MockWddmMemoryManager>(false, localMemoryEnabled, executionEnvironment);
    memoryManager->setForce32BitAllocations(false);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allow32Bit = true;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_EQ(0u, allocation->getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenEnabledLocalMemoryWhenAllocateFailsThenGraphicsAllocationInDevicePoolReturnsError) {
    const bool localMemoryEnabled = true;
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, localMemoryEnabled, executionEnvironment);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;

    wddm->callBaseDestroyAllocations = false;
    wddm->createAllocationStatus = STATUS_NO_MEMORY;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenEnabledLocalMemoryWhenAllocateFailsThenGraphicsAllocationInPhysicalLocalDeviceMemoryReturnsError) {
    const bool localMemoryEnabled = true;
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, localMemoryEnabled, executionEnvironment);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;

    wddm->callBaseDestroyAllocations = false;
    wddm->createAllocationStatus = STATUS_NO_MEMORY;

    auto allocation = memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenAllocatePhysicalLocalDeviceMemoryThenLocalMemoryAllocationHasCorrectStorageInfoAndNoGpuAddress) {
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, true, executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.storageInfo.memoryBanks = 0x1;
    allocData.storageInfo.pageTablesVisibility = 0x2;
    allocData.storageInfo.cloningOfPageTables = false;
    allocData.flags.flushL3 = true;

    auto allocation = memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status);
    EXPECT_NE(nullptr, allocation);

    EXPECT_EQ(allocData.storageInfo.memoryBanks, allocation->storageInfo.memoryBanks);
    EXPECT_EQ(allocData.storageInfo.pageTablesVisibility, allocation->storageInfo.pageTablesVisibility);
    EXPECT_FALSE(allocation->storageInfo.cloningOfPageTables);
    EXPECT_EQ(0u, allocation->getGpuAddress());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenAllocatePhysicalLocalDeviceMemoryWithMultiStorageThenLocalMemoryAllocationHasCorrectStorageInfoAndNoGpuAddress) {
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, true, executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize64k * 4;
    allocData.flags.allocateMemory = true;
    allocData.storageInfo.memoryBanks = 0b11;
    allocData.storageInfo.pageTablesVisibility = 0x2;
    allocData.storageInfo.cloningOfPageTables = false;
    allocData.flags.flushL3 = true;
    allocData.storageInfo.multiStorage = true;

    auto allocation = memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status);
    EXPECT_NE(nullptr, allocation);

    EXPECT_EQ(allocData.storageInfo.memoryBanks, allocation->storageInfo.memoryBanks);
    EXPECT_EQ(allocData.storageInfo.pageTablesVisibility, allocation->storageInfo.pageTablesVisibility);
    EXPECT_FALSE(allocation->storageInfo.cloningOfPageTables);
    EXPECT_EQ(0u, allocation->getGpuAddress());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenAllocatePhysicalLocalDeviceMemoryWithMultiBanksThenLocalMemoryAllocationHasCorrectStorageInfoAndNoGpuAddress) {
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, true, executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize64k * 4;
    allocData.flags.allocateMemory = true;
    allocData.storageInfo.memoryBanks = 0b11;
    allocData.storageInfo.pageTablesVisibility = 0x2;
    allocData.storageInfo.cloningOfPageTables = false;
    allocData.flags.flushL3 = true;
    allocData.storageInfo.multiStorage = false;

    auto allocation = memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status);
    EXPECT_NE(nullptr, allocation);

    EXPECT_EQ(allocData.storageInfo.memoryBanks, allocation->storageInfo.memoryBanks);
    EXPECT_EQ(allocData.storageInfo.pageTablesVisibility, allocation->storageInfo.pageTablesVisibility);
    EXPECT_FALSE(allocation->storageInfo.cloningOfPageTables);
    EXPECT_EQ(0u, allocation->getGpuAddress());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, whenMemoryIsAllocatedInLocalMemoryThenTheAllocationNeedsMakeResidentBeforeLock) {
    const bool localMemoryEnabled = true;
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, localMemoryEnabled, executionEnvironment);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status));
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_TRUE(allocation->needsMakeResidentBeforeLock());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenAllocationWithHighPriorityWhenMemoryIsAllocatedInLocalMemoryThenSetAllocationPriorityIsCalledWithHighPriority) {
    const bool localMemoryEnabled = true;
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, localMemoryEnabled, executionEnvironment);

    AllocationType highPriorityTypes[] = {
        AllocationType::kernelIsa,
        AllocationType::kernelIsaInternal,
        AllocationType::commandBuffer,
        AllocationType::internalHeap,
        AllocationType::linearStream

    };
    for (auto &allocationType : highPriorityTypes) {

        MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
        AllocationData allocData;
        allocData.allFlags = 0;
        allocData.size = MemoryConstants::pageSize;
        allocData.flags.allocateMemory = true;
        allocData.type = allocationType;

        auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status));
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
        EXPECT_EQ(1u, wddm->setAllocationPriorityResult.called);
        EXPECT_EQ(DXGI_RESOURCE_PRIORITY_HIGH, wddm->setAllocationPriorityResult.uint64ParamPassed);

        wddm->setAllocationPriorityResult.called = 0u;
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(WddmMemoryManagerSimpleTest, givenAllocationWithoutHighPriorityWhenMemoryIsAllocatedInLocalMemoryThenSetAllocationPriorityIsCalledWithNormalPriority) {
    const bool localMemoryEnabled = true;
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, localMemoryEnabled, executionEnvironment);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status));
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(1u, wddm->setAllocationPriorityResult.called);
    EXPECT_EQ(static_cast<uint64_t>(DXGI_RESOURCE_PRIORITY_NORMAL), wddm->setAllocationPriorityResult.uint64ParamPassed);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenSetAllocationPriorityFailureWhenMemoryIsAllocatedInLocalMemoryThenNullptrIsReturned) {
    const bool localMemoryEnabled = true;
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, localMemoryEnabled, executionEnvironment);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;

    wddm->callBaseSetAllocationPriority = false;
    wddm->setAllocationPriorityResult.success = false;

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status));
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
}

TEST_F(WddmMemoryManagerSimpleTest, givenSetAllocationPriorityFailureWhenMemoryIsAllocatedInLocalPhysicalMemoryThenNullptrIsReturned) {
    const bool localMemoryEnabled = true;
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, localMemoryEnabled, executionEnvironment);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;

    wddm->callBaseSetAllocationPriority = false;
    wddm->setAllocationPriorityResult.success = false;

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status));
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
}

TEST_F(WddmMemoryManagerSimpleTest, givenSvmGpuAllocationWhenHostPtrProvidedThenUseHostPtrAsGpuVa) {
    size_t size = 2 * MemoryConstants::megaByte;
    AllocationProperties properties{mockRootDeviceIndex, false, size, AllocationType::svmGpu, false, mockDeviceBitfield};
    properties.alignment = size;
    void *svmPtr = reinterpret_cast<void *>(2 * size);
    memoryManager->localMemorySupported[properties.rootDeviceIndex] = true;
    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(properties, svmPtr));
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(nullptr, allocation->getDriverAllocatedCpuPtr());
    // limited platforms will not use heap HeapIndex::heapSvm
    if (executionEnvironment.rootDeviceEnvironments[0]->isFullRangeSvm()) {
        EXPECT_EQ(svmPtr, reinterpret_cast<void *>(allocation->getGpuAddress()));
    }
    EXPECT_EQ(nullptr, allocation->getReservedAddressPtr());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, given32BitAllocationOfBufferWhenItIsAllocatedThenItHas32BitGpuPointer) {
    if constexpr (is64bit) {
        GTEST_SKIP();
    }
    REQUIRE_SVM_OR_SKIP(defaultHwInfo);
    AllocationType allocationTypes[] = {AllocationType::buffer,
                                        AllocationType::sharedBuffer,
                                        AllocationType::scratchSurface,
                                        AllocationType::privateSurface};

    for (auto &allocationType : allocationTypes) {
        size_t size = 2 * MemoryConstants::kiloByte;
        auto alignedSize = alignUp(size, MemoryConstants::pageSize64k);
        AllocationProperties properties{mockRootDeviceIndex, size, allocationType, mockDeviceBitfield};
        memoryManager->localMemorySupported[properties.rootDeviceIndex] = true;
        auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(properties, nullptr));
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(alignedSize, allocation->getUnderlyingBufferSize());
        EXPECT_EQ(nullptr, allocation->getUnderlyingBuffer());
        EXPECT_EQ(nullptr, allocation->getDriverAllocatedCpuPtr());

        EXPECT_NE(nullptr, allocation->getReservedAddressPtr());
        EXPECT_EQ(alignedSize + 2 * MemoryConstants::megaByte, allocation->getReservedAddressSize());
        EXPECT_EQ(castToUint64(allocation->getReservedAddressPtr()), allocation->getGpuAddress());
        EXPECT_EQ(0u, allocation->getGpuAddress() % 2 * MemoryConstants::megaByte);

        EXPECT_GE(allocation->getGpuAddress(), 0u);
        EXPECT_LE(allocation->getGpuAddress(), MemoryConstants::max32BitAddress);

        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(WddmMemoryManagerSimpleTest, givenLocalMemoryAllocationAndRequestedSizeIsHugeThenResultAllocationIsSplitted) {
    givenLocalMemoryAllocationAndRequestedSizeIsHugeThenResultAllocationIsSplitted<is32bit>();
}

HWTEST_F(WddmMemoryManagerSimpleTest, givenWddmMemoryManagerWhenCopyDebugSurfaceToMultiTileAllocationThenCallCopyMemoryToAllocation) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForcePreferredAllocationMethod.set(static_cast<int32_t>(GfxMemoryAllocationMethod::useUmdSystemPtr));
    size_t sourceAllocationSize = MemoryConstants::pageSize;

    std::vector<uint8_t> dataToCopy(sourceAllocationSize, 1u);

    AllocationType debugSurfaces[] = {AllocationType::debugContextSaveArea, AllocationType::debugSbaTrackingBuffer};

    for (auto type : debugSurfaces) {
        AllocationProperties debugSurfaceProperties{0, true, sourceAllocationSize, type, false, false, 0b11};
        auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(debugSurfaceProperties);
        ASSERT_NE(nullptr, allocation);

        auto ret = memoryManager->copyMemoryToAllocation(allocation, 0, dataToCopy.data(), dataToCopy.size());
        EXPECT_TRUE(ret);
        EXPECT_EQ(0u, memoryManager->copyMemoryToAllocationBanksCalled);
        memoryManager->freeGraphicsMemory(allocation);
    }
}

HWTEST_F(WddmMemoryManagerSimpleTest, givenWddmMemoryManagerWhenCopyNotDebugSurfaceToMultiTileAllocationThenCallCopyMemoryToAllocationBanks) {
    size_t sourceAllocationSize = MemoryConstants::pageSize;
    std::vector<uint8_t> dataToCopy(sourceAllocationSize, 1u);

    AllocationProperties props{0, true, sourceAllocationSize, AllocationType::kernelIsa, false, false, 0};
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(props);
    ASSERT_NE(nullptr, allocation);
    allocation->storageInfo.memoryBanks = 0b11;
    memoryManager->copyMemoryToAllocation(allocation, 0, dataToCopy.data(), dataToCopy.size());
    EXPECT_EQ(1u, memoryManager->copyMemoryToAllocationBanksCalled);
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST2_F(WddmMemoryManagerSimpleTest, givenPreferCompressionFlagWhenAllocating64kbAllocationThenCreateGmmWithAuxFlags, IsAtMostXeHpgCore) {
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo();
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = true;

    if (memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    auto nonCompressedAllocation = memoryManager->allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, 1u, AllocationType::buffer, mockDeviceBitfield});
    EXPECT_EQ(0u, nonCompressedAllocation->getDefaultGmm()->resourceParams.Flags.Info.RenderCompressed);

    AllocationProperties properties = {mockRootDeviceIndex, 1u, AllocationType::buffer, mockDeviceBitfield};
    properties.flags.preferCompressed = true;
    auto compressedAllocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);
    EXPECT_EQ(1u, compressedAllocation->getDefaultGmm()->resourceParams.Flags.Info.RenderCompressed);

    memoryManager->freeGraphicsMemory(nonCompressedAllocation);
    memoryManager->freeGraphicsMemory(compressedAllocation);
}

struct WddmWithMockedLock : public WddmMock {
    using WddmMock::WddmMock;

    void *lockResource(const D3DKMT_HANDLE &handle, bool applyMakeResidentPriorToLock, size_t size) override {
        if (handle < storageLocked.size()) {
            storageLocked.set(handle);
        }
        return storages[handle];
    }
    std::bitset<4> storageLocked{};
    uint8_t storages[EngineLimits::maxHandleCount][MemoryConstants::pageSize64k] = {};
};

TEST(WddmMemoryManagerCopyMemoryToAllocationBanksTest, givenAllocationWithMultiTilePlacementWhenCopyDataSpecificMemoryBanksThenLockOnlySpecificStorages) {
    uint8_t sourceData[32]{};
    size_t offset = 3;
    size_t sourceAllocationSize = sizeof(sourceData);
    auto hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrLocalMemory = true;

    MockExecutionEnvironment executionEnvironment(&hwInfo);
    executionEnvironment.initGmm();
    auto wddm = new WddmWithMockedLock(*executionEnvironment.rootDeviceEnvironments[0]);
    wddm->init();
    MemoryManagerCreate<WddmMemoryManager> memoryManager(true, true, executionEnvironment);

    MockWddmAllocation mockAllocation(executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper());

    mockAllocation.storageInfo.memoryBanks = 0b1111;
    DeviceBitfield memoryBanksToCopy = 0b1010;
    mockAllocation.handles.resize(4);
    for (auto index = 0u; index < 4; index++) {
        wddm->storageLocked.set(index, false);
        if (mockAllocation.storageInfo.memoryBanks.test(index)) {
            mockAllocation.handles[index] = index;
        }
    }
    std::vector<uint8_t> dataToCopy(sourceAllocationSize, 1u);
    auto ret = memoryManager.copyMemoryToAllocationBanks(&mockAllocation, offset, dataToCopy.data(), dataToCopy.size(), memoryBanksToCopy);
    EXPECT_TRUE(ret);

    EXPECT_FALSE(wddm->storageLocked.test(0));
    ASSERT_TRUE(wddm->storageLocked.test(1));
    EXPECT_FALSE(wddm->storageLocked.test(2));
    ASSERT_TRUE(wddm->storageLocked.test(3));
    EXPECT_EQ(0, memcmp(ptrOffset(wddm->storages[1], offset), dataToCopy.data(), dataToCopy.size()));
    EXPECT_EQ(0, memcmp(ptrOffset(wddm->storages[3], offset), dataToCopy.data(), dataToCopy.size()));
}

class WddmMemoryManagerTest : public ::Test<GdiDllFixture> {
  public:
    void SetUp() override {
        ::Test<GdiDllFixture>::SetUp();
        rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex].get();
        wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
        if (defaultHwInfo->capabilityTable.ftrRenderCompressedBuffers || defaultHwInfo->capabilityTable.ftrRenderCompressedImages) {
            GMM_TRANSLATIONTABLE_CALLBACKS dummyTTCallbacks = {};

            auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(executionEnvironment, 1u, 1));
            auto hwInfo = *defaultHwInfo;
            EngineInstancesContainer regularEngines = {
                {aub_stream::ENGINE_CCS, EngineUsage::regular}};

            memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(regularEngines[0],
                                                                                                              PreemptionHelper::getDefaultPreemptionMode(hwInfo)));

            for (auto engine : memoryManager->getRegisteredEngines(rootDeviceIndex)) {
                if (engine.getEngineUsage() == EngineUsage::regular) {
                    engine.commandStreamReceiver->pageTableManager.reset(GmmPageTableMngr::create(nullptr, 0, &dummyTTCallbacks));
                }
            }
        }
        wddm->init();
        constexpr uint64_t heap32Base = (is32bit) ? 0x1000 : 0x800000000000;
        wddm->setHeap32(heap32Base, 1000 * MemoryConstants::pageSize - 1);

        rootDeviceEnvironment->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);

        memoryManager = std::make_unique<MockWddmMemoryManager>(executionEnvironment);
    }

    void TearDown() override {
        ::Test<GdiDllFixture>::TearDown();
    }

    MockExecutionEnvironment executionEnvironment{};
    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
    std::unique_ptr<MockWddmMemoryManager> memoryManager;
    WddmMock *wddm = nullptr;
    const uint32_t rootDeviceIndex = 0u;
};

TEST_F(WddmMemoryManagerTest, givenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledWhencreateWddmAllocationFailsThenGraphicsAllocationIsNotCreated) {
    char hostPtr[64];
    memoryManager->setDeferredDeleter(nullptr);
    setMapGpuVaFailConfigFcn(0, 1);

    AllocationData allocationData;
    allocationData.size = sizeof(hostPtr);
    allocationData.hostPtr = hostPtr;
    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);
    EXPECT_EQ(nullptr, allocation);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerTest, GivenGraphicsAllocationWhenAddAndRemoveAllocationToHostPtrManagerThenFragmentHasCorrectValues) {
    void *cpuPtr = (void *)0x30000;
    size_t size = 0x1000;
    uint64_t gpuPtr = 0x123;

    MockWddmAllocation gfxAllocation(rootDeviceEnvironment->getGmmHelper());
    HostPtrEntryKey key{cpuPtr, gfxAllocation.getRootDeviceIndex()};
    gfxAllocation.cpuPtr = cpuPtr;
    gfxAllocation.size = size;
    gfxAllocation.gpuPtr = gpuPtr;
    memoryManager->addAllocationToHostPtrManager(&gfxAllocation);
    auto fragment = memoryManager->getHostPtrManager()->getFragment(key);
    EXPECT_NE(fragment, nullptr);
    EXPECT_TRUE(fragment->driverAllocation);
    EXPECT_EQ(fragment->refCount, 1);
    EXPECT_EQ(fragment->fragmentCpuPointer, cpuPtr);
    EXPECT_EQ(fragment->fragmentSize, size);
    EXPECT_NE(fragment->osInternalStorage, nullptr);
    auto osHandle = static_cast<OsHandleWin *>(fragment->osInternalStorage);
    EXPECT_EQ(osHandle->gmm, gfxAllocation.getDefaultGmm());
    EXPECT_EQ(osHandle->gpuPtr, gpuPtr);
    EXPECT_EQ(osHandle->handle, gfxAllocation.handle);
    EXPECT_NE(fragment->residency, nullptr);

    FragmentStorage fragmentStorage = {};
    fragmentStorage.fragmentCpuPointer = cpuPtr;
    memoryManager->getHostPtrManager()->storeFragment(gfxAllocation.getRootDeviceIndex(), fragmentStorage);
    fragment = memoryManager->getHostPtrManager()->getFragment(key);
    EXPECT_EQ(fragment->refCount, 2);

    fragment->driverAllocation = false;
    memoryManager->removeAllocationFromHostPtrManager(&gfxAllocation);
    fragment = memoryManager->getHostPtrManager()->getFragment(key);
    EXPECT_EQ(fragment->refCount, 2);
    fragment->driverAllocation = true;

    memoryManager->removeAllocationFromHostPtrManager(&gfxAllocation);
    fragment = memoryManager->getHostPtrManager()->getFragment(key);
    EXPECT_EQ(fragment->refCount, 1);

    memoryManager->removeAllocationFromHostPtrManager(&gfxAllocation);
    fragment = memoryManager->getHostPtrManager()->getFragment(key);
    EXPECT_EQ(fragment, nullptr);
}

TEST_F(WddmMemoryManagerTest, WhenAllocatingGpuMemHostPtrThenCpuPtrAndGpuPtrAreSame) {
    // three pages
    void *ptr = alignedMalloc(3 * 4096, 4096);
    ASSERT_NE(nullptr, ptr);

    auto *gpuAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, false, MemoryConstants::pageSize}, ptr);
    // Should be same cpu ptr and gpu ptr
    EXPECT_EQ(ptr, gpuAllocation->getUnderlyingBuffer());

    memoryManager->freeGraphicsMemory(gpuAllocation);
    alignedFree(ptr);
}

TEST_F(WddmMemoryManagerTest, givenDefaultMemoryManagerWhenAllocateWithSizeIsCalledThenSharedHandleIsZero) {
    auto *gpuAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize});

    auto wddmAllocation = static_cast<WddmAllocation *>(gpuAllocation);

    EXPECT_EQ(0u, wddmAllocation->peekSharedHandle());

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenCreateFromSharedHandleIsCalledThenNonNullGraphicsAllocationIsReturned) {
    MockWddmMemoryManager::OsHandleData osHandleData{1u};
    void *pSysMem = reinterpret_cast<void *>(0x1000);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), pSysMem, 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    void *gmmPtrArray[]{gmm->gmmResourceInfo.get()};
    setSizesFcn(gmmPtrArray, 1u, 1024u, 1u);

    AllocationProperties properties(0, false, 4096u, AllocationType::sharedBuffer, false, false, mockDeviceBitfield);

    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    auto wddmAlloc = static_cast<WddmAllocation *>(gpuAllocation);
    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(RESOURCE_HANDLE, wddmAlloc->getResourceHandle());
    EXPECT_EQ(ALLOCATION_HANDLE, wddmAlloc->getDefaultHandle());

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenCreateFromSharedHandleIsCalledThenNonNullGraphicsAllocationsAreReturned) {
    void *pSysMem = reinterpret_cast<void *>(0x1000);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), pSysMem, 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    std::unique_ptr<Gmm> gmm2(new Gmm(rootDeviceEnvironment->getGmmHelper(), pSysMem, 4096u, 0, GMM_RESOURCE_USAGE_OCL_IMAGE, {}, gmmRequirements));
    void *gmmPtrArray[]{gmm->gmmResourceInfo.get(), gmm2->gmmResourceInfo.get()};
    setSizesFcn(gmmPtrArray, 2u, 1024u, 1u);

    for (uint32_t i = 0; i < 3; i++) {
        MockWddmMemoryManager::OsHandleData osHandleData{NT_ALLOCATION_HANDLE, i};
        AllocationProperties properties(0, false, 0u, AllocationType::sharedImage, false, 0);
        auto *gpuAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, false, nullptr);
        auto wddmAlloc = static_cast<WddmAllocation *>(gpuAllocation);
        ASSERT_NE(nullptr, gpuAllocation);
        EXPECT_EQ(NT_RESOURCE_HANDLE, wddmAlloc->getResourceHandle());
        EXPECT_EQ(NT_ALLOCATION_HANDLE, wddmAlloc->getDefaultHandle());
        EXPECT_EQ(AllocationType::sharedImage, wddmAlloc->getAllocationType());

        if (i % 2)
            EXPECT_EQ(GMM_RESOURCE_USAGE_OCL_IMAGE, reinterpret_cast<MockGmmResourceInfo *>(wddmAlloc->getDefaultGmm()->gmmResourceInfo.get())->getResourceUsage());
        else
            EXPECT_EQ(GMM_RESOURCE_USAGE_OCL_BUFFER, reinterpret_cast<MockGmmResourceInfo *>(wddmAlloc->getDefaultGmm()->gmmResourceInfo.get())->getResourceUsage());

        memoryManager->freeGraphicsMemory(gpuAllocation);
    }
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenCreateFromSharedHandleIsCalledWithUmKmDataTranslatorEnabledThenNonNullGraphicsAllocationIsReturned) {
    struct MockUmKmDataTranslator : UmKmDataTranslator {
        using UmKmDataTranslator::isEnabled;
    };

    std::unique_ptr<MockUmKmDataTranslator> translator = std::make_unique<MockUmKmDataTranslator>();
    translator->isEnabled = true;
    std::unique_ptr<NEO::HwDeviceIdWddm> hwDeviceId = std::make_unique<NEO::HwDeviceIdWddm>(wddm->hwDeviceId->getAdapter(),
                                                                                            wddm->hwDeviceId->getAdapterLuid(),
                                                                                            1u,
                                                                                            executionEnvironment.osEnvironment.get(),
                                                                                            std::move(translator));
    wddm->hwDeviceId.reset(hwDeviceId.release());

    void *pSysMem = reinterpret_cast<void *>(0x1000);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), pSysMem, 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    void *gmmPtrArray[]{gmm->gmmResourceInfo.get()};
    setSizesFcn(gmmPtrArray, 1u, 1024u, 1u);

    MockWddmMemoryManager::OsHandleData osHandleData{NT_ALLOCATION_HANDLE};
    AllocationProperties properties(0, false, 0u, AllocationType::sharedImage, false, 0);
    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, false, nullptr);
    auto wddmAlloc = static_cast<WddmAllocation *>(gpuAllocation);
    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(NT_RESOURCE_HANDLE, wddmAlloc->getResourceHandle());
    EXPECT_EQ(NT_ALLOCATION_HANDLE, wddmAlloc->getDefaultHandle());
    EXPECT_EQ(AllocationType::sharedImage, wddmAlloc->getAllocationType());

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenLockUnlockIsCalledThenReturnPtr) {
    auto alloc = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize});

    auto ptr = memoryManager->lockResource(alloc);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(1u, wddm->lockResult.called);
    EXPECT_TRUE(wddm->lockResult.success);

    memoryManager->unlockResource(alloc);
    EXPECT_EQ(1u, wddm->unlockResult.called);
    EXPECT_TRUE(wddm->unlockResult.success);

    memoryManager->freeGraphicsMemory(alloc);
}

TEST_F(WddmMemoryManagerTest, GivenForce32bitAddressingAndRequireSpecificBitnessWhenCreatingAllocationFromSharedHandleThen32BitAllocationIsReturned) {
    MockWddmMemoryManager::OsHandleData osHandleData{1u};
    void *pSysMem = reinterpret_cast<void *>(0x1000);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), pSysMem, 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    void *gmmPtrArray[]{gmm->gmmResourceInfo.get()};
    setSizesFcn(gmmPtrArray, 1u, 1024u, 1u);

    memoryManager->setForce32BitAllocations(true);

    AllocationProperties properties(0, false, 4096u, AllocationType::sharedBuffer, false, false, 0);

    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, true, false, true, nullptr);
    ASSERT_NE(nullptr, gpuAllocation);
    if constexpr (is64bit) {
        EXPECT_TRUE(gpuAllocation->is32BitAllocation());

        uint64_t base = memoryManager->getExternalHeapBaseAddress(gpuAllocation->getRootDeviceIndex(), gpuAllocation->isAllocatedInLocalMemoryPool());
        auto gmmHelper = memoryManager->getGmmHelper(gpuAllocation->getRootDeviceIndex());

        EXPECT_EQ(gmmHelper->canonize(base), gpuAllocation->getGpuBaseAddress());
    }

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, GivenForce32bitAddressingAndNotRequiredSpecificBitnessWhenCreatingAllocationFromSharedHandleThenNon32BitAllocationIsReturned) {
    MockWddmMemoryManager::OsHandleData osHandleData{1u};
    void *pSysMem = reinterpret_cast<void *>(0x1000);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), pSysMem, 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    void *gmmPtrArray[]{gmm->gmmResourceInfo.get()};
    setSizesFcn(gmmPtrArray, 1u, 1024u, 1u);

    memoryManager->setForce32BitAllocations(true);

    AllocationProperties properties(0, false, 4096u, AllocationType::sharedBuffer, false, false, 0);
    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, gpuAllocation);

    EXPECT_FALSE(gpuAllocation->is32BitAllocation());
    if constexpr (is64bit) {
        uint64_t base = 0;
        EXPECT_EQ(base, gpuAllocation->getGpuBaseAddress());
    }

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenFreeAllocFromSharedHandleIsCalledThenDestroyResourceHandle) {
    MockWddmMemoryManager::OsHandleData osHandleData{1u};
    void *pSysMem = reinterpret_cast<void *>(0x1000);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), pSysMem, 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    void *gmmPtrArray[]{gmm->gmmResourceInfo.get()};
    setSizesFcn(gmmPtrArray, 1u, 1024u, 1u);

    AllocationProperties properties(0, false, 4096u, AllocationType::sharedBuffer, false, false, 0);
    auto gpuAllocation = (WddmAllocation *)memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    EXPECT_NE(nullptr, gpuAllocation);
    auto expectedDestroyHandle = gpuAllocation->getResourceHandle();
    EXPECT_NE(0u, expectedDestroyHandle);

    auto lastDestroyed = getMockLastDestroyedResHandleFcn();
    EXPECT_EQ(0u, lastDestroyed);

    memoryManager->freeGraphicsMemory(gpuAllocation);
    lastDestroyed = getMockLastDestroyedResHandleFcn();
    EXPECT_EQ(lastDestroyed, expectedDestroyHandle);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenAllocFromHostPtrIsCalledThenResourceHandleIsNotCreated) {
    auto size = 13u;
    auto hostPtr = reinterpret_cast<const void *>(0x10001);

    AllocationData allocationData{};
    allocationData.size = size;
    allocationData.hostPtr = hostPtr;
    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));

    EXPECT_EQ(0u, allocation->getResourceHandle());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerSizeZeroWhenCreateFromSharedHandleIsCalledThenUpdateSize) {
    MockWddmMemoryManager::OsHandleData osHandleData{1u};
    auto size = 4096u;
    void *pSysMem = reinterpret_cast<void *>(0x1000);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), pSysMem, size, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    void *gmmPtrArray[]{gmm->gmmResourceInfo.get()};
    setSizesFcn(gmmPtrArray, 1u, 1024u, 1u);

    AllocationProperties properties(0, false, size, AllocationType::sharedBuffer, false, false, 0);
    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(size, gpuAllocation->getUnderlyingBufferSize());
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, whenOpenSharedHandleThenRegisterAllocationSize) {
    MockWddmMemoryManager::OsHandleData osHandleData{1u};
    auto size = 4096u;
    void *pSysMem = reinterpret_cast<void *>(0x1000);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), pSysMem, size, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    void *gmmPtrArray[]{gmm->gmmResourceInfo.get()};
    setSizesFcn(gmmPtrArray, 1u, 1024u, 1u);

    auto usedSystemMemorySize = memoryManager->getUsedSystemMemorySize();
    AllocationProperties properties(0, false, size, AllocationType::sharedBuffer, false, false, 0);

    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);

    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(size, gpuAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(usedSystemMemorySize + size, memoryManager->getUsedSystemMemorySize());

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

HWTEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenAllocateGraphicsMemoryWithSetAllocattionPropertisWithAllocationTypeBufferCompressedIsCalledThenIsRendeCompressedTrueAndGpuMappingIsSetWithGoodAddressRange) {
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = MemoryConstants::pageSize;
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    rootDeviceEnvironment->setHwInfoAndInitHelpers(&hwInfo);

    auto memoryManager = std::make_unique<MockWddmMemoryManager>(true, false, executionEnvironment);
    if (memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    rootDeviceEnvironment->executionEnvironment.initializeMemoryManager();
    memoryManager->allocateGraphicsMemoryInNonDevicePool = true;

    MockAllocationProperties properties = {mockRootDeviceIndex, true, size, AllocationType::buffer, mockDeviceBitfield};
    properties.flags.preferCompressed = true;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, ptr);

    ASSERT_NE(nullptr, allocation);
    EXPECT_TRUE(memoryManager->allocationGraphicsMemory64kbCreated);
    EXPECT_TRUE(allocation->getDefaultGmm()->isCompressionEnabled());
    if ((is32bit && rootDeviceEnvironment->isFullRangeSvm()) &&
        allocation->getDefaultGmm()->gmmResourceInfo->is64KBPageSuitable()) {
        auto gmmHelper = memoryManager->getGmmHelper(0);

        EXPECT_LE(gmmHelper->decanonize(allocation->getGpuAddress()), MemoryConstants::max32BitAddress);
    }

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_F(WddmMemoryManagerTest, givenInternalHeapOrLinearStreamTypeWhenAllocatingThenSetCorrectUsage) {
    auto memoryManager = std::make_unique<MockWddmMemoryManager>(true, false, executionEnvironment);

    rootDeviceEnvironment->executionEnvironment.initializeMemoryManager();

    {
        MockAllocationProperties properties = {mockRootDeviceIndex, true, 1, AllocationType::internalHeap, mockDeviceBitfield};

        auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, nullptr);

        ASSERT_NE(nullptr, allocation);
        EXPECT_TRUE(allocation->getDefaultGmm()->resourceParams.Usage == GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER);

        memoryManager->freeGraphicsMemory(allocation);
    }

    {
        MockAllocationProperties properties = {mockRootDeviceIndex, true, 1, AllocationType::linearStream, mockDeviceBitfield};

        auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, nullptr);

        ASSERT_NE(nullptr, allocation);
        EXPECT_TRUE(allocation->getDefaultGmm()->resourceParams.Usage == GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER);

        memoryManager->freeGraphicsMemory(allocation);
    }
}

HWTEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenAllocateGraphicsMemoryWithSetAllocattionPropertisWithAllocationTypeBufferIsCalledThenIsRendeCompressedFalseAndCorrectAddressRange) {
    if (rootDeviceEnvironment->getHelper<ProductHelper>().overrideGfxPartitionLayoutForWsl()) {
        GTEST_SKIP();
    }
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = MemoryConstants::pageSize;
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    rootDeviceEnvironment->setHwInfoAndInitHelpers(&hwInfo);

    auto memoryManager = std::make_unique<MockWddmMemoryManager>(false, false, executionEnvironment);
    memoryManager->allocateGraphicsMemoryInNonDevicePool = true;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{mockRootDeviceIndex, true, size, AllocationType::buffer, mockDeviceBitfield}, ptr);

    auto gfxPartition = memoryManager->getGfxPartition(mockRootDeviceIndex);
    D3DGPU_VIRTUAL_ADDRESS svmRangeMinimumAddress = gfxPartition->getHeapMinimalAddress(HeapIndex::heapSvm);
    D3DGPU_VIRTUAL_ADDRESS svmRangeMaximumAddress = gfxPartition->getHeapLimit(HeapIndex::heapSvm);

    ASSERT_NE(nullptr, allocation);
    EXPECT_FALSE(memoryManager->allocationGraphicsMemory64kbCreated);
    EXPECT_FALSE(allocation->getDefaultGmm()->isCompressionEnabled());
    if (is32bit || rootDeviceEnvironment->isFullRangeSvm()) {
        auto gmmHelper = memoryManager->getGmmHelper(0);

        EXPECT_GE(gmmHelper->decanonize(allocation->getGpuAddress()), svmRangeMinimumAddress);
        EXPECT_LE(gmmHelper->decanonize(allocation->getGpuAddress()), svmRangeMaximumAddress);
    }
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenCreateFromSharedHandleFailsThenReturnNull) {
    MockWddmMemoryManager::OsHandleData osHandleData{1u};
    auto size = 4096u;
    void *pSysMem = reinterpret_cast<void *>(0x1000);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), pSysMem, size, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    void *gmmPtrArray[]{gmm->gmmResourceInfo.get()};
    setSizesFcn(gmmPtrArray, 1u, 1024u, 1u);

    wddm->failOpenSharedHandle = true;

    AllocationProperties properties(0, false, size, AllocationType::sharedBuffer, false, false, 0);
    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    EXPECT_EQ(nullptr, gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, GivenOffsetsWhenAllocatingGpuMemHostThenAllocatedOnlyIfInBounds) {
    if (memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    MockWddmAllocation alloc(rootDeviceEnvironment->getGmmHelper()), allocOffseted(rootDeviceEnvironment->getGmmHelper());
    // three pages
    void *ptr = reinterpret_cast<void *>(0x200000);

    size_t baseOffset = 1024;
    // misaligned buffer spanning across 3 pages
    auto *gpuAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, false, 2 * MemoryConstants::pageSize}, (char *)ptr + baseOffset);
    // Should be same cpu ptr and gpu ptr
    EXPECT_EQ((char *)ptr + baseOffset, gpuAllocation->getUnderlyingBuffer());

    auto hostPtrManager = memoryManager->getHostPtrManager();

    auto fragment = hostPtrManager->getFragment({ptr, rootDeviceIndex});
    ASSERT_NE(nullptr, fragment);
    EXPECT_TRUE(fragment->refCount == 1);
    EXPECT_NE(fragment->osInternalStorage, nullptr);

    // offset by 3 pages, not in boundary
    auto fragment2 = hostPtrManager->getFragment({reinterpret_cast<char *>(ptr) + 3 * 4096, rootDeviceIndex});

    EXPECT_EQ(nullptr, fragment2);

    // offset by one page, still in boundary
    void *offsetPtr = ptrOffset(ptr, 4096);
    auto *gpuAllocation2 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, false, MemoryConstants::pageSize}, offsetPtr);
    // Should be same cpu ptr and gpu ptr
    EXPECT_EQ(offsetPtr, gpuAllocation2->getUnderlyingBuffer());

    auto fragment3 = hostPtrManager->getFragment({offsetPtr, rootDeviceIndex});
    ASSERT_NE(nullptr, fragment3);

    EXPECT_TRUE(fragment3->refCount == 2);
    EXPECT_EQ(alloc.handle, allocOffseted.handle);
    EXPECT_EQ(alloc.getUnderlyingBufferSize(), allocOffseted.getUnderlyingBufferSize());
    EXPECT_EQ(alloc.getAlignedCpuPtr(), allocOffseted.getAlignedCpuPtr());

    memoryManager->freeGraphicsMemory(gpuAllocation2);

    auto fragment4 = hostPtrManager->getFragment({ptr, rootDeviceIndex});
    ASSERT_NE(nullptr, fragment4);

    EXPECT_TRUE(fragment4->refCount == 1);

    memoryManager->freeGraphicsMemory(gpuAllocation);

    fragment4 = hostPtrManager->getFragment({ptr, rootDeviceIndex});
    EXPECT_EQ(nullptr, fragment4);
}

TEST_F(WddmMemoryManagerTest, WhenAllocatingGpuMemThenOsInternalStorageIsPopulatedCorrectly) {
    if (memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper());
    // three pages
    void *ptr = reinterpret_cast<void *>(0x200000);
    auto *gpuAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, false, 3 * MemoryConstants::pageSize}, ptr);
    // Should be same cpu ptr and gpu ptr
    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(ptr, gpuAllocation->getUnderlyingBuffer());

    auto fragment = memoryManager->getHostPtrManager()->getFragment({ptr, rootDeviceIndex});
    ASSERT_NE(nullptr, fragment);
    EXPECT_TRUE(fragment->refCount == 1);
    EXPECT_NE(static_cast<OsHandleWin *>(fragment->osInternalStorage)->handle, 0u);
    EXPECT_NE(static_cast<OsHandleWin *>(fragment->osInternalStorage)->gmm, nullptr);
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, GivenAlignedPointerWhenAllocate32BitMemoryThenGmmCalledWithCorrectPointerAndSize) {
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper());
    uint32_t size = 4096;
    void *ptr = reinterpret_cast<void *>(4096);
    auto *gpuAllocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, size, ptr, AllocationType::buffer);
    EXPECT_EQ(ptr, reinterpret_cast<void *>(gpuAllocation->getDefaultGmm()->resourceParams.pExistingSysMem));
    EXPECT_EQ(size, gpuAllocation->getDefaultGmm()->resourceParams.ExistingSysMemSize);
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, GivenUnAlignedPointerAndSizeWhenAllocate32BitMemoryThenGmmCalledWithCorrectPointerAndSize) {
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper());
    uint32_t size = 0x1001;
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto *gpuAllocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, size, ptr, AllocationType::buffer);
    EXPECT_EQ(reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(gpuAllocation->getDefaultGmm()->resourceParams.pExistingSysMem));
    EXPECT_EQ(0x2000u, gpuAllocation->getDefaultGmm()->resourceParams.ExistingSysMemSize);
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, WhenInitializingWddmThenSystemSharedMemoryIsCorrect) {
    executionEnvironment.prepareRootDeviceEnvironments(4u);
    for (auto i = 0u; i < 4u; i++) {
        executionEnvironment.rootDeviceEnvironments[i]->osInterface.reset();
        auto mockWddm = Wddm::createWddm(nullptr, *executionEnvironment.rootDeviceEnvironments[i].get());
        mockWddm->init();

        int64_t mem = memoryManager->getSystemSharedMemory(i);
        EXPECT_EQ(mem, 4249540608);
    }
}

TEST_F(WddmMemoryManagerTest, GivenBitnessWhenGettingMaxAddressThenCorrectAddressIsReturned) {
    uint64_t maxAddr = memoryManager->getMaxApplicationAddress();
    if (is32bit) {
        EXPECT_EQ(maxAddr, MemoryConstants::max32BitAppAddress);
    } else {
        EXPECT_EQ(maxAddr, MemoryConstants::max64BitAppAddress);
    }
}

TEST_F(WddmMemoryManagerTest, GivenNullptrWhenAllocating32BitMemoryThenAddressIsCorrect) {
    auto *gpuAllocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, 3 * MemoryConstants::pageSize, nullptr, AllocationType::buffer);

    ASSERT_NE(nullptr, gpuAllocation);

    auto gmmHelper = memoryManager->getGmmHelper(gpuAllocation->getRootDeviceIndex());
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapExternal)), gpuAllocation->getGpuAddress());
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapExternal)), gpuAllocation->getGpuAddress() + gpuAllocation->getUnderlyingBufferSize());

    EXPECT_EQ(0u, gpuAllocation->fragmentsStorage.fragmentCount);
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, given32BitAllocationWhenItIsCreatedThenItHasNonZeroGpuAddressToPatch) {
    auto *gpuAllocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, 3 * MemoryConstants::pageSize, nullptr, AllocationType::buffer);

    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_NE(0llu, gpuAllocation->getGpuAddressToPatch());

    auto gmmHelper = memoryManager->getGmmHelper(gpuAllocation->getRootDeviceIndex());
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapExternal)), gpuAllocation->getGpuAddress());
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapExternal)), gpuAllocation->getGpuAddress() + gpuAllocation->getUnderlyingBufferSize());
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, GivenMisalignedHostPtrWhenAllocating32BitMemoryThenTripleAllocationDoesNotOccur) {
    size_t misalignedSize = 0x2500;
    void *misalignedPtr = reinterpret_cast<void *>(0x12500);

    auto *gpuAllocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, misalignedSize, misalignedPtr, AllocationType::buffer);

    ASSERT_NE(nullptr, gpuAllocation);

    EXPECT_EQ(alignSizeWholePage(misalignedPtr, misalignedSize), gpuAllocation->getUnderlyingBufferSize());

    auto gmmHelper = memoryManager->getGmmHelper(gpuAllocation->getRootDeviceIndex());
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapExternal)), gpuAllocation->getGpuAddress());
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapExternal)), gpuAllocation->getGpuAddress() + gpuAllocation->getUnderlyingBufferSize());

    EXPECT_EQ(0u, gpuAllocation->fragmentsStorage.fragmentCount);

    void *alignedPtr = alignDown(misalignedPtr, MemoryConstants::allocationAlignment);
    uint64_t offset = ptrDiff(misalignedPtr, alignedPtr);

    EXPECT_EQ(offset, gpuAllocation->getAllocationOffset());
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, WhenAllocating32BitMemoryThenGpuBaseAddressIsCannonized) {
    auto *gpuAllocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, 3 * MemoryConstants::pageSize, nullptr, AllocationType::buffer);

    ASSERT_NE(nullptr, gpuAllocation);

    auto gmmHelper = memoryManager->getGmmHelper(gpuAllocation->getRootDeviceIndex());
    uint64_t cannonizedAddress = gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapBase(MemoryManager::selectExternalHeap(gpuAllocation->isAllocatedInLocalMemoryPool())));
    EXPECT_EQ(cannonizedAddress, gpuAllocation->getGpuBaseAddress());

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, GivenThreeOsHandlesWhenAskedForDestroyAllocationsThenAllMarkedAllocationsAreDestroyed) {
    OsHandleStorage storage;
    void *pSysMem = reinterpret_cast<void *>(0x1000);
    uint32_t maxOsContextCount = 1u;

    auto osHandle0 = new OsHandleWin();
    auto osHandle1 = new OsHandleWin();
    auto osHandle2 = new OsHandleWin();

    storage.fragmentStorageData[0].osHandleStorage = osHandle0;
    storage.fragmentStorageData[0].residency = new ResidencyData(maxOsContextCount);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    osHandle0->handle = ALLOCATION_HANDLE;
    storage.fragmentStorageData[0].freeTheFragment = true;
    osHandle0->gmm = new Gmm(rootDeviceEnvironment->getGmmHelper(), pSysMem, 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);

    storage.fragmentStorageData[1].osHandleStorage = osHandle1;
    osHandle1->handle = ALLOCATION_HANDLE;
    storage.fragmentStorageData[1].residency = new ResidencyData(maxOsContextCount);

    storage.fragmentStorageData[1].freeTheFragment = false;

    storage.fragmentStorageData[2].osHandleStorage = osHandle2;
    osHandle2->handle = ALLOCATION_HANDLE;
    storage.fragmentStorageData[2].freeTheFragment = true;
    osHandle2->gmm = new Gmm(rootDeviceEnvironment->getGmmHelper(), pSysMem, 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    storage.fragmentStorageData[2].residency = new ResidencyData(maxOsContextCount);

    memoryManager->cleanOsHandles(storage, 0);

    auto destroyWithResourceHandleCalled = 0u;
    D3DKMT_DESTROYALLOCATION2 *ptrToDestroyAlloc2 = nullptr;

    getSizesFcn(destroyWithResourceHandleCalled, ptrToDestroyAlloc2);

    EXPECT_EQ(0u, ptrToDestroyAlloc2->Flags.SynchronousDestroy);
    EXPECT_EQ(1u, ptrToDestroyAlloc2->Flags.AssumeNotInUse);

    EXPECT_EQ(ALLOCATION_HANDLE, osHandle1->handle);

    delete storage.fragmentStorageData[1].osHandleStorage;
    delete storage.fragmentStorageData[1].residency;
}

TEST_F(WddmMemoryManagerTest, GivenNullptrWhenFreeingAllocationThenCrashDoesNotOccur) {
    EXPECT_NO_THROW(memoryManager->freeGraphicsMemory(nullptr));
}

TEST_F(WddmMemoryManagerTest, givenDefaultWddmMemoryManagerWhenAskedForAlignedMallocRestrictionsThenValueIsReturned) {
    AlignedMallocRestrictions *mallocRestrictions = memoryManager->getAlignedMallocRestrictions();
    ASSERT_NE(nullptr, mallocRestrictions);
    EXPECT_EQ(NEO::windowsMinAddress, mallocRestrictions->minAddress);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenCpuMemNotMeetRestrictionsThenReserveMemRangeForMap) {
    if (memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    void *cpuPtr = reinterpret_cast<void *>(memoryManager->getAlignedMallocRestrictions()->minAddress - 0x1000);
    size_t size = 0x1000;

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{0, false, size}, cpuPtr));

    void *expectReserve = reinterpret_cast<void *>(wddm->virtualAllocAddress);

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(expectReserve, allocation->getReservedAddressPtr());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerTest, givenManagerWithDisabledDeferredDeleterWhenMapGpuVaFailThenFailToCreateAllocation) {
    void *ptr = reinterpret_cast<void *>(0x1000);
    size_t size = 0x1000;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), ptr, size, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));

    memoryManager->setDeferredDeleter(nullptr);
    setMapGpuVaFailConfigFcn(0, 1);

    auto gmmHelper = rootDeviceEnvironment->getGmmHelper();
    auto canonizedAddress = gmmHelper->canonize(castToUint64(const_cast<void *>(ptr)));
    WddmAllocation allocation(0, 1u /*num gmms*/, AllocationType::buffer, ptr, canonizedAddress, size, nullptr, MemoryPool::memoryNull, 0u, 1u);
    allocation.setDefaultGmm(gmm.get());
    bool ret = memoryManager->createWddmAllocation(&allocation, allocation.getAlignedCpuPtr());
    EXPECT_FALSE(ret);
}

TEST_F(WddmMemoryManagerTest, givenManagerWithEnabledDeferredDeleterWhenFirstMapGpuVaFailSecondAfterDrainSuccessThenCreateAllocation) {
    void *ptr = reinterpret_cast<void *>(0x10000);
    size_t size = 0x1000;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), ptr, size, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));

    MockDeferredDeleter *deleter = new MockDeferredDeleter;
    memoryManager->setDeferredDeleter(deleter);

    setMapGpuVaFailConfigFcn(0, 1);

    auto gmmHelper = rootDeviceEnvironment->getGmmHelper();
    auto canonizedAddress = gmmHelper->canonize(castToUint64(const_cast<void *>(ptr)));
    WddmAllocation allocation(0, 1u /*num gmms*/, AllocationType::buffer, ptr, canonizedAddress, size, nullptr, MemoryPool::memoryNull, 0u, 1u);
    allocation.setDefaultGmm(gmm.get());
    bool ret = memoryManager->createWddmAllocation(&allocation, allocation.getAlignedCpuPtr());
    EXPECT_TRUE(ret);
    wddm->destroyAllocation(&allocation, nullptr);
}

TEST_F(WddmMemoryManagerTest, givenManagerWithEnabledDeferredDeleterWhenFirstAndMapGpuVaFailSecondAfterDrainFailThenFailToCreateAllocation) {
    void *ptr = reinterpret_cast<void *>(0x1000);
    size_t size = 0x1000;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), ptr, size, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));

    MockDeferredDeleter *deleter = new MockDeferredDeleter;
    memoryManager->setDeferredDeleter(deleter);

    setMapGpuVaFailConfigFcn(0, 2);

    auto gmmHelper = rootDeviceEnvironment->getGmmHelper();
    auto canonizedAddress = gmmHelper->canonize(castToUint64(const_cast<void *>(ptr)));
    WddmAllocation allocation(0, 1u /*num gmms*/, AllocationType::buffer, ptr, canonizedAddress, size, nullptr, MemoryPool::memoryNull, 0u, 1u);
    allocation.setDefaultGmm(gmm.get());
    bool ret = memoryManager->createWddmAllocation(&allocation, allocation.getAlignedCpuPtr());
    EXPECT_FALSE(ret);
}

TEST_F(WddmMemoryManagerTest, givenNullPtrAndSizePassedToCreateInternalAllocationWhenCallIsMadeThenAllocationIsCreatedIn32BitHeapInternal) {
    if (rootDeviceEnvironment->getHelper<ProductHelper>().overrideGfxPartitionLayoutForWsl()) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restorer;
    debugManager.flags.ForcePreferredAllocationMethod.set(static_cast<int32_t>(GfxMemoryAllocationMethod::useUmdSystemPtr));
    auto wddmAllocation = static_cast<WddmAllocation *>(memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, MemoryConstants::pageSize, nullptr, AllocationType::internalHeap));
    ASSERT_NE(nullptr, wddmAllocation);
    auto gmmHelper = memoryManager->getGmmHelper(wddmAllocation->getRootDeviceIndex());
    EXPECT_EQ(wddmAllocation->getGpuBaseAddress(), gmmHelper->canonize(memoryManager->getInternalHeapBaseAddress(wddmAllocation->getRootDeviceIndex(), wddmAllocation->isAllocatedInLocalMemoryPool())));
    EXPECT_NE(nullptr, wddmAllocation->getUnderlyingBuffer());
    EXPECT_EQ(4096u, wddmAllocation->getUnderlyingBufferSize());
    EXPECT_NE((uint64_t)wddmAllocation->getUnderlyingBuffer(), wddmAllocation->getGpuAddress());
    auto cannonizedHeapBase = gmmHelper->canonize(memoryManager->getInternalHeapBaseAddress(rootDeviceIndex, wddmAllocation->isAllocatedInLocalMemoryPool()));
    auto cannonizedHeapEnd = gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(MemoryManager::selectInternalHeap(wddmAllocation->isAllocatedInLocalMemoryPool())));

    EXPECT_GT(wddmAllocation->getGpuAddress(), cannonizedHeapBase);
    EXPECT_LT(wddmAllocation->getGpuAddress() + wddmAllocation->getUnderlyingBufferSize(), cannonizedHeapEnd);

    EXPECT_NE(nullptr, wddmAllocation->getDriverAllocatedCpuPtr());
    EXPECT_TRUE(wddmAllocation->is32BitAllocation());
    memoryManager->freeGraphicsMemory(wddmAllocation);
}

TEST_F(WddmMemoryManagerTest, givenPtrAndSizePassedToCreateInternalAllocationWhenCallIsMadeThenAllocationIsCreatedIn32BitHeapInternal) {
    if (rootDeviceEnvironment->getHelper<ProductHelper>().overrideGfxPartitionLayoutForWsl()) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restorer;
    debugManager.flags.ForcePreferredAllocationMethod.set(static_cast<int32_t>(GfxMemoryAllocationMethod::useUmdSystemPtr));
    auto ptr = reinterpret_cast<void *>(0x1000000);
    auto wddmAllocation = static_cast<WddmAllocation *>(memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, MemoryConstants::pageSize, ptr, AllocationType::internalHeap));
    ASSERT_NE(nullptr, wddmAllocation);
    auto gmmHelper = memoryManager->getGmmHelper(wddmAllocation->getRootDeviceIndex());
    EXPECT_EQ(wddmAllocation->getGpuBaseAddress(), gmmHelper->canonize(memoryManager->getInternalHeapBaseAddress(rootDeviceIndex, wddmAllocation->isAllocatedInLocalMemoryPool())));
    EXPECT_EQ(ptr, wddmAllocation->getUnderlyingBuffer());
    EXPECT_EQ(4096u, wddmAllocation->getUnderlyingBufferSize());
    EXPECT_NE((uint64_t)wddmAllocation->getUnderlyingBuffer(), wddmAllocation->getGpuAddress());

    auto cannonizedHeapBase = gmmHelper->canonize(memoryManager->getInternalHeapBaseAddress(rootDeviceIndex, wddmAllocation->isAllocatedInLocalMemoryPool()));
    auto cannonizedHeapEnd = gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(MemoryManager::selectInternalHeap(wddmAllocation->isAllocatedInLocalMemoryPool())));
    EXPECT_GT(wddmAllocation->getGpuAddress(), cannonizedHeapBase);
    EXPECT_LT(wddmAllocation->getGpuAddress() + wddmAllocation->getUnderlyingBufferSize(), cannonizedHeapEnd);

    EXPECT_EQ(nullptr, wddmAllocation->getDriverAllocatedCpuPtr());
    EXPECT_TRUE(wddmAllocation->is32BitAllocation());
    memoryManager->freeGraphicsMemory(wddmAllocation);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWithNoRegisteredOsContextsWhenCallingIsMemoryBudgetExhaustedThenReturnFalse) {
    EXPECT_FALSE(memoryManager->isMemoryBudgetExhausted());
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerAnd32bitBuildThenSvmPartitionIsAlwaysInitialized) {
    if (is32bit) {
        EXPECT_EQ(memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapSvm), MemoryConstants::max32BitAddress);
    }
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWithRegisteredOsContextWhenCallingIsMemoryBudgetExhaustedThenReturnFalse) {
    executionEnvironment.prepareRootDeviceEnvironments(3u);
    for (auto i = 0u; i < executionEnvironment.rootDeviceEnvironments.size(); i++) {
        executionEnvironment.rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment.rootDeviceEnvironments[i]->initGmm();
    }
    executionEnvironment.initializeMemoryManager();
    memoryManager->allRegisteredEngines.resize(3);
    for (auto i = 0u; i < executionEnvironment.rootDeviceEnvironments.size(); i++) {
        executionEnvironment.rootDeviceEnvironments[i]->osInterface.reset();
        auto wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment.rootDeviceEnvironments[i].get()));
        wddm->init();
        executionEnvironment.rootDeviceEnvironments[i]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
    }
    std::unique_ptr<CommandStreamReceiver> csr(createCommandStream(executionEnvironment, 0u, 1));
    std::unique_ptr<CommandStreamReceiver> csr1(createCommandStream(executionEnvironment, 1u, 2));
    std::unique_ptr<CommandStreamReceiver> csr2(createCommandStream(executionEnvironment, 2u, 3));
    memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular},
                                                                                                      PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo), 1));
    memoryManager->createAndRegisterOsContext(csr1.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular},
                                                                                                       PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo), 2));
    memoryManager->createAndRegisterOsContext(csr2.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular},
                                                                                                       PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo), 3));
    EXPECT_FALSE(memoryManager->isMemoryBudgetExhausted());
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWithRegisteredOsContextWithExhaustedMemoryBudgetWhenCallingIsMemoryBudgetExhaustedThenReturnTrue) {
    executionEnvironment.prepareRootDeviceEnvironments(3u);
    for (auto i = 0u; i < executionEnvironment.rootDeviceEnvironments.size(); i++) {
        executionEnvironment.rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment.rootDeviceEnvironments[i]->initGmm();
    }
    executionEnvironment.initializeMemoryManager();
    memoryManager->allRegisteredEngines.resize(3);
    for (auto i = 0u; i < executionEnvironment.rootDeviceEnvironments.size(); i++) {
        executionEnvironment.rootDeviceEnvironments[i]->osInterface.reset();
        auto wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment.rootDeviceEnvironments[i].get()));
        wddm->init();
        executionEnvironment.rootDeviceEnvironments[i]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
    }
    std::unique_ptr<CommandStreamReceiver> csr(createCommandStream(executionEnvironment, 0u, 1));
    std::unique_ptr<CommandStreamReceiver> csr1(createCommandStream(executionEnvironment, 1u, 2));
    std::unique_ptr<CommandStreamReceiver> csr2(createCommandStream(executionEnvironment, 2u, 3));
    memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular},
                                                                                                      PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo), 1));
    memoryManager->createAndRegisterOsContext(csr1.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular},
                                                                                                       PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo), 2));
    memoryManager->createAndRegisterOsContext(csr2.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular},
                                                                                                       PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo), 3));
    auto osContext = static_cast<OsContextWin *>(memoryManager->getRegisteredEngines(1)[0].osContext);
    osContext->getResidencyController().setMemoryBudgetExhausted();
    EXPECT_TRUE(memoryManager->isMemoryBudgetExhausted());
}

TEST_F(WddmMemoryManagerTest, givenLocalMemoryAllocationWhenCpuPointerNotMeetRestrictionsThenDontReserveMemRangeForMap) {
    const bool localMemoryEnabled = true;
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, localMemoryEnabled, executionEnvironment);
    void *cpuPtr = reinterpret_cast<void *>(memoryManager->getAlignedMallocRestrictions()->minAddress - 0x1000);
    size_t size = 0x1000;

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, size, AllocationType::buffer, mockDeviceBitfield}, cpuPtr));

    ASSERT_NE(nullptr, allocation);
    EXPECT_FALSE(MemoryPoolHelper::isSystemMemoryPool(allocation->getMemoryPool()));
    if (is32bit && this->executionEnvironment.rootDeviceEnvironments[allocation->getRootDeviceIndex()]->isFullRangeSvm()) {
        EXPECT_NE(nullptr, allocation->getReservedAddressPtr());
        EXPECT_EQ(alignUp(size, MemoryConstants::pageSize64k) + 2 * MemoryConstants::megaByte, allocation->getReservedAddressSize());
        EXPECT_EQ(allocation->getGpuAddress(), castToUint64(allocation->getReservedAddressPtr()));
    } else {
        EXPECT_EQ(nullptr, allocation->getReservedAddressPtr());
    }
    memoryManager->freeGraphicsMemory(allocation);
}

class WddmMemoryManagerSimpleTestWithLocalMemory : public WddmMemoryManagerTest {
  public:
    void SetUp() override {
        HardwareInfo localPlatformDevice = *defaultHwInfo;
        localPlatformDevice.featureTable.flags.ftrLocalMemory = true;
        executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&localPlatformDevice);

        WddmMemoryManagerTest::SetUp();
        wddm->init();
    }
    void TearDown() override {
        WddmMemoryManagerTest::TearDown();
    }
    HardwareInfo localPlatformDevice = {};
    FeatureTable ftrTable = {};
};

TEST_F(WddmMemoryManagerSimpleTestWithLocalMemory, givenLocalMemoryAndImageOrSharedResourceWhenAllocateInDevicePoolIsCalledThenLocalMemoryAllocationAndAndStatusSuccessIsReturned) {
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, true, executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;

    ImageDescriptor imgDesc = {};
    imgDesc.imageWidth = 1;
    imgDesc.imageHeight = 1;
    imgDesc.imageType = ImageType::image2D;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    AllocationType types[] = {AllocationType::image,
                              AllocationType::sharedResourceCopy};

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.preferCompressed = true;

    allocData.imgInfo = &imgInfo;

    for (uint32_t i = 0; i < arrayCount(types); i++) {
        allocData.type = types[i];
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
        EXPECT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
        EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
        EXPECT_EQ(0u, allocation->getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly);
        EXPECT_TRUE(allocData.imgInfo->useLocalMemory);
        memoryManager->freeGraphicsMemory(allocation);
    }
}

class MockWddmMemoryManagerTest : public ::testing::Test {
  public:
    void SetUp() override {
        for (auto i = 0; i < 2; i++) {
            executionEnvironment.rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
            wddm = new WddmMock(*executionEnvironment.rootDeviceEnvironments[i].get());
            executionEnvironment.rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
            executionEnvironment.rootDeviceEnvironments[i]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
        }
        executionEnvironment.initializeMemoryManager();
    }

    void TearDown() override {
    }

    WddmMock *wddm = nullptr;
    MockExecutionEnvironment executionEnvironment{nullptr, true, 2u};
    const uint32_t rootDeviceIndex = 1u;
};

TEST_F(MockWddmMemoryManagerTest, givenValidateAllocationFunctionWhenItIsCalledWithTripleAllocationThenSuccessIsReturned) {
    wddm->init();
    MockWddmMemoryManager memoryManager(executionEnvironment);

    auto wddmAlloc = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, false, MemoryConstants::pageSize}, reinterpret_cast<void *>(0x1000)));

    EXPECT_TRUE(memoryManager.validateAllocationMock(wddmAlloc));

    memoryManager.freeGraphicsMemory(wddmAlloc);
}

TEST_F(MockWddmMemoryManagerTest, givenValidateAllocationFunctionWhenItIsCalledWithZeroSizedAllocationThenSuccessIsReturned) {
    wddm->init();
    MockWddmMemoryManager memoryManager(executionEnvironment);

    auto ptr = reinterpret_cast<void *>(0x1234);
    WddmAllocation allocation{0, 1u /*num gmms*/, AllocationType::commandBuffer, ptr, castToUint64(ptr), 0, nullptr, MemoryPool::system64KBPages, 0u, 1u};
    allocation.getHandleToModify(0u) = 1;

    EXPECT_TRUE(memoryManager.validateAllocationMock(&allocation));
}

TEST_F(MockWddmMemoryManagerTest, givenCreateOrReleaseDeviceSpecificMemResourcesWhenCreatingMemoryManagerObjectThenTheseMethodsAreEmpty) {
    wddm->init();
    MockWddmMemoryManager memoryManager(executionEnvironment);
    memoryManager.createDeviceSpecificMemResources(1);
    memoryManager.releaseDeviceSpecificMemResources(1);
}

TEST_F(MockWddmMemoryManagerTest, givenWddmMemoryManagerWhenVerifySharedHandleThenVerifySharedHandleIsCalled) {
    wddm->init();
    MockWddmMemoryManager memoryManager(executionEnvironment);
    osHandle handle = 1;
    memoryManager.verifyHandle(handle, rootDeviceIndex, false);
    EXPECT_EQ(0u, wddm->counterVerifyNTHandle);
    EXPECT_EQ(1u, wddm->counterVerifySharedHandle);
}

TEST_F(MockWddmMemoryManagerTest, givenWddmMemoryManagerWhenVerifyNTHandleThenVerifyNTHandleIsCalled) {
    wddm->init();
    MockWddmMemoryManager memoryManager(executionEnvironment);
    osHandle handle = 1;
    memoryManager.verifyHandle(handle, rootDeviceIndex, true);
    EXPECT_EQ(1u, wddm->counterVerifyNTHandle);
    EXPECT_EQ(0u, wddm->counterVerifySharedHandle);
}

TEST_F(MockWddmMemoryManagerTest, givenWddmMemoryManagerWhenIsNTHandleisCalledThenVerifyNTHandleisCalled) {
    wddm->init();
    MockWddmMemoryManager memoryManager(executionEnvironment);
    osHandle handle = 1;
    memoryManager.isNTHandle(handle, rootDeviceIndex);
    EXPECT_EQ(1u, wddm->counterVerifyNTHandle);
    EXPECT_EQ(0u, wddm->counterVerifySharedHandle);
}

TEST_F(MockWddmMemoryManagerTest, givenEnabled64kbpagesWhenCreatingGraphicsMemoryForBufferWithoutHostPtrThen64kbAddressIsAllocated) {
    DebugManagerStateRestore dbgRestore;
    wddm->init();
    debugManager.flags.Enable64kbpages.set(true);
    MemoryManagerCreate<WddmMemoryManager> memoryManager64k(true, false, executionEnvironment);
    if (memoryManager64k.isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    EXPECT_EQ(0U, wddm->createAllocationResult.called);

    GraphicsAllocation *galloc = memoryManager64k.allocateGraphicsMemoryWithProperties({rootDeviceIndex, MemoryConstants::pageSize64k, AllocationType::bufferHostMemory, mockDeviceBitfield});
    EXPECT_NE(0U, wddm->createAllocationResult.called);
    EXPECT_NE(nullptr, galloc);
    EXPECT_EQ(true, galloc->isLocked());
    EXPECT_NE(nullptr, galloc->getUnderlyingBuffer());
    EXPECT_EQ(0u, (uintptr_t)galloc->getUnderlyingBuffer() % MemoryConstants::pageSize64k);
    EXPECT_EQ(0u, (uintptr_t)galloc->getGpuAddress() % MemoryConstants::pageSize64k);
    memoryManager64k.freeGraphicsMemory(galloc);
}

HWTEST_F(MockWddmMemoryManagerTest, givenEnabled64kbPagesWhenAllocationIsCreatedWithSizeSmallerThan64kbThenGraphicsAllocationsHas64kbAlignedUnderlyingSize) {
    DebugManagerStateRestore dbgRestore;
    wddm->init();
    debugManager.flags.Enable64kbpages.set(true);
    debugManager.flags.EnableCpuCacheForResources.set(0);
    MockWddmMemoryManager memoryManager(true, false, executionEnvironment);
    AllocationData allocationData;
    allocationData.size = 1u;
    allocationData.rootDeviceIndex = rootDeviceIndex;

    auto graphicsAllocation = memoryManager.allocateGraphicsMemory64kb(allocationData);

    EXPECT_NE(nullptr, graphicsAllocation);
    EXPECT_EQ(MemoryConstants::pageSize64k, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_NE(0llu, graphicsAllocation->getGpuAddress());
    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    auto &productHelper = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getProductHelper();
    EXPECT_EQ(productHelper.isCachingOnCpuAvailable(), graphicsAllocation->getDefaultGmm()->resourceParams.Flags.Info.Cacheable);

    memoryManager.freeGraphicsMemory(graphicsAllocation);
}

TEST_F(MockWddmMemoryManagerTest, givenWddmWhenallocateGraphicsMemory64kbThenLockResultAndmapGpuVirtualAddressIsCalled) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.Enable64kbpages.set(true);
    wddm->init();
    MockWddmMemoryManager memoryManager64k(executionEnvironment);
    uint32_t lockCount = wddm->lockResult.called;
    uint32_t mapGpuVirtualAddressResult = wddm->mapGpuVirtualAddressResult.called;
    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    GraphicsAllocation *galloc = memoryManager64k.allocateGraphicsMemory64kb(allocationData);
    EXPECT_EQ(lockCount + 1, wddm->lockResult.called);
    EXPECT_EQ(mapGpuVirtualAddressResult + 1, wddm->mapGpuVirtualAddressResult.called);

    if (is32bit || executionEnvironment.rootDeviceEnvironments[0]->isFullRangeSvm()) {
        EXPECT_NE(nullptr, wddm->mapGpuVirtualAddressResult.cpuPtrPassed);
    } else {
        EXPECT_EQ(nullptr, wddm->mapGpuVirtualAddressResult.cpuPtrPassed);
    }
    memoryManager64k.freeGraphicsMemory(galloc);
}

TEST_F(MockWddmMemoryManagerTest, givenAllocateGraphicsMemoryForBufferAndRequestedSizeIsHugeThenResultAllocationIsSplitted) {
    DebugManagerStateRestore dbgRestore;

    wddm->init();
    wddm->mapGpuVaStatus = true;
    VariableBackup<bool> restorer{&wddm->callBaseMapGpuVa, false};

    for (bool enable64KBpages : {true, false}) {
        wddm->createAllocationResult.called = 0U;
        debugManager.flags.Enable64kbpages.set(enable64KBpages);
        MemoryManagerCreate<MockWddmMemoryManager> memoryManager(true, false, executionEnvironment);
        if (memoryManager.isLimitedGPU(0)) {
            GTEST_SKIP();
        }
        EXPECT_EQ(0u, wddm->createAllocationResult.called);

        memoryManager.hugeGfxMemoryChunkSize = MemoryConstants::pageSize64k - MemoryConstants::pageSize;

        WddmAllocation *wddmAlloc = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemoryWithProperties({rootDeviceIndex, MemoryConstants::pageSize64k * 3, AllocationType::buffer, mockDeviceBitfield}));
        EXPECT_NE(nullptr, wddmAlloc);
        EXPECT_EQ(4u, wddmAlloc->getNumGmms());
        EXPECT_EQ(4u, wddm->createAllocationResult.called);

        auto gmmHelper = executionEnvironment.rootDeviceEnvironments[wddmAlloc->getRootDeviceIndex()]->getGmmHelper();
        EXPECT_EQ(wddmAlloc->getGpuAddressToModify(), gmmHelper->canonize(wddmAlloc->getReservedGpuVirtualAddress()));

        memoryManager.freeGraphicsMemory(wddmAlloc);
    }
}

TEST_F(MockWddmMemoryManagerTest, givenDefaultMemoryManagerWhenItIsCreatedThenCorrectHugeGfxMemoryChunkIsSet) {
    MockWddmMemoryManager memoryManager(executionEnvironment);
    EXPECT_EQ(memoryManager.getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod::allocateByKmd), 4 * MemoryConstants::gigaByte - MemoryConstants::pageSize64k);
    EXPECT_EQ(memoryManager.getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod::useUmdSystemPtr), 4 * MemoryConstants::gigaByte - MemoryConstants::pageSize64k);
}

TEST_F(MockWddmMemoryManagerTest, givenDebugOverrideWhenQueryIsDoneThenProperSizeIsReturned) {
    DebugManagerStateRestore dbgRestore;
    NEO::debugManager.flags.ForceWddmHugeChunkSizeMB.set(256);
    MockWddmMemoryManager memoryManager(executionEnvironment);
    EXPECT_EQ(256 * MemoryConstants::megaByte, memoryManager.getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod::allocateByKmd));
}

TEST_F(MockWddmMemoryManagerTest, givenAllocateGraphicsMemoryForHostBufferAndRequestedSizeIsHugeThenResultAllocationIsSplitted) {
    DebugManagerStateRestore dbgRestore;

    wddm->init();
    wddm->mapGpuVaStatus = true;
    VariableBackup<bool> restorer{&wddm->callBaseMapGpuVa, false};

    debugManager.flags.Enable64kbpages.set(true);
    MemoryManagerCreate<MockWddmMemoryManager> memoryManager(true, false, executionEnvironment);
    if (memoryManager.isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    EXPECT_EQ(0u, wddm->createAllocationResult.called);

    memoryManager.hugeGfxMemoryChunkSize = MemoryConstants::pageSize64k - MemoryConstants::pageSize;

    std::vector<uint8_t> hostPtr(MemoryConstants::pageSize64k * 3);
    AllocationProperties allocProps{rootDeviceIndex, MemoryConstants::pageSize64k * 3, AllocationType::bufferHostMemory, mockDeviceBitfield};
    allocProps.flags.allocateMemory = false;
    WddmAllocation *wddmAlloc = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemoryWithProperties(allocProps, hostPtr.data()));

    EXPECT_NE(nullptr, wddmAlloc);
    EXPECT_EQ(4u, wddmAlloc->getNumGmms());
    EXPECT_EQ(4u, wddm->createAllocationResult.called);

    auto gmmHelper = memoryManager.getGmmHelper(wddmAlloc->getRootDeviceIndex());
    EXPECT_EQ(wddmAlloc->getGpuAddressToModify(), gmmHelper->canonize(wddmAlloc->getReservedGpuVirtualAddress()));

    memoryManager.freeGraphicsMemory(wddmAlloc);
}

TEST_F(MockWddmMemoryManagerTest, givenDefaultMemoryManagerWhenItIsCreatedThenAsyncDeleterEnabledIsTrue) {
    wddm->init();
    WddmMemoryManager memoryManager(executionEnvironment);
    EXPECT_TRUE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_NE(nullptr, memoryManager.getDeferredDeleter());
}

TEST_F(MockWddmMemoryManagerTest, givenEnabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsTrueAndDeleterIsNotNullptr) {
    wddm->init();
    bool defaultEnableDeferredDeleterFlag = debugManager.flags.EnableDeferredDeleter.get();
    debugManager.flags.EnableDeferredDeleter.set(true);
    WddmMemoryManager memoryManager(executionEnvironment);
    EXPECT_TRUE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_NE(nullptr, memoryManager.getDeferredDeleter());
    debugManager.flags.EnableDeferredDeleter.set(defaultEnableDeferredDeleterFlag);
}

TEST_F(MockWddmMemoryManagerTest, givenDisabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsFalseAndDeleterIsNullptr) {
    wddm->init();
    bool defaultEnableDeferredDeleterFlag = debugManager.flags.EnableDeferredDeleter.get();
    debugManager.flags.EnableDeferredDeleter.set(false);
    WddmMemoryManager memoryManager(executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    debugManager.flags.EnableDeferredDeleter.set(defaultEnableDeferredDeleterFlag);
}

TEST_F(MockWddmMemoryManagerTest, givenPageTableManagerWhenMapAuxGpuVaCalledThenUseWddmToMap) {
    auto &productHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    if (!productHelper.isPageTableManagerSupported(*defaultHwInfo)) {
        GTEST_SKIP();
    }
    wddm->init();
    WddmMemoryManager memoryManager(executionEnvironment);
    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(executionEnvironment, rootDeviceIndex, 1));
    auto hwInfo = *defaultHwInfo;
    EngineInstancesContainer regularEngines = {
        {aub_stream::ENGINE_CCS, EngineUsage::regular}};

    memoryManager.createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(regularEngines[0],
                                                                                                     PreemptionHelper::getDefaultPreemptionMode(hwInfo)));

    auto mockMngr = new MockGmmPageTableMngr();
    for (auto engine : memoryManager.getRegisteredEngines(1u)) {
        engine.commandStreamReceiver->pageTableManager.reset(mockMngr);
    }

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(AllocationProperties(1, MemoryConstants::pageSize, AllocationType::internalHostMemory, mockDeviceBitfield));

    GMM_DDI_UPDATEAUXTABLE expectedDdiUpdateAuxTable = {};
    expectedDdiUpdateAuxTable.BaseGpuVA = allocation->getGpuAddress();
    expectedDdiUpdateAuxTable.BaseResInfo = allocation->getDefaultGmm()->gmmResourceInfo->peekGmmResourceInfo();
    expectedDdiUpdateAuxTable.DoNotWait = true;
    expectedDdiUpdateAuxTable.Map = true;

    auto expectedCallCount = static_cast<uint32_t>(regularEngines.size());

    auto result = memoryManager.mapAuxGpuVA(allocation);
    EXPECT_TRUE(result);
    EXPECT_TRUE(memcmp(&expectedDdiUpdateAuxTable, &mockMngr->updateAuxTableParamsPassed[0].ddiUpdateAuxTable, sizeof(GMM_DDI_UPDATEAUXTABLE)) == 0);
    memoryManager.freeGraphicsMemory(allocation);
    EXPECT_EQ(expectedCallCount, mockMngr->updateAuxTableCalled);
}

TEST_F(MockWddmMemoryManagerTest, givenCompressedAllocationWhenMappedGpuVaAndPageTableNotSupportedThenMapAuxVa) {
    auto &productHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    if (!productHelper.isPageTableManagerSupported(*defaultHwInfo)) {
        GTEST_SKIP();
    }
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[1].get();
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), reinterpret_cast<void *>(123), 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    gmm->setCompressionEnabled(true);
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    WddmMock wddm(*executionEnvironment.rootDeviceEnvironments[1].get());
    wddm.init();
    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(executionEnvironment, 1u, 1));
    auto hwInfo = *defaultHwInfo;
    EngineInstancesContainer regularEngines = {
        {aub_stream::ENGINE_CCS, EngineUsage::regular}};

    executionEnvironment.memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(regularEngines[0],
                                                                                                                           PreemptionHelper::getDefaultPreemptionMode(hwInfo)));

    auto mockMngr = new MockGmmPageTableMngr();
    for (auto engine : executionEnvironment.memoryManager->getRegisteredEngines(1u)) {
        engine.commandStreamReceiver->pageTableManager.reset(mockMngr);
    }

    auto hwInfoMock = hardwareInfoTable[wddm.getGfxPlatform()->eProductFamily];
    ASSERT_NE(nullptr, hwInfoMock);
    auto result = wddm.mapGpuVirtualAddress(gmm.get(), ALLOCATION_HANDLE, wddm.getGfxPartition().Standard.Base, wddm.getGfxPartition().Standard.Limit, 0u, gpuVa, AllocationType::unknown);
    ASSERT_TRUE(result);

    auto gmmHelper = rootDeviceEnvironment->getGmmHelper();
    EXPECT_EQ(gmmHelper->canonize(wddm.getGfxPartition().Standard.Base), gpuVa);
    EXPECT_EQ(0u, mockMngr->updateAuxTableCalled);
}

TEST_F(MockWddmMemoryManagerTest, givenCompressedAllocationWhenMappedGpuVaAndPageTableSupportedThenMapAuxVa) {
    auto &productHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    if (!productHelper.isPageTableManagerSupported(*defaultHwInfo)) {
        GTEST_SKIP();
    }
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[1].get();
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), reinterpret_cast<void *>(123), 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    gmm->setCompressionEnabled(true);
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    WddmMock wddm(*executionEnvironment.rootDeviceEnvironments[1].get());
    wddm.init();
    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(executionEnvironment, 1u, 1));
    auto hwInfo = *defaultHwInfo;
    EngineInstancesContainer regularEngines = {
        {aub_stream::ENGINE_CCS, EngineUsage::regular}};

    executionEnvironment.memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(regularEngines[0],
                                                                                                                           PreemptionHelper::getDefaultPreemptionMode(hwInfo)));

    auto mockMngr = new MockGmmPageTableMngr();
    for (auto engine : executionEnvironment.memoryManager->getRegisteredEngines(1)) {
        engine.commandStreamReceiver->pageTableManager.reset(mockMngr);
    }

    GMM_DDI_UPDATEAUXTABLE expectedDdiUpdateAuxTable = {};
    auto gmmHelper = rootDeviceEnvironment->getGmmHelper();
    expectedDdiUpdateAuxTable.BaseGpuVA = gmmHelper->canonize(wddm.getGfxPartition().Standard.Base);
    expectedDdiUpdateAuxTable.BaseResInfo = gmm->gmmResourceInfo->peekGmmResourceInfo();
    expectedDdiUpdateAuxTable.DoNotWait = true;
    expectedDdiUpdateAuxTable.Map = true;

    auto expectedCallCount = executionEnvironment.memoryManager->getRegisteredEngines(1).size();

    auto hwInfoMock = hardwareInfoTable[wddm.getGfxPlatform()->eProductFamily];
    ASSERT_NE(nullptr, hwInfoMock);
    auto result = wddm.mapGpuVirtualAddress(gmm.get(), ALLOCATION_HANDLE, wddm.getGfxPartition().Standard.Base, wddm.getGfxPartition().Standard.Limit, 0u, gpuVa, AllocationType::unknown);
    ASSERT_TRUE(result);
    EXPECT_EQ(gmmHelper->canonize(wddm.getGfxPartition().Standard.Base), gpuVa);
    EXPECT_TRUE(memcmp(&expectedDdiUpdateAuxTable, &mockMngr->updateAuxTableParamsPassed[0].ddiUpdateAuxTable, sizeof(GMM_DDI_UPDATEAUXTABLE)) == 0);
    EXPECT_EQ(expectedCallCount, mockMngr->updateAuxTableCalled);
}

TEST_F(MockWddmMemoryManagerTest, givenCompressedAllocationAndPageTableSupportedWhenReleaseingThenUnmapAuxVa) {
    auto &productHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    if (!productHelper.isPageTableManagerSupported(*defaultHwInfo)) {
        GTEST_SKIP();
    }
    wddm->init();
    WddmMemoryManager memoryManager(executionEnvironment);
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 123;

    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(executionEnvironment, 1u, 1));
    auto hwInfo = *defaultHwInfo;
    EngineInstancesContainer regularEngines = {
        {aub_stream::ENGINE_CCS, EngineUsage::regular}};

    memoryManager.createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(regularEngines[0],
                                                                                                     PreemptionHelper::getDefaultPreemptionMode(hwInfo)));

    auto mockMngr = new MockGmmPageTableMngr();
    for (auto engine : memoryManager.getRegisteredEngines(1u)) {
        engine.commandStreamReceiver->pageTableManager.reset(mockMngr);
    }

    auto wddmAlloc = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemoryWithProperties(AllocationProperties(1, MemoryConstants::pageSize, AllocationType::internalHostMemory, mockDeviceBitfield)));
    wddmAlloc->setGpuAddress(gpuVa);
    wddmAlloc->getDefaultGmm()->setCompressionEnabled(true);

    GMM_DDI_UPDATEAUXTABLE expectedDdiUpdateAuxTable = {};
    expectedDdiUpdateAuxTable.BaseGpuVA = gpuVa;
    expectedDdiUpdateAuxTable.BaseResInfo = wddmAlloc->getDefaultGmm()->gmmResourceInfo->peekGmmResourceInfo();
    expectedDdiUpdateAuxTable.DoNotWait = true;
    expectedDdiUpdateAuxTable.Map = false;

    auto expectedCallCount = memoryManager.getRegisteredEngines(1u).size();

    memoryManager.freeGraphicsMemory(wddmAlloc);

    EXPECT_TRUE(memcmp(&expectedDdiUpdateAuxTable, &mockMngr->updateAuxTableParamsPassed[0].ddiUpdateAuxTable, sizeof(GMM_DDI_UPDATEAUXTABLE)) == 0);
    EXPECT_EQ(expectedCallCount, mockMngr->updateAuxTableCalled);
}

TEST_F(MockWddmMemoryManagerTest, givenNonCompressedAllocationWhenReleaseingThenDontUnmapAuxVa) {
    wddm->init();
    WddmMemoryManager memoryManager(executionEnvironment);

    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(executionEnvironment, 1u, 1));
    auto hwInfo = *defaultHwInfo;
    EngineInstancesContainer regularEngines = {
        {aub_stream::ENGINE_CCS, EngineUsage::regular}};

    memoryManager.createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(regularEngines[0],
                                                                                                     PreemptionHelper::getDefaultPreemptionMode(hwInfo)));

    auto mockMngr = new MockGmmPageTableMngr();
    for (auto engine : memoryManager.getRegisteredEngines(1u)) {
        engine.commandStreamReceiver->pageTableManager.reset(mockMngr);
    }

    auto wddmAlloc = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize}));
    wddmAlloc->getDefaultGmm()->setCompressionEnabled(false);

    memoryManager.freeGraphicsMemory(wddmAlloc);
    EXPECT_EQ(0u, mockMngr->updateAuxTableCalled);
}

TEST_F(MockWddmMemoryManagerTest, givenNonCompressedAllocationWhenMappedGpuVaThenDontMapAuxVa) {
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[1].get();
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), reinterpret_cast<void *>(123), 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    gmm->setCompressionEnabled(false);
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    WddmMock wddm(*rootDeviceEnvironment);
    wddm.init();

    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(executionEnvironment, 1u, 1));
    auto hwInfo = *defaultHwInfo;
    EngineInstancesContainer regularEngines = {
        {aub_stream::ENGINE_CCS, EngineUsage::regular}};

    executionEnvironment.memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(regularEngines[0],
                                                                                                                           PreemptionHelper::getDefaultPreemptionMode(hwInfo)));

    auto mockMngr = new MockGmmPageTableMngr();
    for (auto engine : executionEnvironment.memoryManager->getRegisteredEngines(1u)) {
        engine.commandStreamReceiver->pageTableManager.reset(mockMngr);
    }

    auto result = wddm.mapGpuVirtualAddress(gmm.get(), ALLOCATION_HANDLE, wddm.getGfxPartition().Standard.Base, wddm.getGfxPartition().Standard.Limit, 0u, gpuVa, AllocationType::unknown);
    ASSERT_TRUE(result);
    EXPECT_EQ(0u, mockMngr->updateAuxTableCalled);
}

TEST_F(MockWddmMemoryManagerTest, givenFailingAllocationWhenMappedGpuVaThenReturnFalse) {
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[1].get();
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), reinterpret_cast<void *>(123), 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    gmm->setCompressionEnabled(false);
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    WddmMock wddm(*rootDeviceEnvironment);
    wddm.init();

    auto result = wddm.mapGpuVirtualAddress(gmm.get(), 0, 0, 0, 0, gpuVa, AllocationType::unknown);
    ASSERT_FALSE(result);
}

TEST_F(MockWddmMemoryManagerTest, givenCompressedFlagSetWhenInternalIsUnsetThenDontUpdateAuxTable) {
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    wddm->init();
    WddmMemoryManager memoryManager(executionEnvironment);

    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(executionEnvironment, 1u, 1));
    auto hwInfo = *defaultHwInfo;
    EngineInstancesContainer regularEngines = {
        {aub_stream::ENGINE_CCS, EngineUsage::regular}};

    memoryManager.createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(regularEngines[0],
                                                                                                     PreemptionHelper::getDefaultPreemptionMode(hwInfo)));

    auto mockMngr = new MockGmmPageTableMngr();
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[1].get();
    for (auto engine : memoryManager.getRegisteredEngines(1u)) {
        engine.commandStreamReceiver->pageTableManager.reset(mockMngr);
    }
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto myGmm = new Gmm(rootDeviceEnvironment->getGmmHelper(), reinterpret_cast<void *>(123), 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    myGmm->setCompressionEnabled(false);
    myGmm->gmmResourceInfo->getResourceFlags()->Info.RenderCompressed = 1;

    auto wddmAlloc = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize}));
    delete wddmAlloc->getDefaultGmm();
    wddmAlloc->setDefaultGmm(myGmm);

    auto result = wddm->mapGpuVirtualAddress(myGmm, ALLOCATION_HANDLE, wddm->getGfxPartition().Standard.Base, wddm->getGfxPartition().Standard.Limit, 0u, gpuVa, AllocationType::unknown);
    EXPECT_TRUE(result);
    memoryManager.freeGraphicsMemory(wddmAlloc);
    EXPECT_EQ(0u, mockMngr->updateAuxTableCalled);
}

HWTEST_F(MockWddmMemoryManagerTest, givenCompressedFlagSetWhenInternalIsSetThenUpdateAuxTable) {
    auto &productHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    if (!productHelper.isPageTableManagerSupported(*defaultHwInfo)) {
        GTEST_SKIP();
    }
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    wddm->init();
    WddmMemoryManager memoryManager(executionEnvironment);

    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(executionEnvironment, 1u, 1));
    auto hwInfo = *defaultHwInfo;
    EngineInstancesContainer regularEngines = {
        {aub_stream::ENGINE_CCS, EngineUsage::regular}};

    memoryManager.createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(regularEngines[0],
                                                                                                     PreemptionHelper::getDefaultPreemptionMode(hwInfo)));

    auto mockMngr = new MockGmmPageTableMngr();
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[1].get();
    rootDeviceEnvironment->executionEnvironment.initializeMemoryManager();
    for (auto engine : memoryManager.getRegisteredEngines(1u)) {
        engine.commandStreamReceiver->pageTableManager.reset(mockMngr);
    }
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto myGmm = new Gmm(rootDeviceEnvironment->getGmmHelper(), reinterpret_cast<void *>(123), 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    myGmm->setCompressionEnabled(true);
    myGmm->gmmResourceInfo->getResourceFlags()->Info.RenderCompressed = 1;

    auto wddmAlloc = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize}));
    delete wddmAlloc->getDefaultGmm();
    wddmAlloc->setDefaultGmm(myGmm);

    auto expectedCallCount = memoryManager.getRegisteredEngines(rootDeviceIndex).size();

    auto result = wddm->mapGpuVirtualAddress(myGmm, ALLOCATION_HANDLE, wddm->getGfxPartition().Standard.Base, wddm->getGfxPartition().Standard.Limit, 0u, gpuVa, AllocationType::unknown);
    EXPECT_TRUE(result);

    auto ultCsr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(csr.get());
    ultCsr->directSubmissionAvailable = true;
    EXPECT_FALSE(ultCsr->stopDirectSubmissionCalled);

    memoryManager.freeGraphicsMemory(wddmAlloc);

    EXPECT_TRUE(ultCsr->stopDirectSubmissionCalled);
    EXPECT_EQ(expectedCallCount, mockMngr->updateAuxTableCalled);
}

struct WddmMemoryManagerWithAsyncDeleterTest : public ::testing::Test {
    void SetUp() override {
        wddm = new WddmMock(*executionEnvironment.rootDeviceEnvironments[0].get());
        executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
        wddm->resetGdi(new MockGdi());
        wddm->callBaseDestroyAllocations = false;
        wddm->init();
        deleter = new MockDeferredDeleter;
        memoryManager = std::make_unique<MockWddmMemoryManager>(executionEnvironment);
        memoryManager->setDeferredDeleter(deleter);
    }

    void TearDown() override {
    }

    MockExecutionEnvironment executionEnvironment{};
    MockDeferredDeleter *deleter = nullptr;
    std::unique_ptr<MockWddmMemoryManager> memoryManager;
    HardwareInfo *hwInfo;
    WddmMock *wddm;
};

TEST_F(WddmMemoryManagerWithAsyncDeleterTest, givenWddmWhenAsyncDeleterIsEnabledThenCanDeferDeletions) {
    EXPECT_EQ(0, deleter->deferDeletionCalled);
    memoryManager->tryDeferDeletions(nullptr, 0, 0, 0, AllocationType::unknown);
    EXPECT_EQ(1, deleter->deferDeletionCalled);
    EXPECT_EQ(1u, wddm->destroyAllocationResult.called);
}

TEST_F(WddmMemoryManagerWithAsyncDeleterTest, givenWddmWhenAsyncDeleterIsDisabledThenCannotDeferDeletions) {
    memoryManager->setDeferredDeleter(nullptr);
    memoryManager->tryDeferDeletions(nullptr, 0, 0, 0, AllocationType::unknown);
    EXPECT_EQ(1u, wddm->destroyAllocationResult.called);
}

class WddmMemoryManagerTest2 : public ::testing::Test {
  public:
    MockWddmMemoryManager *memoryManager = nullptr;

    void SetUp() override {
        // wddm is deleted by memory manager

        wddm = new WddmMock(*executionEnvironment.rootDeviceEnvironments[0]);
        ASSERT_NE(nullptr, wddm);
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo);
        wddm->init();
        executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
        osInterface = executionEnvironment.rootDeviceEnvironments[0]->osInterface.get();
        memoryManager = new (std::nothrow) MockWddmMemoryManager(executionEnvironment);
        executionEnvironment.memoryManager.reset(memoryManager);
        // assert we have memory manager
        ASSERT_NE(nullptr, memoryManager);
        csr.reset(createCommandStream(executionEnvironment, 0u, 1));
        auto &gfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
        osContext = memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment.rootDeviceEnvironments[0])[0],
                                                                                                                      preemptionMode));
        osContext->incRefInternal();
    }

    void TearDown() override {
        osContext->decRefInternal();
    }

    MockExecutionEnvironment executionEnvironment{};
    WddmMock *wddm = nullptr;
    std::unique_ptr<CommandStreamReceiver> csr;
    OSInterface *osInterface;
    OsContext *osContext;
};

TEST_F(WddmMemoryManagerTest2, givenReadOnlyMemoryWhenCreateAllocationFailsThenPopulateOsHandlesReturnsInvalidPointer) {
    OsHandleStorage handleStorage;
    handleStorage.fragmentCount = 1;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 0x1000;
    handleStorage.fragmentStorageData[0].freeTheFragment = false;

    wddm->callBaseCreateAllocationsAndMapGpuVa = false;
    wddm->createAllocationsAndMapGpuVaStatus = STATUS_GRAPHICS_NO_VIDEO_MEMORY;

    auto result = memoryManager->populateOsHandles(handleStorage, 0);

    EXPECT_EQ(MemoryManager::AllocationStatus::InvalidHostPointer, result);
    handleStorage.fragmentStorageData[0].freeTheFragment = true;
    memoryManager->cleanOsHandles(handleStorage, 0);
}

TEST_F(WddmMemoryManagerTest2, givenReadOnlyMemoryPassedToPopulateOsHandlesWhenCreateAllocationFailsThenAllocatedFragmentsAreNotStored) {
    OsHandleStorage handleStorage;
    OsHandleWin handle;
    handleStorage.fragmentCount = 2;
    handleStorage.fragmentStorageData[0].osHandleStorage = &handle;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 0x1000;

    handleStorage.fragmentStorageData[1].cpuPtr = reinterpret_cast<void *>(0x2000);
    handleStorage.fragmentStorageData[1].fragmentSize = 0x6000;

    wddm->callBaseCreateAllocationsAndMapGpuVa = false;
    wddm->createAllocationsAndMapGpuVaStatus = STATUS_GRAPHICS_NO_VIDEO_MEMORY;

    auto result = memoryManager->populateOsHandles(handleStorage, mockRootDeviceIndex);
    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());

    EXPECT_EQ(MemoryManager::AllocationStatus::InvalidHostPointer, result);
    auto numberOfStoredFragments = hostPtrManager->getFragmentCount();
    EXPECT_EQ(0u, numberOfStoredFragments);
    EXPECT_EQ(nullptr, hostPtrManager->getFragment({handleStorage.fragmentStorageData[1].cpuPtr, mockRootDeviceIndex}));

    handleStorage.fragmentStorageData[1].freeTheFragment = true;
    memoryManager->cleanOsHandles(handleStorage, mockRootDeviceIndex);
}

TEST(WddmMemoryManagerTest3, givenDefaultWddmMemoryManagerWhenItIsQueriedForInternalHeapBaseThenHeapInternalBaseIsReturned) {
    MockExecutionEnvironment executionEnvironment{};
    auto wddm = new WddmMock(*executionEnvironment.rootDeviceEnvironments[0].get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
    wddm->init();
    MockWddmMemoryManager memoryManager(executionEnvironment);
    auto heapBase = wddm->getGfxPartition().Heap32[static_cast<uint32_t>(HeapIndex::heapInternalDeviceMemory)].Base;
    heapBase = std::max(heapBase, static_cast<uint64_t>(wddm->getWddmMinAddress()));
    EXPECT_EQ(heapBase, memoryManager.getInternalHeapBaseAddress(0, true));
}

TEST(WddmMemoryManagerTest3, givenUsedTagAllocationInWddmMemoryManagerWhenCleanupMemoryManagerThenDontAccessCsr) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForcePreferredAllocationMethod.set(static_cast<int32_t>(GfxMemoryAllocationMethod::useUmdSystemPtr));
    MockExecutionEnvironment executionEnvironment{};
    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(executionEnvironment, 0, 1));
    auto wddm = new WddmMock(*executionEnvironment.rootDeviceEnvironments[0].get());
    auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo);
    wddm->init();

    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
    executionEnvironment.memoryManager = std::make_unique<WddmMemoryManager>(executionEnvironment);
    auto osContext = executionEnvironment.memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular},
                                                                                                                                            preemptionMode));
    csr->setupContext(*osContext);

    auto tagAllocator = csr->getEventPerfCountAllocator(100);
    auto allocation = tagAllocator->getTag()->getBaseGraphicsAllocation();
    allocation->getDefaultGraphicsAllocation()->updateTaskCount(1, csr->getOsContext().getContextId());
    csr.reset();
    EXPECT_NO_THROW(executionEnvironment.memoryManager.reset());
}

TEST(WddmMemoryManagerTest3, givenMultipleRootDeviceWhenMemoryManagerGetsWddmThenWddmIsFromCorrectRootDevice) {
    VariableBackup<bool> emptyFilesBackup(&NEO::returnEmptyFilesVector, true);
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleRootDevices.set(4);
    VariableBackup<UltHwConfig> backup{&ultHwConfig};
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    MockExecutionEnvironment executionEnvironment{};
    prepareDeviceEnvironments(executionEnvironment);

    MockWddmMemoryManager wddmMemoryManager(executionEnvironment);
    for (auto i = 0u; i < executionEnvironment.rootDeviceEnvironments.size(); i++) {
        auto wddmFromRootDevice = executionEnvironment.rootDeviceEnvironments[i]->osInterface->getDriverModel()->as<Wddm>();
        EXPECT_EQ(wddmFromRootDevice, &wddmMemoryManager.getWddm(i));
    }
}

TEST(WddmMemoryManagerTest3, givenMultipleRootDeviceWhenCreateMemoryManagerThenTakeMaxMallocRestrictionAvailable) {
    uint32_t numRootDevices = 4u;
    VariableBackup<bool> emptyFilesBackup(&NEO::returnEmptyFilesVector, true);
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    VariableBackup<UltHwConfig> backup{&ultHwConfig};
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    MockExecutionEnvironment executionEnvironment{};
    prepareDeviceEnvironments(executionEnvironment);
    for (auto i = 0u; i < numRootDevices; i++) {
        auto wddm = static_cast<WddmMock *>(executionEnvironment.rootDeviceEnvironments[i]->osInterface->getDriverModel()->as<Wddm>());
        wddm->minAddress = i * (numRootDevices - i);
    }

    MockWddmMemoryManager wddmMemoryManager(executionEnvironment);

    EXPECT_EQ(4u, wddmMemoryManager.getAlignedMallocRestrictions()->minAddress);
}

TEST(WddmMemoryManagerTest3, givenNoLocalMemoryOnAnyDeviceWhenIsCpuCopyRequiredIsCalledThenFalseIsReturned) {
    VariableBackup<bool> emptyFilesBackup(&NEO::returnEmptyFilesVector, true);
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(false);
    VariableBackup<UltHwConfig> backup{&ultHwConfig};
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    MockExecutionEnvironment executionEnvironment{};
    prepareDeviceEnvironments(executionEnvironment);
    MockWddmMemoryManager wddmMemoryManager(executionEnvironment);
    EXPECT_FALSE(wddmMemoryManager.isCpuCopyRequired(&restorer));
}

TEST(WddmMemoryManagerTest3, givenLocalPointerPassedToIsCpuCopyRequiredThenFalseIsReturned) {
    VariableBackup<bool> emptyFilesBackup(&NEO::returnEmptyFilesVector, true);
    MockExecutionEnvironment executionEnvironment{};
    VariableBackup<UltHwConfig> backup{&ultHwConfig};
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    prepareDeviceEnvironments(executionEnvironment);
    MockWddmMemoryManager wddmMemoryManager(executionEnvironment);
    EXPECT_FALSE(wddmMemoryManager.isCpuCopyRequired(&backup));
    // call multiple times to make sure that result is constant
    EXPECT_FALSE(wddmMemoryManager.isCpuCopyRequired(&backup));
    EXPECT_FALSE(wddmMemoryManager.isCpuCopyRequired(&backup));
    EXPECT_FALSE(wddmMemoryManager.isCpuCopyRequired(&backup));
    EXPECT_FALSE(wddmMemoryManager.isCpuCopyRequired(&backup));
}

TEST(WddmMemoryManagerTest3, givenWddmMemoryManagerWhenGetLocalMemoryIsCalledThenSizeOfLocalMemoryIsReturned) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(4u);

    for (auto i = 0u; i < 4u; i++) {
        executionEnvironment.rootDeviceEnvironments[i]->osInterface.reset();
        auto wddmMock = Wddm::createWddm(nullptr, *executionEnvironment.rootDeviceEnvironments[i]);
        wddmMock->init();

        static_cast<WddmMock *>(wddmMock)->dedicatedVideoMemory = 32 * MemoryConstants::gigaByte;
    }

    MockWddmMemoryManager memoryManager(executionEnvironment);
    for (auto i = 0u; i < 4u; i++) {
        auto wddmMock = executionEnvironment.rootDeviceEnvironments[i]->osInterface->getDriverModel()->as<Wddm>();

        auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo();
        auto deviceMask = std::max(static_cast<uint32_t>(maxNBitValue(hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount)), 1u);

        EXPECT_EQ(wddmMock->getDedicatedVideoMemory(), memoryManager.getLocalMemorySize(i, deviceMask));
    }
}

TEST(WddmMemoryManagerTest3, givenMultipleTilesWhenGetLocalMemorySizeIsCalledThenReturnCorrectValue) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1u);

    auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo();

    executionEnvironment.rootDeviceEnvironments[0]->osInterface.reset();
    auto wddmMock = Wddm::createWddm(nullptr, *executionEnvironment.rootDeviceEnvironments[0]);
    wddmMock->init();

    hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid = 1;
    hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount = 4;

    static_cast<WddmMock *>(wddmMock)->dedicatedVideoMemory = 32 * MemoryConstants::gigaByte;

    MockWddmMemoryManager memoryManager(executionEnvironment);

    auto singleRegionSize = wddmMock->getDedicatedVideoMemory() / hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount;

    EXPECT_EQ(singleRegionSize, memoryManager.getLocalMemorySize(0, 0b0001));
    EXPECT_EQ(singleRegionSize, memoryManager.getLocalMemorySize(0, 0b0010));
    EXPECT_EQ(singleRegionSize, memoryManager.getLocalMemorySize(0, 0b0100));
    EXPECT_EQ(singleRegionSize, memoryManager.getLocalMemorySize(0, 0b1000));

    EXPECT_EQ(singleRegionSize * 2, memoryManager.getLocalMemorySize(0, 0b0011));

    EXPECT_EQ(wddmMock->getDedicatedVideoMemory(), memoryManager.getLocalMemorySize(0, 0b1111));
}

TEST(WddmMemoryManagerTest3, givenNewlyConstructedResidencyDataThenItIsNotResidentOnAnyOsContext) {
    auto maxOsContextCount = 3u;
    ResidencyData residencyData(maxOsContextCount);
    for (auto contextId = 0u; contextId < maxOsContextCount; contextId++) {
        EXPECT_EQ(false, residencyData.resident[contextId]);
    }
}

TEST(WddmMemoryManagerTest3, WhenWddmMemoryManagerIsCreatedThenItIsNonCopyable) {
    EXPECT_FALSE(std::is_move_constructible<WddmMemoryManager>::value);
    EXPECT_FALSE(std::is_copy_constructible<WddmMemoryManager>::value);
}

TEST(WddmMemoryManagerTest3, WhenWddmMemoryManagerIsCreatedThenItIsNonAssignable) {
    EXPECT_FALSE(std::is_move_assignable<WddmMemoryManager>::value);
    EXPECT_FALSE(std::is_copy_assignable<WddmMemoryManager>::value);
}

TEST(WddmMemoryManagerTest3, givenAllocationTypeWhenPassedToWddmAllocationConstructorThenAllocationTypeIsStored) {
    WddmAllocation allocation{0, 1u /*num gmms*/, AllocationType::commandBuffer, nullptr, 0, 0, nullptr, MemoryPool::memoryNull, 0u, 1u};
    EXPECT_EQ(AllocationType::commandBuffer, allocation.getAllocationType());
}

TEST(WddmMemoryManagerTest3, givenMemoryPoolWhenPassedToWddmAllocationConstructorThenMemoryPoolIsStored) {
    WddmAllocation allocation{0, 1u /*num gmms*/, AllocationType::commandBuffer, nullptr, 0, 0, nullptr, MemoryPool::system64KBPages, 0u, 1u};
    EXPECT_EQ(MemoryPool::system64KBPages, allocation.getMemoryPool());

    WddmAllocation allocation2{0, 1u /*num gmms*/, AllocationType::commandBuffer, nullptr, 0, 0, 0u, MemoryPool::systemCpuInaccessible, 0u, 1u};
    EXPECT_EQ(MemoryPool::systemCpuInaccessible, allocation2.getMemoryPool());
}

TEST(WddmMemoryManagerTest3, WhenExternalHeapIsCreatedThenItHasCorrectBase) {
    MockExecutionEnvironment executionEnvironment{};
    auto wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment.rootDeviceEnvironments[0].get()));
    wddm->init();
    uint64_t base = 0x56000;
    uint64_t size = 0x9000;
    wddm->setHeap32(base, size);

    std::unique_ptr<WddmMemoryManager> memoryManager = std::unique_ptr<WddmMemoryManager>(new WddmMemoryManager(executionEnvironment));

    EXPECT_EQ(base, memoryManager->getExternalHeapBaseAddress(0, false));
}

TEST(WddmMemoryManagerTest3, givenWmmWhenAsyncDeleterIsEnabledAndWaitForDeletionsIsCalledThenDeleterInWddmIsSetToNullptr) {
    MockExecutionEnvironment executionEnvironment{};
    auto wddm = new WddmMock(*executionEnvironment.rootDeviceEnvironments[0].get());
    wddm->init();
    bool actualDeleterFlag = debugManager.flags.EnableDeferredDeleter.get();
    debugManager.flags.EnableDeferredDeleter.set(true);
    MockWddmMemoryManager memoryManager(executionEnvironment);
    EXPECT_NE(nullptr, memoryManager.getDeferredDeleter());
    memoryManager.waitForDeletions();
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    debugManager.flags.EnableDeferredDeleter.set(actualDeleterFlag);
}

class WddmMemoryManagerBindlessHeapHelperCustomHeapAllocatorCfgTest : public WddmMemoryManagerSimpleTest {
  public:
    void SetUp() override {
        debugManager.flags.UseExternalAllocatorForSshAndDsh.set(1);
        WddmMemoryManagerSimpleTest::SetUp();
        wddm->callBaseMapGpuVa = false;

        heapBase = alignUp(0xAAAAAAAA, alignment);
        heapFrontStart = heapBase;
        heapRegularStart = heapFrontStart + heapFrontSize;

        heapFrontWindow = std::make_unique<HeapAllocator>(heapFrontStart, heapFrontSize, alignment, 0);
        heapRegular = std::make_unique<HeapAllocator>(heapRegularStart, heapRegularSize, alignment, 0);
    }

    void TearDown() override {
        WddmMemoryManagerSimpleTest::TearDown();
    }

    DebugManagerStateRestore restore{};

    size_t allocationSize = MemoryConstants::pageSize64k;
    size_t alignment = MemoryConstants::pageSize64k;

    size_t heapFrontSize = 1 * MemoryConstants::gigaByte;
    size_t heapRegularSize = 2 * MemoryConstants::gigaByte;

    uint64_t heapBase = 0u;
    uint64_t heapFrontStart = 0u;
    uint64_t heapRegularStart = 0u;

    std::unique_ptr<HeapAllocator> heapFrontWindow;
    std::unique_ptr<HeapAllocator> heapRegular;
};

TEST_F(WddmMemoryManagerBindlessHeapHelperCustomHeapAllocatorCfgTest, givenCustomHeapAllocatorForFrontWindowWhenAllocatingThenGpuAddressAndBaseAreAssignedByCustomAllocator) {
    memoryManager->addCustomHeapAllocatorConfig(AllocationType::linearStream, true, {heapFrontWindow.get(), heapBase});
    memoryManager->addCustomHeapAllocatorConfig(AllocationType::linearStream, false, {heapRegular.get(), heapBase});

    NEO::AllocationProperties properties{mockRootDeviceIndex, true, allocationSize, AllocationType::linearStream, false, mockDeviceBitfield};
    properties.flags.use32BitFrontWindow = 1;
    properties.alignment = alignment;

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(properties));
    ASSERT_NE(nullptr, allocation);

    EXPECT_EQ(heapFrontStart, allocation->getGpuBaseAddress());
    EXPECT_EQ(heapFrontStart, allocation->getGpuAddress());

    EXPECT_LE(allocationSize, allocation->getUnderlyingBufferSize());
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_TRUE(allocation->isAllocInFrontWindowPool());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerBindlessHeapHelperCustomHeapAllocatorCfgTest, givenCustomHeapAllocatorForNonFrontWindowHeapWhenAllocatingThenGpuAddressAndBaseAreAssignedByCustomAllocator) {
    memoryManager->addCustomHeapAllocatorConfig(AllocationType::linearStream, true, {heapFrontWindow.get(), heapBase});
    memoryManager->addCustomHeapAllocatorConfig(AllocationType::linearStream, false, {heapRegular.get(), heapBase});

    NEO::AllocationProperties properties{mockRootDeviceIndex, true, allocationSize, AllocationType::linearStream, false, mockDeviceBitfield};
    properties.flags.use32BitFrontWindow = 0;
    properties.alignment = alignment;

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(properties));
    ASSERT_NE(nullptr, allocation);

    EXPECT_EQ(heapBase, allocation->getGpuBaseAddress());
    EXPECT_EQ(heapRegularStart, allocation->getGpuAddress());

    EXPECT_LE(allocationSize, allocation->getUnderlyingBufferSize());
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_FALSE(allocation->isAllocInFrontWindowPool());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerBindlessHeapHelperCustomHeapAllocatorCfgTest, givenCustomHeapAllocatorCfgWithoutGpuVaBaseWhenAllocatingThenGpuBaseAddressIsNotObtainedFromCfg) {
    CustomHeapAllocatorConfig cfg1;
    cfg1.allocator = heapFrontWindow.get();
    CustomHeapAllocatorConfig cfg2;
    cfg2.allocator = heapRegular.get();

    memoryManager->addCustomHeapAllocatorConfig(AllocationType::linearStream, true, cfg1);
    memoryManager->addCustomHeapAllocatorConfig(AllocationType::linearStream, false, cfg2);

    NEO::AllocationProperties properties{mockRootDeviceIndex, true, allocationSize, AllocationType::linearStream, false, mockDeviceBitfield};
    properties.flags.use32BitFrontWindow = 1;
    properties.alignment = alignment;

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(properties));
    ASSERT_NE(nullptr, allocation);

    EXPECT_NE(heapFrontStart, allocation->getGpuBaseAddress());
    EXPECT_EQ(heapFrontStart, allocation->getGpuAddress());

    EXPECT_LE(allocationSize, allocation->getUnderlyingBufferSize());
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_TRUE(allocation->isAllocInFrontWindowPool());

    memoryManager->freeGraphicsMemory(allocation);
}