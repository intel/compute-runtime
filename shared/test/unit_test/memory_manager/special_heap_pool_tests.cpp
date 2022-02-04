/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/test/common/fixtures/front_window_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

namespace NEO {

using FrontWindowAllocatorTests = Test<MemManagerFixture>;

TEST_F(FrontWindowAllocatorTests, givenAllocateInFrontWindowPoolFlagWhenAllocate32BitGraphicsMemoryThenAllocateAtHeapBegining) {
    AllocationData allocData = {};
    allocData.flags.use32BitFrontWindow = true;
    allocData.size = MemoryConstants::kiloByte;
    auto allocation(memManager->allocate32BitGraphicsMemoryImpl(allocData, false));
    EXPECT_EQ(allocation->getGpuBaseAddress(), allocation->getGpuAddress());
    memManager->freeGraphicsMemory(allocation);
}

TEST_F(FrontWindowAllocatorTests, givenAllocateInFrontWindowPoolFlagWhenAllocate32BitGraphicsMemoryThenAlocationInFrontWindowPoolRange) {
    AllocationData allocData = {};
    allocData.flags.use32BitFrontWindow = true;
    allocData.size = MemoryConstants::kiloByte;
    auto allocation(memManager->allocate32BitGraphicsMemoryImpl(allocData, false));
    auto heap = memManager->heapAssigner.get32BitHeapIndex(allocData.type, false, *defaultHwInfo, true);
    EXPECT_EQ(GmmHelper::canonize(memManager->getGfxPartition(0)->getHeapMinimalAddress(heap)), allocation->getGpuAddress());
    EXPECT_LT(ptrOffset(allocation->getGpuAddress(), allocation->getUnderlyingBufferSize()), GmmHelper::canonize(memManager->getGfxPartition(0)->getHeapLimit(heap)));
    memManager->freeGraphicsMemory(allocation);
}

TEST_F(FrontWindowAllocatorTests, givenInitializedHeapsWhenUseExternalAllocatorForSshAndDshEnabledThenExternalHeapHaveFrontWindowPool) {
    EXPECT_NE(memManager->getGfxPartition(0)->getHeapLimit(HeapIndex::HEAP_EXTERNAL_FRONT_WINDOW), 0u);
}

TEST_F(FrontWindowAllocatorTests, givenInitializedHeapsWhenUseExternalAllocatorForSshAndDshEnabledThenFrontWindowPoolIsAtBeginingOfExternalHeap) {
    EXPECT_EQ(memManager->getGfxPartition(0)->getHeapBase(HeapIndex::HEAP_EXTERNAL_FRONT_WINDOW), memManager->getGfxPartition(0)->getHeapBase(HeapIndex::HEAP_EXTERNAL_FRONT_WINDOW));
    EXPECT_EQ(memManager->getGfxPartition(0)->getHeapMinimalAddress(HeapIndex::HEAP_EXTERNAL_FRONT_WINDOW), memManager->getGfxPartition(0)->getHeapBase(HeapIndex::HEAP_EXTERNAL_FRONT_WINDOW));
}

TEST_F(FrontWindowAllocatorTests, givenInitializedHeapsWhenUseExternalAllocatorForSshAndDshEnabledThenMinimalAdressIsNotAtBeginingOfExternalHeap) {
    EXPECT_GT(memManager->getGfxPartition(0)->getHeapMinimalAddress(HeapIndex::HEAP_EXTERNAL), memManager->getGfxPartition(0)->getHeapBase(HeapIndex::HEAP_EXTERNAL));
}

TEST_F(FrontWindowAllocatorTests, givenInitializedHeapsWhenUseExternalAllocatorForSshAndDshEnabledThenMinimalAdressIsAtBeginingOfExternalFrontWindowHeap) {
    EXPECT_EQ(memManager->getGfxPartition(0)->getHeapMinimalAddress(HeapIndex::HEAP_EXTERNAL_FRONT_WINDOW), memManager->getGfxPartition(0)->getHeapBase(HeapIndex::HEAP_EXTERNAL_FRONT_WINDOW));
}

TEST_F(FrontWindowAllocatorTests, givenInitializedHeapsWhenUseExternalAllocatorForSshAndDshEnabledThenMinimalAdressIsAtBeginingOfExternalDeviceFrontWindowHeap) {
    EXPECT_EQ(memManager->getGfxPartition(0)->getHeapMinimalAddress(HeapIndex::HEAP_EXTERNAL_DEVICE_FRONT_WINDOW), memManager->getGfxPartition(0)->getHeapBase(HeapIndex::HEAP_EXTERNAL_DEVICE_FRONT_WINDOW));
}

TEST_F(FrontWindowAllocatorTests, givenInitializedHeapsWhenUseExternalAllocatorForSshAndDshDisabledThenMinimalAdressEqualBeginingOfExternalHeap) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseExternalAllocatorForSshAndDsh.set(false);
    memManager.reset(new FrontWindowMemManagerMock(*pDevice->getExecutionEnvironment()));
    EXPECT_EQ(memManager->getGfxPartition(0)->getHeapMinimalAddress(HeapIndex::HEAP_EXTERNAL), memManager->getGfxPartition(0)->getHeapBase(HeapIndex::HEAP_EXTERNAL) + GfxPartition::heapGranularity);
}

TEST_F(FrontWindowAllocatorTests, givenInitializedHeapsWhenUseExternalAllocatorForSshAndDshDisabledThenExternalHeapDoesntHaveFrontWindowPool) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseExternalAllocatorForSshAndDsh.set(false);
    memManager.reset(new FrontWindowMemManagerMock(*pDevice->getExecutionEnvironment()));
    EXPECT_EQ(memManager->getGfxPartition(0)->getHeapLimit(HeapIndex::HEAP_EXTERNAL_FRONT_WINDOW), 0u);
}

TEST_F(FrontWindowAllocatorTests, givenLinearStreamAllocWhenSelectingHeapWithFrontWindowThenCorrectIndexReturned) {
    GraphicsAllocation allocation{0, AllocationType::LINEAR_STREAM, nullptr, 0, 0, 0, MemoryPool::MemoryNull};
    EXPECT_EQ(HeapIndex::HEAP_EXTERNAL_FRONT_WINDOW, memManager->selectHeap(&allocation, true, true, true));
}

} // namespace NEO