/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/test/unit_test/page_fault_manager/cpu_page_fault_manager_tests_fixture.h"
#include "shared/test/unit_test/test_macros/test_checks_shared.h"

#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"

using namespace NEO;

TEST_F(PageFaultManagerTest, givenUnifiedMemoryAllocsWhenInsertingAllocsThenAllocsAreTrackedByPageFaultManager) {
    unifiedMemoryManager = reinterpret_cast<void *>(0x1111);
    void *cmdQ = reinterpret_cast<void *>(0xFFFF);

    void *alloc1 = reinterpret_cast<void *>(0x1);
    void *alloc2 = reinterpret_cast<void *>(0x100);

    pageFaultManager->insertAllocation(alloc1, 10, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager), cmdQ, {});
    EXPECT_EQ(pageFaultManager->memoryData.size(), 1u);
    EXPECT_EQ(pageFaultManager->memoryData[alloc1].size, 10u);
    EXPECT_EQ(pageFaultManager->memoryData[alloc1].cmdQ, cmdQ);
    EXPECT_EQ(pageFaultManager->memoryData[alloc1].domain, PageFaultManager::AllocationDomain::Cpu);
    EXPECT_EQ(pageFaultManager->memoryData[alloc1].unifiedMemoryManager, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager));
    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 0);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 0);
    EXPECT_TRUE(pageFaultManager->isAubWritable);

    pageFaultManager->insertAllocation(alloc2, 20, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager), cmdQ, {});
    EXPECT_EQ(pageFaultManager->memoryData.size(), 2u);
    EXPECT_EQ(pageFaultManager->memoryData[alloc1].size, 10u);
    EXPECT_EQ(pageFaultManager->memoryData[alloc1].cmdQ, cmdQ);
    EXPECT_EQ(pageFaultManager->memoryData[alloc1].domain, PageFaultManager::AllocationDomain::Cpu);
    EXPECT_EQ(pageFaultManager->memoryData[alloc1].unifiedMemoryManager, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager));
    EXPECT_EQ(pageFaultManager->memoryData[alloc2].size, 20u);
    EXPECT_EQ(pageFaultManager->memoryData[alloc2].cmdQ, cmdQ);
    EXPECT_EQ(pageFaultManager->memoryData[alloc2].domain, PageFaultManager::AllocationDomain::Cpu);
    EXPECT_EQ(pageFaultManager->memoryData[alloc2].unifiedMemoryManager, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager));
    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 0);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 0);
    EXPECT_TRUE(pageFaultManager->isAubWritable);

    pageFaultManager->removeAllocation(alloc1);
    EXPECT_EQ(pageFaultManager->memoryData.size(), 1u);
    EXPECT_EQ(pageFaultManager->memoryData[alloc2].size, 20u);
    EXPECT_EQ(pageFaultManager->memoryData[alloc2].unifiedMemoryManager, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager));

    auto invalidAccess = [&]() { return pageFaultManager->memoryData.at(alloc1); };
    EXPECT_THROW(invalidAccess(), std::out_of_range);
}

TEST_F(PageFaultManagerTest, givenUnifiedMemoryAllocWhenRemovingProtectedAllocThenAllocIsAccessible) {
    unifiedMemoryManager = reinterpret_cast<void *>(0x1111);
    void *alloc1 = reinterpret_cast<void *>(0x1);

    pageFaultManager->insertAllocation(alloc1, 10, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager), nullptr, {});
    EXPECT_EQ(pageFaultManager->memoryData.size(), 1u);
    EXPECT_EQ(pageFaultManager->memoryData[alloc1].size, 10u);
    EXPECT_EQ(pageFaultManager->memoryData[alloc1].unifiedMemoryManager, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager));

    pageFaultManager->memoryData[alloc1].domain = PageFaultManager::AllocationDomain::Gpu;

    pageFaultManager->removeAllocation(alloc1);
    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 1);
    EXPECT_EQ(pageFaultManager->allowedMemoryAccessAddress, alloc1);
    EXPECT_EQ(pageFaultManager->accessAllowedSize, 10u);
}

TEST_F(PageFaultManagerTest, givenUnifiedMemoryAllocWhenRemovingNotTrackedAllocThenMemoryDataPointerIsNotDereferenced) {
    void *alloc1 = reinterpret_cast<void *>(0x1);

    pageFaultManager->removeAllocation(alloc1);
}

