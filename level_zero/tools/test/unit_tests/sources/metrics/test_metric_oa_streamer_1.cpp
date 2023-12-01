/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/test_macros/test_base.h"

#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_oa.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

using MetricStreamerTest = Test<MetricContextFixture>;

TEST_F(MetricStreamerTest, givenInvalidMetricGroupTypeWhenZetMetricStreamerOpenIsCalledThenReturnsFail) {

    // One api: device handle.
    zet_device_handle_t metricDeviceHandle = device->toHandle();

    // One api: event handle.
    ze_event_handle_t eventHandle = {};

    // One api: streamer handle.
    zet_metric_streamer_handle_t streamerHandle = {};
    zet_metric_streamer_desc_t streamerDesc = {};

    // One api: metric group handle.
    Mock<MetricGroup> metricGroup;
    zet_metric_group_handle_t metricGroupHandle = metricGroup.toHandle();
    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};

    metricGroupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;

    metricGroup.getPropertiesOutProperties = &metricGroupProperties;

    // Metric streamer open.
    EXPECT_EQ(zetMetricStreamerOpen(context->toHandle(), metricDeviceHandle, metricGroupHandle, &streamerDesc, eventHandle, &streamerHandle), ZE_RESULT_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(streamerHandle, nullptr);
}

TEST_F(MetricStreamerTest, givenValidArgumentsWhenZetMetricStreamerOpenIsCalledThenReturnsSuccess) {

    zet_device_handle_t metricDeviceHandle = device->toHandle();

    ze_event_handle_t eventHandle = {};

    zet_metric_streamer_handle_t streamerHandle = {};
    zet_metric_streamer_desc_t streamerDesc = {};

    streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
    streamerDesc.notifyEveryNReports = 32768;
    streamerDesc.samplingPeriod = 1000;

    Mock<MetricGroup> metricGroup;
    zet_metric_group_handle_t metricGroupHandle = metricGroup.toHandle();
    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};

    metricsDeviceParams.ConcurrentGroupsCount = 1;

    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
    metricsConcurrentGroupParams.Description = "OA description";

    Mock<MetricsDiscovery::IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.RawReportSize = 256;

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;

    mockMetricsLibrary->initializationState = ZE_RESULT_SUCCESS;

    auto &metricSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    EXPECT_TRUE(metricSource.loadDependencies());
    EXPECT_TRUE(metricSource.isInitialized());

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(metricDeviceHandle, &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    EXPECT_EQ(zetMetricGroupGet(metricDeviceHandle, &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    EXPECT_EQ(zetMetricGroupGetProperties(metricGroupHandle, &metricGroupProperties), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupProperties.domain, 0u);
    EXPECT_EQ(metricGroupProperties.samplingType, ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
    EXPECT_EQ(metricGroupProperties.metricCount, metricsSetParams.MetricsCount);
    EXPECT_EQ(strcmp(metricGroupProperties.description, metricsSetParams.ShortName), 0);
    EXPECT_EQ(strcmp(metricGroupProperties.name, metricsSetParams.SymbolName), 0);

    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), metricDeviceHandle, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    EXPECT_EQ(zetMetricStreamerOpen(context->toHandle(), metricDeviceHandle, metricGroupHandle, &streamerDesc, eventHandle, &streamerHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(streamerHandle, nullptr);

    EXPECT_EQ(zetMetricStreamerClose(streamerHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(streamerHandle, nullptr);
}

TEST_F(MetricStreamerTest, givenRawReportSizeAsZeroWhenZetMetricStreamerOpenIsCalledThenReturnsSuccess) {

    zet_device_handle_t metricDeviceHandle = device->toHandle();

    ze_event_handle_t eventHandle = {};

    zet_metric_streamer_handle_t streamerHandle = {};
    zet_metric_streamer_desc_t streamerDesc = {};

    streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
    streamerDesc.notifyEveryNReports = 32768;
    streamerDesc.samplingPeriod = 1000;

    Mock<MetricGroup> metricGroup;
    zet_metric_group_handle_t metricGroupHandle = metricGroup.toHandle();
    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};

    metricsDeviceParams.ConcurrentGroupsCount = 1;

    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
    metricsConcurrentGroupParams.Description = "OA description";

    Mock<MetricsDiscovery::IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.RawReportSize = 0;

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;

    mockMetricsLibrary->initializationState = ZE_RESULT_SUCCESS;

    auto &metricSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    EXPECT_TRUE(metricSource.loadDependencies());
    EXPECT_TRUE(metricSource.isInitialized());

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(metricDeviceHandle, &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    EXPECT_EQ(zetMetricGroupGet(metricDeviceHandle, &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    EXPECT_EQ(zetMetricGroupGetProperties(metricGroupHandle, &metricGroupProperties), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupProperties.domain, 0u);
    EXPECT_EQ(metricGroupProperties.samplingType, ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
    EXPECT_EQ(metricGroupProperties.metricCount, metricsSetParams.MetricsCount);
    EXPECT_EQ(strcmp(metricGroupProperties.description, metricsSetParams.ShortName), 0);
    EXPECT_EQ(strcmp(metricGroupProperties.name, metricsSetParams.SymbolName), 0);

    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), metricDeviceHandle, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    EXPECT_EQ(zetMetricStreamerOpen(context->toHandle(), metricDeviceHandle, metricGroupHandle, &streamerDesc, eventHandle, &streamerHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(streamerHandle, nullptr);

    EXPECT_EQ(zetMetricStreamerClose(streamerHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(streamerHandle, nullptr);
}

TEST_F(MetricStreamerTest, givenValidArgumentsAndMetricGroupsIsNotActivatedWhenZetMetricStreamerOpenIsCalledThenReturnsNotReady) {

    zet_device_handle_t metricDeviceHandle = device->toHandle();

    ze_event_handle_t eventHandle = {};

    zet_metric_streamer_handle_t streamerHandle = {};
    zet_metric_streamer_desc_t streamerDesc = {};

    streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
    streamerDesc.notifyEveryNReports = 32768;
    streamerDesc.samplingPeriod = 1000;

    Mock<MetricGroup> metricGroup;
    zet_metric_group_handle_t metricGroupHandle = metricGroup.toHandle();

    metricsDeviceParams.ConcurrentGroupsCount = 1;

    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
    metricsConcurrentGroupParams.Description = "OA description";

    Mock<MetricsDiscovery::IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.RawReportSize = 256;

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(metricDeviceHandle, &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);

    EXPECT_EQ(zetMetricGroupGet(metricDeviceHandle, &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);

    EXPECT_EQ(zetMetricStreamerOpen(context->toHandle(), metricDeviceHandle, metricGroupHandle, &streamerDesc, eventHandle, &streamerHandle), ZE_RESULT_NOT_READY);
    EXPECT_EQ(streamerHandle, nullptr);
}

TEST_F(MetricStreamerTest, givenValidArgumentsWhenZetMetricStreamerOpenIsCalledTwiceThenReturnsObjectInUse) {

    // One api: device handle.
    zet_device_handle_t metricDeviceHandle = device->toHandle();

    // One api: event handle.
    ze_event_handle_t eventHandle = {};

    // One api: streamer handle.
    zet_metric_streamer_handle_t firstStreamerHandle = {};
    zet_metric_streamer_handle_t secondStreamerHandle = {};
    zet_metric_streamer_desc_t streamerDesc = {};

    streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
    streamerDesc.notifyEveryNReports = 32768;
    streamerDesc.samplingPeriod = 1000;

    // One api: metric group handle.
    Mock<MetricGroup> metricGroup;
    zet_metric_group_handle_t metricGroupHandle = metricGroup.toHandle();
    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};

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

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;

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
    EXPECT_EQ(metricGroupProperties.samplingType, ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
    EXPECT_EQ(metricGroupProperties.metricCount, metricsSetParams.MetricsCount);
    EXPECT_EQ(strcmp(metricGroupProperties.description, metricsSetParams.ShortName), 0);
    EXPECT_EQ(strcmp(metricGroupProperties.name, metricsSetParams.SymbolName), 0);

    // Metric group activation.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), metricDeviceHandle, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    // Metric streamer open.
    EXPECT_EQ(zetMetricStreamerOpen(context->toHandle(), metricDeviceHandle, metricGroupHandle, &streamerDesc, eventHandle, &firstStreamerHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(firstStreamerHandle, nullptr);

    EXPECT_EQ(zetMetricStreamerOpen(context->toHandle(), metricDeviceHandle, metricGroupHandle, &streamerDesc, eventHandle, &secondStreamerHandle), ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE);
    EXPECT_EQ(secondStreamerHandle, nullptr);

    // Metric streamer close.
    EXPECT_EQ(zetMetricStreamerClose(firstStreamerHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(firstStreamerHandle, nullptr);
}

TEST_F(MetricStreamerTest, givenCorrectArgumentsWhenZetMetricQueryPoolCreateExtIsCalledThenMetricStreamerIsNotAvailable) {

    // One api: device handle.
    zet_device_handle_t metricDevice = device->toHandle();

    // One api: event handle.
    ze_event_handle_t eventHandle = {};

    // One api: streamer handle.
    zet_metric_streamer_handle_t streamerHandle = {};
    zet_metric_streamer_desc_t streamerDesc = {};

    streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
    streamerDesc.notifyEveryNReports = 32768;
    streamerDesc.samplingPeriod = 1000;

    // One api: metric group handle.
    Mock<MetricGroup> metricGroup;
    zet_metric_group_handle_t metricGroupHandle = metricGroup.toHandle();
    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
    metricGroupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;

    // One api: query pool handle.
    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};
    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE;

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;

    QueryHandle_1_0 queryHandle = {&value};
    ContextHandle_1_0 contextHandle = {&value};

    metricGroup.getPropertiesOutProperties = &metricGroupProperties;

    mockMetricsLibrary->g_mockApi->contextCreateOutHandle = contextHandle;
    mockMetricsLibrary->g_mockApi->queryCreateOutHandle = queryHandle;
    mockMetricsLibrary->g_mockApi->getParameterOutValue = value;

    // Metric query pool create.
    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroup.toHandle(), &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(poolHandle, nullptr);

    // Metric streamer open.
    EXPECT_EQ(zetMetricStreamerOpen(context->toHandle(), metricDevice, metricGroupHandle, &streamerDesc, eventHandle, &streamerHandle), ZE_RESULT_ERROR_NOT_AVAILABLE);
    EXPECT_EQ(streamerHandle, nullptr);

    // Metric query pool destroy.
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricStreamerTest, givenInvalidArgumentsWhenZetMetricStreamerReadDataIsCalledThenReturnsFail) {

    // One api: device handle.
    zet_device_handle_t metricDeviceHandle = device->toHandle();

    // One api: event handle.
    ze_event_handle_t eventHandle = {};

    // One api: streamer handle.
    zet_metric_streamer_handle_t streamerHandle = {};
    zet_metric_streamer_desc_t streamerDesc = {};

    streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
    streamerDesc.notifyEveryNReports = 32768;
    streamerDesc.samplingPeriod = 1000;

    // One api: metric group handle.
    Mock<MetricGroup> metricGroup;
    zet_metric_group_handle_t metricGroupHandle = metricGroup.toHandle();
    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};

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

    openMetricsAdapter();
    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;

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
    EXPECT_EQ(metricGroupProperties.samplingType, ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
    EXPECT_EQ(metricGroupProperties.metricCount, metricsSetParams.MetricsCount);
    EXPECT_EQ(strcmp(metricGroupProperties.description, metricsSetParams.ShortName), 0);
    EXPECT_EQ(strcmp(metricGroupProperties.name, metricsSetParams.SymbolName), 0);

    // Metric group activation.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), metricDeviceHandle, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    // Metric streamer open.
    EXPECT_EQ(zetMetricStreamerOpen(context->toHandle(), metricDeviceHandle, metricGroupHandle, &streamerDesc, eventHandle, &streamerHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(streamerHandle, nullptr);

    // Metric streamer close.
    EXPECT_EQ(zetMetricStreamerClose(streamerHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricStreamerTest, givenValidArgumentsWhenZetMetricStreamerReadDataIsCalledThenReturnsSuccess) {

    // One api: device handle.
    zet_device_handle_t metricDeviceHandle = device->toHandle();

    // One api: event handle.
    ze_event_handle_t eventHandle = {};

    // One api: streamer handle.
    zet_metric_streamer_handle_t streamerHandle = {};
    zet_metric_streamer_desc_t streamerDesc = {};

    streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
    streamerDesc.notifyEveryNReports = 32768;
    streamerDesc.samplingPeriod = 1000;

    // One api: metric group handle.
    Mock<MetricGroup> metricGroup;
    zet_metric_group_handle_t metricGroupHandle = metricGroup.toHandle();
    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};

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

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;

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
    EXPECT_EQ(metricGroupProperties.samplingType, ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
    EXPECT_EQ(metricGroupProperties.metricCount, metricsSetParams.MetricsCount);
    EXPECT_EQ(strcmp(metricGroupProperties.description, metricsSetParams.ShortName), 0);
    EXPECT_EQ(strcmp(metricGroupProperties.name, metricsSetParams.SymbolName), 0);

    // Metric group activation.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), metricDeviceHandle, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    // Metric streamer open.
    EXPECT_EQ(zetMetricStreamerOpen(context->toHandle(), metricDeviceHandle, metricGroupHandle, &streamerDesc, eventHandle, &streamerHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(streamerHandle, nullptr);

    // Metric streamer: get desired raw data size.
    size_t rawSize = 0;
    uint32_t reportCount = 256;
    EXPECT_EQ(zetMetricStreamerReadData(streamerHandle, reportCount, &rawSize, nullptr), ZE_RESULT_SUCCESS);

    // Metric streamer: read the data.
    std::vector<uint8_t> rawData;
    rawData.resize(rawSize);
    EXPECT_EQ(zetMetricStreamerReadData(streamerHandle, reportCount, &rawSize, rawData.data()), ZE_RESULT_SUCCESS);

    // Metric streamer close.
    EXPECT_EQ(zetMetricStreamerClose(streamerHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricStreamerTest, givenValidArgumentsWhenZetMetricStreamerReadDataIsCalledWithMaxReportCountOverTheSupportedThenReturnsSuccess) {

    zet_device_handle_t metricDeviceHandle = device->toHandle();
    ze_event_handle_t eventHandle = {};
    zet_metric_streamer_handle_t streamerHandle = {};
    zet_metric_streamer_desc_t streamerDesc = {};

    streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
    streamerDesc.notifyEveryNReports = 32768;
    streamerDesc.samplingPeriod = 1000;
    Mock<MetricGroup> metricGroup;
    zet_metric_group_handle_t metricGroupHandle = metricGroup.toHandle();
    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
    metricsDeviceParams.ConcurrentGroupsCount = 1;
    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
    metricsConcurrentGroupParams.Description = "OA description";

    Mock<MetricsDiscovery::IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.RawReportSize = 256;

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(metricDeviceHandle, &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    EXPECT_EQ(zetMetricGroupGet(metricDeviceHandle, &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    EXPECT_EQ(zetMetricGroupGetProperties(metricGroupHandle, &metricGroupProperties), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupProperties.domain, 0u);
    EXPECT_EQ(metricGroupProperties.samplingType, ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
    EXPECT_EQ(metricGroupProperties.metricCount, metricsSetParams.MetricsCount);
    EXPECT_EQ(strcmp(metricGroupProperties.description, metricsSetParams.ShortName), 0);
    EXPECT_EQ(strcmp(metricGroupProperties.name, metricsSetParams.SymbolName), 0);

    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), metricDeviceHandle, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    EXPECT_EQ(zetMetricStreamerOpen(context->toHandle(), metricDeviceHandle, metricGroupHandle, &streamerDesc, eventHandle, &streamerHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(streamerHandle, nullptr);

    size_t rawSize = 0;
    uint32_t reportCount = 65537;
    EXPECT_EQ(zetMetricStreamerReadData(streamerHandle, reportCount, &rawSize, nullptr), ZE_RESULT_SUCCESS);

    std::vector<uint8_t> rawData;
    rawData.resize(rawSize);
    EXPECT_EQ(zetMetricStreamerReadData(streamerHandle, reportCount, &rawSize, rawData.data()), ZE_RESULT_SUCCESS);

    EXPECT_EQ(zetMetricStreamerClose(streamerHandle), ZE_RESULT_SUCCESS);

    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), metricDeviceHandle, 0, nullptr), ZE_RESULT_SUCCESS);
}

TEST_F(MetricStreamerTest, givenInvalidArgumentsWhenZetCommandListAppendMetricStreamerMarkerIsCalledThenReturnsFail) {

    // One api: device handle.
    zet_device_handle_t metricDeviceHandle = device->toHandle();

    // One api: event handle.
    ze_event_handle_t eventHandle = {};

    // One api: streamer handle.
    zet_metric_streamer_handle_t streamerHandle = {};
    zet_metric_streamer_desc_t streamerDesc = {};

    streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
    streamerDesc.notifyEveryNReports = 32768;
    streamerDesc.samplingPeriod = 1000;

    // One api: metric group handle.
    Mock<MetricGroup> metricGroup;
    zet_metric_group_handle_t metricGroupHandle = metricGroup.toHandle();
    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};

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

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;

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
    EXPECT_EQ(metricGroupProperties.samplingType, ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
    EXPECT_EQ(metricGroupProperties.metricCount, metricsSetParams.MetricsCount);
    EXPECT_EQ(strcmp(metricGroupProperties.description, metricsSetParams.ShortName), 0);
    EXPECT_EQ(strcmp(metricGroupProperties.name, metricsSetParams.SymbolName), 0);

    // Metric group activation.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), metricDeviceHandle, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    // Metric streamer open.
    EXPECT_EQ(zetMetricStreamerOpen(context->toHandle(), metricDeviceHandle, metricGroupHandle, &streamerDesc, eventHandle, &streamerHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(streamerHandle, nullptr);

    // Metric streamer close.
    EXPECT_EQ(zetMetricStreamerClose(streamerHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricStreamerTest, givenValidArgumentsWhenZetCommandListAppendMetricStreamerMarkerIsCalledThenReturnsSuccess) {

    // One api: device handle.
    zet_device_handle_t metricDeviceHandle = device->toHandle();

    // One api: event handle.
    ze_event_handle_t eventHandle = {};

    // One api: command list handle.
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue));

    // One api: streamer handle.
    zet_metric_streamer_handle_t streamerHandle = {};
    zet_metric_streamer_desc_t streamerDesc = {};

    streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
    streamerDesc.notifyEveryNReports = 32768;
    streamerDesc.samplingPeriod = 1000;

    // One api: metric group handle.
    Mock<MetricGroup> metricGroup;
    zet_metric_group_handle_t metricGroupHandle = metricGroup.toHandle();
    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};

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

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;

    ContextHandle_1_0 contextHandle = {&value};

    CommandBufferSize_1_0 commandBufferSize = {};
    commandBufferSize.GpuMemorySize = 100;

    uint32_t markerValue = 0x12345678;

    openMetricsAdapter();

    mockMetricsLibrary->g_mockApi->contextCreateOutHandle = contextHandle;
    mockMetricsLibrary->g_mockApi->commandBufferGetSizeOutSize = commandBufferSize;

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;

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
    EXPECT_EQ(metricGroupProperties.samplingType, ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
    EXPECT_EQ(metricGroupProperties.metricCount, metricsSetParams.MetricsCount);
    EXPECT_EQ(strcmp(metricGroupProperties.description, metricsSetParams.ShortName), 0);
    EXPECT_EQ(strcmp(metricGroupProperties.name, metricsSetParams.SymbolName), 0);

    // Metric group activation.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), metricDeviceHandle, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    // Metric streamer open.
    EXPECT_EQ(zetMetricStreamerOpen(context->toHandle(), metricDeviceHandle, metricGroupHandle, &streamerDesc, eventHandle, &streamerHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(streamerHandle, nullptr);

    // Metric streamer marker.
    EXPECT_EQ(zetCommandListAppendMetricStreamerMarker(commandList->toHandle(), streamerHandle, markerValue), ZE_RESULT_SUCCESS);

    // Metric streamer close.
    EXPECT_EQ(zetMetricStreamerClose(streamerHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricStreamerTest, givenMultipleMarkerInsertionsWhenZetCommandListAppendMetricStreamerMarkerIsCalledThenReturnsSuccess) {

    // One api: device handle.
    zet_device_handle_t metricDeviceHandle = device->toHandle();

    // One api: event handle.
    ze_event_handle_t eventHandle = {};

    // One api: command list handle.
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue));

    // One api: streamer handle.
    zet_metric_streamer_handle_t streamerHandle = {};
    zet_metric_streamer_desc_t streamerDesc = {};

    streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
    streamerDesc.notifyEveryNReports = 32768;
    streamerDesc.samplingPeriod = 1000;

    // One api: metric group handle.
    Mock<MetricGroup> metricGroup;
    zet_metric_group_handle_t metricGroupHandle = metricGroup.toHandle();
    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};

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

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;

    ContextHandle_1_0 contextHandle = {&value};

    CommandBufferSize_1_0 commandBufferSize = {};
    commandBufferSize.GpuMemorySize = 100;

    openMetricsAdapter();

    mockMetricsLibrary->g_mockApi->contextCreateOutHandle = contextHandle;
    mockMetricsLibrary->g_mockApi->commandBufferGetSizeOutSize = commandBufferSize;

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;

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
    EXPECT_EQ(metricGroupProperties.samplingType, ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
    EXPECT_EQ(metricGroupProperties.metricCount, metricsSetParams.MetricsCount);
    EXPECT_EQ(strcmp(metricGroupProperties.description, metricsSetParams.ShortName), 0);
    EXPECT_EQ(strcmp(metricGroupProperties.name, metricsSetParams.SymbolName), 0);

    // Metric group activation.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), metricDeviceHandle, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    // Metric streamer open.
    EXPECT_EQ(zetMetricStreamerOpen(context->toHandle(), metricDeviceHandle, metricGroupHandle, &streamerDesc, eventHandle, &streamerHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(streamerHandle, nullptr);

    // Metric streamer marker.
    std::array<uint32_t, 10> markerValues = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0};
    for (auto &markerValue : markerValues) {
        EXPECT_EQ(zetCommandListAppendMetricStreamerMarker(commandList->toHandle(), streamerHandle, markerValue), ZE_RESULT_SUCCESS);
    }

    // Metric streamer close.
    EXPECT_EQ(zetMetricStreamerClose(streamerHandle), ZE_RESULT_SUCCESS);
}

} // namespace ult
} // namespace L0
