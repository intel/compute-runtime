/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test_checks_shared.h"
#include "shared/test/unit_test/page_fault_manager/cpu_page_fault_manager_tests_fixture.h"

using namespace NEO;

TEST_F(PageFaultManagerTest, givenUnifiedMemoryAllocsWhenInsertingAllocsThenAllocsAreTrackedByPageFaultManager) {
    void *cmdQ = reinterpret_cast<void *>(0xFFFF);
    void *alloc1 = reinterpret_cast<void *>(0x1);
    void *alloc2 = reinterpret_cast<void *>(0x100);

    pageFaultManager->insertAllocation(alloc1, 10, unifiedMemoryManager.get(), cmdQ, {});
    EXPECT_EQ(pageFaultManager->memoryData.size(), 1u);
    EXPECT_EQ(pageFaultManager->memoryData[alloc1].size, 10u);
    EXPECT_EQ(pageFaultManager->memoryData[alloc1].cmdQ, cmdQ);
    EXPECT_EQ(pageFaultManager->memoryData[alloc1].domain, PageFaultManager::AllocationDomain::Cpu);
    EXPECT_EQ(pageFaultManager->memoryData[alloc1].unifiedMemoryManager, unifiedMemoryManager.get());
    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 0);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 0);
    EXPECT_TRUE(pageFaultManager->isAubWritable);

    pageFaultManager->insertAllocation(alloc2, 20, unifiedMemoryManager.get(), cmdQ, {});
    EXPECT_EQ(pageFaultManager->memoryData.size(), 2u);
    EXPECT_EQ(pageFaultManager->memoryData[alloc1].size, 10u);
    EXPECT_EQ(pageFaultManager->memoryData[alloc1].cmdQ, cmdQ);
    EXPECT_EQ(pageFaultManager->memoryData[alloc1].domain, PageFaultManager::AllocationDomain::Cpu);
    EXPECT_EQ(pageFaultManager->memoryData[alloc1].unifiedMemoryManager, unifiedMemoryManager.get());
    EXPECT_EQ(pageFaultManager->memoryData[alloc2].size, 20u);
    EXPECT_EQ(pageFaultManager->memoryData[alloc2].cmdQ, cmdQ);
    EXPECT_EQ(pageFaultManager->memoryData[alloc2].domain, PageFaultManager::AllocationDomain::Cpu);
    EXPECT_EQ(pageFaultManager->memoryData[alloc2].unifiedMemoryManager, unifiedMemoryManager.get());
    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 0);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 0);
    EXPECT_TRUE(pageFaultManager->isAubWritable);

    pageFaultManager->removeAllocation(alloc1);
    EXPECT_EQ(pageFaultManager->memoryData.size(), 1u);
    EXPECT_EQ(pageFaultManager->memoryData[alloc2].size, 20u);
    EXPECT_EQ(pageFaultManager->memoryData[alloc2].unifiedMemoryManager, unifiedMemoryManager.get());

    auto invalidAccess = [&]() { return pageFaultManager->memoryData.at(alloc1); };
    EXPECT_THROW(invalidAccess(), std::out_of_range);
}

TEST_F(PageFaultManagerTest, givenUnifiedMemoryAllocWhenRemovingProtectedAllocThenAllocIsAccessible) {
    void *alloc1 = reinterpret_cast<void *>(0x1);

    pageFaultManager->insertAllocation(alloc1, 10, unifiedMemoryManager.get(), nullptr, {});
    EXPECT_EQ(pageFaultManager->memoryData.size(), 1u);
    EXPECT_EQ(pageFaultManager->memoryData[alloc1].size, 10u);
    EXPECT_EQ(pageFaultManager->memoryData[alloc1].unifiedMemoryManager, unifiedMemoryManager.get());

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

TEST_F(PageFaultManagerTest, givenUnifiedMemoryAllocWhenRemovingAllocsThenNonGpuAllocsContainerUpdated) {
    void *alloc1 = reinterpret_cast<void *>(0x1);
    void *alloc2 = reinterpret_cast<void *>(0x2);

    pageFaultManager->insertAllocation(alloc1, 10u, unifiedMemoryManager.get(), nullptr, {});
    pageFaultManager->insertAllocation(alloc2, 20u, unifiedMemoryManager.get(), nullptr, {});
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 2u);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs[0], alloc1);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs[1], alloc2);

    pageFaultManager->removeAllocation(alloc1);
    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 0);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 1u);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs[0], alloc2);

    pageFaultManager->memoryData.at(alloc2).domain = PageFaultManager::AllocationDomain::Gpu;
    pageFaultManager->removeAllocation(alloc2);
    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 1);
    EXPECT_EQ(pageFaultManager->allowedMemoryAccessAddress, alloc2);
    EXPECT_EQ(pageFaultManager->accessAllowedSize, 20u);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 1u);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs[0], alloc2);
}

