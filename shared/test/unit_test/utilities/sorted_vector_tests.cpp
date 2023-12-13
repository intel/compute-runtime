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
        return false;
    }
};
using TestedSortedVector = NEO::BaseSortedPointerWithValueVector<size_t, Comparator>;

TEST(SortedVectorTest, givenBaseSortedVectorWhenGettingNullptrThenNullptrIsReturned) {
    TestedSortedVector testedVector;
    EXPECT_EQ(nullptr, testedVector.get(nullptr));
}