TEST_F(PageFaultManagerTest, givenUnifiedMemoryAllocsWhenMovingToGpuDomainAllocsThenAllocationsInGpuDomainAreNotMoved) {
    unifiedMemoryManager = reinterpret_cast<void *>(0x1111);
    void *unifiedMemoryManager2 = reinterpret_cast<void *>(0x2222);
    void *cmdQ = reinterpret_cast<void *>(0xFFFF);

    void *alloc1 = reinterpret_cast<void *>(0x1);
    void *alloc2 = reinterpret_cast<void *>(0x100);
    void *alloc3 = reinterpret_cast<void *>(0x10000);

    pageFaultManager->insertAllocation(alloc1, 10, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager), cmdQ, {});
    pageFaultManager->insertAllocation(alloc2, 20, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager2), cmdQ, {});
    pageFaultManager->insertAllocation(alloc3, 30, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager), cmdQ, {});
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->memoryData.size(), 3u);
    pageFaultManager->memoryData.at(alloc3).domain = PageFaultManager::AllocationDomain::Gpu;

    pageFaultManager->moveAllocationsWithinUMAllocsManagerToGpuDomain(reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager));

    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 0);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 1);

    EXPECT_EQ(pageFaultManager->protectedMemoryAccessAddress, alloc1);
    EXPECT_EQ(pageFaultManager->protectedSize, 10u);
    EXPECT_EQ(pageFaultManager->transferToGpuAddress, alloc1);
    EXPECT_FALSE(pageFaultManager->isAubWritable);
}

TEST_F(PageFaultManagerTest, givenUnifiedMemoryAllocsWhenMovingToGpuDomainAllocsThenAllocationsInNoneDomainAreNotMoved) {
    unifiedMemoryManager = reinterpret_cast<void *>(0x1111);
    void *cmdQ = reinterpret_cast<void *>(0xFFFF);

    void *alloc1 = reinterpret_cast<void *>(0x1);
    void *alloc2 = reinterpret_cast<void *>(0x100);

    pageFaultManager->insertAllocation(alloc1, 10, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager), cmdQ, {});
    pageFaultManager->insertAllocation(alloc2, 20, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager), cmdQ, {});
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->memoryData.size(), 2u);
    pageFaultManager->memoryData.at(alloc1).domain = PageFaultManager::AllocationDomain::Cpu;
    pageFaultManager->memoryData.at(alloc2).domain = PageFaultManager::AllocationDomain::None;

    pageFaultManager->moveAllocationsWithinUMAllocsManagerToGpuDomain(reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager));

    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 0);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 1);

    EXPECT_EQ(pageFaultManager->protectedMemoryAccessAddress, alloc1);
    EXPECT_EQ(pageFaultManager->protectedSize, 10u);
    EXPECT_EQ(pageFaultManager->transferToGpuAddress, alloc1);
    EXPECT_FALSE(pageFaultManager->isAubWritable);
}

TEST_F(PageFaultManagerTest, givenUnifiedMemoryAllocWhenMoveToGpuDomainThenTransferToGpuIsCalled) {
    void *cmdQ = reinterpret_cast<void *>(0xFFFF);

    void *alloc = reinterpret_cast<void *>(0x1);

    pageFaultManager->insertAllocation(alloc, 10, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager), cmdQ, {});
    EXPECT_EQ(pageFaultManager->memoryData.size(), 1u);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);

    pageFaultManager->moveAllocationToGpuDomain(alloc);

    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 0);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 1);

    EXPECT_EQ(pageFaultManager->protectedMemoryAccessAddress, alloc);
    EXPECT_EQ(pageFaultManager->protectedSize, 10u);
    EXPECT_EQ(pageFaultManager->transferToGpuAddress, alloc);
    EXPECT_FALSE(pageFaultManager->isAubWritable);
}

