/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace L0 {
namespace ult {

using MetricEnumerationTest = Test<MetricContextFixture>;

TEST_F(MetricEnumerationTest, givenIncorrectMetricsDiscoveryDeviceWhenZetGetMetricGroupIsCalledThenReturnsFail) {

    EXPECT_CALL(*mockMetricEnumeration, loadMetricsDiscovery())
        .Times(0);

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockOpenMetricsDevice(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(nullptr), Return(TCompletionCode::CC_ERROR_GENERAL)));

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_ERROR_UNKNOWN);
    EXPECT_EQ(metricGroupCount, 0u);
}

TEST_F(MetricEnumerationTest, givenIncorrectMetricsDiscoveryInterfaceVersionWhenZetGetMetricGroupIsCalledThenReturnsFail) {

    metricsDeviceParams.Version.MajorNumber = 0;
    metricsDeviceParams.Version.MinorNumber = 1;

    EXPECT_CALL(*mockMetricEnumeration, loadMetricsDiscovery())
        .Times(0);

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockOpenMetricsDevice(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&metricsDevice), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockCloseMetricsDevice(_))
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsDevice, GetParams())
        .Times(1)
        .WillOnce(Return(&metricsDeviceParams));

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_ERROR_UNKNOWN);
    EXPECT_EQ(metricGroupCount, 0u);
}

TEST_F(MetricEnumerationTest, givenNoConcurrentMetricGroupsWhenZetGetMetricGroupIsCalledThenReturnsZeroMetricsGroups) {

    EXPECT_CALL(*mockMetricEnumeration, loadMetricsDiscovery())
        .Times(0);

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockOpenMetricsDevice(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&metricsDevice), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockCloseMetricsDevice(_))
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsDevice, GetParams())
        .Times(1)
        .WillOnce(Return(&metricsDeviceParams));

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 0u);
}

TEST_F(MetricEnumerationTest, givenTwoConcurrentMetricGroupsWhenZetGetMetricGroupIsCalledThenReturnsTwoMetricsGroups) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 2;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup0;
    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup1;

    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.SymbolName = "OA";
    metricsConcurrentGroupParams.MetricSetsCount = 1;

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;

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
        .Times(2)
        .WillOnce(Return(&metricsConcurrentGroup0))
        .WillOnce(Return(&metricsConcurrentGroup1));

    EXPECT_CALL(metricsConcurrentGroup0, GetParams())
        .WillRepeatedly(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup1, GetParams())
        .WillRepeatedly(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup0, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsConcurrentGroup1, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 2u);

    std::vector<zet_metric_group_handle_t> metricGroups;
    metricGroups.resize(metricGroupCount);
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 2u);
    EXPECT_NE(metricGroups[0], nullptr);
    EXPECT_NE(metricGroups[1], nullptr);
}

TEST_F(MetricEnumerationTest, givenInvalidArgumentsWhenZetGetMetricGroupPropertiesIsCalledThenReturnsFail) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.SymbolName = "OA";
    metricsConcurrentGroupParams.MetricSetsCount = 1;

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;

    // One api: metric group handle.
    zet_metric_group_handle_t metricGroupHandle = {};

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

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);
}

TEST_F(MetricEnumerationTest, givenValidArgumentsWhenZetGetMetricGroupPropertiesIsCalledThenReturnsSuccess) {

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

    // One api: metric group handle.
    zet_metric_group_handle_t metricGroupHandle = {};
    zet_metric_group_properties_t metricGroupProperties = {};

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

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Metric group properties.
    EXPECT_EQ(zetMetricGroupGetProperties(metricGroupHandle, &metricGroupProperties), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupProperties.domain, 0u);
    EXPECT_EQ(metricGroupProperties.samplingType, ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED);
    EXPECT_EQ(metricGroupProperties.metricCount, metricsSetParams.MetricsCount);
    EXPECT_EQ(strcmp(metricGroupProperties.description, metricsSetParams.ShortName), 0);
    EXPECT_EQ(strcmp(metricGroupProperties.name, metricsSetParams.SymbolName), 0);
}

TEST_F(MetricEnumerationTest, givenInvalidArgumentsWhenZetMetricGetIsCalledThenReturnsFail) {

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

    // One api: metric group handle.
    zet_metric_group_handle_t metricGroupHandle = {};

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

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);
}

