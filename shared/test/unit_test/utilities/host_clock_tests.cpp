/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/host_clock.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(HostClockTests, givenTimestampsInNanosecondsWhenGettingElapsedTimeInNanosecondsThenDifferenceIsReturned) {
    EXPECT_EQ(999, HostClock::getElapsedTimeInNs(1'000, 1'999));
    EXPECT_EQ(0, HostClock::getElapsedTimeInNs(3'000, 3'000));
}

TEST(HostClockTests, givenTimestampsInNanosecondsWhenGettingElapsedTimeInMicrosecondsThenItIsTruncated) {
    EXPECT_EQ(0, HostClock::getElapsedTimeInUs(1'000, 1'999));
    EXPECT_EQ(2, HostClock::getElapsedTimeInUs(1'000, 3'000));
}

TEST(HostClockTests, givenTimestampsInNanosecondsWhenGettingElapsedTimeInMillisecondsThenItIsTruncated) {
    EXPECT_EQ(0, HostClock::getElapsedTimeInMs(0, 999'999));
    EXPECT_EQ(3, HostClock::getElapsedTimeInMs(1'000'000, 4'500'000));
}
