/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/metrics/os_interface_metric.h"

namespace L0 {
namespace ult {

class MetricIpSamplingLinuxTest : public DeviceFixture,
                                  public ::testing::Test {
  public:
    void SetUp() override {
        DeviceFixture::setUp();
        metricIpSamplingOsInterface = MetricIpSamplingOsInterface::create(static_cast<L0::Device &>(*device));
    }

    void TearDown() override {
        DeviceFixture::tearDown();
    }
    std::unique_ptr<MetricIpSamplingOsInterface> metricIpSamplingOsInterface = nullptr;
};

HWTEST2_F(MetricIpSamplingLinuxTest, GivenUnsupportedProductFamilyIsUsedWhenIsDependencyAvailableIsCalledThenReturnFailure, IsXeHpgCore) {

    EXPECT_FALSE(metricIpSamplingOsInterface->isDependencyAvailable());
}

} // namespace ult
} // namespace L0
