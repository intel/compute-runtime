/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/fixtures/front_window_fixture.h"

namespace NEO {

using FrontWindowAllocatorTests = Test<MemManagerFixture>;

TEST_F(FrontWindowAllocatorTests, givenAllocateInFrontWindowPoolFlagWhenAllocate32BitGraphicsMemoryThenAllocateAtHeapBegining) {
    AllocationData allocData = {};
    allocData.flags.use32BitFrontWindow = true;
    allocData.size = MemoryConstants::kiloByte;
    auto allocation(memManager->allocate32BitGraphicsMemoryImpl(allocData));
    EXPECT_EQ(allocation->getGpuBaseAddress(), allocation->getGpuAddress());
    memManager->freeGraphicsMemory(allocation);
}

TEST_F(FrontWindowAllocatorTests, givenAllocateInFrontWindowPoolFlagWhenAllocate32BitGraphicsMemoryThenAllocationInFrontWindowPoolRange) {
    AllocationData allocData = {};
    allocData.flags.use32BitFrontWindow = true;
    allocData.size = MemoryConstants::kiloByte;
    auto allocation(memManager->allocate32BitGraphicsMemoryImpl(allocData));
    auto heap = memManager->heapAssigners[allocData.rootDeviceIndex]->get32BitHeapIndex(allocData.type, false, *defaultHwInfo, true);
    auto gmmHelper = memManager->getGmmHelper(allocData.rootDeviceIndex);

    EXPECT_EQ(gmmHelper->canonize(memManager->getGfxPartition(0)->getHeapMinimalAddress(heap)), allocation->getGpuAddress());
    EXPECT_LT(ptrOffset(allocation->getGpuAddress(), allocation->getUnderlyingBufferSize()), gmmHelper->canonize(memManager->getGfxPartition(0)->getHeapLimit(heap)));

    memManager->freeGraphicsMemory(allocation);
}

TEST_F(FrontWindowAllocatorTests, givenInitializedHeapsWhenUseExternalAllocatorForSshAndDshEnabledThenExternalHeapHaveFrontWindowPool) {
    EXPECT_NE(memManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapExternalFrontWindow), 0u);
}

TEST_F(FrontWindowAllocatorTests, givenInitializedHeapsWhenUseExternalAllocatorForSshAndDshEnabledThenFrontWindowPoolIsAtBeginingOfExternalHeap) {
    EXPECT_EQ(memManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapExternalFrontWindow), memManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapExternalFrontWindow));
    EXPECT_EQ(memManager->getGfxPartition(0)->getHeapMinimalAddress(HeapIndex::heapExternalFrontWindow), memManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapExternalFrontWindow));
}

TEST_F(FrontWindowAllocatorTests, givenInitializedHeapsWhenUseExternalAllocatorForSshAndDshEnabledThenMinimalAddressIsNotAtBeginingOfExternalHeap) {
    EXPECT_GT(memManager->getGfxPartition(0)->getHeapMinimalAddress(HeapIndex::heapExternal), memManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapExternal));
}

TEST_F(FrontWindowAllocatorTests, givenInitializedHeapsWhenUseExternalAllocatorForSshAndDshEnabledThenMinimalAddressIsAtBeginingOfExternalFrontWindowHeap) {
    EXPECT_EQ(memManager->getGfxPartition(0)->getHeapMinimalAddress(HeapIndex::heapExternalFrontWindow), memManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapExternalFrontWindow));
}

TEST_F(FrontWindowAllocatorTests, givenInitializedHeapsWhenUseExternalAllocatorForSshAndDshEnabledThenMinimalAddressIsAtBeginingOfExternalDeviceFrontWindowHeap) {
    EXPECT_EQ(memManager->getGfxPartition(0)->getHeapMinimalAddress(HeapIndex::heapExternalDeviceFrontWindow), memManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapExternalDeviceFrontWindow));
}

TEST_F(FrontWindowAllocatorTests, givenInitializedHeapsWhenUseExternalAllocatorForSshAndDshDisabledThenMinimalAddressEqualBeginingOfExternalHeap) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.UseExternalAllocatorForSshAndDsh.set(false);
    memManager.reset(new FrontWindowMemManagerMock(*pDevice->getExecutionEnvironment()));
    EXPECT_EQ(memManager->getGfxPartition(0)->getHeapMinimalAddress(HeapIndex::heapExternal), memManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapExternal) + GfxPartition::heapGranularity);
}

TEST_F(FrontWindowAllocatorTests, givenInitializedHeapsWhenUseExternalAllocatorForSshAndDshDisabledThenExternalHeapDoesntHaveFrontWindowPool) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.UseExternalAllocatorForSshAndDsh.set(false);
    memManager.reset(new FrontWindowMemManagerMock(*pDevice->getExecutionEnvironment()));
    EXPECT_EQ(memManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapExternalFrontWindow), 0u);
}

TEST_F(FrontWindowAllocatorTests, givenLinearStreamAllocWhenSelectingHeapWithFrontWindowThenCorrectIndexReturned) {
    GraphicsAllocation allocation{0, 1u /*num gmms*/, AllocationType::linearStream, nullptr, 0, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount};
    EXPECT_EQ(HeapIndex::heapExternalFrontWindow, memManager->selectHeap(&allocation, true, true, true));
}

} // namespace NEO
