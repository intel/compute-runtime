/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"

#include "gtest/gtest.h"

TEST(PtrMath, WhenUsingPtrOffsetThenReturnBasePointerPlusOffset) {
    auto addrBefore = (uintptr_t)ptrGarbage;
    auto ptrBefore = addrBefore;

    size_t offset = 0x1234;
    auto ptrAfter = ptrOffset(ptrBefore, offset);
    auto addrAfter = ptrAfter;

    EXPECT_EQ(offset, addrAfter - addrBefore);
}

TEST(PtrMath, WhenUsingPtrDiffThenReturnOffsetBetweenTwoPointers) {
    size_t offset = 0x1234;
    auto addrBefore = (uintptr_t)ptrGarbage;
    auto addrAfter = addrBefore + offset;

    EXPECT_EQ(offset, ptrDiff(addrAfter, addrBefore));
}

TEST(PtrMath, WhenConvertingAddressToPtrThenPtrIsCorrect) {
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
TEST(PtrMath, givenCastToUint64FunctionWhenConstPointerIsPassedItIsCalledThenProperValueIsReturned) {
    uintptr_t address = 0xf0000000;
    const void *addressWithTrailingBitSet = reinterpret_cast<const void *>(address);
    uint64_t expectedUint64Address = 0xf0000000;
    auto uintAddress = castToUint64(addressWithTrailingBitSet);
    EXPECT_EQ(uintAddress, expectedUint64Address);
}

TEST(ptrOffset, WhenGettingPtrOffsetThen64BitIsPreserved) {
    uint64_t ptrBefore = 0x800000000;
    size_t offset = 0x1234;
    auto ptrAfter = ptrOffset(ptrBefore, offset);
    EXPECT_EQ(0x800001234ull, ptrAfter);
}

TEST(ptrDiff, WhenGettingPtrDiffThen64BitIsPreserved) {
    auto ptrAfter = 0x800001234ull;

    auto ptrBefore = ptrDiff(ptrAfter, (size_t)0x1234);
    EXPECT_EQ(0x800000000ull, ptrBefore);

    auto ptrBefore2 = ptrDiff(ptrAfter, 0x1234);
    EXPECT_EQ(0x800000000ull, ptrBefore2);

    auto ptrBefore3 = ptrDiff(ptrAfter, 0x1234ull);
    EXPECT_EQ(0x800000000ull, ptrBefore3);
}