TEST_F(PageFaultManagerTest, givenUnifiedMemoryAllocWhenAllocNotPresentInNonGpuDomainContainerThenDoNotCallErase) {
    void *alloc1 = reinterpret_cast<void *>(0x1);

    pageFaultManager->insertAllocation(alloc1, 10u, unifiedMemoryManager.get(), nullptr, {});
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 1u);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs[0], alloc1);

    pageFaultManager->moveAllocationToGpuDomain(alloc1);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 0u);
    pageFaultManager->memoryData.at(alloc1).domain = PageFaultManager::AllocationDomain::Cpu;
    pageFaultManager->removeAllocation(alloc1);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 0u);
}

TEST_F(PageFaultManagerTest, givenNonGpuAllocsContainerWhenMovingToGpuDomainMultipleTimesThenDoNotEraseUnexistingElement) {
    void *alloc1 = reinterpret_cast<void *>(0x1);
    void *alloc2 = reinterpret_cast<void *>(0x2);

    pageFaultManager->insertAllocation(alloc1, 10u, unifiedMemoryManager.get(), nullptr, {});
    pageFaultManager->insertAllocation(alloc2, 20u, unifiedMemoryManager.get(), nullptr, {});
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 2u);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs[0], alloc1);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs[1], alloc2);

    pageFaultManager->moveAllocationToGpuDomain(alloc1);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 1u);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs[0], alloc2);
    pageFaultManager->memoryData.at(alloc1).domain = PageFaultManager::AllocationDomain::None;
    pageFaultManager->moveAllocationToGpuDomain(alloc1);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 1u);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs[0], alloc2);
}

TEST_F(PageFaultManagerTest, givenUnifiedMemoryAllocsWhenMovingToGpuDomainAllocsThenAllocationsInGpuDomainAreNotMoved) {
    auto unifiedMemoryManager2 = std::make_unique<SVMAllocsManager>(memoryManager.get(), false);
    void *cmdQ = reinterpret_cast<void *>(0xFFFF);

    void *alloc1 = reinterpret_cast<void *>(0x1);
    void *alloc2 = reinterpret_cast<void *>(0x100);
    void *alloc3 = reinterpret_cast<void *>(0x10000);

    pageFaultManager->insertAllocation(alloc1, 10, unifiedMemoryManager.get(), cmdQ, {});
    pageFaultManager->insertAllocation(alloc2, 20, unifiedMemoryManager2.get(), cmdQ, {});
    pageFaultManager->insertAllocation(alloc3, 30, unifiedMemoryManager.get(), cmdQ, {});
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->memoryData.size(), 3u);
    pageFaultManager->memoryData.at(alloc3).domain = PageFaultManager::AllocationDomain::Gpu;

    pageFaultManager->moveAllocationsWithinUMAllocsManagerToGpuDomain(unifiedMemoryManager.get());

    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 0);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 1);
    EXPECT_EQ(pageFaultManager->protectedMemoryAccessAddress, alloc1);
    EXPECT_EQ(pageFaultManager->protectedSize, 10u);
    EXPECT_EQ(pageFaultManager->transferToGpuAddress, alloc1);
}

TEST_F(PageFaultManagerTest, givenUnifiedMemoryAllocWhenMovingAllocsOfGivenUMManagerThenNonGpuAllocsContainerCleared) {
    void *alloc1 = reinterpret_cast<void *>(0x1);
    void *alloc2 = reinterpret_cast<void *>(0x1);

    pageFaultManager->insertAllocation(alloc1, 10u, unifiedMemoryManager.get(), nullptr, {});
    pageFaultManager->insertAllocation(alloc2, 20u, unifiedMemoryManager.get(), nullptr, {});

    pageFaultManager->memoryData.at(alloc2).domain = PageFaultManager::AllocationDomain::Gpu;
    pageFaultManager->moveAllocationsWithinUMAllocsManagerToGpuDomain(unifiedMemoryManager.get());
    EXPECT_EQ(pageFaultManager->memoryData.at(alloc1).domain, PageFaultManager::AllocationDomain::Gpu);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 0u);
}

