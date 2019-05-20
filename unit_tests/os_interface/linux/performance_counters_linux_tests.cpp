/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/options.h"
#include "unit_tests/os_interface/linux/mock_performance_counters_linux.h"
#include "unit_tests/os_interface/mock_performance_counters.h"

#include "gtest/gtest.h"

using namespace NEO;

struct PerformanceCountersLinuxTest : public PerformanceCountersFixture,
                                      public ::testing::Test {

    void SetUp() override {
        PerformanceCountersFixture::SetUp();
    }
    void TearDown() override {
        PerformanceCountersFixture::TearDown();
    }
};
