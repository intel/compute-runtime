/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/os_interface/performance_counters.h"
#include "test.h"

using namespace NEO;

struct PerformanceCountersGenTest : public ::testing::Test {
};

class MockPerformanceCountersGen : public PerformanceCounters {
  public:
    MockPerformanceCountersGen() : PerformanceCounters() {
    }
};