TEST_F(PageFaultManagerTest, givenUnifiedMemoryAllocsWhenMovingToGpuDomainWithPrintUsmSharedMigrationDebugKeyThenMessageIsPrinted) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.PrintUmdSharedMigration.set(1);

    auto unifiedMemoryManager2 = std::make_unique<SVMAllocsManager>(memoryManager.get(), false);
    void *cmdQ = reinterpret_cast<void *>(0xFFFF);
    void *alloc1 = reinterpret_cast<void *>(0x1);
    void *alloc2 = reinterpret_cast<void *>(0x100);
    void *alloc3 = reinterpret_cast<void *>(0x10000);

    pageFaultManager->insertAllocation(alloc1, 10, unifiedMemoryManager.get(), cmdQ, {});
    pageFaultManager->insertAllocation(alloc2, 20, unifiedMemoryManager2.get(), cmdQ, {});
    pageFaultManager->insertAllocation(alloc3, 30, unifiedMemoryManager.get(), cmdQ, {});
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->memoryData.size(), 3u);
    pageFaultManager->memoryData.at(alloc3).domain = PageFaultManager::AllocationDomain::Gpu;

    testing::internal::CaptureStdout(); // start capturing

    pageFaultManager->moveAllocationsWithinUMAllocsManagerToGpuDomain(unifiedMemoryManager.get());

    std::string output = testing::internal::GetCapturedStdout(); // stop capturing

    std::string expectedString = "UMD transferring shared allocation";
    uint32_t occurrences = 0u;
    uint32_t expectedOccurrences = 1u;
    size_t idx = output.find(expectedString);
    while (idx != std::string::npos) {
        occurrences++;
        idx = output.find(expectedString, idx + 1);
    }
    EXPECT_EQ(expectedOccurrences, occurrences);
}

TEST_F(PageFaultManagerTest, givenUnifiedMemoryAllocsWhenMovingToGpuDomainAllocsThenAllocationsInNoneDomainAreNotMoved) {
    void *cmdQ = reinterpret_cast<void *>(0xFFFF);

    void *alloc1 = reinterpret_cast<void *>(0x1);
    void *alloc2 = reinterpret_cast<void *>(0x100);

    pageFaultManager->insertAllocation(alloc1, 10, unifiedMemoryManager.get(), cmdQ, {});
    pageFaultManager->insertAllocation(alloc2, 20, unifiedMemoryManager.get(), cmdQ, {});
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->memoryData.size(), 2u);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 2u);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs[0], alloc1);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs[1], alloc2);
    pageFaultManager->memoryData.at(alloc1).domain = PageFaultManager::AllocationDomain::Cpu;
    pageFaultManager->memoryData.at(alloc2).domain = PageFaultManager::AllocationDomain::None;

    pageFaultManager->moveAllocationsWithinUMAllocsManagerToGpuDomain(unifiedMemoryManager.get());

    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 0);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 1);
    EXPECT_EQ(pageFaultManager->protectedMemoryAccessAddress, alloc1);
    EXPECT_EQ(pageFaultManager->protectedSize, 10u);
    EXPECT_EQ(pageFaultManager->transferToGpuAddress, alloc1);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 0u);
}

TEST_F(PageFaultManagerTest, givenUnifiedMemoryAllocWhenMoveToGpuDomainThenTransferToGpuIsCalled) {
    void *cmdQ = reinterpret_cast<void *>(0xFFFF);
    void *alloc = reinterpret_cast<void *>(0x1);

    pageFaultManager->insertAllocation(alloc, 10, unifiedMemoryManager.get(), cmdQ, {});
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
}

TEST_F(PageFaultManagerTest, givenUnifiedMemoryAllocWhenMovingToGpuDomainThenUpdateNonGpuAllocsContainer) {
    void *alloc1 = reinterpret_cast<void *>(0x1);
    void *alloc2 = reinterpret_cast<void *>(0x2);

    pageFaultManager->insertAllocation(alloc1, 10u, unifiedMemoryManager.get(), nullptr, {});
    pageFaultManager->insertAllocation(alloc2, 20u, unifiedMemoryManager.get(), nullptr, {});
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 2u);

    pageFaultManager->moveAllocationToGpuDomain(alloc1);
    EXPECT_EQ(pageFaultManager->memoryData.at(alloc1).domain, PageFaultManager::AllocationDomain::Gpu);
    const auto &allocsVec = unifiedMemoryManager->nonGpuDomainAllocs;
    EXPECT_EQ(std::find(allocsVec.cbegin(), allocsVec.cend(), alloc1), allocsVec.cend());
    EXPECT_EQ(allocsVec.size(), 1u);
    EXPECT_EQ(allocsVec[0], alloc2);

    pageFaultManager->memoryData.at(alloc2).domain = PageFaultManager::AllocationDomain::Gpu;
    pageFaultManager->moveAllocationToGpuDomain(alloc2);
    EXPECT_NE(std::find(allocsVec.cbegin(), allocsVec.cend(), alloc2), allocsVec.cend());
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 1u);
}