TEST_F(PageFaultManagerTest, givenUnifiedMemoryAllocInGpuDomainWhenMovingToGpuDomainThenNothingIsCalled) {
    void *cmdQ = reinterpret_cast<void *>(0xFFFF);

    void *alloc = reinterpret_cast<void *>(0x1);

    pageFaultManager->insertAllocation(alloc, 10, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager), cmdQ, {});
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->memoryData.size(), 1u);
    pageFaultManager->memoryData.at(alloc).domain = PageFaultManager::AllocationDomain::Gpu;

    pageFaultManager->moveAllocationToGpuDomain(alloc);

    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 0);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 0);
    EXPECT_TRUE(pageFaultManager->isAubWritable);
}

TEST_F(PageFaultManagerTest, whenMovingToGpuDomainUntrackedAllocThenNothingIsCalled) {
    void *alloc = reinterpret_cast<void *>(0x1);

    EXPECT_EQ(pageFaultManager->memoryData.size(), 0u);

    pageFaultManager->moveAllocationToGpuDomain(alloc);

    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 0);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 0);
    EXPECT_TRUE(pageFaultManager->isAubWritable);
}

TEST_F(PageFaultManagerTest, givenHandlerRegisteredAndUntrackedPageFaultAddressWhenVerifyingThenFalseIsReturned) {
    void *alloc1 = reinterpret_cast<void *>(0x1);
    void *alloc2 = reinterpret_cast<void *>(0x100);
    void *alloc3 = reinterpret_cast<void *>(0x10000);

    pageFaultManager->insertAllocation(alloc1, 10, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager), nullptr, {});
    pageFaultManager->insertAllocation(alloc2, 20, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager), nullptr, {});
    EXPECT_EQ(pageFaultManager->memoryData.size(), 2u);

    auto retVal = pageFaultManager->verifyPageFault(alloc3);
    EXPECT_FALSE(retVal);
}

TEST_F(PageFaultManagerTest, givenTrackedPageFaultAddressWhenVerifyingThenProperAllocIsTransferredToCpuDomain) {
    void *alloc1 = reinterpret_cast<void *>(0x1);
    void *alloc2 = reinterpret_cast<void *>(0x100);

    pageFaultManager->insertAllocation(alloc1, 10, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager), nullptr, {});
    pageFaultManager->insertAllocation(alloc2, 20, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager), nullptr, {});
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->memoryData.size(), 2u);

    pageFaultManager->verifyPageFault(alloc1);

    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 1);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 0);

    EXPECT_EQ(pageFaultManager->allowedMemoryAccessAddress, alloc1);
    EXPECT_EQ(pageFaultManager->accessAllowedSize, 10u);
    EXPECT_TRUE(pageFaultManager->isAubWritable);
}

TEST_F(PageFaultManagerTest, givenInitialPlacementCpuWhenVerifyingPagefaultThenFirstAccessDoesNotInvokeTransfer) {
    void *alloc = reinterpret_cast<void *>(0x1);

    MemoryProperties memoryProperties{};
    memoryProperties.flags.usmInitialPlacementCpu = 1;
    pageFaultManager->insertAllocation(alloc, 10, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager), nullptr, memoryProperties);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->memoryData.size(), 1u);
    pageFaultManager->verifyPageFault(alloc);

    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 1);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 0);

    EXPECT_EQ(pageFaultManager->allowedMemoryAccessAddress, alloc);
    EXPECT_EQ(pageFaultManager->accessAllowedSize, 10u);
    EXPECT_TRUE(pageFaultManager->isAubWritable);
}

TEST_F(PageFaultManagerTest, givenInitialPlacementGpuWhenVerifyingPagefaultThenFirstAccessDoesNotInvokeTransfer) {
    void *alloc = reinterpret_cast<void *>(0x1);

    MemoryProperties memoryProperties{};
    memoryProperties.flags.usmInitialPlacementGpu = 1;
    pageFaultManager->insertAllocation(alloc, 10, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager), nullptr, memoryProperties);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->memoryData.size(), 1u);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(pageFaultManager->protectedMemoryAccessAddress, alloc);
    EXPECT_EQ(pageFaultManager->protectedSize, 10u);
    pageFaultManager->verifyPageFault(alloc);

    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 1);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 0);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 1);

    EXPECT_EQ(pageFaultManager->allowedMemoryAccessAddress, alloc);
    EXPECT_EQ(pageFaultManager->accessAllowedSize, 10u);
    EXPECT_TRUE(pageFaultManager->isAubWritable);
}

