/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/device_factory.h"

#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_oa.h"

namespace L0 {
namespace ult {

class MetricQueryPoolTest : public MetricContextFixture,
                            public ::testing::Test {
  protected:
    void SetUp() override;
    void TearDown() override;

  public:
    std::unique_ptr<L0::DriverHandle> driverHandle;
};

class MultiDeviceMetricQueryPoolTest : public MetricMultiDeviceFixture,
                                       public ::testing::Test {
  protected:
    void SetUp() override;
    void TearDown() override;

  public:
    std::unique_ptr<L0::DriverHandle> driverHandle;
};

} // namespace ult
} // namespace L0
