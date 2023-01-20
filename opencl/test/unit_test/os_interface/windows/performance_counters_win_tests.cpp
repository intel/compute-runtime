/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/performance_counters_win.h"

#include "opencl/test/unit_test/os_interface/mock_performance_counters.h"

#include "gtest/gtest.h"

using namespace NEO;

struct PerformanceCountersWinTest : public PerformanceCountersFixture,
                                    public ::testing::Test {
  public:
    void SetUp() override {
        PerformanceCountersFixture::setUp();
    }

    void TearDown() override {
        PerformanceCountersFixture::tearDown();
    }
};
