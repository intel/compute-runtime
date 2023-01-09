/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/mt_helpers.h"

#include "gtest/gtest.h"

#include <thread>

TEST(MtTestInterlockedMaxFixture, givenCurrentPagingFenceValueWhenValueChangedThenValueIsSet) {
    std::atomic<uint64_t> currentPagingFenceValue;
    std::atomic<int> testCount;
    std::atomic<int> maxValue;
    currentPagingFenceValue.store(0);
    testCount.store(100);
    maxValue.store(0);
    int threadsCount = 8;
    std::thread threads[8];
    for (int i = 0; i < threadsCount; i++) {
        threads[i] = std::thread([&]() {
            while (testCount-- > 0) {
                uint64_t newVal = ++maxValue;
                NEO::MultiThreadHelpers::interlockedMax(currentPagingFenceValue, newVal);
            }
        });
    }
    for (int i = 0; i < threadsCount; i++) {
        threads[i].join();
    }
    uint64_t endValue = currentPagingFenceValue.load();
    EXPECT_EQ(endValue, 100u);
}

TEST(AtomicCompareExchangeWeakSpinTest, givenCurrentEqualsExpectedWhenAtomicCompareExchangeWeakSpinCalledThenReturnsTrue) {
    using TestedType = int;
    std::atomic<TestedType> current = 0;
    TestedType expected = 0;
    const TestedType desired = 1;

    EXPECT_TRUE(NEO::MultiThreadHelpers::atomicCompareExchangeWeakSpin(current, expected, desired));

    EXPECT_EQ(current, 1);
    EXPECT_EQ(expected, 0);
}

TEST(AtomicCompareExchangeWeakSpinTest, givenCurrentNotEqualsExpectedWhenAtomicCompareExchangeWeakSpinCalledThenReturnsFalseAndUpdatesExpected) {
    using TestedType = int;
    std::atomic<TestedType> current = 0;
    TestedType expected = 1;
    const TestedType desired = 2;

    EXPECT_FALSE(NEO::MultiThreadHelpers::atomicCompareExchangeWeakSpin(current, expected, desired));

    EXPECT_EQ(current, 0);
    EXPECT_EQ(expected, current);
}
