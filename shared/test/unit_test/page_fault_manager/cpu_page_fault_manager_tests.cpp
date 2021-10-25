/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test_checks_shared.h"
#include "shared/test/unit_test/page_fault_manager/cpu_page_fault_manager_tests_fixture.h"

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
}

TEST_F(PageFaultManagerTest, givenUnifiedMemoryAllocsWhenMovingToGpuDomainWithPrintUsmSharedMigrationDebugKeyThenMessageIsPrinted) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.PrintUmdSharedMigration.set(1);

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

    testing::internal::CaptureStdout(); // start capturing

    pageFaultManager->moveAllocationsWithinUMAllocsManagerToGpuDomain(reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager));

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
}

TEST_F(PageFaultManagerTest, givenUnifiedMemoryAllocWhenMoveToGpuDomainWithPrintUsmSharedMigrationDebugKeyThenMessageIsPrinted) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.PrintUmdSharedMigration.set(1);

    void *cmdQ = reinterpret_cast<void *>(0xFFFF);

    void *alloc = reinterpret_cast<void *>(0x1);

    pageFaultManager->insertAllocation(alloc, 10, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager), cmdQ, {});
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
    memoryProperties.allocFlags.usmInitialPlacementCpu = 1;
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
    memoryProperties.allocFlags.usmInitialPlacementGpu = 1;
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

TEST_F(PageFaultManagerTest, givenTbxWhenVerifyingPagefaultThenVerifyPagefaultUnprotectsAndTransfersMemory) {
    void *alloc = reinterpret_cast<void *>(0x1);

    pageFaultManager->gpuDomainHandler = &MockPageFaultManager::handleGpuDomainTransferForAubAndTbx;

    MemoryProperties memoryProperties{};
    memoryProperties.allocFlags.usmInitialPlacementGpu = 1;
    pageFaultManager->insertAllocation(alloc, 10, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager), nullptr, memoryProperties);

    pageFaultManager->moveAllocationToGpuDomain(alloc);

    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 0);
    EXPECT_EQ(pageFaultManager->protectedMemoryAccessAddress, alloc);
    EXPECT_EQ(pageFaultManager->protectedSize, 10u);

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

    MemoryProperties memoryProperties{};
    memoryProperties.allocFlags.usmInitialPlacementGpu = 1;
    pageFaultManager->insertAllocation(alloc, 10, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager), nullptr, memoryProperties);

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

    MemoryProperties memoryProperties{};
    memoryProperties.allocFlags.usmInitialPlacementGpu = 1;
    pageFaultManager->insertAllocation(alloc, 10, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager), nullptr, memoryProperties);

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

    MemoryProperties memoryProperties{};
    memoryProperties.allocFlags.usmInitialPlacementGpu = 1;
    pageFaultManager->insertAllocation(alloc, 10, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager), nullptr, memoryProperties);

    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 0);
    EXPECT_EQ(pageFaultManager->protectedMemoryAccessAddress, alloc);
    EXPECT_EQ(pageFaultManager->protectedSize, 10u);

    pageFaultManager->verifyPageFault(alloc);

    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 1);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->allowedMemoryAccessAddress, alloc);
    EXPECT_EQ(pageFaultManager->accessAllowedSize, 10u);
    EXPECT_TRUE(pageFaultManager->isAubWritable);
}

TEST_F(PageFaultManagerTest, givenInitialPlacementCpuWhenMovingToGpuDomainThenFirstAccessInvokesTransfer) {
    void *cmdQ = reinterpret_cast<void *>(0xFFFF);

    void *alloc = reinterpret_cast<void *>(0x1);

    MemoryProperties memoryProperties{};
    memoryProperties.allocFlags.usmInitialPlacementCpu = 1;
    pageFaultManager->insertAllocation(alloc, 10, reinterpret_cast<SVMAllocsManager *>(unifiedMemoryManager), cmdQ, memoryProperties);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->memoryData.size(), 1u);

    pageFaultManager->moveAllocationToGpuDomain(alloc);

    EXPECT_EQ(pageFaultManager->allowMemoryAccessCalled, 0);
    EXPECT_EQ(pageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(pageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(pageFaultManager->transferToGpuCalled, 1);

    EXPECT_EQ(pageFaultManager->protectedMemoryAccessAddress, alloc);
    EXPECT_EQ(pageFaultManager->protectedSize, 10u);
    EXPECT_EQ(pageFaultManager->transferToGpuAddress, alloc);
}

TEST_F(PageFaultManagerTest, givenInitialPlacementGpuWhenMovingToGpuDomainThenFirstAccessDoesNotInvokeTransfer) {
    void *cmdQ = reinterpret_cast<void *>(0xFFFF);

    void *alloc = reinterpret_cast<void *>(0x1);

    MemoryProperties memoryProperties{};
    memoryProperties.allocFlags.usmInitialPlacementGpu = 1;
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
}

TEST_F(PageFaultManagerTest, givenAllocationMovedToGpuDomainWhenVerifyingPagefaultThenAllocationIsMovedToCpuDomain) {
    void *cmdQ = reinterpret_cast<void *>(0xFFFF);

    void *alloc = reinterpret_cast<void *>(0x1);

    MemoryProperties memoryProperties{};
    memoryProperties.allocFlags.usmInitialPlacementGpu = 1;
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

    std::set<uint32_t> rootDeviceIndices{mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    void *cmdQ = reinterpret_cast<void *>(0xFFFF);
    auto memoryManager = std::make_unique<MockMemoryManager>(executionEnvironment);
    auto unifiedMemoryManager = std::make_unique<SVMAllocsManager>(memoryManager.get(), false);
    auto properties = SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::SHARED_UNIFIED_MEMORY, rootDeviceIndices, deviceBitfields);
    void *alloc1 = unifiedMemoryManager->createSharedUnifiedMemoryAllocation(10, properties, cmdQ);

    pageFaultManager->baseAubWritable(false, alloc1, unifiedMemoryManager.get());

    auto gpuAlloc = unifiedMemoryManager->getSVMAlloc(alloc1)->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex);
    EXPECT_FALSE(gpuAlloc->isAubWritable(GraphicsAllocation::allBanks));

    unifiedMemoryManager->freeSVMAlloc(alloc1);
}