TEST_F(PageFaultManagerTest, givenInitialPlacementCpuWhenMovingToGpuDomainThenFirstAccessInvokesTransfer) {
    void *cmdQ = reinterpret_cast<void *>(0xFFFF);

    void *alloc = reinterpret_cast<void *>(0x1);

    MemoryProperties memoryProperties{};
    memoryProperties.flags.usmInitialPlacementCpu = 1;
    pageFaultManager->insertAllocation(alloc, 10, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager), cmdQ, memoryProperties);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->memoryData.size(), 1u);

    pageFaultManager->moveAllocationToGpuDomain(alloc);

    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 0);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 1);
    EXPECT_FALSE(pageFaultManager->isAubWritable);

    EXPECT_EQ(pageFaultManager->protectedMemoryAccessAddress, alloc);
    EXPECT_EQ(pageFaultManager->protectedSize, 10u);
    EXPECT_EQ(pageFaultManager->transferToGpuAddress, alloc);
}

TEST_F(PageFaultManagerTest, givenInitialPlacementGpuWhenMovingToGpuDomainThenFirstAccessDoesNotInvokeTransfer) {
    void *cmdQ = reinterpret_cast<void *>(0xFFFF);

    void *alloc = reinterpret_cast<void *>(0x1);

    MemoryProperties memoryProperties{};
    memoryProperties.flags.usmInitialPlacementGpu = 1;
    pageFaultManager->insertAllocation(alloc, 10, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager), cmdQ, memoryProperties);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->memoryData.size(), 1u);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(pageFaultManager->protectedMemoryAccessAddress, alloc);
    EXPECT_EQ(pageFaultManager->protectedSize, 10u);

    pageFaultManager->moveAllocationToGpuDomain(alloc);

    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 0);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 0);
    EXPECT_FALSE(pageFaultManager->isAubWritable);
}

TEST_F(PageFaultManagerTest, givenAllocationMovedToGpuDomainWhenVerifyingPagefaultThenAllocationIsMovedToCpuDomain) {
    void *cmdQ = reinterpret_cast<void *>(0xFFFF);

    void *alloc = reinterpret_cast<void *>(0x1);

    MemoryProperties memoryProperties{};
    memoryProperties.flags.usmInitialPlacementGpu = 1;
    pageFaultManager->insertAllocation(alloc, 10, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager), cmdQ, memoryProperties);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->memoryData.size(), 1u);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(pageFaultManager->protectedMemoryAccessAddress, alloc);
    EXPECT_EQ(pageFaultManager->protectedSize, 10u);

    pageFaultManager->moveAllocationToGpuDomain(alloc);
    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 0);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 0);
    EXPECT_FALSE(pageFaultManager->isAubWritable);

    pageFaultManager->verifyPageFault(alloc);
    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 1);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 1);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 0);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(pageFaultManager->allowedMemoryAccessAddress, alloc);
    EXPECT_EQ(pageFaultManager->accessAllowedSize, 10u);
    EXPECT_TRUE(pageFaultManager->isAubWritable);
}

TEST_F(PageFaultManagerTest, givenUnifiedMemoryAllocWhenSetAubWritableIsCalledThenAllocIsAubWritable) {
    MockExecutionEnvironment executionEnvironment;
    REQUIRE_SVM_OR_SKIP(executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo());

    void *cmdQ = reinterpret_cast<void *>(0xFFFF);
    auto memoryManager = std::make_unique<MockMemoryManager>(executionEnvironment);
    auto unifiedMemoryManager = std::make_unique<SVMAllocsManager>(memoryManager.get());
    auto properties = SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::SHARED_UNIFIED_MEMORY);
    properties.subdeviceBitfield = mockDeviceBitfield;
    void *alloc1 = unifiedMemoryManager->createSharedUnifiedMemoryAllocation(mockRootDeviceIndex, 10, properties, cmdQ);

    pageFaultManager->baseAubWritable(false, alloc1, unifiedMemoryManager.get());

    auto gpuAlloc = unifiedMemoryManager->getSVMAlloc(alloc1)->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex);
    EXPECT_FALSE(gpuAlloc->isAubWritable(GraphicsAllocation::allBanks));

    unifiedMemoryManager->freeSVMAlloc(alloc1);
}
