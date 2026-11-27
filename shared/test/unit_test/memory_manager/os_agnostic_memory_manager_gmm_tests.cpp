/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

class OsAgnosticMemoryManagerBufferGmmTests : public ::testing::Test {
  public:
    void SetUp() override {
        this->hwInfo = *defaultHwInfo;
        this->hwInfo.featureTable.flags.ftrLocalMemory = true;
        this->executionEnvironment = std::make_unique<MockExecutionEnvironment>(&this->hwInfo);
        this->executionEnvironment->initGmm();
        this->memoryManager = std::make_unique<MockMemoryManager>(true, *this->executionEnvironment);
        this->allocationData.rootDeviceIndex = 0u;
        this->allocationData.size = MemoryConstants::pageSize;
        this->allocationData.type = AllocationType::commandBuffer;
    }

    void expectBufferGmm(GraphicsAllocation *allocation, bool isSystemMemory) const {
        ASSERT_NE(nullptr, allocation);
        const auto &rootDeviceEnvironment = *this->executionEnvironment->rootDeviceEnvironments[0];
        this->expectBufferGmmWithUsage(allocation, isSystemMemory, CacheSettingsHelper::getGmmUsageType(allocation->getAllocationType(), this->allocationData.flags.uncacheable, rootDeviceEnvironment.getProductHelper(), *rootDeviceEnvironment.getHardwareInfo()));
    }

    void expectBufferGmmWithUsage(GraphicsAllocation *allocation, bool isSystemMemory, GmmResourceUsageType expectedUsage) const {
        ASSERT_NE(nullptr, allocation);
        auto gmm = allocation->getDefaultGmm();
        ASSERT_NE(nullptr, gmm);
        EXPECT_EQ(expectedUsage, gmm->getResourceUsageType());
        EXPECT_FALSE(gmm->isCompressionEnabled());
        auto resourceParams = reinterpret_cast<GMM_RESCREATE_PARAMS *>(gmm->resourceParamsData.data());
        EXPECT_EQ(allocation->getUnderlyingBufferSize(), resourceParams->BaseWidth64);
        EXPECT_EQ(isSystemMemory, !!resourceParams->Flags.Info.NonLocalOnly);
    }

    HardwareInfo hwInfo{};
    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
    std::unique_ptr<MockMemoryManager> memoryManager;
    AllocationData allocationData{};
    uint8_t hostMemory[MemoryConstants::pageSize]{};
};

TEST_F(OsAgnosticMemoryManagerBufferGmmTests, givenAllocationDataWhenAllocatingWithAlignmentThenAllocationHasSystemMemoryBufferGmm) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);
    expectBufferGmm(allocation, true);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(OsAgnosticMemoryManagerBufferGmmTests, givenUncacheableAllocationDataWhenAllocatingWithAlignmentThenBufferGmmHasUncachedUsage) {
    allocationData.type = AllocationType::buffer;
    allocationData.flags.uncacheable = true;
    auto allocation = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);
    expectBufferGmm(allocation, true);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(OsAgnosticMemoryManagerBufferGmmTests, givenGpuAddressWhenAllocatingThenAllocationHasSystemMemoryBufferGmm) {
    allocationData.gpuAddress = MemoryConstants::pageSize64k;
    auto allocation = memoryManager->allocateGraphicsMemoryWithGpuVa(allocationData);
    expectBufferGmm(allocation, true);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(OsAgnosticMemoryManagerBufferGmmTests, givenCachelineMisalignedNonSvmHostPtrWhenAllocatingThenAllocationHasUserPtrBufferGmm) {
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
    rootDeviceEnvironment.getMutableHardwareInfo()->caps.misalignedUserPtr2WayCoherent = true;
    allocationData.type = AllocationType::externalHostPtr;
    allocationData.flags.flushL3 = true;
    allocationData.hostPtr = hostMemory;
    allocationData.size = MemoryConstants::cacheLineSize + 1;
    const auto expectedUsage = CacheSettingsHelper::getGmmUsageTypeForCachelineMisalignedUserPtr(*rootDeviceEnvironment.getHardwareInfo(), rootDeviceEnvironment.getReleaseHelper());
    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);
    expectBufferGmmWithUsage(allocation, true, expectedUsage);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(OsAgnosticMemoryManagerBufferGmmTests, givenOsHandleStorageWhenCreatingAllocationThenAllocationHasSystemMemoryBufferGmm) {
    allocationData.type = AllocationType::bufferHostMemory;
    allocationData.hostPtr = hostMemory;
    OsHandleStorage handleStorage;
    auto allocation = memoryManager->createGraphicsAllocation(handleStorage, allocationData);
    expectBufferGmm(allocation, true);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(OsAgnosticMemoryManagerBufferGmmTests, givenHostPtrWhenAllocating32BitThenAllocationHasSystemMemoryBufferGmm) {
    allocationData.type = AllocationType::internalHeap;
    allocationData.hostPtr = hostMemory;
    auto allocation = memoryManager->allocate32BitGraphicsMemoryImpl(allocationData);
    expectBufferGmm(allocation, true);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(OsAgnosticMemoryManagerBufferGmmTests, givenNoHostPtrWhenAllocating32BitThenAllocationHasSystemMemoryBufferGmm) {
    allocationData.type = AllocationType::internalHeap;
    auto allocation = memoryManager->allocate32BitGraphicsMemoryImpl(allocationData);
    expectBufferGmm(allocation, true);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(OsAgnosticMemoryManagerBufferGmmTests, givenBufferWhenAllocatingInDevicePoolThenAllocationHasLocalMemoryBufferGmm) {
    allocationData.type = AllocationType::buffer;
    allocationData.flags.uncacheable = true;
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, status);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    expectBufferGmm(allocation, false);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(OsAgnosticMemoryManagerBufferGmmTests, givenSvmGpuWhenAllocatingInDevicePoolThenAllocationHasLocalMemoryBufferGmm) {
    allocationData.type = AllocationType::svmGpu;
    allocationData.hostPtr = hostMemory;
    allocationData.flags.uncacheable = true;
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, status);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    expectBufferGmm(allocation, false);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(OsAgnosticMemoryManagerBufferGmmTests, givenUncacheableSharedHandleWithoutImageInfoWhenCreatingAllocationThenAllocationHasSystemMemoryBufferGmm) {
    allocationData.flags.uncacheable = true;
    MockAllocationProperties properties(0u, false, MemoryConstants::pageSize, AllocationType::sharedBuffer);
    properties.flags.uncacheable = true;
    auto allocation = memoryManager->createGraphicsAllocationFromSharedHandle(OsHandleData{1u}, properties, false, false, false, nullptr);
    expectBufferGmm(allocation, true);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(OsAgnosticMemoryManagerBufferGmmTests, givenPhysicalHostMemoryWhenMappingToVirtualMemoryThenMappedAllocationHasSystemMemoryBufferGmm) {
    MockGraphicsAllocation physicalAllocation{hostMemory, sizeof(hostMemory)};
    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0u);
    MultiGraphicsAllocation multiGraphicsAllocation{1u};
    constexpr uint64_t gpuRange = MemoryConstants::pageSize64k;
    EXPECT_TRUE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, &physicalAllocation, gpuRange, MemoryConstants::pageSize, 0u, true));
    expectBufferGmm(multiGraphicsAllocation.getGraphicsAllocation(0u), true);
    EXPECT_TRUE(memoryManager->unMapPhysicalHostMemoryFromVirtualMemory(multiGraphicsAllocation, &physicalAllocation, gpuRange, MemoryConstants::pageSize, true));
}
