/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace L0 {
namespace ult {

using MetricTracerTest = Test<MetricDeviceFixture>;

TEST_F(MetricTracerTest, givenInvalidMetricGroupTypeWhenZetMetricTracerOpenIsCalledThenReturnsFail) {

    // One api: device handle.
    zet_device_handle_t metricDeviceHandle = device->toHandle();

    // One api: event handle.
    ze_event_handle_t eventHandle = {};

    // One api: tracer handle.
    zet_metric_tracer_handle_t tracerHandle = {};
    zet_metric_tracer_desc_t tracerDesc = {};

    // One api: metric group handle.
    Mock<MetricGroup> metricGroup;
    zet_metric_group_handle_t metricGroupHandle = metricGroup.toHandle();
    zet_metric_group_properties_t metricGroupProperties = {};

    metricGroupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_EVENT_BASED;

    EXPECT_CALL(metricGroup, getProperties(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(metricGroupProperties), Return(ZE_RESULT_SUCCESS)));

    // Metric tracer open.
    EXPECT_EQ(zetMetricTracerOpen(metricDeviceHandle, metricGroupHandle, &tracerDesc, eventHandle, &tracerHandle), ZE_RESULT_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(tracerHandle, nullptr);
}

TEST_F(MetricTracerTest, givenValidArgumentsWhenZetMetricTracerOpenIsCalledThenReturnsSuccess) {

    // One api: device handle.
    zet_device_handle_t metricDeviceHandle = device->toHandle();

    // One api: event handle.
    ze_event_handle_t eventHandle = {};

    // One api: tracer handle.
    zet_metric_tracer_handle_t tracerHandle = {};
    zet_metric_tracer_desc_t tracerDesc = {};

    tracerDesc.version = ZET_METRIC_TRACER_DESC_VERSION_CURRENT;
    tracerDesc.notifyEveryNReports = 32768;
    tracerDesc.samplingPeriod = 1000;

    // One api: metric group handle.
    Mock<MetricGroup> metricGroup;
    zet_metric_group_handle_t metricGroupHandle = metricGroup.toHandle();
    zet_metric_group_properties_t metricGroupProperties = {};

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
    metricsConcurrentGroupParams.Description = "OA description";

    // Metrics Discovery metric set.
    Mock<MetricsDiscovery::IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.RawReportSize = 256;

    EXPECT_CALL(*mockMetricEnumeration, loadMetricsDiscovery())
        .Times(0);

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockOpenMetricsDevice(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&metricsDevice), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockCloseMetricsDevice(_))
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsDevice, GetParams())
        .WillRepeatedly(Return(&metricsDeviceParams));

    EXPECT_CALL(metricsDevice, GetConcurrentGroup(_))
        .Times(1)
        .WillOnce(Return(&metricsConcurrentGroup));

    EXPECT_CALL(metricsConcurrentGroup, GetParams())
        .Times(1)
        .WillRepeatedly(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsConcurrentGroup, OpenIoStream(_, _, _, _))
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsConcurrentGroup, CloseIoStream())
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(metricDeviceHandle, &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(metricDeviceHandle, &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Metric group properties.
    EXPECT_EQ(zetMetricGroupGetProperties(metricGroupHandle, &metricGroupProperties), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupProperties.domain, 0u);
    EXPECT_EQ(metricGroupProperties.samplingType, ZET_METRIC_GROUP_SAMPLING_TYPE_TIME_BASED);
    EXPECT_EQ(metricGroupProperties.metricCount, metricsSetParams.MetricsCount);
    EXPECT_EQ(strcmp(metricGroupProperties.description, metricsSetParams.ShortName), 0);
    EXPECT_EQ(strcmp(metricGroupProperties.name, metricsSetParams.SymbolName), 0);

    // Metric group activation.
    EXPECT_EQ(zetDeviceActivateMetricGroups(metricDeviceHandle, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    // Metric tracer open.
    EXPECT_EQ(zetMetricTracerOpen(metricDeviceHandle, metricGroupHandle, &tracerDesc, eventHandle, &tracerHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(tracerHandle, nullptr);

    // Metric tracer close.
    EXPECT_EQ(zetMetricTracerClose(tracerHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(tracerHandle, nullptr);
}

TEST_F(MetricTracerTest, givenInvalidArgumentsWhenZetMetricTracerReadDataIsCalledThenReturnsFail) {

    // One api: device handle.
    zet_device_handle_t metricDeviceHandle = device->toHandle();

    // One api: event handle.
    ze_event_handle_t eventHandle = {};

    // One api: tracer handle.
    zet_metric_tracer_handle_t tracerHandle = {};
    zet_metric_tracer_desc_t tracerDesc = {};

    tracerDesc.version = ZET_METRIC_TRACER_DESC_VERSION_CURRENT;
    tracerDesc.notifyEveryNReports = 32768;
    tracerDesc.samplingPeriod = 1000;

    // One api: metric group handle.
    Mock<MetricGroup> metricGroup;
    zet_metric_group_handle_t metricGroupHandle = metricGroup.toHandle();
    zet_metric_group_properties_t metricGroupProperties = {};

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
    metricsConcurrentGroupParams.Description = "OA description";

    // Metrics Discovery metric set.
    Mock<MetricsDiscovery::IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.RawReportSize = 256;

    EXPECT_CALL(*mockMetricEnumeration, loadMetricsDiscovery())
        .Times(0);

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockOpenMetricsDevice(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&metricsDevice), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockCloseMetricsDevice(_))
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsDevice, GetParams())
        .WillRepeatedly(Return(&metricsDeviceParams));

    EXPECT_CALL(metricsDevice, GetConcurrentGroup(_))
        .Times(1)
        .WillOnce(Return(&metricsConcurrentGroup));

    EXPECT_CALL(metricsConcurrentGroup, GetParams())
        .Times(1)
        .WillRepeatedly(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsConcurrentGroup, OpenIoStream(_, _, _, _))
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsConcurrentGroup, CloseIoStream())
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(metricDeviceHandle, &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(metricDeviceHandle, &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Metric group properties.
    EXPECT_EQ(zetMetricGroupGetProperties(metricGroupHandle, &metricGroupProperties), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupProperties.domain, 0u);
    EXPECT_EQ(metricGroupProperties.samplingType, ZET_METRIC_GROUP_SAMPLING_TYPE_TIME_BASED);
    EXPECT_EQ(metricGroupProperties.metricCount, metricsSetParams.MetricsCount);
    EXPECT_EQ(strcmp(metricGroupProperties.description, metricsSetParams.ShortName), 0);
    EXPECT_EQ(strcmp(metricGroupProperties.name, metricsSetParams.SymbolName), 0);

    // Metric group activation.
    EXPECT_EQ(zetDeviceActivateMetricGroups(metricDeviceHandle, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    // Metric tracer open.
    EXPECT_EQ(zetMetricTracerOpen(metricDeviceHandle, metricGroupHandle, &tracerDesc, eventHandle, &tracerHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(tracerHandle, nullptr);

    // Metric tracer close.
    EXPECT_EQ(zetMetricTracerClose(tracerHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricTracerTest, givenValidArgumentsWhenZetMetricTracerReadDataIsCalledThenReturnsSuccess) {

    // One api: device handle.
    zet_device_handle_t metricDeviceHandle = device->toHandle();

    // One api: event handle.
    ze_event_handle_t eventHandle = {};

    // One api: tracer handle.
    zet_metric_tracer_handle_t tracerHandle = {};
    zet_metric_tracer_desc_t tracerDesc = {};

    tracerDesc.version = ZET_METRIC_TRACER_DESC_VERSION_CURRENT;
    tracerDesc.notifyEveryNReports = 32768;
    tracerDesc.samplingPeriod = 1000;

    // One api: metric group handle.
    Mock<MetricGroup> metricGroup;
    zet_metric_group_handle_t metricGroupHandle = metricGroup.toHandle();
    zet_metric_group_properties_t metricGroupProperties = {};

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
    metricsConcurrentGroupParams.Description = "OA description";

    // Metrics Discovery metric set.
    Mock<MetricsDiscovery::IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.RawReportSize = 256;

    EXPECT_CALL(*mockMetricEnumeration, loadMetricsDiscovery())
        .Times(0);

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockOpenMetricsDevice(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&metricsDevice), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockCloseMetricsDevice(_))
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsDevice, GetParams())
        .WillRepeatedly(Return(&metricsDeviceParams));

    EXPECT_CALL(metricsDevice, GetConcurrentGroup(_))
        .Times(1)
        .WillOnce(Return(&metricsConcurrentGroup));

    EXPECT_CALL(metricsConcurrentGroup, GetParams())
        .Times(1)
        .WillRepeatedly(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsConcurrentGroup, OpenIoStream(_, _, _, _))
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsConcurrentGroup, ReadIoStream(_, _, _))
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsConcurrentGroup, CloseIoStream())
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(metricDeviceHandle, &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(metricDeviceHandle, &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Metric group properties.
    EXPECT_EQ(zetMetricGroupGetProperties(metricGroupHandle, &metricGroupProperties), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupProperties.domain, 0u);
    EXPECT_EQ(metricGroupProperties.samplingType, ZET_METRIC_GROUP_SAMPLING_TYPE_TIME_BASED);
    EXPECT_EQ(metricGroupProperties.metricCount, metricsSetParams.MetricsCount);
    EXPECT_EQ(strcmp(metricGroupProperties.description, metricsSetParams.ShortName), 0);
    EXPECT_EQ(strcmp(metricGroupProperties.name, metricsSetParams.SymbolName), 0);

    // Metric group activation.
    EXPECT_EQ(zetDeviceActivateMetricGroups(metricDeviceHandle, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    // Metric tracer open.
    EXPECT_EQ(zetMetricTracerOpen(metricDeviceHandle, metricGroupHandle, &tracerDesc, eventHandle, &tracerHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(tracerHandle, nullptr);

    // Metric tracer: get desired raw data size.
    size_t rawSize = 0;
    uint32_t reportCount = 256;
    EXPECT_EQ(zetMetricTracerReadData(tracerHandle, reportCount, &rawSize, nullptr), ZE_RESULT_SUCCESS);

    // Metric tracer: read the data.
    std::vector<uint8_t> rawData;
    rawData.resize(rawSize);
    EXPECT_EQ(zetMetricTracerReadData(tracerHandle, reportCount, &rawSize, rawData.data()), ZE_RESULT_SUCCESS);

    // Metric tracer close.
    EXPECT_EQ(zetMetricTracerClose(tracerHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricTracerTest, givenInvalidArgumentsWhenZetCommandListAppendMetricTracerMarkerIsCalledThenReturnsFail) {

    // One api: device handle.
    zet_device_handle_t metricDeviceHandle = device->toHandle();

    // One api: event handle.
    ze_event_handle_t eventHandle = {};

    // One api: tracer handle.
    zet_metric_tracer_handle_t tracerHandle = {};
    zet_metric_tracer_desc_t tracerDesc = {};

    tracerDesc.version = ZET_METRIC_TRACER_DESC_VERSION_CURRENT;
    tracerDesc.notifyEveryNReports = 32768;
    tracerDesc.samplingPeriod = 1000;

    // One api: metric group handle.
    Mock<MetricGroup> metricGroup;
    zet_metric_group_handle_t metricGroupHandle = metricGroup.toHandle();
    zet_metric_group_properties_t metricGroupProperties = {};

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
    metricsConcurrentGroupParams.Description = "OA description";

    // Metrics Discovery metric set.
    Mock<MetricsDiscovery::IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.RawReportSize = 256;

    EXPECT_CALL(*mockMetricEnumeration, loadMetricsDiscovery())
        .Times(0);

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockOpenMetricsDevice(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&metricsDevice), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockCloseMetricsDevice(_))
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsDevice, GetParams())
        .WillRepeatedly(Return(&metricsDeviceParams));

    EXPECT_CALL(metricsDevice, GetConcurrentGroup(_))
        .Times(1)
        .WillOnce(Return(&metricsConcurrentGroup));

    EXPECT_CALL(metricsConcurrentGroup, GetParams())
        .Times(1)
        .WillRepeatedly(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsConcurrentGroup, OpenIoStream(_, _, _, _))
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsConcurrentGroup, CloseIoStream())
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(metricDeviceHandle, &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(metricDeviceHandle, &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Metric group properties.
    EXPECT_EQ(zetMetricGroupGetProperties(metricGroupHandle, &metricGroupProperties), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupProperties.domain, 0u);
    EXPECT_EQ(metricGroupProperties.samplingType, ZET_METRIC_GROUP_SAMPLING_TYPE_TIME_BASED);
    EXPECT_EQ(metricGroupProperties.metricCount, metricsSetParams.MetricsCount);
    EXPECT_EQ(strcmp(metricGroupProperties.description, metricsSetParams.ShortName), 0);
    EXPECT_EQ(strcmp(metricGroupProperties.name, metricsSetParams.SymbolName), 0);

    // Metric group activation.
    EXPECT_EQ(zetDeviceActivateMetricGroups(metricDeviceHandle, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    // Metric tracer open.
    EXPECT_EQ(zetMetricTracerOpen(metricDeviceHandle, metricGroupHandle, &tracerDesc, eventHandle, &tracerHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(tracerHandle, nullptr);

    // Metric tracer close.
    EXPECT_EQ(zetMetricTracerClose(tracerHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricTracerTest, givenValidArgumentsWhenZetCommandListAppendMetricTracerMarkerIsCalledThenReturnsSuccess) {

    // One api: device handle.
    zet_device_handle_t metricDeviceHandle = device->toHandle();

    // One api: event handle.
    ze_event_handle_t eventHandle = {};

    // One api: command list handle
    MockCommandList commandList;

    // One api: tracer handle.
    zet_metric_tracer_handle_t tracerHandle = {};
    zet_metric_tracer_desc_t tracerDesc = {};

    tracerDesc.version = ZET_METRIC_TRACER_DESC_VERSION_CURRENT;
    tracerDesc.notifyEveryNReports = 32768;
    tracerDesc.samplingPeriod = 1000;

    // One api: metric group handle.
    Mock<MetricGroup> metricGroup;
    zet_metric_group_handle_t metricGroupHandle = metricGroup.toHandle();
    zet_metric_group_properties_t metricGroupProperties = {};

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
    metricsConcurrentGroupParams.Description = "OA description";

    // Metrics Discovery metric set.
    Mock<MetricsDiscovery::IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.RawReportSize = 256;

    EXPECT_CALL(*mockMetricEnumeration, loadMetricsDiscovery())
        .Times(0);

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockOpenMetricsDevice(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&metricsDevice), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockCloseMetricsDevice(_))
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsDevice, GetParams())
        .WillRepeatedly(Return(&metricsDeviceParams));

    EXPECT_CALL(metricsDevice, GetConcurrentGroup(_))
        .Times(1)
        .WillOnce(Return(&metricsConcurrentGroup));

    EXPECT_CALL(metricsConcurrentGroup, GetParams())
        .Times(1)
        .WillRepeatedly(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsConcurrentGroup, OpenIoStream(_, _, _, _))
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsConcurrentGroup, CloseIoStream())
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(metricDeviceHandle, &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(metricDeviceHandle, &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Metric group properties.
    EXPECT_EQ(zetMetricGroupGetProperties(metricGroupHandle, &metricGroupProperties), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupProperties.domain, 0u);
    EXPECT_EQ(metricGroupProperties.samplingType, ZET_METRIC_GROUP_SAMPLING_TYPE_TIME_BASED);
    EXPECT_EQ(metricGroupProperties.metricCount, metricsSetParams.MetricsCount);
    EXPECT_EQ(strcmp(metricGroupProperties.description, metricsSetParams.ShortName), 0);
    EXPECT_EQ(strcmp(metricGroupProperties.name, metricsSetParams.SymbolName), 0);

    // Metric group activation.
    EXPECT_EQ(zetDeviceActivateMetricGroups(metricDeviceHandle, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    // Metric tracer open.
    EXPECT_EQ(zetMetricTracerOpen(metricDeviceHandle, metricGroupHandle, &tracerDesc, eventHandle, &tracerHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(tracerHandle, nullptr);

    // Metric tracer close.
    EXPECT_EQ(zetMetricTracerClose(tracerHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricTracerTest, givenMultipleMarkerInsertionsWhenZetCommandListAppendMetricTracerMarkerIsCalledThenReturnsSuccess) {

    // One api: device handle.
    zet_device_handle_t metricDeviceHandle = device->toHandle();

    // One api: event handle.
    ze_event_handle_t eventHandle = {};

    // One api: command list handle
    MockCommandList commandList;

    // One api: tracer handle.
    zet_metric_tracer_handle_t tracerHandle = {};
    zet_metric_tracer_desc_t tracerDesc = {};

    tracerDesc.version = ZET_METRIC_TRACER_DESC_VERSION_CURRENT;
    tracerDesc.notifyEveryNReports = 32768;
    tracerDesc.samplingPeriod = 1000;

    // One api: metric group handle.
    Mock<MetricGroup> metricGroup;
    zet_metric_group_handle_t metricGroupHandle = metricGroup.toHandle();
    zet_metric_group_properties_t metricGroupProperties = {};

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
    metricsConcurrentGroupParams.Description = "OA description";

    // Metrics Discovery metric set.
    Mock<MetricsDiscovery::IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.RawReportSize = 256;

    EXPECT_CALL(*mockMetricEnumeration, loadMetricsDiscovery())
        .Times(0);

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockOpenMetricsDevice(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&metricsDevice), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockCloseMetricsDevice(_))
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsDevice, GetParams())
        .WillRepeatedly(Return(&metricsDeviceParams));

    EXPECT_CALL(metricsDevice, GetConcurrentGroup(_))
        .Times(1)
        .WillOnce(Return(&metricsConcurrentGroup));

    EXPECT_CALL(metricsConcurrentGroup, GetParams())
        .Times(1)
        .WillRepeatedly(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsConcurrentGroup, OpenIoStream(_, _, _, _))
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsConcurrentGroup, CloseIoStream())
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(metricDeviceHandle, &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(metricDeviceHandle, &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Metric group properties.
    EXPECT_EQ(zetMetricGroupGetProperties(metricGroupHandle, &metricGroupProperties), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupProperties.domain, 0u);
    EXPECT_EQ(metricGroupProperties.samplingType, ZET_METRIC_GROUP_SAMPLING_TYPE_TIME_BASED);
    EXPECT_EQ(metricGroupProperties.metricCount, metricsSetParams.MetricsCount);
    EXPECT_EQ(strcmp(metricGroupProperties.description, metricsSetParams.ShortName), 0);
    EXPECT_EQ(strcmp(metricGroupProperties.name, metricsSetParams.SymbolName), 0);

    // Metric group activation.
    EXPECT_EQ(zetDeviceActivateMetricGroups(metricDeviceHandle, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    // Metric tracer open.
    EXPECT_EQ(zetMetricTracerOpen(metricDeviceHandle, metricGroupHandle, &tracerDesc, eventHandle, &tracerHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(tracerHandle, nullptr);

    // Metric tracer close.
    EXPECT_EQ(zetMetricTracerClose(tracerHandle), ZE_RESULT_SUCCESS);
}

} // namespace ult
} // namespace L0
