/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

#include "os_interface/performance_counters.h"

using namespace NEO;

struct PerformanceCountersGenTest : public ::testing::Test {
};

class MockPerformanceCountersGen : public PerformanceCounters {
  public:
    MockPerformanceCountersGen() : PerformanceCounters() {
    }
};