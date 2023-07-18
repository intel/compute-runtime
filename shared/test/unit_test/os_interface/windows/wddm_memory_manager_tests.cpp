/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/execution_environment_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_client_context_base.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/os_interface/windows/mock_wddm_memory_manager.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"

#include <memory>

using namespace NEO;
using namespace ::testing;

namespace NEO {
namespace Directory {
extern bool ReturnEmptyFilesVector;
}
} // namespace NEO

class MockAllocateGraphicsMemoryWithAlignmentWddm : public MemoryManagerCreate<WddmMemoryManager> {
  public:
    using WddmMemoryManager::allocateGraphicsMemoryWithAlignment;

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
    std::unique_ptr<VariableBackup<bool>> returnEmptyFilesVectorBackup;

    MockAllocateGraphicsMemoryWithAlignmentWddm *memoryManager = nullptr;
    WddmMock *wddm = nullptr;
    ExecutionEnvironment *executionEnvironment = nullptr;

    void SetUp() override {
        returnEmptyFilesVectorBackup = std::make_unique<VariableBackup<bool>>(&NEO::Directory::ReturnEmptyFilesVector, true);
        HardwareInfo *hwInfo = nullptr;
        executionEnvironment = getExecutionEnvironmentImpl(hwInfo, 1);

        memoryManager = new MockAllocateGraphicsMemoryWithAlignmentWddm(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
        wddm = static_cast<WddmMock *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Wddm>());
    }

    void TearDown() override {
        delete executionEnvironment;
    }
};

TEST_F(WddmMemoryManagerTests, GivenAllocDataWithSVMCPUSetWhenAllocateGraphicsMemoryWithAlignmentThenProperFunctionIsUsed) {
    NEO::AllocationData allocData = {};
    allocData.type = NEO::AllocationType::SVM_CPU;
    allocData.forceKMDAllocation = true;
    allocData.makeGPUVaDifferentThanCPUPtr = true;
    memoryManager->allocateGraphicsMemoryWithAlignment(allocData);

    if (preferredAllocationMethod == GfxMemoryAllocationMethod::AllocateByKmd) {
        EXPECT_TRUE(memoryManager->allocateGraphicsMemoryUsingKmdAndMapItToCpuVACalled);
    } else {
        EXPECT_TRUE(memoryManager->allocateSystemMemoryAndCreateGraphicsAllocationFromItCalled);
    }
}

