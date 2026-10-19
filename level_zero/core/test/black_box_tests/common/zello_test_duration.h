/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <chrono>

namespace LevelZeroBlackBoxTests {

struct TestDuration {
    enum Units {
        seconds,
        milliseconds,
        microseconds,
        max
    };
    TestDuration(int argc, char *argv[]);
    TestDuration() : TestDuration(Units::seconds) {}
    TestDuration(Units units);
    ~TestDuration();

  private:
    std::chrono::high_resolution_clock::time_point startTime;
    Units units;
};

} // namespace LevelZeroBlackBoxTests
