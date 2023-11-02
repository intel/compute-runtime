/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/heap_assigner.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"

namespace L0 {
namespace ult {

using AlocationHelperTests = Test<DeviceFixture>;

using Platforms = IsAtMostProduct<IGFX_TIGERLAKE_LP>;

HWTEST2_F(AlocationHelperTests, givenLinearStreamTypeWhenUseExternalAllocatorForSshAndDshDisabledThenUse32BitIsFalse, Platforms) {
    HeapAssigner heapAssigner{false};
    EXPECT_FALSE(heapAssigner.use32BitHeap(AllocationType::LINEAR_STREAM));
}

HWTEST2_F(AlocationHelperTests, givenLinearStreamTypeWhenUseExternalAllocatorForSshAndDshEnabledThenUse32BitIsTrue, Platforms) {
    HeapAssigner heapAssigner{true};
    EXPECT_TRUE(heapAssigner.use32BitHeap(AllocationType::LINEAR_STREAM));
}

HWTEST2_F(AlocationHelperTests, givenLinearStreamTypeWhenUseIternalAllocatorThenUseHeapExternal, Platforms) {
    HeapAssigner heapAssigner{true};
    auto heapIndex = heapAssigner.get32BitHeapIndex(AllocationType::LINEAR_STREAM, true, *defaultHwInfo.get(), false);
    EXPECT_EQ(heapIndex, NEO::HeapIndex::HEAP_EXTERNAL_DEVICE_MEMORY);
}

TEST_F(AlocationHelperTests, givenLinearStreamAllocationWhenSelectingHeapWithUseExternalAllocatorForSshAndDshEnabledThenExternalHeapIsUsed) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseExternalAllocatorForSshAndDsh.set(true);
    std::unique_ptr<MemoryManagerMock> mockMemoryManager(new MemoryManagerMock(*device->getNEODevice()->getExecutionEnvironment()));
    GraphicsAllocation allocation{0, AllocationType::LINEAR_STREAM, nullptr, 0, 0, MemoryPool::MemoryNull, MemoryManager::maxOsContextCount, 0llu};

    allocation.set32BitAllocation(false);
    EXPECT_EQ(MemoryManager::selectExternalHeap(allocation.isAllocatedInLocalMemoryPool()), mockMemoryManager->selectHeap(&allocation, false, false, false));
    EXPECT_TRUE(mockMemoryManager->heapAssigners[0]->apiAllowExternalHeapForSshAndDsh);
}

TEST_F(AlocationHelperTests, givenExternalHeapIndexWhenMapingToExternalFrontWindowThenEternalFrontWindowReturned) {
    EXPECT_EQ(HeapIndex::HEAP_EXTERNAL_FRONT_WINDOW, HeapAssigner::mapExternalWindowIndex(HeapIndex::HEAP_EXTERNAL));
}

TEST_F(AlocationHelperTests, givenExternalDeviceHeapIndexWhenMapingToExternalFrontWindowThenEternalDeviceFrontWindowReturned) {
    EXPECT_EQ(HeapIndex::HEAP_EXTERNAL_DEVICE_FRONT_WINDOW, HeapAssigner::mapExternalWindowIndex(HeapIndex::HEAP_EXTERNAL_DEVICE_MEMORY));
}

TEST_F(AlocationHelperTests, givenOtherThanExternalHeapIndexWhenMapingToExternalFrontWindowThenAbortHasBeenThrown) {
    EXPECT_THROW(HeapAssigner::mapExternalWindowIndex(HeapIndex::HEAP_STANDARD), std::exception);
}

} // namespace ult
} // namespace L0
