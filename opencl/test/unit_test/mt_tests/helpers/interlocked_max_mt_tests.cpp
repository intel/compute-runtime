/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/interlocked_max.h"

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
                interlockedMax(currentPagingFenceValue, newVal);
            }
        });
    }
    for (int i = 0; i < threadsCount; i++) {
        threads[i].join();
    }
    uint64_t endValue = currentPagingFenceValue.load();
    EXPECT_EQ(endValue, 100u);
}