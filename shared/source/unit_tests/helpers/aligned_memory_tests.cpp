/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"

#include "gtest/gtest.h"

#include <cstdint>

TEST(AlignedFree, nullptrShouldntCrash) {
    alignedFree(nullptr);
}
void *ptrAlignedToPage = (void *)0x1000;
void *ptrNotAlignedToPage = (void *)0x1001;

struct AlignedMalloc : public ::testing::TestWithParam<size_t> {
  public:
    void SetUp() override {
        ptr = nullptr;
        alignAlloc = GetParam();
    }

    void TearDown() override {
        alignedFree(ptr);
    }

    void *ptr;
    size_t alignAlloc;
};

TEST_P(AlignedMalloc, size0) {
    size_t sizeAlloc = 0;
    ptr = alignedMalloc(sizeAlloc, alignAlloc);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(0u, (uintptr_t)ptr % alignAlloc);
}

TEST_P(AlignedMalloc, size4096) {
    size_t sizeAlloc = 4096;
    ptr = alignedMalloc(sizeAlloc, alignAlloc);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(0u, (uintptr_t)ptr % alignAlloc);
}

TEST(AlignedMallocTests, size0align4096) {
    size_t sizeAlloc = 0;
    auto ptr = alignedMalloc(sizeAlloc, 4096);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(0u, (uintptr_t)ptr % 4096);
    alignedFree(ptr);
}

INSTANTIATE_TEST_CASE_P(
    AlignedMallocParameterized,
    AlignedMalloc,
    testing::Values(
        1,
        4,
        8,
        64,
        4096));

struct AlignUp : public ::testing::TestWithParam<size_t> {
};

TEST_P(AlignUp, belowAlignmentBefore) {
    uintptr_t addrBefore = 0x1fffffff;
    auto ptrBefore = (uint32_t *)addrBefore;

    auto alignment = GetParam();
    auto ptrAfter = alignUp(ptrBefore, alignment);
    auto addrAfter = (uintptr_t)ptrAfter;

    EXPECT_EQ(0u, addrAfter % alignment);
}

TEST_P(AlignUp, AtAlignmentBefore) {
    uintptr_t addrBefore = 0x20000000;
    auto ptrBefore = (uint32_t *)addrBefore;

    auto alignment = GetParam();
    auto ptrAfter = alignUp(ptrBefore, alignment);
    auto addrAfter = (uintptr_t)ptrAfter;

    EXPECT_EQ(0u, addrAfter % alignment);
}

TEST_P(AlignUp, AboveAlignmentBefore) {
    uintptr_t addrBefore = 0x20000001;
    auto ptrBefore = (uint32_t *)addrBefore;

    auto alignment = GetParam();
    auto ptrAfter = alignUp(ptrBefore, alignment);
    auto addrAfter = (uintptr_t)ptrAfter;

    EXPECT_EQ(0u, addrAfter % alignment);
}

TEST_P(AlignUp, preserve64Bit) {
    uint64_t aligned = 1ULL << 48;
    auto alignment = GetParam();
    auto result = alignUp(aligned, alignment);
    EXPECT_EQ(aligned, result);
}

INSTANTIATE_TEST_CASE_P(
    AlignUpParameterized,
    AlignUp,
    testing::Values(
        1,
        4,
        8,
        32,
        64,
        256,
        4096));

TEST(AlignWholeSize, alignWholeSizeToPage) {
    int size = 1;
    auto retSize = alignSizeWholePage(ptrAlignedToPage, size);
    EXPECT_EQ(retSize, 4096u);
}
TEST(AlignWholeSize, sizeGreaterThenPageResultsIn2Pages) {

    int size = 4097;
    auto retSize = alignSizeWholePage(ptrAlignedToPage, size);
    EXPECT_EQ(retSize, 4096u * 2);
}
TEST(AlignWholeSize, allocationNotPageAligned) {

    int size = 4097;
    auto retSize = alignSizeWholePage(ptrNotAlignedToPage, size);
    EXPECT_EQ(retSize, 4096u * 2);
}
TEST(AlignWholeSize, ptrNotAligned) {

    int size = 1;
    auto retSize = alignSizeWholePage(ptrNotAlignedToPage, size);
    EXPECT_EQ(retSize, 4096u);
}
TEST(AlignWholeSize, allocationFitsToOnePage) {

    int size = 4095;
    auto retSize = alignSizeWholePage(ptrNotAlignedToPage, size);
    EXPECT_EQ(retSize, 4096u);
}
TEST(AlignWholeSize, allocationFitsTo2Pages) {

    int size = 4095 + 4096;
    auto retSize = alignSizeWholePage(ptrNotAlignedToPage, size);
    EXPECT_EQ(retSize, 4096u * 2);
}
TEST(AlignWholeSize, allocationOverlapsToAnotherPage) {

    int size = 4096;
    auto retSize = alignSizeWholePage(ptrNotAlignedToPage, size);
    EXPECT_EQ(retSize, 4096u * 2);
}
TEST(AlignWholeSize, allocationOverlapsTo2AnotherPage) {

    int size = 4096 * 2;
    auto retSize = alignSizeWholePage(ptrNotAlignedToPage, size);
    EXPECT_EQ(retSize, 4096u * 3);
}
TEST(AlignWholeSize, ptrProperlyAlignedTo2Pages) {

    int size = 4096 * 2;
    auto retSize = alignSizeWholePage(ptrAlignedToPage, size);
    EXPECT_EQ(retSize, 4096u * 2);
}