TEST_F(PageFaultManagerTest, givenUnifiedMemoryAllocWhenMoveToGpuDomainWithPrintUsmSharedMigrationDebugKeyThenMessageIsPrinted) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.PrintUmdSharedMigration.set(1);

    void *cmdQ = reinterpret_cast<void *>(0xFFFF);
    void *alloc = reinterpret_cast<void *>(0x1);

    pageFaultManager->insertAllocation(alloc, 10, unifiedMemoryManager.get(), cmdQ, {});
    EXPECT_EQ(pageFaultManager->memoryData.size(), 1u);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);

    testing::internal::CaptureStdout(); // start capturing

    pageFaultManager->moveAllocationToGpuDomain(alloc);

    std::string output = testing::internal::GetCapturedStdout(); // stop capturing

    std::string expectedString = "UMD transferring shared allocation";
    uint32_t occurrences = 0u;
    uint32_t expectedOccurrences = 1u;
    size_t idx = output.find(expectedString);
    while (idx != std::string::npos) {
        occurrences++;
        idx = output.find(expectedString, idx + 1);
    }
    EXPECT_EQ(expectedOccurrences, occurrences);
}

TEST_F(PageFaultManagerTest, givenUnifiedMemoryAllocInGpuDomainWhenMovingToGpuDomainThenNothingIsCalled) {
    void *cmdQ = reinterpret_cast<void *>(0xFFFF);
    void *alloc = reinterpret_cast<void *>(0x1);

    pageFaultManager->insertAllocation(alloc, 10, unifiedMemoryManager.get(), cmdQ, {});
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

    pageFaultManager->insertAllocation(alloc1, 10, unifiedMemoryManager.get(), nullptr, {});
    pageFaultManager->insertAllocation(alloc2, 20, unifiedMemoryManager.get(), nullptr, {});
    EXPECT_EQ(pageFaultManager->memoryData.size(), 2u);

    auto retVal = pageFaultManager->verifyPageFault(alloc3);
    EXPECT_FALSE(retVal);
}

TEST_F(PageFaultManagerTest, givenTrackedPageFaultAddressWhenVerifyingThenProperAllocIsTransferredToCpuDomain) {
    void *alloc1 = reinterpret_cast<void *>(0x1);
    void *alloc2 = reinterpret_cast<void *>(0x100);

    pageFaultManager->insertAllocation(alloc1, 10, unifiedMemoryManager.get(), nullptr, {});
    pageFaultManager->insertAllocation(alloc2, 20, unifiedMemoryManager.get(), nullptr, {});
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

    memoryProperties.allocFlags.usmInitialPlacementCpu = 1;
    pageFaultManager->insertAllocation(alloc, 10, unifiedMemoryManager.get(), nullptr, memoryProperties);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->memoryData.size(), 1u);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 1u);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs[0], alloc);

    pageFaultManager->verifyPageFault(alloc);
    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 1);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 0);
    EXPECT_EQ(pageFaultManager->allowedMemoryAccessAddress, alloc);
    EXPECT_EQ(pageFaultManager->accessAllowedSize, 10u);
    EXPECT_TRUE(pageFaultManager->isAubWritable);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 1u);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs[0], alloc);
}

TEST_F(PageFaultManagerTest, givenAllocsMovedToGpuDomainWhenVerifyingPageFaultThenTransferTheAllocsBackToCpuDomain) {
    void *alloc1 = reinterpret_cast<void *>(0x1);
    void *alloc2 = reinterpret_cast<void *>(0x100);

    pageFaultManager->insertAllocation(alloc1, 10, unifiedMemoryManager.get(), nullptr, {});
    pageFaultManager->insertAllocation(alloc2, 20, unifiedMemoryManager.get(), nullptr, {});
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 2u);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs[0], alloc1);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs[1], alloc2);

    pageFaultManager->moveAllocationsWithinUMAllocsManagerToGpuDomain(unifiedMemoryManager.get());
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 0u);

    pageFaultManager->verifyPageFault(alloc2);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 1u);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs[0], alloc2);

    pageFaultManager->gpuDomainHandler = &MockPageFaultManager::handleGpuDomainTransferForAubAndTbx;

    pageFaultManager->verifyPageFault(alloc1);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 2u);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs[1], alloc1);
}

