/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <algorithm>
#include "test.h"
#include "gtest/gtest.h"
#include "runtime/utilities/timer_util.h"

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
