/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/timer_util.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

#include <algorithm>

using namespace NEO;

TEST(TimerTest, WhenGettingStartEndThenEndIsAfterStart) {

    Timer::setFreq();
    Timer timer;

    timer.start();
    timer.end();
    long long start = timer.getStart();
    EXPECT_NE(0, start);

    long long end = timer.getEnd();
    EXPECT_NE(0, end);
    EXPECT_GE(end, start);
}

TEST(TimerTest, WhenAssigningTimerThenStartTimeIsCopied) {

    Timer::setFreq();
    Timer timer1, timer2;

    timer1.start();
    timer1.end();

    timer2 = timer1;

    EXPECT_EQ(timer1.getStart(), timer2.getStart());
}
