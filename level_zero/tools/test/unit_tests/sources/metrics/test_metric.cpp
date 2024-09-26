/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test_base.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_source.h"
#include <level_zero/zet_api.h>

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

TEST(MultidevMetric, GivenMultideviceMetricCreatedThenReferenceIsUpdatedSuccessfully) {
    MockMetricSource metricSource{};
    MockMetric mockMetric(metricSource);

    std::vector<MetricImp *> subDeviceMetrics;
    subDeviceMetrics.push_back(&mockMetric);

    MultiDeviceMetricImp *multiDevMetric = MultiDeviceMetricImp::create(metricSource, subDeviceMetrics);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, multiDevMetric->destroy());

    zet_metric_properties_t properties;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, multiDevMetric->getProperties(&properties));

    MetricImp *metric = multiDevMetric->getMetricAtSubDeviceIndex(0);
    EXPECT_EQ(metric, &mockMetric);
    MultiDeviceMetricImp *multiDevMetricGet = mockMetric.getRootDevMetric();
    EXPECT_EQ(multiDevMetricGet, multiDevMetric);

    // asking for an index beyond the size of submetrics array should be handled
    metric = multiDevMetric->getMetricAtSubDeviceIndex(2);
    EXPECT_EQ(metric, nullptr);

    delete multiDevMetric;
}

} // namespace ult
} // namespace L0
