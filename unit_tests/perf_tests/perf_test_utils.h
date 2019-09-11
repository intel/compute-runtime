/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/utilities/timer_util.h"

#include "gtest/gtest.h"

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
