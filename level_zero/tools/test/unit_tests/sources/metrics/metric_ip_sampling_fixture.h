/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include <level_zero/zet_api.h>

namespace L0 {
namespace ult {
class MockMetricIpSamplingOsInterface;
class MetricIpSamplingFixture : public MultiDeviceFixture,
                                public ::testing::Test {
  public:
    void SetUp() override;
    void TearDown() override;

    std::vector<MockMetricIpSamplingOsInterface *> osInterfaceVector = {};
    std::vector<L0::Device *> testDevices = {};
};

} // namespace ult
} // namespace L0
