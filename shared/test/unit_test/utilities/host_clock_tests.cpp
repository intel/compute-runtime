/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/host_clock.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(HostClockTests, givenTimestampsInNanosecondsWhenGettingElapsedTimeInMicrosecondsThenItIsTruncated) {
    EXPECT_EQ(0, HostClock::getElapsedTimeInUs(1'000, 1'999));
    EXPECT_EQ(2, HostClock::getElapsedTimeInUs(1'000, 3'000));
}