TEST_F(PageFaultManagerTest, givenAllocsFromCpuDomainWhenVerifyingPageFaultThenDoNotAppendAllocsToNonGpuContainer) {
    void *alloc1 = reinterpret_cast<void *>(0x1);
    void *alloc2 = reinterpret_cast<void *>(0x100);

    pageFaultManager->insertAllocation(alloc1, 10, unifiedMemoryManager.get(), nullptr, {});
    pageFaultManager->insertAllocation(alloc2, 20, unifiedMemoryManager.get(), nullptr, {});
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 2u);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs[0], alloc1);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs[1], alloc2);

    pageFaultManager->moveAllocationsWithinUMAllocsManagerToGpuDomain(unifiedMemoryManager.get());
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 0u);

    pageFaultManager->memoryData.at(alloc2).domain = PageFaultManager::AllocationDomain::None;
    pageFaultManager->verifyPageFault(alloc2);
    EXPECT_EQ(pageFaultManager->memoryData.at(alloc2).domain, PageFaultManager::AllocationDomain::Cpu);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 0u);

    pageFaultManager->gpuDomainHandler = &MockPageFaultManager::handleGpuDomainTransferForAubAndTbx;
    pageFaultManager->memoryData.at(alloc1).domain = PageFaultManager::AllocationDomain::None;
    pageFaultManager->verifyPageFault(alloc1);
    EXPECT_EQ(pageFaultManager->memoryData.at(alloc1).domain, PageFaultManager::AllocationDomain::Cpu);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 0u);
}

TEST_F(PageFaultManagerTest, givenTbxWhenVerifyingPagefaultThenVerifyPagefaultUnprotectsAndTransfersMemory) {
    void *alloc = reinterpret_cast<void *>(0x1);

    pageFaultManager->gpuDomainHandler = &MockPageFaultManager::handleGpuDomainTransferForAubAndTbx;
    memoryProperties.allocFlags.usmInitialPlacementGpu = 1;
    pageFaultManager->insertAllocation(alloc, 10, unifiedMemoryManager.get(), nullptr, memoryProperties);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(pageFaultManager->protectedMemoryAccessAddress, alloc);
    EXPECT_EQ(pageFaultManager->protectedSize, 10u);

    pageFaultManager->moveAllocationToGpuDomain(alloc);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 0);

    pageFaultManager->verifyPageFault(alloc);
    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 1);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 1);
    EXPECT_EQ(pageFaultManager->allowedMemoryAccessAddress, alloc);
    EXPECT_EQ(pageFaultManager->accessAllowedSize, 10u);
    EXPECT_TRUE(pageFaultManager->isAubWritable);
}

TEST_F(PageFaultManagerTest, whenVerifyingPagefaultWithPrintUsmSharedMigrationDebugKeyThenMessageIsPrinted) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.PrintUmdSharedMigration.set(1);

    void *alloc = reinterpret_cast<void *>(0x1);

    pageFaultManager->gpuDomainHandler = &MockPageFaultManager::handleGpuDomainTransferForHw;
    memoryProperties.allocFlags.usmInitialPlacementGpu = 1;
    pageFaultManager->insertAllocation(alloc, 10, unifiedMemoryManager.get(), nullptr, memoryProperties);

    pageFaultManager->moveAllocationToGpuDomain(alloc);

    testing::internal::CaptureStdout(); // start capturing

    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 0);
    EXPECT_EQ(pageFaultManager->protectedMemoryAccessAddress, alloc);
    EXPECT_EQ(pageFaultManager->protectedSize, 10u);

    pageFaultManager->verifyPageFault(alloc);

    std::string output = testing::internal::GetCapturedStdout(); // stop capturing

    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 1);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 1);
    EXPECT_EQ(pageFaultManager->allowedMemoryAccessAddress, alloc);
    EXPECT_EQ(pageFaultManager->accessAllowedSize, 10u);
    EXPECT_TRUE(pageFaultManager->isAubWritable);

    std::string expectedString = "UMD transferring shared allocation";
    uint32_t occurrences = 0u;
    uint32_t expectedOccurrences = 1u;
    size_t idx = output.find(expectedString);
    while (idx != std::string::npos) {
        occurrences++;
        idx = output.find(expectedString, idx + 1);
    }
    EXPECT_EQ(expectedOccurrences, occurrences);
}

