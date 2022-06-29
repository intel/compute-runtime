/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/heap_assigner.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"

namespace L0 {
namespace ult {

using AlocationHelperTests = Test<DeviceFixture>;

using Platforms = IsAtMostProduct<IGFX_TIGERLAKE_LP>;

HWTEST2_F(AlocationHelperTests, givenLinearStreamTypeWhenUseExternalAllocatorForSshAndDshDisabledThenUse32BitIsFalse, Platforms) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseExternalAllocatorForSshAndDsh.set(false);
    HeapAssigner heapAssigner = {};
    EXPECT_FALSE(heapAssigner.use32BitHeap(AllocationType::LINEAR_STREAM));
}

HWTEST2_F(AlocationHelperTests, givenLinearStreamTypeWhenUseExternalAllocatorForSshAndDshEnabledThenUse32BitIsTrue, Platforms) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseExternalAllocatorForSshAndDsh.set(true);
    HeapAssigner heapAssigner = {};
    EXPECT_TRUE(heapAssigner.use32BitHeap(AllocationType::LINEAR_STREAM));
}

HWTEST2_F(AlocationHelperTests, givenLinearStreamTypeWhenUseIternalAllocatorThenUseHeapExternal, Platforms) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseExternalAllocatorForSshAndDsh.set(true);
    HeapAssigner heapAssigner = {};
    auto heapIndex = heapAssigner.get32BitHeapIndex(AllocationType::LINEAR_STREAM, true, *defaultHwInfo.get(), false);
    EXPECT_EQ(heapIndex, NEO::HeapIndex::HEAP_EXTERNAL_DEVICE_MEMORY);
}
struct MockMemoryManagerAllocationHelper : public MemoryManagerMock {
    MockMemoryManagerAllocationHelper(NEO::ExecutionEnvironment &executionEnvironment) : MemoryManagerMock(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment)) {}
    GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData, bool useLocalMemory) override {
        passedUseLocalMem = useLocalMemory;
        return nullptr;
    }
    bool passedUseLocalMem = false;
};
TEST_F(AlocationHelperTests, GivenLinearStreamAllocTypeWhenUseExternalAllocatorForSshAndDshEnabledThenUseLocalMemEqualHwHelperValue) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseExternalAllocatorForSshAndDsh.set(true);
    AllocationData allocationData;
    allocationData.type = AllocationType::LINEAR_STREAM;
    std::unique_ptr<MockMemoryManagerAllocationHelper> mockMemoryManager(new MockMemoryManagerAllocationHelper(*device->getNEODevice()->getExecutionEnvironment()));
    mockMemoryManager->allocateGraphicsMemory(allocationData);
    EXPECT_EQ(mockMemoryManager->passedUseLocalMem, HwInfoConfig::get(device->getHwInfo().platform.eProductFamily)->heapInLocalMem(device->getHwInfo()));
}

TEST_F(AlocationHelperTests, GivenInternalAllocTypeWhenUseExternalAllocatorForSshAndDshDisabledThenUseLocalMemEqualFalse) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseExternalAllocatorForSshAndDsh.set(false);
    AllocationData allocationData;
    allocationData.type = AllocationType::KERNEL_ISA;
    std::unique_ptr<MockMemoryManagerAllocationHelper> mockMemoryManager(new MockMemoryManagerAllocationHelper(*device->getNEODevice()->getExecutionEnvironment()));
    mockMemoryManager->allocateGraphicsMemory(allocationData);
    EXPECT_FALSE(mockMemoryManager->passedUseLocalMem);
}

TEST_F(AlocationHelperTests, givenLinearStreamAllocationWhenSelectingHeapWithUseExternalAllocatorForSshAndDshEnabledThenExternalHeapIsUsed) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseExternalAllocatorForSshAndDsh.set(true);
    std::unique_ptr<MockMemoryManagerAllocationHelper> mockMemoryManager(new MockMemoryManagerAllocationHelper(*device->getNEODevice()->getExecutionEnvironment()));
    GraphicsAllocation allocation{0, AllocationType::LINEAR_STREAM, nullptr, 0, 0, MemoryPool::MemoryNull, MemoryManager::maxOsContextCount, 0llu};

    allocation.set32BitAllocation(false);
    EXPECT_EQ(MemoryManager::selectExternalHeap(allocation.isAllocatedInLocalMemoryPool()), mockMemoryManager->selectHeap(&allocation, false, false, false));
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