TEST(AlignDown, ptrAlignedToPageWhenAlignedDownReturnsTheSamePointer) {
    void *ptr = (void *)0x1000;

    auto alignedDownPtr = alignDown(ptr, MemoryConstants::pageSize);
    EXPECT_EQ(ptr, alignedDownPtr);
}
TEST(AlignDown, ptrNotAlignedToPageWhenAlignedDownReturnsPageAlignedPointer) {
    void *ptr = (void *)0x1001;
    void *expected_ptr = (void *)0x1000;
    auto alignedDownPtr = alignDown(ptr, MemoryConstants::pageSize);
    EXPECT_EQ(expected_ptr, alignedDownPtr);
}

TEST(AlignDown, ptrNotAlignedToPage2WhenAlignedDownReturnsPageAlignedPointer) {
    void *ptr = (void *)0x1241;
    void *expected_ptr = (void *)0x1000;
    auto alignedDownPtr = alignDown(ptr, MemoryConstants::pageSize);
    EXPECT_EQ(expected_ptr, alignedDownPtr);
}

TEST(AlignDown, ptrNotAlignedToPage3WhenAlignedDownReturnsPageAlignedPointer) {
    void *ptr = (void *)0x3241;
    void *expected_ptr = (void *)0x3000;
    auto alignedDownPtr = alignDown(ptr, MemoryConstants::pageSize);
    EXPECT_EQ(expected_ptr, alignedDownPtr);
}

TEST(AlignDown, ptrNotAlignedToDwordWhenAlignedDownReturnsDwordAlignedPointer) {
    void *ptr = (void *)0x3241;
    void *expected_ptr = (void *)0x3240;
    auto alignedDownPtr = alignDown(ptr, 4);
    EXPECT_EQ(expected_ptr, alignedDownPtr);
}

TEST(AlignDown, preserve64Bit) {
    uint64_t aligned = 1ULL << 48;
    auto result = alignDown(aligned, MemoryConstants::pageSize);
    EXPECT_EQ(aligned, result);
}

TEST(DLLbitness, Given32or64BitLibraryWhenAskedIfItIs32BitThenProperValueIsReturned) {
    auto pointerSize = sizeof(void *);
    if (pointerSize == 8) {
        EXPECT_FALSE(is32bit);
        EXPECT_TRUE(is64bit);
    } else if (pointerSize == 4) {
        EXPECT_TRUE(is32bit);
        EXPECT_FALSE(is64bit);
    } else {
        FAIL() << "unrecognized bitness";
    }
    auto pointerSizeFromSizeT = sizeof(size_t);
    if (pointerSizeFromSizeT == 8) {
        EXPECT_FALSE(is32bit);
        EXPECT_TRUE(is64bit);
    } else if (pointerSizeFromSizeT == 4) {
        EXPECT_TRUE(is32bit);
        EXPECT_FALSE(is64bit);
    } else {
        FAIL() << "unrecognized bitness";
    }
}

template <typename T>
class IsAlignedTests : public ::testing::Test {
};

typedef ::testing::Types<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float, double> IsAlignedTypes;

TYPED_TEST_CASE(IsAlignedTests, IsAlignedTypes);
TYPED_TEST(IsAlignedTests, aligned) {
    TypeParam *ptr = reinterpret_cast<TypeParam *>(static_cast<uintptr_t>(0xdeadbeefu));
    // one byte alignment should always return true
    if (alignof(TypeParam) == 1)
        EXPECT_TRUE(isAligned(ptr));
    else
        EXPECT_FALSE(isAligned(ptr));

    auto ptr1 = reinterpret_cast<TypeParam *>(reinterpret_cast<uintptr_t>(ptr) & ~(alignof(TypeParam) - 1));
    EXPECT_TRUE(isAligned(ptr1));

    auto ptr2 = reinterpret_cast<TypeParam *>(reinterpret_cast<uintptr_t>(ptr) & ~((alignof(TypeParam) << 1) - 1));
    EXPECT_TRUE(isAligned(ptr2));

    // this is hard to align in the middle of byte aligned types
    if (alignof(TypeParam) == 1)
        return;

    auto ptr3 = reinterpret_cast<TypeParam *>(reinterpret_cast<uintptr_t>(ptr) & ~((alignof(TypeParam) >> 1) - 1));
    EXPECT_FALSE(isAligned(ptr3));
}

TEST(IsAligned, nonPointerType) {
    EXPECT_TRUE(isAligned<3>(0));
    EXPECT_FALSE(isAligned<3>(1));
    EXPECT_FALSE(isAligned<3>(2));
    EXPECT_TRUE(isAligned<3>(3));
    EXPECT_FALSE(isAligned<3>(4));
    EXPECT_FALSE(isAligned<3>(5));
    EXPECT_TRUE(isAligned<3>(6));
}

TEST(IsAligned, supportsConstexprEvaluation) {
    static_assert(false == isAligned<3>(2), "");
    static_assert(true == isAligned<3>(3), "");
}
