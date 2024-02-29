/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_oa.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

using MetricEnumerationProgrammableTest = Test<MetricMultiDeviceFixture>;

TEST_F(MetricEnumerationProgrammableTest, whenProgrammableApisAreCalledUnsupportedErrorIsReturned) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
    metricsConcurrentGroupParams.Description = "OA description";

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.MetricsCount = 1;

    // Metrics Discovery:: metric.
    Mock<IMetric_1_0> metric;
    TMetricParams_1_0 metricParams = {};
    metricParams.SymbolName = "Metric symbol name";
    metricParams.ShortName = "Metric short name";
    metricParams.LongName = "Metric long name";
    metricParams.ResultType = MetricsDiscovery::TMetricResultType::RESULT_UINT64;
    metricParams.MetricType = MetricsDiscovery::TMetricType::METRIC_TYPE_RATIO;

    // One api: metric group handle.
    zet_metric_group_handle_t metricGroupHandle = {};

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;
    metricsSet.GetMetricResult = &metric;

    metric.GetParamsResult = &metricParams;

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);
    // Obtain metric.
    uint32_t metricCount = 0;
    zet_metric_handle_t metricHandle = {};
    EXPECT_EQ(zetMetricGet(metricGroupHandle, &metricCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricCount, 1u);
    EXPECT_EQ(zetMetricGet(metricGroupHandle, &metricCount, &metricHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(metricHandle, nullptr);
    EXPECT_TRUE(static_cast<OaMetricImp *>(metricHandle)->isImmutable());

    EXPECT_EQ(zetMetricGroupAddMetricExp(metricGroupHandle, metricHandle, nullptr, nullptr), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    EXPECT_EQ(zetMetricGroupRemoveMetricExp(metricGroupHandle, metricHandle), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    EXPECT_EQ(zetMetricGroupCloseExp(metricGroupHandle), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    EXPECT_EQ(zetMetricGroupDestroyExp(metricGroupHandle), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

    EXPECT_EQ(zetMetricDestroyExp(metricHandle), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    auto &metricSource = devices[0]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    char name[ZET_MAX_METRIC_GROUP_NAME] = {};
    char description[ZET_MAX_METRIC_GROUP_DESCRIPTION] = {};
    zet_metric_group_sampling_type_flag_t samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;
    zet_metric_group_handle_t *pMetricGroupHandle = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, metricSource.metricGroupCreate(
                                                       name, description, samplingType, pMetricGroupHandle));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, metricSource.metricProgrammableGet(
                                                       &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetMetricProgrammableGetExp(devices[0]->toHandle(), &count, nullptr));
    zet_metric_programmable_exp_handle_t hMetricProgrammable{};
    zet_metric_programmable_exp_properties_t properties{};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetMetricProgrammableGetPropertiesExp(hMetricProgrammable, &properties));
    uint32_t parameterCount{};
    zet_metric_programmable_param_info_exp_t parameterInfo{};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetMetricProgrammableGetParamInfoExp(hMetricProgrammable, &parameterCount, &parameterInfo));
    uint32_t parameterOrdinal{};
    uint32_t valueInfoCount{};
    zet_metric_programmable_param_value_info_exp_t valueInfo{};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetMetricProgrammableGetParamValueInfoExp(hMetricProgrammable, parameterOrdinal, &valueInfoCount, &valueInfo));
    zet_metric_programmable_param_value_exp_t parameterValues{};
    const char metricName[ZET_MAX_METRIC_NAME] = "";
    const char metricDescription[ZET_MAX_METRIC_DESCRIPTION] = "";
    uint32_t metricHandleCount = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetMetricCreateFromProgrammableExp(hMetricProgrammable, &parameterValues, parameterCount, metricName, metricDescription, &metricHandleCount, nullptr));
}

} // namespace ult
} // namespace L0
