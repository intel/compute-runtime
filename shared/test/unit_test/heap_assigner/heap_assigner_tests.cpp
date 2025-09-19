/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/heap_assigner.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {

class AllocationHelperTests : public Test<DeviceFixture> {
  public:
    HeapAssigner heapAssigner{false};
};

HWTEST_F(AllocationHelperTests, givenKernelIsaTypeWhenUse32BitHeapCalledThenTrueReturned) {

    EXPECT_TRUE(heapAssigner.use32BitHeap(AllocationType::kernelIsa));
    EXPECT_TRUE(heapAssigner.use32BitHeap(AllocationType::kernelIsaInternal));
}

HWTEST_F(AllocationHelperTests, givenKernelIsaTypeWhenUseIternalAllocatorThenUseHeapInternal) {
    auto heapIndex = heapAssigner.get32BitHeapIndex(AllocationType::kernelIsa, true, *defaultHwInfo, false);
    EXPECT_EQ(heapIndex, NEO::HeapIndex::heapInternalDeviceMemory);

    heapIndex = heapAssigner.get32BitHeapIndex(AllocationType::kernelIsaInternal, true, *defaultHwInfo, false);
    EXPECT_EQ(heapIndex, NEO::HeapIndex::heapInternalDeviceMemory);
}

HWTEST_F(AllocationHelperTests, givenNotInternalTypeWhenUseIternalAllocatorThenUseHeapExternal) {
    auto heapIndex = heapAssigner.get32BitHeapIndex(AllocationType::linearStream, true, *defaultHwInfo, false);
    EXPECT_EQ(heapIndex, NEO::HeapIndex::heapExternalDeviceMemory);
}

HWTEST_F(AllocationHelperTests, givenKernelIsaTypesWhenUseInternalAllocatorCalledThenTrueReturned) {
    EXPECT_TRUE(heapAssigner.useInternal32BitHeap(AllocationType::kernelIsa));
    EXPECT_TRUE(heapAssigner.useInternal32BitHeap(AllocationType::kernelIsaInternal));
}

HWTEST_F(AllocationHelperTests, givenInternalHeapTypeWhenUseInternalAllocatorCalledThenTrueReturned) {
    EXPECT_TRUE(heapAssigner.useInternal32BitHeap(AllocationType::internalHeap));
}

HWTEST_F(AllocationHelperTests, givenNotInternalHeapTypeWhenUseInternalAllocatorCalledThenFalseReturned) {
    EXPECT_FALSE(heapAssigner.useInternal32BitHeap(AllocationType::buffer));
}

} // namespace NEO
