/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/heap_assigner.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/test_macros/test.h"

namespace NEO {

class AlocationHelperTests : public Test<DeviceFixture> {
  public:
    HeapAssigner heapAssigner = {};
};

HWTEST_F(AlocationHelperTests, givenKernelIsaTypeWhenUse32BitHeapCalledThenTrueReturned) {

    EXPECT_TRUE(heapAssigner.use32BitHeap(AllocationType::KERNEL_ISA));
    EXPECT_TRUE(heapAssigner.use32BitHeap(AllocationType::KERNEL_ISA_INTERNAL));
}

HWTEST_F(AlocationHelperTests, givenKernelIsaTypeWhenUseIternalAllocatorThenUseHeapInternal) {
    auto heapIndex = heapAssigner.get32BitHeapIndex(AllocationType::KERNEL_ISA, true, *defaultHwInfo, false);
    EXPECT_EQ(heapIndex, NEO::HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY);

    heapIndex = heapAssigner.get32BitHeapIndex(AllocationType::KERNEL_ISA_INTERNAL, true, *defaultHwInfo, false);
    EXPECT_EQ(heapIndex, NEO::HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY);
}

HWTEST_F(AlocationHelperTests, givenNotInternalTypeWhenUseIternalAllocatorThenUseHeapExternal) {
    auto heapIndex = heapAssigner.get32BitHeapIndex(AllocationType::LINEAR_STREAM, true, *defaultHwInfo, false);
    EXPECT_EQ(heapIndex, NEO::HeapIndex::HEAP_EXTERNAL_DEVICE_MEMORY);
}

HWTEST_F(AlocationHelperTests, givenKernelIsaTypesWhenUseInternalAllocatorCalledThenTrueReturned) {
    EXPECT_TRUE(heapAssigner.useInternal32BitHeap(AllocationType::KERNEL_ISA));
    EXPECT_TRUE(heapAssigner.useInternal32BitHeap(AllocationType::KERNEL_ISA_INTERNAL));
}

HWTEST_F(AlocationHelperTests, givenInternalHeapTypeWhenUseInternalAllocatorCalledThenTrueReturned) {
    EXPECT_TRUE(heapAssigner.useInternal32BitHeap(AllocationType::INTERNAL_HEAP));
}

HWTEST_F(AlocationHelperTests, givenNotInternalHeapTypeWhenUseInternalAllocatorCalledThenFalseReturned) {
    EXPECT_FALSE(heapAssigner.useInternal32BitHeap(AllocationType::BUFFER));
}

} // namespace NEO