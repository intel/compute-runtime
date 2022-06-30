/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/metrics/os_metric_ip_sampling.h"

namespace L0 {
namespace ult {

class MetricIpSamplingLinuxTest : public DeviceFixture,
                                  public ::testing::Test {
  public:
    void SetUp() override {
        DeviceFixture::SetUp();
        metricIpSamplingOsInterface = MetricIpSamplingOsInterface::create(static_cast<L0::Device &>(*device));
    }

    void TearDown() override {
        DeviceFixture::TearDown();
    }
    std::unique_ptr<MetricIpSamplingOsInterface> metricIpSamplingOsInterface = nullptr;
};

HWTEST2_F(MetricIpSamplingLinuxTest, GivenUnsupportedProductFamilyIsUsedWhenIsDependencyAvailableIsCalledThenReturnFailure, IsNotXeHpcCore) {

    auto hwInfo = neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->platform.eProductFamily = productFamily;
    EXPECT_FALSE(metricIpSamplingOsInterface->isDependencyAvailable());
}

} // namespace ult
} // namespace L0