TEST_F(WddmMemoryManagerTests, GivenNotCompressedAndNotLockableAllocationTypeWhenAllocateUsingKmdAndMapToCpuVaThenSetResourceLockable) {
    NEO::AllocationData allocData = {};
    allocData.type = NEO::AllocationType::BUFFER;

    EXPECT_FALSE(GraphicsAllocation::isLockable(allocData.type));
    allocData.forceKMDAllocation = true;
    allocData.makeGPUVaDifferentThanCPUPtr = true;

    memoryManager->callBaseAllocateGraphicsMemoryUsingKmdAndMapItToCpuVA = true;
    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(allocData, false);

    EXPECT_NE(nullptr, graphicsAllocation);

    EXPECT_TRUE(graphicsAllocation->isAllocationLockable());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(WddmMemoryManagerTests, GivenCompressedAndNotLockableAllocationTypeWhenAllocateUsingKmdAndMapToCpuVaThenSetResourceNotLockable) {
    NEO::AllocationData allocData = {};
    allocData.type = NEO::AllocationType::BUFFER;

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

TEST_F(WddmMemoryManagerTests, givenWddmMemoryManagerWithoutLocalMemoryWhenGettingGlobalMemoryPercentThenCorrectValueIsReturned) {
    MockWddmMemoryManager memoryManager(true, false, *executionEnvironment);
    uint32_t rootDeviceIndex = 0u;
    EXPECT_EQ(0.8, memoryManager.getPercentOfGlobalMemoryAvailable(rootDeviceIndex));
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
        returnEmptyFilesVectorBackup = std::make_unique<VariableBackup<bool>>(&NEO::Directory::ReturnEmptyFilesVector, true);
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
        NEO::AllocationData allocData = {};
        allocData.type = NEO::AllocationType::SVM_CPU;
        allocData.forceKMDAllocation = true;
        allocData.makeGPUVaDifferentThanCPUPtr = true;
        auto graphicsAllocation = memoryManager->allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(allocData, false);

        if (preferredAllocationMethod == GfxMemoryAllocationMethod::AllocateByKmd) {
            EXPECT_FALSE(memoryManager->mapGpuVirtualAddressWithCpuPtr);
        } else {
            EXPECT_TRUE(memoryManager->mapGpuVirtualAddressWithCpuPtr);
        }

        memoryManager->freeGraphicsMemory(graphicsAllocation);
    }
    {
        NEO::AllocationData allocData = {};
        allocData.type = NEO::AllocationType::EXTERNAL_HOST_PTR;
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
    allocData.type = NEO::AllocationType::SVM_CPU;
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
    allocData.type = NEO::AllocationType::SVM_CPU;
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
    allocData.type = NEO::AllocationType::SVM_CPU;
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
    allocData.type = NEO::AllocationType::SVM_CPU;
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
    allocData.type = NEO::AllocationType::BUFFER;

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
    allocData.type = NEO::AllocationType::BUFFER_HOST_MEMORY;
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
    allocData.type = NEO::AllocationType::BUFFER;

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(allocData, false);

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
                                                           NEO::AllocationType::BUFFER, nullptr, 0,
                                                           size, nullptr, MemoryPool::LocalMemory,
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
                                                           NEO::AllocationType::BUFFER, nullptr, 0,
                                                           size, nullptr, MemoryPool::System64KBPages,
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

    for (auto type : {AllocationType::BUFFER,
                      AllocationType::SHARED_BUFFER,
                      AllocationType::SCRATCH_SURFACE,
                      AllocationType::LINEAR_STREAM,
                      AllocationType::PRIVATE_SURFACE}) {
        EXPECT_TRUE(wddmMemoryManager->isStatelessAccessRequired(type));
    }
    for (auto type : {AllocationType::BUFFER_HOST_MEMORY,
                      AllocationType::COMMAND_BUFFER,
                      AllocationType::CONSTANT_SURFACE,
                      AllocationType::EXTERNAL_HOST_PTR,
                      AllocationType::FILL_PATTERN,
                      AllocationType::GLOBAL_SURFACE,
                      AllocationType::IMAGE,
                      AllocationType::INDIRECT_OBJECT_HEAP,
                      AllocationType::INSTRUCTION_HEAP,
                      AllocationType::INTERNAL_HEAP,
                      AllocationType::INTERNAL_HOST_MEMORY,
                      AllocationType::KERNEL_ARGS_BUFFER,
                      AllocationType::KERNEL_ISA,
                      AllocationType::KERNEL_ISA_INTERNAL,
                      AllocationType::MAP_ALLOCATION,
                      AllocationType::MCS,
                      AllocationType::PIPE,
                      AllocationType::PREEMPTION,
                      AllocationType::PRINTF_SURFACE,
                      AllocationType::PROFILING_TAG_BUFFER,
                      AllocationType::SHARED_CONTEXT_IMAGE,
                      AllocationType::SHARED_IMAGE,
                      AllocationType::SHARED_RESOURCE_COPY,
                      AllocationType::SURFACE_STATE_HEAP,
                      AllocationType::SVM_CPU,
                      AllocationType::SVM_GPU,
                      AllocationType::SVM_ZERO_COPY,
                      AllocationType::TAG_BUFFER,
                      AllocationType::GLOBAL_FENCE,
                      AllocationType::TIMESTAMP_PACKET_TAG_BUFFER,
                      AllocationType::WRITE_COMBINED,
                      AllocationType::RING_BUFFER,
                      AllocationType::SEMAPHORE_BUFFER,
                      AllocationType::DEBUG_CONTEXT_SAVE_AREA,
                      AllocationType::DEBUG_SBA_TRACKING_BUFFER,
                      AllocationType::DEBUG_MODULE_AREA,
                      AllocationType::UNIFIED_SHARED_MEMORY,
                      AllocationType::WORK_PARTITION_SURFACE,
                      AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER,
                      AllocationType::SW_TAG_BUFFER,
                      AllocationType::DEFERRED_TASKS_LIST,
                      AllocationType::ASSERT_BUFFER}) {
        EXPECT_FALSE(wddmMemoryManager->isStatelessAccessRequired(type));
    }
}

TEST_F(WddmMemoryManagerTests, givenForcePreferredAllocationMethodFlagSetWhenGettingPreferredAllocationMethodThenValueFlagIsReturned) {
    DebugManagerStateRestore restorer;
    EXPECT_EQ(preferredAllocationMethod, MockWddmMemoryManager::getPreferredAllocationMethod());

    for (const auto &allocationMethod : {GfxMemoryAllocationMethod::UseUmdSystemPtr, GfxMemoryAllocationMethod::AllocateByKmd}) {
        DebugManager.flags.ForcePreferredAllocationMethod.set(static_cast<int32_t>(allocationMethod));
        EXPECT_EQ(allocationMethod, MockWddmMemoryManager::getPreferredAllocationMethod());
    }
}
