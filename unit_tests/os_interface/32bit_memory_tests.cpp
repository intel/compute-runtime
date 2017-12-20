/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "gtest/gtest.h"
#include "runtime/os_interface/32bit_memory.h"
#include "runtime/helpers/aligned_memory.h"

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
