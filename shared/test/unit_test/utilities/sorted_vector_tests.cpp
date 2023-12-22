/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/sorted_vector.h"

#include "gtest/gtest.h"

struct Comparator {
    bool operator()(const std::unique_ptr<size_t> &svmData, const void *ptr, const void *otherPtr) {
        return ptr == otherPtr;
    }
};
using TestedSortedVector = NEO::BaseSortedPointerWithValueVector<size_t, Comparator>;

TEST(SortedVectorTest, givenBaseSortedVectorWhenGettingNullptrThenNullptrIsReturned) {
    TestedSortedVector testedVector;
    EXPECT_EQ(nullptr, testedVector.get(nullptr));
}

TEST(SortedVectorTest, givenBaseSortedVectorWhenCallingExtractThenCorrectValueIsReturned) {
    TestedSortedVector testedVector;
    void *ptr = reinterpret_cast<void *>(0x1);
    testedVector.insert(ptr, 1u);

    EXPECT_EQ(nullptr, testedVector.extract(nullptr));
    auto valuePtr = testedVector.extract(ptr);
    EXPECT_EQ(1u, *valuePtr);
    EXPECT_EQ(nullptr, testedVector.extract(ptr));

    testedVector.insert(reinterpret_cast<void *>(0x1), 1u);
    testedVector.insert(reinterpret_cast<void *>(0x2), 2u);
    testedVector.insert(reinterpret_cast<void *>(0x3), 3u);
    testedVector.insert(reinterpret_cast<void *>(0x4), 4u);
    testedVector.insert(reinterpret_cast<void *>(0x5), 5u);

    valuePtr = testedVector.extract(reinterpret_cast<void *>(0x1));
    EXPECT_EQ(1u, *valuePtr);
}
