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

#include "runtime/helpers/ptr_math.h"
#include "gtest/gtest.h"

TEST(PtrMath, ptrOffset) {
    auto addrBefore = (uintptr_t)ptrGarbage;
    auto ptrBefore = addrBefore;

    size_t offset = 0x1234;
    auto ptrAfter = ptrOffset(ptrBefore, offset);
    auto addrAfter = ptrAfter;

    EXPECT_EQ(offset, addrAfter - addrBefore);
}

TEST(PtrMath, ptrDiff) {
    size_t offset = 0x1234;
    auto addrBefore = (uintptr_t)ptrGarbage;
    auto addrAfter = addrBefore + offset;

    EXPECT_EQ(offset, ptrDiff(addrAfter, addrBefore));
}

TEST(PtrMath, addrToPtr) {
    uint32_t addr32Bit = 0x3456;
    uint64_t addr64Bit = 0xf000000000003456;
    void *ptr32BitAddr = (void *)((uintptr_t)addr32Bit);
    void *ptr64BitAddr = (void *)((uintptr_t)addr64Bit);

    EXPECT_EQ(ptr32BitAddr, addrToPtr(addr32Bit));
    EXPECT_EQ(ptr64BitAddr, addrToPtr(addr64Bit));
}