TEST_F(PageFaultManagerTest, givenTbxWhenVerifyingPagefaultWithPrintUsmSharedMigrationDebugKeyThenMessageIsPrinted) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.PrintUmdSharedMigration.set(1);

    void *alloc = reinterpret_cast<void *>(0x1);

    pageFaultManager->gpuDomainHandler = &MockPageFaultManager::handleGpuDomainTransferForAubAndTbx;
    memoryProperties.allocFlags.usmInitialPlacementGpu = 1;
    pageFaultManager->insertAllocation(alloc, 10, unifiedMemoryManager.get(), nullptr, memoryProperties);
    pageFaultManager->moveAllocationToGpuDomain(alloc);

    testing::internal::CaptureStdout(); // start capturing

    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 0);
    EXPECT_EQ(pageFaultManager->protectedMemoryAccessAddress, alloc);
    EXPECT_EQ(pageFaultManager->protectedSize, 10u);

    pageFaultManager->verifyPageFault(alloc);

    std::string output = testing::internal::GetCapturedStdout(); // stop capturing

    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 1);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 1);
    EXPECT_EQ(pageFaultManager->allowedMemoryAccessAddress, alloc);
    EXPECT_EQ(pageFaultManager->accessAllowedSize, 10u);
    EXPECT_TRUE(pageFaultManager->isAubWritable);

    std::string expectedString = "UMD transferring shared allocation";
    uint32_t occurrences = 0u;
    uint32_t expectedOccurrences = 1u;
    size_t idx = output.find(expectedString);
    while (idx != std::string::npos) {
        occurrences++;
        idx = output.find(expectedString, idx + 1);
    }
    EXPECT_EQ(expectedOccurrences, occurrences);
}

TEST_F(PageFaultManagerTest, givenTbxAndInitialPlacementGpuWhenVerifyingPagefaultThenMemoryIsUnprotectedOnly) {
    void *alloc = reinterpret_cast<void *>(0x1);

    pageFaultManager->gpuDomainHandler = &MockPageFaultManager::handleGpuDomainTransferForAubAndTbx;
    memoryProperties.allocFlags.usmInitialPlacementGpu = 1;
    pageFaultManager->insertAllocation(alloc, 10, unifiedMemoryManager.get(), nullptr, memoryProperties);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->protectedMemoryAccessAddress, alloc);
    EXPECT_EQ(pageFaultManager->protectedSize, 10u);
    EXPECT_EQ(pageFaultManager->memoryData.size(), 1u);

    pageFaultManager->verifyPageFault(alloc);

    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 1);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->allowedMemoryAccessAddress, alloc);
    EXPECT_EQ(pageFaultManager->accessAllowedSize, 10u);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 1);
    EXPECT_TRUE(pageFaultManager->isAubWritable);
}

TEST_F(PageFaultManagerTest, givenInitialPlacementCpuWhenMovingToGpuDomainThenFirstAccessInvokesTransfer) {
    void *cmdQ = reinterpret_cast<void *>(0xFFFF);
    void *alloc = reinterpret_cast<void *>(0x1);

    memoryProperties.allocFlags.usmInitialPlacementCpu = 1;
    pageFaultManager->insertAllocation(alloc, 10, unifiedMemoryManager.get(), cmdQ, memoryProperties);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 0);
    EXPECT_EQ(pageFaultManager->memoryData.size(), 1u);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 1u);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs[0], alloc);

    pageFaultManager->moveAllocationToGpuDomain(alloc);
    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 0);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 1);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 0u);

    EXPECT_EQ(pageFaultManager->protectedMemoryAccessAddress, alloc);
    EXPECT_EQ(pageFaultManager->protectedSize, 10u);
    EXPECT_EQ(pageFaultManager->transferToGpuAddress, alloc);
}

TEST_F(PageFaultManagerTest, givenAllocationMovedToGpuDomainWhenVerifyingPagefaultThenAllocationIsMovedToCpuDomain) {
    void *cmdQ = reinterpret_cast<void *>(0xFFFF);
    void *alloc = reinterpret_cast<void *>(0x1);

    memoryProperties.allocFlags.usmInitialPlacementGpu = 1;
    pageFaultManager->insertAllocation(alloc, 10, unifiedMemoryManager.get(), cmdQ, memoryProperties);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->memoryData.size(), 1u);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(pageFaultManager->protectedMemoryAccessAddress, alloc);
    EXPECT_EQ(pageFaultManager->protectedSize, 10u);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 1u);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs[0], alloc);

    pageFaultManager->moveAllocationToGpuDomain(alloc);
    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 0);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 0);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 0u);

    pageFaultManager->verifyPageFault(alloc);
    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 1);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 1);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 0);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(pageFaultManager->allowedMemoryAccessAddress, alloc);
    EXPECT_EQ(pageFaultManager->accessAllowedSize, 10u);
    EXPECT_TRUE(pageFaultManager->isAubWritable);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs.size(), 1u);
    EXPECT_EQ(unifiedMemoryManager->nonGpuDomainAllocs[0], alloc);
}

