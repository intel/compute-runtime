/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_oa.h"
#include "level_zero/zet_intel_gpu_metric.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {
using MetricGroupUserDefinedTest = Test<MetricContextFixture>;

TEST_F(MetricGroupUserDefinedTest, givenImmutableMetricGroupAndMetricsWhenUsingApisForProgrammableMetricsThenErrorIsReturned) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup;
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
    metricsConcurrentGroupParams.Description = "OA description";
    metricsConcurrentGroupParams.IoMeasurementInformationCount = 1;

    Mock<MetricsDiscovery::IEquation_1_0> ioReadEquation;
    MetricsDiscovery::TEquationElement_1_0 ioEquationElement = {};
    ioEquationElement.Type = MetricsDiscovery::EQUATION_ELEM_IMM_UINT64;
    ioEquationElement.ImmediateUInt64 = 0;

    ioReadEquation.getEquationElement.push_back(&ioEquationElement);

    Mock<MetricsDiscovery::IInformation_1_0> ioMeasurement;
    MetricsDiscovery::TInformationParams_1_0 oaInformation = {};
    oaInformation.SymbolName = "BufferOverflow";
    oaInformation.IoReadEquation = &ioReadEquation;

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.MetricsCount = 1;

    // Metrics Discovery:: metric.
    Mock<IMetric_1_13> metric;
    TMetricParams_1_13 metricParams = {};
    metricParams.SymbolName = "Metric symbol name";
    metricParams.ShortName = "Metric short name";
    metricParams.LongName = "Metric long name";
    metricParams.ResultType = MetricsDiscovery::TMetricResultType::RESULT_UINT64;
    metricParams.MetricType = MetricsDiscovery::TMetricType::METRIC_TYPE_DURATION;

    // One api: metric group handle.
    zet_metric_group_handle_t metricGroupHandle = {};

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);
    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;
    metricsConcurrentGroup.GetIoMeasurementInformationResult = &ioMeasurement;
    ioMeasurement.GetParamsResult = &oaInformation;

    metricsSet.GetParamsResult = &metricsSetParams;
    metricsSet.GetMetricResult = &metric;

    metric.GetParamsResult = &metricParams;

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Obtain metric.
    uint32_t metricCount = 0;
    zet_metric_handle_t metricHandle = {};
    EXPECT_EQ(zetMetricGet(metricGroupHandle, &metricCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricCount, 1u);
    EXPECT_EQ(zetMetricGet(metricGroupHandle, &metricCount, &metricHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(metricHandle, nullptr);

    size_t errorStringSize = 0;
    EXPECT_EQ(zetMetricGroupAddMetricExp(metricGroupHandle, metricHandle, &errorStringSize, nullptr), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    EXPECT_EQ(zetMetricGroupRemoveMetricExp(metricGroupHandle, metricHandle), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    EXPECT_EQ(zetMetricGroupCloseExp(metricGroupHandle), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    EXPECT_EQ(zetMetricGroupDestroyExp(metricGroupHandle), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    EXPECT_EQ(zetMetricDestroyExp(metricHandle), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    EXPECT_EQ(1u, metric.GetParamsCalled);
}

} // namespace ult
} // namespace L0
