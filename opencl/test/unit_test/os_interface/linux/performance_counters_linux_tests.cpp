/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/os_interface/linux/mock_performance_counters_linux.h"
#include "opencl/test/unit_test/os_interface/mock_performance_counters.h"

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