TEST_F(PageFaultManagerTest, givenUnifiedMemoryAllocWhenSetAubWritableIsCalledThenAllocIsAubWritable) {
    REQUIRE_SVM_OR_SKIP(executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo());

    std::set<uint32_t> rootDeviceIndices{mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    auto properties = SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::SHARED_UNIFIED_MEMORY, rootDeviceIndices, deviceBitfields);
    void *cmdQ = reinterpret_cast<void *>(0xFFFF);
    void *alloc1 = unifiedMemoryManager->createSharedUnifiedMemoryAllocation(10, properties, cmdQ);

    pageFaultManager->baseAubWritable(false, alloc1, unifiedMemoryManager.get());

    auto gpuAlloc = unifiedMemoryManager->getSVMAlloc(alloc1)->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex);
    EXPECT_FALSE(gpuAlloc->isAubWritable(GraphicsAllocation::allBanks));

    unifiedMemoryManager->freeSVMAlloc(alloc1);
}

TEST_F(PageFaultManagerTest, givenAubOrTbxCsrWhenSelectingHandlerThenAubAndTbxGpuDomainHandlerIsSet) {
    DebugManagerStateRestore restorer;

    for (auto csrType : {CommandStreamReceiverType::CSR_AUB, CommandStreamReceiverType::CSR_TBX,
                         CommandStreamReceiverType::CSR_HW_WITH_AUB, CommandStreamReceiverType::CSR_TBX_WITH_AUB}) {
        DebugManager.flags.SetCommandStreamReceiver.set(csrType);

        auto pageFaultManager2 = std::make_unique<MockPageFaultManager>();
        pageFaultManager2->selectGpuDomainHandler();

        EXPECT_EQ(pageFaultManager->getAubAndTbxHandlerAddress(), reinterpret_cast<void *>(pageFaultManager2->gpuDomainHandler));
    }
}

TEST_F(PageFaultManagerTest, givenHwCsrWhenSelectingHandlerThenHwGpuDomainHandlerIsSet) {
    DebugManagerStateRestore restorer;

    EXPECT_EQ(pageFaultManager->getHwHandlerAddress(), reinterpret_cast<void *>(pageFaultManager->gpuDomainHandler));

    DebugManager.flags.SetCommandStreamReceiver.set(CommandStreamReceiverType::CSR_HW);

    auto pageFaultManager2 = std::make_unique<MockPageFaultManager>();
    pageFaultManager2->selectGpuDomainHandler();

    EXPECT_EQ(pageFaultManager->gpuDomainHandler, reinterpret_cast<void *>(pageFaultManager2->gpuDomainHandler));
}

struct PageFaultManagerTestWithDebugFlag : public ::testing::TestWithParam<uint32_t> {
    void SetUp() override {
        memoryManager = std::make_unique<MockMemoryManager>(executionEnvironment);
        unifiedMemoryManager = std::make_unique<SVMAllocsManager>(memoryManager.get(), false);
        pageFaultManager = std::make_unique<MockPageFaultManager>();
        cmdQ = reinterpret_cast<void *>(0xFFFF);
    }

    MemoryProperties memoryProperties{};
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<MockMemoryManager> memoryManager;
    std::unique_ptr<SVMAllocsManager> unifiedMemoryManager;
    std::unique_ptr<MockPageFaultManager> pageFaultManager;
    void *cmdQ;
};

