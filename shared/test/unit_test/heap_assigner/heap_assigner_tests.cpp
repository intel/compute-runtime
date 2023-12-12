/*
 * Copyright (C) 2020-2023 Intel Corporation
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

class AlocationHelperTests : public Test<DeviceFixture> {
  public:
    HeapAssigner heapAssigner{false};
};

HWTEST_F(AlocationHelperTests, givenKernelIsaTypeWhenUse32BitHeapCalledThenTrueReturned) {

    EXPECT_TRUE(heapAssigner.use32BitHeap(AllocationType::kernelIsa));
    EXPECT_TRUE(heapAssigner.use32BitHeap(AllocationType::kernelIsaInternal));
}

HWTEST_F(AlocationHelperTests, givenKernelIsaTypeWhenUseIternalAllocatorThenUseHeapInternal) {
    auto heapIndex = heapAssigner.get32BitHeapIndex(AllocationType::kernelIsa, true, *defaultHwInfo, false);
    EXPECT_EQ(heapIndex, NEO::HeapIndex::heapInternalDeviceMemory);

    heapIndex = heapAssigner.get32BitHeapIndex(AllocationType::kernelIsaInternal, true, *defaultHwInfo, false);
    EXPECT_EQ(heapIndex, NEO::HeapIndex::heapInternalDeviceMemory);
}

HWTEST_F(AlocationHelperTests, givenNotInternalTypeWhenUseIternalAllocatorThenUseHeapExternal) {
    auto heapIndex = heapAssigner.get32BitHeapIndex(AllocationType::linearStream, true, *defaultHwInfo, false);
    EXPECT_EQ(heapIndex, NEO::HeapIndex::heapExternalDeviceMemory);
}

HWTEST_F(AlocationHelperTests, givenKernelIsaTypesWhenUseInternalAllocatorCalledThenTrueReturned) {
    EXPECT_TRUE(heapAssigner.useInternal32BitHeap(AllocationType::kernelIsa));
    EXPECT_TRUE(heapAssigner.useInternal32BitHeap(AllocationType::kernelIsaInternal));
}

HWTEST_F(AlocationHelperTests, givenInternalHeapTypeWhenUseInternalAllocatorCalledThenTrueReturned) {
    EXPECT_TRUE(heapAssigner.useInternal32BitHeap(AllocationType::internalHeap));
}

HWTEST_F(AlocationHelperTests, givenNotInternalHeapTypeWhenUseInternalAllocatorCalledThenFalseReturned) {
    EXPECT_FALSE(heapAssigner.useInternal32BitHeap(AllocationType::buffer));
}

} // namespace NEO
