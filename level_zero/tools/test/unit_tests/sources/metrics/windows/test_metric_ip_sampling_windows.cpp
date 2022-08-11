/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/metrics/os_metric_ip_sampling.h"

using ::testing::_;
using ::testing::Return;

namespace L0 {
namespace ult {

class MetricIpSamplingWindowsTest : public DeviceFixture,
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

TEST_F(MetricIpSamplingWindowsTest, WhenIpSamplingOsInterfaceIsUsedReturnUnsupported) {

    EXPECT_FALSE(metricIpSamplingOsInterface->isDependencyAvailable());
    EXPECT_FALSE(metricIpSamplingOsInterface->isNReportsAvailable());
    EXPECT_EQ(metricIpSamplingOsInterface->getRequiredBufferSize(0), 0);
    EXPECT_EQ(metricIpSamplingOsInterface->getUnitReportSize(), 0);
    EXPECT_EQ(metricIpSamplingOsInterface->readData(nullptr, nullptr), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    uint32_t dummy;
    EXPECT_EQ(metricIpSamplingOsInterface->startMeasurement(dummy, dummy), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    EXPECT_EQ(metricIpSamplingOsInterface->stopMeasurement(), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

} // namespace ult
} // namespace L0