TEST_F(MetricEnumerationTest, givenValidArgumentsWhenZetMetricGetIsCalledThenReturnsCorrectMetricCount) {

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

    // One api: metric group handle.
    zet_metric_group_handle_t metricGroupHandle = {};

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

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(1)
        .WillOnce(Return(&metric));

    EXPECT_CALL(metric, GetParams())
        .Times(1)
        .WillOnce(Return(&metricParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Obtain metric count.
    uint32_t metricCount = 0;
    EXPECT_EQ(zetMetricGet(metricGroupHandle, &metricCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricCount, 1u);
}

TEST_F(MetricEnumerationTest, givenValidArgumentsWhenZetMetricGetIsCalledThenReturnsCorrectMetric) {

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

    // One api: metric group handle.
    zet_metric_group_handle_t metricGroupHandle = {};

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

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(1)
        .WillOnce(Return(&metric));

    EXPECT_CALL(metric, GetParams())
        .Times(1)
        .WillOnce(Return(&metricParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

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
}

TEST_F(MetricEnumerationTest, givenInvalidArgumentsWhenZetMetricGetPropertiestIsCalledThenReturnsFail) {

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

    // One api: metric group handle.
    zet_metric_group_handle_t metricGroupHandle = {};

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

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(1)
        .WillOnce(Return(&metric));

    EXPECT_CALL(metric, GetParams())
        .Times(1)
        .WillOnce(Return(&metricParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

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
}

TEST_F(MetricEnumerationTest, givenValidArgumentsWhenZetMetricGetPropertiestIsCalledThenReturnSuccess) {

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
    zet_metric_properties_t metricProperties = {};

    // One api: metric group handle.
    zet_metric_group_handle_t metricGroupHandle = {};

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

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(1)
        .WillOnce(Return(&metric));

    EXPECT_CALL(metric, GetParams())
        .Times(1)
        .WillOnce(Return(&metricParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

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

    // Obtain metric params.
    EXPECT_EQ(zetMetricGetProperties(metricHandle, &metricProperties), ZE_RESULT_SUCCESS);
    EXPECT_EQ(strcmp(metricProperties.name, metricParams.SymbolName), 0);
    EXPECT_EQ(strcmp(metricProperties.description, metricParams.LongName), 0);
    EXPECT_EQ(metricProperties.metricType, ZET_METRIC_TYPE_RATIO);
    EXPECT_EQ(metricProperties.resultType, ZET_VALUE_TYPE_UINT64);
}

TEST_F(MetricEnumerationTest, givenValidArgumentsWhenZetMetricGetPropertiestIsCalledThenReturnSuccessExt) {

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
    zet_metric_properties_t metricProperties = {};

    // One api: metric group handle.
    zet_metric_group_handle_t metricGroupHandle = {};

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

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(1)
        .WillOnce(Return(&metric));

    EXPECT_CALL(metric, GetParams())
        .Times(1)
        .WillOnce(Return(&metricParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

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

    // Obtain metric params.
    EXPECT_EQ(zetMetricGetProperties(metricHandle, &metricProperties), ZE_RESULT_SUCCESS);
    EXPECT_EQ(strcmp(metricProperties.name, metricParams.SymbolName), 0);
    EXPECT_EQ(strcmp(metricProperties.description, metricParams.LongName), 0);
    EXPECT_EQ(metricProperties.metricType, ZET_METRIC_TYPE_RATIO);
    EXPECT_EQ(metricProperties.resultType, ZET_VALUE_TYPE_UINT64);
}

TEST_F(MetricEnumerationTest, givenInvalidArgumentsWhenzetContextActivateMetricGroupsIsCalledThenReturnsFail) {

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

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(1)
        .WillOnce(Return(&metric));

    EXPECT_CALL(metric, GetParams())
        .Times(1)
        .WillOnce(Return(&metricParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);
}

TEST_F(MetricEnumerationTest, givenValidEventBasedMetricGroupWhenzetContextActivateMetricGroupsIsCalledThenReturnsSuccess) {

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

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(1)
        .WillOnce(Return(&metric));

    EXPECT_CALL(metric, GetParams())
        .Times(1)
        .WillOnce(Return(&metricParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Activate metric group.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 1, &metricGroupHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTest, givenValidEventBasedMetricGroupWhenZetContextActivateMetricGroupsIsCalledThenReturnsSuccess) {

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

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(1)
        .WillOnce(Return(&metric));

    EXPECT_CALL(metric, GetParams())
        .Times(1)
        .WillOnce(Return(&metricParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Activate metric group.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 1, &metricGroupHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTest, givenValidTimeBasedMetricGroupWhenzetContextActivateMetricGroupsIsCalledThenReturnsSuccess) {

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
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
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

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(1)
        .WillOnce(Return(&metric));

    EXPECT_CALL(metric, GetParams())
        .Times(1)
        .WillOnce(Return(&metricParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Activate metric group.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 1, &metricGroupHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTest, givenActivateTheSameMetricGroupTwiceWhenzetContextActivateMetricGroupsIsCalledThenReturnsSuccess) {

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
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
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

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(1)
        .WillOnce(Return(&metric));

    EXPECT_CALL(metric, GetParams())
        .Times(1)
        .WillOnce(Return(&metricParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Activate metric group.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 1, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 1, &metricGroupHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTest, givenActivateTwoMetricGroupsWithDifferentDomainsWhenzetContextActivateMetricGroupsIsCalledThenReturnsSuccess) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 2;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup0;
    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup1;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams0 = {};
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams1 = {};
    metricsConcurrentGroupParams0.MetricSetsCount = 1;
    metricsConcurrentGroupParams1.MetricSetsCount = 1;
    metricsConcurrentGroupParams0.SymbolName = "OA";
    metricsConcurrentGroupParams1.SymbolName = "OA";

    // Metrics Discovery: metric set
    Mock<IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;

    // One api: metric group.
    zet_metric_group_handle_t metricGroupHandle[2] = {};
    zet_metric_group_properties_t metricGroupProperties[2] = {};

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
        .Times(2)
        .WillOnce(Return(&metricsConcurrentGroup0))
        .WillOnce(Return(&metricsConcurrentGroup1));

    EXPECT_CALL(metricsConcurrentGroup0, GetParams())
        .Times(1)
        .WillOnce(Return(&metricsConcurrentGroupParams0));

    EXPECT_CALL(metricsConcurrentGroup1, GetParams())
        .Times(1)
        .WillOnce(Return(&metricsConcurrentGroupParams1));

    EXPECT_CALL(metricsConcurrentGroup0, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsConcurrentGroup1, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    // Metric group handles.
    uint32_t metricGroupCount = 2;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 2u);
    EXPECT_NE(metricGroupHandle[0], nullptr);
    EXPECT_NE(metricGroupHandle[1], nullptr);

    // Metric group properties.
    EXPECT_EQ(zetMetricGroupGetProperties(metricGroupHandle[0], &metricGroupProperties[0]), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricGroupGetProperties(metricGroupHandle[1], &metricGroupProperties[1]), ZE_RESULT_SUCCESS);

    // Activate two metric groups with a different domains.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 1, &metricGroupHandle[0]), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 1, &metricGroupHandle[1]), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTest, givenActivateTwoMetricGroupsWithDifferentDomainsAtOnceWhenzetContextActivateMetricGroupsIsCalledThenReturnsSuccess) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 2;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup0;
    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup1;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams0 = {};
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams1 = {};
    metricsConcurrentGroupParams0.MetricSetsCount = 1;
    metricsConcurrentGroupParams1.MetricSetsCount = 1;
    metricsConcurrentGroupParams0.SymbolName = "OA";
    metricsConcurrentGroupParams1.SymbolName = "OA";

    // Metrics Discovery: metric set
    Mock<IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;

    // One api: metric group.
    zet_metric_group_handle_t metricGroupHandle[2] = {};
    zet_metric_group_properties_t metricGroupProperties[2] = {};

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
        .Times(2)
        .WillOnce(Return(&metricsConcurrentGroup0))
        .WillOnce(Return(&metricsConcurrentGroup1));

    EXPECT_CALL(metricsConcurrentGroup0, GetParams())
        .Times(1)
        .WillOnce(Return(&metricsConcurrentGroupParams0));

    EXPECT_CALL(metricsConcurrentGroup1, GetParams())
        .Times(1)
        .WillOnce(Return(&metricsConcurrentGroupParams1));

    EXPECT_CALL(metricsConcurrentGroup0, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsConcurrentGroup1, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    // Metric group handles.
    uint32_t metricGroupCount = 2;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 2u);
    EXPECT_NE(metricGroupHandle[0], nullptr);
    EXPECT_NE(metricGroupHandle[1], nullptr);

    // Metric group properties.
    EXPECT_EQ(zetMetricGroupGetProperties(metricGroupHandle[0], &metricGroupProperties[0]), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricGroupGetProperties(metricGroupHandle[1], &metricGroupProperties[1]), ZE_RESULT_SUCCESS);

    // Activate two metric groups with a different domains.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 2, metricGroupHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTest, givenActivateTwoMetricGroupsWithTheSameDomainsWhenzetContextActivateMetricGroupsIsCalledThenReturnsFail) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 2;
    metricsConcurrentGroupParams.SymbolName = "OA";

    // Metrics Discovery: metric set
    Mock<IMetricSet_1_5> metricsSet0;
    Mock<IMetricSet_1_5> metricsSet1;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams0 = {};
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams1 = {};
    metricsSetParams0.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams1.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;

    // One api: metric group.
    zet_metric_group_handle_t metricGroupHandle[2] = {};
    zet_metric_group_properties_t metricGroupProperties[2] = {};

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
        .WillOnce(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .Times(2)
        .WillOnce(Return(&metricsSet0))
        .WillOnce(Return(&metricsSet1));

    EXPECT_CALL(metricsSet0, GetParams())
        .WillRepeatedly(Return(&metricsSetParams0));

    EXPECT_CALL(metricsSet1, GetParams())
        .WillRepeatedly(Return(&metricsSetParams1));

    EXPECT_CALL(metricsSet0, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsSet1, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    // Metric group handles.
    uint32_t metricGroupCount = 2;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 2u);
    EXPECT_NE(metricGroupHandle[0], nullptr);
    EXPECT_NE(metricGroupHandle[1], nullptr);

    // Metric group properties.
    EXPECT_EQ(zetMetricGroupGetProperties(metricGroupHandle[0], &metricGroupProperties[0]), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricGroupGetProperties(metricGroupHandle[1], &metricGroupProperties[1]), ZE_RESULT_SUCCESS);

    // Activate two metric groups with a different domains.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 1, &metricGroupHandle[0]), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 1, &metricGroupHandle[1]), ZE_RESULT_ERROR_UNKNOWN);
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 2, metricGroupHandle), ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(MetricEnumerationTest, givenDeactivateTestsWhenzetContextActivateMetricGroupsIsCalledThenReturnsApropriateResults) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 2;
    metricsConcurrentGroupParams.SymbolName = "OA";

    // Metrics Discovery: metric set
    Mock<IMetricSet_1_5> metricsSet0;
    Mock<IMetricSet_1_5> metricsSet1;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams0 = {};
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams1 = {};
    metricsSetParams0.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
    metricsSetParams1.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;

    // One api: metric group.
    zet_metric_group_handle_t metricGroupHandle[2] = {};
    zet_metric_group_properties_t metricGroupProperties[2] = {};

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
        .WillOnce(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .Times(2)
        .WillOnce(Return(&metricsSet0))
        .WillOnce(Return(&metricsSet1));

    EXPECT_CALL(metricsSet0, GetParams())
        .WillRepeatedly(Return(&metricsSetParams0));

    EXPECT_CALL(metricsSet1, GetParams())
        .WillRepeatedly(Return(&metricsSetParams1));

    EXPECT_CALL(metricsSet0, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsSet1, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    // Metric group handles.
    uint32_t metricGroupCount = 2;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 2u);
    EXPECT_NE(metricGroupHandle[0], nullptr);
    EXPECT_NE(metricGroupHandle[1], nullptr);

    // Metric group properties.
    EXPECT_EQ(zetMetricGroupGetProperties(metricGroupHandle[0], &metricGroupProperties[0]), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricGroupGetProperties(metricGroupHandle[1], &metricGroupProperties[1]), ZE_RESULT_SUCCESS);

    // Activate two metric groups with a different domains.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 1, &metricGroupHandle[0]), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 1, &metricGroupHandle[1]), ZE_RESULT_ERROR_UNKNOWN);
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 2, metricGroupHandle), ZE_RESULT_ERROR_UNKNOWN);

    // Deactivate all.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 0, nullptr), ZE_RESULT_SUCCESS);

    // Activate two metric groups at once.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 2, metricGroupHandle), ZE_RESULT_ERROR_UNKNOWN);

    // Deactivate all.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 0, nullptr), ZE_RESULT_SUCCESS);

    // Activate one domain.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 1, metricGroupHandle), ZE_RESULT_SUCCESS);

    // Deactivate all.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 0, nullptr), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTest, givenInvalidArgumentsWhenZetMetricGroupCalculateMetricValuesIsCalledThenReturnsFail) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";

    // Metrics Discovery: metric set
    Mock<IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;

    // One api: metric group.
    zet_metric_group_handle_t metricGroupHandle = {};

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
        .WillOnce(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .Times(1)
        .WillOnce(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    // Metric group handles.
    uint32_t metricGroupCount = 1;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);
}

TEST_F(MetricEnumerationTest, givenIncorrectRawReportSizeWhenZetMetricGroupCalculateMetricValuesIsCalledThenReturnsFail) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";

    // Metrics Discovery: metric set
    Mock<IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.QueryReportSize = 256;

    // One api: metric group.
    zet_metric_group_handle_t metricGroupHandle = {};

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
        .WillOnce(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .Times(1)
        .WillOnce(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    // Metric group handles.
    uint32_t metricGroupCount = 1;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Raw results.
    const size_t rawResultsSize = 300;
    uint8_t rawResults[rawResultsSize] = {};

    // Invalid raw buffer size provided by the user.
    uint32_t calculatedResults = 0;
    EXPECT_NE(metricsSetParams.QueryReportSize, rawResultsSize);
    EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &calculatedResults, nullptr), ZE_RESULT_ERROR_UNKNOWN);

    // Invalid raw buffer size provided by the driver.
    metricsSetParams.QueryReportSize = 0;
    EXPECT_NE(metricsSetParams.QueryReportSize, rawResultsSize);
    EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &calculatedResults, nullptr), ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(MetricEnumerationTest, givenCorrectRawReportSizeWhenZetMetricGroupCalculateMetricValuesIsCalledThenReturnsCorrectCalculatedReportCount) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";

    // Metrics Discovery: metric set
    Mock<IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.QueryReportSize = 256;
    metricsSetParams.MetricsCount = 11;

    // Metrics Discovery: metric
    Mock<IMetric_1_0> metric;
    MetricsDiscovery::TMetricParams_1_0 metricParams = {};

    // One api: metric group.
    zet_metric_group_handle_t metricGroupHandle = {};

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
        .WillOnce(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(11)
        .WillRepeatedly(Return(&metric));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metric, GetParams())
        .WillRepeatedly(Return(&metricParams));

    // Metric group handles.
    uint32_t metricGroupCount = 1;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Return correct calculated report count for single raw report.
    {
        size_t rawResultsSize = metricsSetParams.QueryReportSize;
        std::vector<uint8_t> rawResults(rawResultsSize);
        uint32_t calculatedResults = 0;

        EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults.data(), &calculatedResults, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(calculatedResults, metricsSetParams.MetricsCount);
    }

    // Return correct calculated report count for two raw report.
    {
        size_t rawResultsSize = 2 * metricsSetParams.QueryReportSize;
        std::vector<uint8_t> rawResults(rawResultsSize);
        uint32_t calculatedResults = 0;

        EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults.data(), &calculatedResults, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(calculatedResults, 2 * metricsSetParams.MetricsCount);
    }
}

TEST_F(MetricEnumerationTest, givenCorrectRawReportSizeAndLowerProvidedCalculatedReportCountThanObtainedFromApiWhenZetMetricGroupCalculateMetricValuesIsCalledThenReturnsCorrectCalculatedReportCount) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";

    // Metrics Discovery: metric set
    Mock<IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.QueryReportSize = 256;
    metricsSetParams.MetricsCount = 11;

    // Metrics Discovery: metric
    Mock<IMetric_1_0> metric;
    MetricsDiscovery::TMetricParams_1_0 metricParams = {};

    // One api: metric group.
    zet_metric_group_handle_t metricGroupHandle = {};

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
        .WillOnce(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(11)
        .WillRepeatedly(Return(&metric));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metric, GetParams())
        .WillRepeatedly(Return(&metricParams));

    uint32_t returnedMetricCount = 2;
    EXPECT_CALL(metricsSet, CalculateMetrics(_, _, _, _, _, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<4>(returnedMetricCount), Return(TCompletionCode::CC_OK)));

    // Metric group handles.
    uint32_t metricGroupCount = 1;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Return correct calculated report count for single raw report.
    size_t rawResultsSize = metricsSetParams.QueryReportSize;
    std::vector<uint8_t> rawResults(rawResultsSize);
    uint32_t calculatedResultsCount = 0;

    EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults.data(), &calculatedResultsCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(calculatedResultsCount, metricsSetParams.MetricsCount);

    // Provide lower calculated report count than returned earlier from api.
    calculatedResultsCount = 2;
    std::vector<zet_typed_value_t> caculatedrawResults(calculatedResultsCount);
    EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults.data(), &calculatedResultsCount, caculatedrawResults.data()), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTest, givenCorrectRawReportSizeAndCorrectCalculatedReportCountWhenZetMetricGroupCalculateMetricValuesIsCalledThenReturnsSuccess) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";

    // Metrics Discovery: metric set
    Mock<IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.QueryReportSize = 256;
    metricsSetParams.MetricsCount = 11;

    // Metrics Discovery: metric
    Mock<IMetric_1_0> metric;
    MetricsDiscovery::TMetricParams_1_0 metricParams = {};

    // One api: metric group.
    zet_metric_group_handle_t metricGroupHandle = {};

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
        .WillOnce(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(11)
        .WillRepeatedly(Return(&metric));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsSet, CalculateMetrics(_, _, _, _, _, _, _))
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metric, GetParams())
        .WillRepeatedly(Return(&metricParams));

    // Metric group handles.
    uint32_t metricGroupCount = 1;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Return correct calculated report count for single raw report.
    size_t rawResultsSize = metricsSetParams.QueryReportSize;
    std::vector<uint8_t> rawResults(rawResultsSize);
    uint32_t calculatedResultsCount = 0;

    EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults.data(), &calculatedResultsCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(calculatedResultsCount, metricsSetParams.MetricsCount);

    // Provide incorrect calculated report buffer.
    std::vector<zet_typed_value_t> caculatedrawResults(calculatedResultsCount);
    EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults.data(), &calculatedResultsCount, caculatedrawResults.data()), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTest, givenCorrectRawReportSizeAndCorrectCalculatedReportCountWhenZetMetricGroupCalculateMetricValuesIsCalledThenMaxValuesAreReturned) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";

    // Metrics Discovery: metric set
    Mock<IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.QueryReportSize = 256;
    metricsSetParams.MetricsCount = 11;

    // Metrics Discovery: metric
    Mock<IMetric_1_0> metric;
    MetricsDiscovery::TMetricParams_1_0 metricParams = {};

    // One api: metric group.
    zet_metric_group_handle_t metricGroupHandle = {};

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
        .WillOnce(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(11)
        .WillRepeatedly(Return(&metric));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsSet, CalculateMetrics(_, _, _, _, _, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<4>(metricsSetParams.MetricsCount), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(metric, GetParams())
        .WillRepeatedly(Return(&metricParams));

    // Metric group handles.
    uint32_t metricGroupCount = 1;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Return correct calculated report count for single raw report.
    size_t rawResultsSize = metricsSetParams.QueryReportSize;
    std::vector<uint8_t> rawResults(rawResultsSize);
    uint32_t calculatedResultsCount = 0;

    EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_MAX_METRIC_VALUES, rawResultsSize, rawResults.data(), &calculatedResultsCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(calculatedResultsCount, metricsSetParams.MetricsCount);

    // Provide incorrect calculated report buffer.
    std::vector<zet_typed_value_t> caculatedrawResults(calculatedResultsCount);
    EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_MAX_METRIC_VALUES, rawResultsSize, rawResults.data(), &calculatedResultsCount, caculatedrawResults.data()), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTest, givenIncorrectCalculationTypeWhenZetMetricGroupCalculateMetricValuesIsCalledThenMaxValuesAreReturned) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";

    // Metrics Discovery: metric set
    Mock<IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.QueryReportSize = 256;
    metricsSetParams.MetricsCount = 11;

    // Metrics Discovery: metric
    Mock<IMetric_1_0> metric;
    MetricsDiscovery::TMetricParams_1_0 metricParams = {};

    // One api: metric group.
    zet_metric_group_handle_t metricGroupHandle = {};

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
        .WillOnce(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(11)
        .WillRepeatedly(Return(&metric));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metric, GetParams())
        .WillRepeatedly(Return(&metricParams));

    // Metric group handles.
    uint32_t metricGroupCount = 1;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Return correct calculated report count for single raw report.
    size_t rawResultsSize = metricsSetParams.QueryReportSize;
    std::vector<uint8_t> rawResults(rawResultsSize);
    uint32_t calculatedResultsCount = 0;

    EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults.data(), &calculatedResultsCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(calculatedResultsCount, metricsSetParams.MetricsCount);
}

TEST_F(MetricEnumerationTest, givenInitializedMetricEnumerationWhenIsInitializedIsCalledThenMetricEnumerationWillNotBeInitializedAgain) {

    mockMetricEnumeration->initializationState = ZE_RESULT_SUCCESS;
    EXPECT_EQ(mockMetricEnumeration->baseIsInitialized(), true);
}

TEST_F(MetricEnumerationTest, givenNotInitializedMetricEnumerationWhenIsInitializedIsCalledThenMetricEnumerationWillBeInitialized) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";

    // Metrics Discovery: metric set
    Mock<IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.QueryReportSize = 256;
    metricsSetParams.MetricsCount = 11;

    // Metrics Discovery: metric
    Mock<IMetric_1_0> metric;
    MetricsDiscovery::TMetricParams_1_0 metricParams = {};

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
        .WillOnce(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(11)
        .WillRepeatedly(Return(&metric));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metric, GetParams())
        .WillRepeatedly(Return(&metricParams));

    mockMetricEnumeration->initializationState = ZE_RESULT_ERROR_UNINITIALIZED;
    EXPECT_EQ(mockMetricEnumeration->baseIsInitialized(), true);
}

TEST_F(MetricEnumerationTest, givenLoadedMetricsLibraryAndDiscoveryAndMetricsLibraryInitializedWhenLoadDependenciesThenReturnSuccess) {

    EXPECT_CALL(*mockMetricEnumeration, loadMetricsDiscovery())
        .Times(1)
        .WillOnce(Return(ZE_RESULT_SUCCESS));

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(1)
        .WillOnce(Return(true));

    mockMetricsLibrary->initializationState = ZE_RESULT_SUCCESS;

    auto &metricContext = device->getMetricContext();
    EXPECT_EQ(metricContext.loadDependencies(), true);
    EXPECT_EQ(metricContext.isInitialized(), true);
}

TEST_F(MetricEnumerationTest, givenNotLoadedMetricsLibraryAndDiscoveryWhenLoadDependenciesThenReturnFail) {

    EXPECT_CALL(*mockMetricEnumeration, loadMetricsDiscovery())
        .Times(1)
        .WillOnce(Return(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE));

    auto &metricContext = device->getMetricContext();
    EXPECT_EQ(metricContext.loadDependencies(), false);
    EXPECT_EQ(metricContext.isInitialized(), false);
}

TEST_F(MetricEnumerationTest, givenLoadedMetricsLibraryAndDiscoveryAndMetricsLibraryNotInitializedWhenLoadDependenciesThenReturnFail) {

    EXPECT_CALL(*mockMetricEnumeration, loadMetricsDiscovery())
        .Times(1)
        .WillOnce(Return(ZE_RESULT_SUCCESS));

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(1)
        .WillOnce(Return(true));

    mockMetricsLibrary->initializationState = ZE_RESULT_ERROR_NOT_AVAILABLE;

    auto &metricContext = device->getMetricContext();
    EXPECT_EQ(metricContext.loadDependencies(), false);
    EXPECT_EQ(metricContext.isInitialized(), false);
}

} // namespace ult
} // namespace L0