TEST(PageFaultManager, givenAubOrTbxCsrWhenSelectingHandlerThenAubAndTbxGpuDomainHandlerIsSet) {
    DebugManagerStateRestore restorer;

    auto pageFaultManager = std::make_unique<MockPageFaultManager>();

    for (auto csrType : {CommandStreamReceiverType::CSR_AUB, CommandStreamReceiverType::CSR_TBX, CommandStreamReceiverType::CSR_TBX_WITH_AUB}) {
        DebugManager.flags.SetCommandStreamReceiver.set(csrType);

        pageFaultManager = std::make_unique<MockPageFaultManager>();
        pageFaultManager->selectGpuDomainHandler();

        EXPECT_EQ(pageFaultManager->getAubAndTbxHandlerAddress(), reinterpret_cast<void *>(pageFaultManager->gpuDomainHandler));
    }
}

TEST(PageFaultManager, givenHwCsrWhenSelectingHandlerThenHwGpuDomainHandlerIsSet) {
    DebugManagerStateRestore restorer;

    auto pageFaultManager = std::make_unique<MockPageFaultManager>();
    auto defaultHandler = pageFaultManager->gpuDomainHandler;

    EXPECT_EQ(pageFaultManager->getHwHandlerAddress(), reinterpret_cast<void *>(pageFaultManager->gpuDomainHandler));

    for (auto csrType : {CommandStreamReceiverType::CSR_HW, CommandStreamReceiverType::CSR_HW_WITH_AUB}) {
        DebugManager.flags.SetCommandStreamReceiver.set(csrType);

        pageFaultManager = std::make_unique<MockPageFaultManager>();
        pageFaultManager->selectGpuDomainHandler();

        EXPECT_EQ(defaultHandler, reinterpret_cast<void *>(pageFaultManager->gpuDomainHandler));
    }
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

    memoryProperties.allocFlags.usmInitialPlacementCpu = 0;
    memoryProperties.allocFlags.usmInitialPlacementGpu = 1;
    pageFaultManager->insertAllocation(allocs[1], 10, unifiedMemoryManager.get(), cmdQ, memoryProperties);
    EXPECT_EQ(expectedDomain, pageFaultManager->memoryData.at(allocs[1]).domain);

    memoryProperties.allocFlags.usmInitialPlacementCpu = 1;
    memoryProperties.allocFlags.usmInitialPlacementGpu = 0;
    pageFaultManager->insertAllocation(allocs[2], 10, unifiedMemoryManager.get(), cmdQ, memoryProperties);
    EXPECT_EQ(expectedDomain, pageFaultManager->memoryData.at(allocs[2]).domain);

    memoryProperties.allocFlags.usmInitialPlacementCpu = 1;
    memoryProperties.allocFlags.usmInitialPlacementGpu = 1;
    pageFaultManager->insertAllocation(allocs[3], 10, unifiedMemoryManager.get(), cmdQ, memoryProperties);
    EXPECT_EQ(expectedDomain, pageFaultManager->memoryData.at(allocs[3]).domain);
}

INSTANTIATE_TEST_SUITE_P(
    PageFaultManagerTestWithDebugFlag,
    PageFaultManagerTestWithDebugFlag,
    ::testing::Values(0, 1));

TEST_F(PageFaultManagerTest, givenNoUsmInitialPlacementFlagsWHenInsertingUsmAllocationThenUseTheDefaultDomain) {
    MemoryProperties memoryProperties{};
    MockExecutionEnvironment executionEnvironment{};
    auto memoryManager = std::make_unique<MockMemoryManager>(executionEnvironment);
    auto unifiedMemoryManager = std::make_unique<SVMAllocsManager>(memoryManager.get(), false);
    auto pageFaultManager = std::make_unique<MockPageFaultManager>();
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

    memoryProperties.allocFlags.usmInitialPlacementCpu = 0;
    memoryProperties.allocFlags.usmInitialPlacementGpu = 1;
    pageFaultManager->insertAllocation(allocs[1], 10, unifiedMemoryManager.get(), cmdQ, memoryProperties);
    EXPECT_EQ(PageFaultManager::AllocationDomain::None, pageFaultManager->memoryData.at(allocs[1]).domain);

    memoryProperties.allocFlags.usmInitialPlacementCpu = 1;
    memoryProperties.allocFlags.usmInitialPlacementGpu = 0;
    pageFaultManager->insertAllocation(allocs[2], 10, unifiedMemoryManager.get(), cmdQ, memoryProperties);
    EXPECT_EQ(PageFaultManager::AllocationDomain::Cpu, pageFaultManager->memoryData.at(allocs[2]).domain);

    memoryProperties.allocFlags.usmInitialPlacementCpu = 1;
    memoryProperties.allocFlags.usmInitialPlacementGpu = 1;
    pageFaultManager->insertAllocation(allocs[3], 10, unifiedMemoryManager.get(), cmdQ, memoryProperties);
    EXPECT_EQ(PageFaultManager::AllocationDomain::Cpu, pageFaultManager->memoryData.at(allocs[3]).domain);
}
