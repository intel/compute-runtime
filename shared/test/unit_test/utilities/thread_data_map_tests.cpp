/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/thread_data_hash.h"
#include "shared/source/utilities/thread_data_map.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(ThreadDataTrackerTest, givenEmptyTrackerWhenGetCommonThreadDataThenZeroCountReturned) {
    ThreadDataTracker tracker;

    auto [hash, threadData] = tracker.getCommonThreadData();

    EXPECT_EQ(0u, hash);
    EXPECT_TRUE(threadData.empty());
}

TEST(ThreadDataTrackerTest, givenNewHashWhenRegisteringThreadDataThenEntryAddedWithCountOne) {
    ThreadDataTracker tracker;
    const uint8_t data[] = {1, 2, 3, 4};

    tracker.registerThreadData(42u, {data, sizeof(data)});
    auto [hash, threadData] = tracker.getCommonThreadData();

    EXPECT_EQ(42u, hash);
    EXPECT_EQ(sizeof(data), threadData.size());
}

TEST(ThreadDataTrackerTest, givenExistingHashWhenRegisteringThreadDataThenCountIncremented) {
    ThreadDataTracker tracker;
    const uint8_t data[] = {1, 2, 3, 4};

    tracker.registerThreadData(42u, {data, sizeof(data)});
    tracker.registerThreadData(42u, {data, sizeof(data)});
    auto [hash, threadData] = tracker.getCommonThreadData();

    EXPECT_EQ(42u, hash);
    EXPECT_EQ(sizeof(data), threadData.size());
}

TEST(ThreadDataTrackerTest, givenMultipleHashesWhenGetCommonThreadDataThenMostFrequentReturned) {
    ThreadDataTracker tracker;
    const uint8_t dataA[] = {1, 2};
    const uint8_t dataB[] = {3, 4};
    const uint8_t dataC[] = {3, 4};
    tracker.registerThreadData(1u, {dataA, sizeof(dataA)});
    tracker.registerThreadData(2u, {dataB, sizeof(dataB)});
    tracker.registerThreadData(2u, {dataB, sizeof(dataB)});
    tracker.registerThreadData(3u, {dataC, sizeof(dataC)});

    auto [hash, threadData] = tracker.getCommonThreadData();

    EXPECT_EQ(2u, hash);
    EXPECT_EQ(sizeof(dataB), threadData.size());
}

TEST(ThreadDataTrackerTest, givenSameCountWhenGetCommonThreadDataThenLargestSizeIsReturned) {
    ThreadDataTracker tracker;
    const uint8_t dataSmall[] = {1, 2};
    const uint8_t dataLargeA[] = {1, 2, 3, 4};
    const uint8_t dataLargeB[] = {5, 6, 7, 8};

    tracker.registerThreadData(1u, {dataSmall, sizeof(dataSmall)});
    tracker.registerThreadData(2u, {dataLargeA, sizeof(dataLargeA)});
    tracker.registerThreadData(3u, {dataLargeB, sizeof(dataLargeB)});

    auto [hash, threadData] = tracker.getCommonThreadData();

    EXPECT_EQ(2u, hash);
    EXPECT_EQ(sizeof(dataLargeA), threadData.size());
}

TEST(ThreadDataTrackerTest, givenEntriesLargeEnoughWhenMapRehashedThenEntriesArePreserved) {
    ThreadDataTracker tracker;
    const uint8_t data[] = {1, 2, 3, 4};

    for (uint64_t h = 1; h <= 6; ++h) {
        tracker.registerThreadData(h, {data, sizeof(data)});
    }
    tracker.registerThreadData(1u, {data, sizeof(data)});

    auto [hash, threadData] = tracker.getCommonThreadData();

    EXPECT_EQ(1u, hash);
    EXPECT_EQ(sizeof(data), threadData.size());
}

TEST(ThreadDataTrackerTest, givenZeroHashWhenRegisteringThreadDataThenEntryStoredAndRetrievedCorrectly) {
    ThreadDataTracker tracker;
    const uint8_t data[] = {1, 2, 3, 4};

    tracker.registerThreadData(0u, {data, sizeof(data)});
    tracker.registerThreadData(0u, {data, sizeof(data)});

    auto [hash, threadData] = tracker.getCommonThreadData();

    EXPECT_EQ(0u, hash);
    EXPECT_EQ(sizeof(data), threadData.size());
}

TEST(ThreadDataTrackerTest, givenTrackerWhenGetCommonCalledThenContainerIsCleared) {
    ThreadDataTracker tracker;
    const uint8_t data[] = {1, 2, 3, 4};

    tracker.registerThreadData(42u, {data, sizeof(data)});
    tracker.getCommonThreadData();

    auto [hash, threadData] = tracker.getCommonThreadData();
    EXPECT_EQ(0u, hash);
    EXPECT_TRUE(threadData.empty());
}

class ComputeThreadDataHashTest : public ::testing::TestWithParam<size_t> {};

TEST_P(ComputeThreadDataHashTest, givenDataWhenHashComputedThenHashIsCorrect) {
    ThreadDataTracker tracker;
    std::vector<uint8_t> crossThreadData(GetParam(), 0xAB);

    uint64_t hash = ThreadDataHash::computeThreadDataHash({crossThreadData.data(), crossThreadData.size()}, {});
    EXPECT_NE(0u, hash);
    EXPECT_EQ(hash, ThreadDataHash::computeThreadDataHash({crossThreadData.data(), crossThreadData.size()}, {}));

    tracker.registerThreadData(hash, {crossThreadData.data(), crossThreadData.size()});
    auto [resultHash, resultData] = tracker.getCommonThreadData();
    EXPECT_EQ(hash, resultHash);
}

INSTANTIATE_TEST_SUITE_P(HashCalculationTest, ComputeThreadDataHashTest,
                         ::testing::Values(4u, 8u, 40u, 80u, 128u, 250u));
