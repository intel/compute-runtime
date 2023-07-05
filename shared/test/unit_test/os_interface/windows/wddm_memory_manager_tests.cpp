/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
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

class MockAllocateGraphicsMemoryUsingKmdAndMapItToCpuVAWddm : public MemoryManagerCreate<WddmMemoryManager> {
  public:
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