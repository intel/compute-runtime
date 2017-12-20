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

#pragma once
#include "gtest/gtest.h"
#include "runtime/utilities/timer_util.h"
#include <stdint.h>

extern const char *perfLogPath;
extern long long refTime;
void setReferenceTime();

bool getTestRatio(uint64_t hash, double &ratio);

bool saveTestRatio(uint64_t hash, double ratio);

bool isInRange(double data, double reference, double rangePercentage);
bool isLowerThanReference(double data, double reference, double rangePercentage);

bool updateTestRatio(uint64_t hash, double ratio);

template <typename T>
T majorityVote(T time1, T time2, T time3) {
    T minTime1 = 0;
    T minTime2 = 0;

    if (time1 < time2) {
        minTime1 = time1;
        minTime2 = time2;
    } else {
        minTime1 = time2;
        minTime2 = time1;
    }

    if (minTime2 > time3)
        minTime2 = time3;

    return (minTime1 + minTime2) / 2;
}
