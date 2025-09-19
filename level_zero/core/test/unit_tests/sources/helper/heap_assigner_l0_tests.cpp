/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/heap_assigner.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"

namespace L0 {
namespace ult {

using AllocationHelperTests = Test<DeviceFixture>;

using Platforms = IsAtMostProduct<IGFX_TIGERLAKE_LP>;

HWTEST2_F(AllocationHelperTests, givenLinearStreamTypeWhenUseExternalAllocatorForSshAndDshDisabledThenUse32BitIsFalse, Platforms) {
    HeapAssigner heapAssigner{false};
    EXPECT_FALSE(heapAssigner.use32BitHeap(AllocationType::linearStream));
}

HWTEST2_F(AllocationHelperTests, givenLinearStreamTypeWhenUseExternalAllocatorForSshAndDshEnabledThenUse32BitIsTrue, Platforms) {
    HeapAssigner heapAssigner{true};
    EXPECT_TRUE(heapAssigner.use32BitHeap(AllocationType::linearStream));
}

HWTEST2_F(AllocationHelperTests, givenLinearStreamTypeWhenUseIternalAllocatorThenUseHeapExternal, Platforms) {
    HeapAssigner heapAssigner{true};
    auto heapIndex = heapAssigner.get32BitHeapIndex(AllocationType::linearStream, true, *defaultHwInfo.get(), false);
    EXPECT_EQ(heapIndex, NEO::HeapIndex::heapExternalDeviceMemory);
}

TEST_F(AllocationHelperTests, givenLinearStreamAllocationWhenSelectingHeapWithUseExternalAllocatorForSshAndDshEnabledThenExternalHeapIsUsed) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.UseExternalAllocatorForSshAndDsh.set(true);
    std::unique_ptr<MemoryManagerMock> mockMemoryManager(new MemoryManagerMock(*device->getNEODevice()->getExecutionEnvironment()));
    GraphicsAllocation allocation{0, 1u /*num gmms*/, AllocationType::linearStream, nullptr, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu};

    allocation.set32BitAllocation(false);
    EXPECT_EQ(MemoryManager::selectExternalHeap(allocation.isAllocatedInLocalMemoryPool()), mockMemoryManager->selectHeap(&allocation, false, false, false));
    EXPECT_TRUE(mockMemoryManager->heapAssigners[0]->apiAllowExternalHeapForSshAndDsh);
}

TEST_F(AllocationHelperTests, givenExternalHeapIndexWhenMapingToExternalFrontWindowThenEternalFrontWindowReturned) {
    EXPECT_EQ(HeapIndex::heapExternalFrontWindow, HeapAssigner::mapExternalWindowIndex(HeapIndex::heapExternal));
}

TEST_F(AllocationHelperTests, givenExternalDeviceHeapIndexWhenMapingToExternalFrontWindowThenEternalDeviceFrontWindowReturned) {
    EXPECT_EQ(HeapIndex::heapExternalDeviceFrontWindow, HeapAssigner::mapExternalWindowIndex(HeapIndex::heapExternalDeviceMemory));
}

TEST_F(AllocationHelperTests, givenOtherThanExternalHeapIndexWhenMapingToExternalFrontWindowThenAbortHasBeenThrown) {
    EXPECT_THROW(HeapAssigner::mapExternalWindowIndex(HeapIndex::heapStandard), std::exception);
}

} // namespace ult
} // namespace L0
