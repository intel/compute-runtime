/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/aligned_memory.h"
#include "runtime/os_interface/32bit_memory.h"

#include "gtest/gtest.h"

using namespace OCLRT;

TEST(Memory32Bit, Given32bitAllocatorWhenAskedForAllocationThen32BitPointerIsReturned) {
    size_t size = 100u;
    Allocator32bit allocator32bit;
    auto ptr = allocator32bit.allocate(size);
    uintptr_t address64bit = (uintptr_t)ptr - allocator32bit.getBase();

    if (is32BitOsAllocatorAvailable) {
        EXPECT_LT(address64bit, max32BitAddress);
    }

    EXPECT_LT(0u, address64bit);

    auto ret = allocator32bit.free(ptr, size);
    EXPECT_EQ(0, ret);
}
TEST(Memory32Bit, Given32bitAllocatorWhenAskedForAllocationThenPageAlignedPointerIsReturnedWithSizeAlignedToPage) {
    size_t size = 100u;
    Allocator32bit allocator32bit;
    auto ptr = allocator32bit.allocate(size);
    EXPECT_EQ(ptr, alignDown(ptr, MemoryConstants::pageSize));
    auto ret = allocator32bit.free(ptr, size);
    EXPECT_EQ(0, ret);
}

TEST(Memory32Bit, Given32bitAllocatorWithBaseAndSizeWhenAskedForBaseThenCorrectBaseIsReturned) {
    uint64_t size = 100u;
    uint64_t base = 0x23000;
    Allocator32bit allocator32bit(base, size);

    EXPECT_EQ(base, allocator32bit.getBase());
}