TEST_P(PageFaultManagerTestWithDebugFlag, givenDebugFlagWhenInsertingAllocationThenItOverridesHints) {
    DebugManagerStateRestore restore;
    DebugManager.flags.UsmInitialPlacement.set(GetParam()); // Should be ignored by the driver, when passing hints
    const auto expectedDomain = GetParam() == 1 ? PageFaultManager::AllocationDomain::None : PageFaultManager::AllocationDomain::Cpu;

    void *allocs[] = {
        reinterpret_cast<void *>(0x1),
        reinterpret_cast<void *>(0x2),
        reinterpret_cast<void *>(0x3),
        reinterpret_cast<void *>(0x4),
    };

    memoryProperties.allocFlags.usmInitialPlacementCpu = 0;
    memoryProperties.allocFlags.usmInitialPlacementGpu = 0;
    pageFaultManager->insertAllocation(allocs[0], 10, unifiedMemoryManager.get(), cmdQ, memoryProperties);
    EXPECT_EQ(expectedDomain, pageFaultManager->memoryData.at(allocs[0]).domain);
    EXPECT_EQ(allocs[0], unifiedMemoryManager->nonGpuDomainAllocs[0]);

    memoryProperties.allocFlags.usmInitialPlacementCpu = 0;
    memoryProperties.allocFlags.usmInitialPlacementGpu = 1;
    pageFaultManager->insertAllocation(allocs[1], 10, unifiedMemoryManager.get(), cmdQ, memoryProperties);
    EXPECT_EQ(expectedDomain, pageFaultManager->memoryData.at(allocs[1]).domain);
    EXPECT_EQ(allocs[1], unifiedMemoryManager->nonGpuDomainAllocs[1]);

    memoryProperties.allocFlags.usmInitialPlacementCpu = 1;
    memoryProperties.allocFlags.usmInitialPlacementGpu = 0;
    pageFaultManager->insertAllocation(allocs[2], 10, unifiedMemoryManager.get(), cmdQ, memoryProperties);
    EXPECT_EQ(expectedDomain, pageFaultManager->memoryData.at(allocs[2]).domain);
    EXPECT_EQ(allocs[2], unifiedMemoryManager->nonGpuDomainAllocs[2]);

    memoryProperties.allocFlags.usmInitialPlacementCpu = 1;
    memoryProperties.allocFlags.usmInitialPlacementGpu = 1;
    pageFaultManager->insertAllocation(allocs[3], 10, unifiedMemoryManager.get(), cmdQ, memoryProperties);
    EXPECT_EQ(expectedDomain, pageFaultManager->memoryData.at(allocs[3]).domain);
    EXPECT_EQ(allocs[3], unifiedMemoryManager->nonGpuDomainAllocs[3]);
}

INSTANTIATE_TEST_SUITE_P(
    PageFaultManagerTestWithDebugFlag,
    PageFaultManagerTestWithDebugFlag,
    ::testing::Values(0, 1));

TEST_F(PageFaultManagerTest, givenNoUsmInitialPlacementFlagsWHenInsertingUsmAllocationThenUseTheDefaultDomain) {
    auto cmdQ = reinterpret_cast<void *>(0xFFFF);
    void *allocs[] = {
        reinterpret_cast<void *>(0x1),
        reinterpret_cast<void *>(0x2),
        reinterpret_cast<void *>(0x3),
        reinterpret_cast<void *>(0x4),
    };

    memoryProperties.allocFlags.usmInitialPlacementCpu = 0;
    memoryProperties.allocFlags.usmInitialPlacementGpu = 0;
    pageFaultManager->insertAllocation(allocs[0], 10, unifiedMemoryManager.get(), cmdQ, memoryProperties);
    EXPECT_EQ(PageFaultManager::AllocationDomain::Cpu, pageFaultManager->memoryData.at(allocs[0]).domain);
    EXPECT_EQ(allocs[0], unifiedMemoryManager->nonGpuDomainAllocs[0]);

    memoryProperties.allocFlags.usmInitialPlacementCpu = 0;
    memoryProperties.allocFlags.usmInitialPlacementGpu = 1;
    pageFaultManager->insertAllocation(allocs[1], 10, unifiedMemoryManager.get(), cmdQ, memoryProperties);
    EXPECT_EQ(PageFaultManager::AllocationDomain::None, pageFaultManager->memoryData.at(allocs[1]).domain);
    EXPECT_EQ(allocs[1], unifiedMemoryManager->nonGpuDomainAllocs[1]);

    memoryProperties.allocFlags.usmInitialPlacementCpu = 1;
    memoryProperties.allocFlags.usmInitialPlacementGpu = 0;
    pageFaultManager->insertAllocation(allocs[2], 10, unifiedMemoryManager.get(), cmdQ, memoryProperties);
    EXPECT_EQ(PageFaultManager::AllocationDomain::Cpu, pageFaultManager->memoryData.at(allocs[2]).domain);
    EXPECT_EQ(allocs[2], unifiedMemoryManager->nonGpuDomainAllocs[2]);

    memoryProperties.allocFlags.usmInitialPlacementCpu = 1;
    memoryProperties.allocFlags.usmInitialPlacementGpu = 1;
    pageFaultManager->insertAllocation(allocs[3], 10, unifiedMemoryManager.get(), cmdQ, memoryProperties);
    EXPECT_EQ(PageFaultManager::AllocationDomain::Cpu, pageFaultManager->memoryData.at(allocs[3]).domain);
    EXPECT_EQ(allocs[3], unifiedMemoryManager->nonGpuDomainAllocs[3]);
}
