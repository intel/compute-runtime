/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_oa.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

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
    zet_metric_group_properties_t metricGroupProperties = {};

    metricGroupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;

    EXPECT_CALL(metricGroup, getProperties(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(metricGroupProperties), Return(ZE_RESULT_SUCCESS)));

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
    zet_metric_group_properties_t metricGroupProperties = {};

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

    EXPECT_CALL(*mockMetricEnumeration, loadMetricsDiscovery())
        .Times(1)
        .WillOnce(Return(ZE_RESULT_SUCCESS));

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(1)
        .WillOnce(Return(true));

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
    zet_metric_group_properties_t metricGroupProperties = {};

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

    EXPECT_CALL(*mockMetricEnumeration, loadMetricsDiscovery())
        .Times(1)
        .WillOnce(Return(ZE_RESULT_SUCCESS));

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(1)
        .WillOnce(Return(true));

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

    openMetricsAdapter();

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
    zet_metric_group_properties_t metricGroupProperties = {};
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

    EXPECT_CALL(*mockMetricEnumeration, isInitialized())
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, getContextData(_, _))
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(metricGroup, getProperties(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(metricGroupProperties), Return(ZE_RESULT_SUCCESS)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryCreate(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<1>(queryHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetParameter(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(value), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(contextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

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

    openMetricsAdapter();
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

    openMetricsAdapter();

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
    zet_metric_group_properties_t metricGroupProperties = {};
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

    openMetricsAdapter();

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

MATCHER_P(streamerMarkerDataAreEqual, marker, "") {
    const uint32_t streamerMarkerLowBitsMask = 0x1FFFFFF;
    const uint32_t streamerMarkerHighBitsShift = 25;

    return (arg->CommandsType == ObjectType::MarkerStreamUser) &&
           (arg->MarkerStreamUser.Value == (marker & streamerMarkerLowBitsMask)) &&
           (arg->MarkerStreamUser.Reserved == (marker >> streamerMarkerHighBitsShift));
}

TEST_F(MetricStreamerTest, givenValidArgumentsWhenZetCommandListAppendMetricStreamerMarkerIsCalledThenReturnsSuccess) {

    // One api: device handle.
    zet_device_handle_t metricDeviceHandle = device->toHandle();

    // One api: event handle.
    ze_event_handle_t eventHandle = {};

    // One api: command list handle.
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));

    // One api: streamer handle.
    zet_metric_streamer_handle_t streamerHandle = {};
    zet_metric_streamer_desc_t streamerDesc = {};

    streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
    streamerDesc.notifyEveryNReports = 32768;
    streamerDesc.samplingPeriod = 1000;

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

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;

    ContextHandle_1_0 contextHandle = {&value};

    CommandBufferSize_1_0 commandBufferSize = {};
    commandBufferSize.GpuMemorySize = 100;

    uint32_t markerValue = 0x12345678;

    openMetricsAdapter();

    EXPECT_CALL(*mockMetricEnumeration, isInitialized())
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, getContextData(_, _))
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(contextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

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

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGetSize(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<1>(::testing::ByRef(commandBufferSize)), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGet(streamerMarkerDataAreEqual(markerValue)))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

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
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));

    // One api: streamer handle.
    zet_metric_streamer_handle_t streamerHandle = {};
    zet_metric_streamer_desc_t streamerDesc = {};

    streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
    streamerDesc.notifyEveryNReports = 32768;
    streamerDesc.samplingPeriod = 1000;

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

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;

    ContextHandle_1_0 contextHandle = {&value};

    CommandBufferSize_1_0 commandBufferSize = {};
    commandBufferSize.GpuMemorySize = 100;

    openMetricsAdapter();

    EXPECT_CALL(*mockMetricEnumeration, isInitialized())
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary, getContextData(_, _))
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(contextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(1)
        .WillOnce(Return(StatusCode::Success));

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

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGetSize(_, _))
        .Times(10)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<1>(::testing::ByRef(commandBufferSize)), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockCommandBufferGet(_))
        .Times(10)
        .WillRepeatedly(Return(StatusCode::Success));

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
