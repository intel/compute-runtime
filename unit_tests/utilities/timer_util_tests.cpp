/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/utilities/timer_util.h"
#include "test.h"

#include "gtest/gtest.h"

#include <algorithm>

using namespace OCLRT;

TEST(TimerTest, Get) {
    Timer::setFreq();
    Timer timer;

    auto loopCount = 100u;

    unsigned long long maxDelta = 0;
    unsigned long long minDelta = -1;

    while (loopCount--) {
        timer.start();
        timer.end();
        unsigned long long currentDelta = timer.get();
        maxDelta = std::max(currentDelta, maxDelta);
        minDelta = std::min(currentDelta, minDelta);
    }

    EXPECT_LE(minDelta, 10000u);
    //thread switch may cost up to 2s
    EXPECT_LE(maxDelta, 2000000000u);
}

TEST(TimerTest, GetStartEnd) {

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

TEST(TimerTest, Assignement) {

    Timer::setFreq();
    Timer timer1, timer2;

    timer1.start();
    timer1.end();

    timer2 = timer1;

    EXPECT_EQ(timer1.getStart(), timer2.getStart());
}
