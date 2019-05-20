/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/performance_counters.h"
#include "test.h"

using namespace NEO;

struct PerformanceCountersGenTest : public ::testing::Test {
};

class MockPerformanceCountersGen : public PerformanceCounters {
  public:
    MockPerformanceCountersGen() : PerformanceCounters() {
    }
};