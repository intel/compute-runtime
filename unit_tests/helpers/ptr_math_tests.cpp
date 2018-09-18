/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
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

TEST(PtrMath, givenCastToUint64FunctionWhenItIsCalledThenProperValueIsReturned) {
    uintptr_t address = 0xf0000000;
    void *addressWithTrailingBitSet = reinterpret_cast<void *>(address);
    uint64_t expectedUint64Address = 0xf0000000;
    auto uintAddress = castToUint64(addressWithTrailingBitSet);
    EXPECT_EQ(uintAddress, expectedUint64Address);
}
