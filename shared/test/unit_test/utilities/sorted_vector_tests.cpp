/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/sorted_vector.h"

#include "gtest/gtest.h"

struct Data {
    size_t size;
};
using TestedSortedVector = NEO::BaseSortedPointerWithValueVector<Data>;

TEST(SortedVectorTest, givenBaseSortedVectorWhenGettingNullptrThenNullptrIsReturned) {
    TestedSortedVector testedVector;
    EXPECT_EQ(nullptr, testedVector.get(nullptr));
}

TEST(SortedVectorTest, givenBaseSortedVectorWhenCallingExtractThenCorrectValueIsReturned) {
    TestedSortedVector testedVector;
    void *ptr = reinterpret_cast<void *>(0x1);
    testedVector.insert(ptr, Data{1u});

    EXPECT_EQ(nullptr, testedVector.extract(nullptr));
    auto valuePtr = testedVector.extract(ptr);
    EXPECT_EQ(1u, valuePtr->size);
    EXPECT_EQ(nullptr, testedVector.extract(ptr));

    testedVector.insert(reinterpret_cast<void *>(0x1), Data{1u});
    testedVector.insert(reinterpret_cast<void *>(0x2), Data{2u});
    testedVector.insert(reinterpret_cast<void *>(0x3), Data{3u});
    testedVector.insert(reinterpret_cast<void *>(0x4), Data{4u});
    testedVector.insert(reinterpret_cast<void *>(0x5), Data{5u});

    valuePtr = testedVector.extract(reinterpret_cast<void *>(0x1));
    EXPECT_EQ(1u, valuePtr->size);
}

TEST(SortedVectorTest, givenBaseSortedVectorWhenRemovingNotStoredPointerThenStoredEntriesAreKept) {
    TestedSortedVector testedVector;
    testedVector.insert(reinterpret_cast<void *>(0x1000), Data{0x10u});
    testedVector.insert(reinterpret_cast<void *>(0x2000), Data{0x10u});

    EXPECT_FALSE(testedVector.remove(reinterpret_cast<void *>(0x3000)));

    EXPECT_EQ(2u, testedVector.getNumAllocs());
    EXPECT_NE(nullptr, testedVector.get(reinterpret_cast<void *>(0x1000)));
    EXPECT_NE(nullptr, testedVector.get(reinterpret_cast<void *>(0x2000)));

    EXPECT_TRUE(testedVector.remove(reinterpret_cast<void *>(0x1000)));

    EXPECT_EQ(1u, testedVector.getNumAllocs());
    EXPECT_EQ(nullptr, testedVector.get(reinterpret_cast<void *>(0x1000)));
    EXPECT_NE(nullptr, testedVector.get(reinterpret_cast<void *>(0x2000)));
}
