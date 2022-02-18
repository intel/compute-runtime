/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_oa.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <unordered_map>

using ::testing::_;
using ::testing::Return;

namespace L0 {
namespace ult {

using MetricEnumerationTest = Test<MetricContextFixture>;

TEST_F(MetricEnumerationTest, givenIncorrectMetricsDiscoveryDeviceWhenZetGetMetricGroupIsCalledThenNoMetricGroupsAreReturned) {

    EXPECT_CALL(*mockMetricEnumeration, loadMetricsDiscovery())
        .Times(0);

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockOpenAdapterGroup(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(nullptr), Return(TCompletionCode::CC_ERROR_GENERAL)));

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 0u);
}

TEST_F(MetricEnumerationTest, givenCorrectMetricDiscoveryWhenLoadMetricsDiscoveryIsCalledThenReturnsSuccess) {

    EXPECT_CALL(*mockMetricEnumeration, loadMetricsDiscovery())
        .Times(1);

    EXPECT_EQ(mockMetricEnumeration->loadMetricsDiscovery(), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTest, givenIncorrectMetricDiscoveryWhenLoadMetricsDiscoveryIsCalledThenReturnsFail) {

    mockMetricEnumeration->hMetricsDiscovery = nullptr;
    mockMetricEnumeration->openAdapterGroup = nullptr;

    EXPECT_EQ(mockMetricEnumeration->baseLoadMetricsDiscovery(), ZE_RESULT_ERROR_NOT_AVAILABLE);
}

TEST_F(MetricEnumerationTest, givenIncorrectMetricDiscoveryWhenMetricGroupGetIsCalledThenNoMetricGroupsAreReturned) {

    mockMetricEnumeration->hMetricsDiscovery = nullptr;
    mockMetricEnumeration->openAdapterGroup = nullptr;

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 0u);
}

TEST_F(MetricEnumerationTest, givenIncorrectMetricsDiscoveryInterfaceVersionWhenZetGetMetricGroupIsCalledThenNoMetricGroupsAreReturned) {

    metricsDeviceParams.Version.MajorNumber = 0;
    metricsDeviceParams.Version.MinorNumber = 1;

    openMetricsAdapter();

    EXPECT_CALL(metricsDevice, GetParams())
        .Times(1)
        .WillOnce(Return(&metricsDeviceParams));

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 0u);
}

TEST_F(MetricEnumerationTest, givenNoConcurrentMetricGroupsWhenZetGetMetricGroupIsCalledThenNoMetricGroupsAreReturned) {

    openMetricsAdapter();

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

    openMetricsAdapter();

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

using MultiDeviceMetricEnumerationTest = Test<MetricMultiDeviceFixture>;

TEST_F(MultiDeviceMetricEnumerationTest, givenMultipleDevicesAndValidEventBasedMetricGroupWhenzetContextActivateMetricGroupsIsCalledThenReturnsSuccess) {

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

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

    EXPECT_CALL(metricsDevice, GetParams())
        .WillRepeatedly(Return(&metricsDeviceParams));

    EXPECT_CALL(metricsDevice, GetConcurrentGroup(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroup));

    EXPECT_CALL(metricsConcurrentGroup, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metric));

    EXPECT_CALL(metric, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Activate metric group.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), devices[0]->toHandle(), 1, &metricGroupHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MultiDeviceMetricEnumerationTest, givenMultipleDevicesAndTwoMetricGroupsWithTheSameDomainsWhenzetContextActivateMetricGroupsIsCalledThenReturnsFail) {

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup0;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams0 = {};
    metricsConcurrentGroupParams0.MetricSetsCount = 2;
    metricsConcurrentGroupParams0.SymbolName = "OA";
    metricsConcurrentGroupParams0.Description = "OA description";

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_5> metricsSet0;
    Mock<MetricsDiscovery::IMetricSet_1_5> metricsSet1;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams0 = {};
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams1 = {};
    metricsSetParams0.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams0.SymbolName = "Metric set ZERO";
    metricsSetParams0.ShortName = "Metric set ZERO description";
    metricsSetParams0.MetricsCount = 1;
    metricsSetParams1.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
    metricsSetParams1.SymbolName = "Metric set ONE name";
    metricsSetParams1.ShortName = "Metric set ONE description";
    metricsSetParams1.MetricsCount = 1;

    // Metrics Discovery:: metric.
    Mock<IMetric_1_0> metric0;
    TMetricParams_1_0 metricParams0 = {};
    metricParams0.SymbolName = "Metric ZERO symbol name";
    metricParams0.ShortName = "Metric ZERO short name";
    metricParams0.LongName = "Metric ZERO long name";
    metricParams0.ResultType = MetricsDiscovery::TMetricResultType::RESULT_UINT64;
    metricParams0.MetricType = MetricsDiscovery::TMetricType::METRIC_TYPE_RATIO;

    Mock<IMetric_1_0> metric1;
    TMetricParams_1_0 metricParams1 = {};
    metricParams1.SymbolName = "Metric ONE symbol name";
    metricParams1.ShortName = "Metric ONE short name";
    metricParams1.LongName = "Metric ONE long name";
    metricParams1.ResultType = MetricsDiscovery::TMetricResultType::RESULT_UINT64;
    metricParams1.MetricType = MetricsDiscovery::TMetricType::METRIC_TYPE_RATIO;

    openMetricsAdapter();

    EXPECT_CALL(metricsDevice, GetParams())
        .WillRepeatedly(Return(&metricsDeviceParams));

    EXPECT_CALL(metricsDevice, GetConcurrentGroup(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroup0));

    EXPECT_CALL(metricsConcurrentGroup0, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroupParams0));

    EXPECT_CALL(metricsConcurrentGroup0, GetMetricSet(_))
        .Times(subDeviceCount * 2)
        .WillOnce(Return(&metricsSet0))
        .WillOnce(Return(&metricsSet1))
        .WillOnce(Return(&metricsSet0))
        .WillOnce(Return(&metricsSet1));

    EXPECT_CALL(metricsSet0, GetParams())
        .WillRepeatedly(Return(&metricsSetParams0));

    EXPECT_CALL(metricsSet1, GetParams())
        .WillRepeatedly(Return(&metricsSetParams1));

    EXPECT_CALL(metricsSet0, GetMetric(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metric0));

    EXPECT_CALL(metricsSet1, GetMetric(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metric1));

    EXPECT_CALL(metricsSet0, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metricsSet1, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metric0, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricParams0));

    EXPECT_CALL(metric1, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricParams1));

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 2u);

    // Metric group handle.
    std::vector<zet_metric_group_handle_t> metricGroupHandles;
    metricGroupHandles.resize(metricGroupCount);

    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, metricGroupHandles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 2u);
    EXPECT_NE(metricGroupHandles[0], nullptr);
    EXPECT_NE(metricGroupHandles[1], nullptr);

    zet_metric_group_properties_t properties0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupGetProperties(metricGroupHandles[0], &properties0));

    zet_metric_group_properties_t properties1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupGetProperties(metricGroupHandles[1], &properties1));

    // Activate metric groups.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), devices[0]->toHandle(), 4, metricGroupHandles.data()), ZE_RESULT_ERROR_UNKNOWN);
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

    openMetricsAdapter();

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

    openMetricsAdapter();

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

    openMetricsAdapter();

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

    openMetricsAdapter();

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

    openMetricsAdapter();

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

    openMetricsAdapter();

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

TEST_F(MetricEnumerationTest, givenIncorrectRawReportSizeWhenZetMetricGroupCalculateMetricValuesExpIsCalledThenReturnsFail) {

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
    metricsSetParams.QueryReportSize = 0;

    // One api: metric group.
    zet_metric_group_handle_t metricGroupHandle = {};

    openMetricsAdapter();

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
    uint32_t dataCount = 0;
    uint32_t totalMetricCount = 0;
    EXPECT_NE(metricsSetParams.QueryReportSize, rawResultsSize);
    EXPECT_EQ(zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_ERROR_UNKNOWN);
    EXPECT_EQ(dataCount, 0u);
    EXPECT_EQ(totalMetricCount, 0u);

    // Invalid raw buffer size provided by the driver.
    EXPECT_NE(metricsSetParams.QueryReportSize, rawResultsSize);
    EXPECT_EQ(zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_ERROR_UNKNOWN);
    EXPECT_EQ(dataCount, 0u);
    EXPECT_EQ(totalMetricCount, 0u);
}

TEST_F(MetricEnumerationTest, givenCorrectRawReportSizeWhenZetMetricGroupCalculateMetricValuesExpIsCalledThenReturnsSuccess) {

    metricsDeviceParams.ConcurrentGroupsCount = 1;

    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";

    Mock<IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.QueryReportSize = 256;
    metricsSetParams.MetricsCount = 11;

    Mock<IMetric_1_0> metric;
    MetricsDiscovery::TMetricParams_1_0 metricParams = {};

    zet_metric_group_handle_t metricGroupHandle = {};

    uint32_t returnedMetricCount = 1;

    openMetricsAdapter();

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

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(metricsSetParams.MetricsCount)
        .WillRepeatedly(Return(&metric));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metric, GetParams())
        .WillRepeatedly(Return(&metricParams));

    EXPECT_CALL(metricsSet, CalculateMetrics(_, _, _, _, _, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<4>(returnedMetricCount), Return(TCompletionCode::CC_OK)));

    uint32_t metricGroupCount = 1;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Raw results.
    const size_t rawResultsSize = 256;
    uint8_t rawResults[rawResultsSize] = {};

    // Valid raw buffer size provided by the user.
    uint32_t dataCount = 0;
    uint32_t totalMetricCount = 0;
    EXPECT_EQ(metricsSetParams.QueryReportSize, rawResultsSize);
    EXPECT_EQ(zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(dataCount, 1u);
    EXPECT_EQ(totalMetricCount, metricsSetParams.MetricsCount);

    // Copy calculated metrics.
    std::vector<uint32_t> metricCounts(dataCount);
    std::vector<zet_typed_value_t> caculatedRawResults(totalMetricCount);
    EXPECT_EQ(zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, metricCounts.data(), caculatedRawResults.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricCounts[0], totalMetricCount);
}

TEST_F(MetricEnumerationTest, givenFailedCalculateMetricsWhenZetMetricGroupCalculateMetricValuesExpIsCalledThenReturnsFail) {

    metricsDeviceParams.ConcurrentGroupsCount = 1;

    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";

    Mock<IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.QueryReportSize = 256;
    metricsSetParams.MetricsCount = 11;

    Mock<IMetric_1_0> metric;
    MetricsDiscovery::TMetricParams_1_0 metricParams = {};

    zet_metric_group_handle_t metricGroupHandle = {};

    openMetricsAdapter();

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

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(metricsSetParams.MetricsCount)
        .WillRepeatedly(Return(&metric));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metric, GetParams())
        .WillRepeatedly(Return(&metricParams));

    EXPECT_CALL(metricsSet, CalculateMetrics(_, _, _, _, _, _, _))
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_ERROR_GENERAL));

    uint32_t metricGroupCount = 1;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Raw results.
    const size_t rawResultsSize = 256;
    uint8_t rawResults[rawResultsSize] = {};

    // Valid raw buffer size provided by the user.
    uint32_t dataCount = 0;
    uint32_t totalMetricCount = 0;
    EXPECT_EQ(metricsSetParams.QueryReportSize, rawResultsSize);
    EXPECT_EQ(zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(dataCount, 1u);
    EXPECT_EQ(totalMetricCount, metricsSetParams.MetricsCount);

    // Copy calculated metrics.
    std::vector<uint32_t> metricCounts(dataCount);
    std::vector<zet_typed_value_t> caculatedRawResults(totalMetricCount);
    EXPECT_EQ(zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, metricCounts.data(), caculatedRawResults.data()), ZE_RESULT_ERROR_UNKNOWN);
    EXPECT_EQ(metricCounts[0], 0u);
}

TEST_F(MetricEnumerationTest, givenInvalidQueryReportSizeWhenZetMetricGroupCalculateMultipleMetricValuesExpIsCalledTwiceThenReturnsFail) {

    metricsDeviceParams.ConcurrentGroupsCount = 1;

    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";

    Mock<IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.QueryReportSize = 0;
    metricsSetParams.MetricsCount = 11;

    Mock<IMetric_1_0> metric;
    MetricsDiscovery::TMetricParams_1_0 metricParams = {};

    zet_metric_group_handle_t metricGroupHandle = {};

    openMetricsAdapter();

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

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(metricsSetParams.MetricsCount)
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

    // Raw results.
    const size_t rawResultsSize = 284;
    uint8_t rawResults[rawResultsSize] = {};
    MetricGroupCalculateHeader *pRawHeader = reinterpret_cast<MetricGroupCalculateHeader *>(rawResults);
    pRawHeader->magic = MetricGroupCalculateHeader::magicValue;
    pRawHeader->dataCount = 1;
    pRawHeader->rawDataOffsets = sizeof(MetricGroupCalculateHeader);
    pRawHeader->rawDataSizes = pRawHeader->rawDataOffsets + sizeof(uint32_t) * pRawHeader->dataCount;
    pRawHeader->rawDataOffset = pRawHeader->rawDataSizes + sizeof(uint32_t) * pRawHeader->dataCount;

    uint32_t *pRawDataOffsets = reinterpret_cast<uint32_t *>(rawResults + pRawHeader->rawDataOffsets);
    uint32_t *pRawDataSizes = reinterpret_cast<uint32_t *>(rawResults + pRawHeader->rawDataSizes);
    pRawDataOffsets[0] = 0;
    pRawDataSizes[0] = static_cast<uint32_t>(rawResultsSize - pRawHeader->rawDataOffset);

    // Invalid raw buffer size provided by the driver.
    uint32_t dataCount = 0;
    uint32_t totalMetricCount = 0;
    EXPECT_EQ(zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_ERROR_UNKNOWN);
    EXPECT_EQ(dataCount, 0u);
    EXPECT_EQ(totalMetricCount, 0u);
}

TEST_F(MetricEnumerationTest, givenCorrectRawDataHeaderWhenZetMetricGroupCalculateMultipleMetricValuesExpIsCalledTwiceThenReturnsSuccess) {

    metricsDeviceParams.ConcurrentGroupsCount = 1;

    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";

    Mock<IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.QueryReportSize = 256;
    metricsSetParams.MetricsCount = 11;

    Mock<IMetric_1_0> metric;
    MetricsDiscovery::TMetricParams_1_0 metricParams = {};

    zet_metric_group_handle_t metricGroupHandle = {};

    uint32_t returnedMetricCount = 1;

    openMetricsAdapter();

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

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(metricsSetParams.MetricsCount)
        .WillRepeatedly(Return(&metric));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metric, GetParams())
        .WillRepeatedly(Return(&metricParams));

    EXPECT_CALL(metricsSet, CalculateMetrics(_, _, _, _, _, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<4>(returnedMetricCount), Return(TCompletionCode::CC_OK)));

    // Metric group handles.
    uint32_t metricGroupCount = 1;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Raw results.
    const size_t rawResultsSize = 284;
    uint8_t rawResults[rawResultsSize] = {};
    MetricGroupCalculateHeader *pRawHeader = reinterpret_cast<MetricGroupCalculateHeader *>(rawResults);
    pRawHeader->magic = MetricGroupCalculateHeader::magicValue;
    pRawHeader->dataCount = 1;
    pRawHeader->rawDataOffsets = sizeof(MetricGroupCalculateHeader);
    pRawHeader->rawDataSizes = pRawHeader->rawDataOffsets + sizeof(uint32_t) * pRawHeader->dataCount;
    pRawHeader->rawDataOffset = pRawHeader->rawDataSizes + sizeof(uint32_t) * pRawHeader->dataCount;

    uint32_t *pRawDataOffsets = reinterpret_cast<uint32_t *>(rawResults + pRawHeader->rawDataOffsets);
    uint32_t *pRawDataSizes = reinterpret_cast<uint32_t *>(rawResults + pRawHeader->rawDataSizes);
    pRawDataOffsets[0] = 0;
    pRawDataSizes[0] = static_cast<uint32_t>(rawResultsSize - pRawHeader->rawDataOffset);

    // Valid raw buffer.
    uint32_t dataCount = 0;
    uint32_t totalMetricCount = 0;
    EXPECT_NE(metricsSetParams.QueryReportSize, rawResultsSize);
    EXPECT_EQ(zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(dataCount, 1u);
    EXPECT_EQ(totalMetricCount, metricsSetParams.MetricsCount);

    // Copy calculated metrics.
    std::vector<uint32_t> metricCounts(dataCount);
    std::vector<zet_typed_value_t> caculatedRawResults(totalMetricCount);
    EXPECT_EQ(zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, metricCounts.data(), caculatedRawResults.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricCounts[0], totalMetricCount);
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

    openMetricsAdapter();

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

    openMetricsAdapter();

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

    openMetricsAdapter();

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

    openMetricsAdapter();

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

    openMetricsAdapter();

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

    openMetricsAdapter();

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

    auto &metricSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    EXPECT_EQ(metricSource.loadDependencies(), true);
    EXPECT_EQ(metricSource.isInitialized(), true);
}

TEST_F(MetricEnumerationTest, givenNotLoadedMetricsLibraryAndDiscoveryWhenLoadDependenciesThenReturnFail) {

    EXPECT_CALL(*mockMetricEnumeration, loadMetricsDiscovery())
        .Times(1)
        .WillOnce(Return(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE));

    auto &metricSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    EXPECT_EQ(metricSource.loadDependencies(), false);
    EXPECT_EQ(metricSource.isInitialized(), false);
}

TEST_F(MetricEnumerationTest, givenRootDeviceWhenLoadDependenciesIsCalledThenLegacyOpenMetricsDeviceWillBeCalled) {

    auto &metricSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    Mock<IAdapterGroup_1_9> mockAdapterGroup;
    Mock<IAdapter_1_9> mockAdapter;
    Mock<IMetricsDevice_1_5> mockDevice;

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricEnumeration, loadMetricsDiscovery())
        .Times(1)
        .WillOnce(Return(ZE_RESULT_SUCCESS));

    EXPECT_CALL(*Mock<MetricEnumeration>::g_mockApi, MockOpenAdapterGroup(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&mockAdapterGroup), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(*mockMetricEnumeration, getMetricsAdapter())
        .Times(1)
        .WillOnce(Return(&mockAdapter));

    EXPECT_CALL(mockAdapter, OpenMetricsDevice(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&mockDevice), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(mockAdapter, CloseMetricsDevice(_))
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(mockDevice, GetParams())
        .Times(1)
        .WillOnce(Return(&metricsDeviceParams));

    EXPECT_CALL(mockAdapterGroup, Close())
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    // Use first sub device.
    device->getMetricDeviceContext().setSubDeviceIndex(0);
    mockMetricsLibrary->initializationState = ZE_RESULT_SUCCESS;

    EXPECT_EQ(metricSource.loadDependencies(), true);
    EXPECT_EQ(metricSource.isInitialized(), true);
    EXPECT_EQ(mockMetricEnumeration->baseIsInitialized(), true);
    EXPECT_EQ(mockMetricEnumeration->cleanupMetricsDiscovery(), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTest, givenSubDeviceWhenLoadDependenciesIsCalledThenOpenMetricsSubDeviceWillBeCalled) {

    auto &metricSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    Mock<IAdapterGroup_1_9> mockAdapterGroup;
    Mock<IAdapter_1_9> mockAdapter;
    Mock<IMetricsDevice_1_5> mockDevice;

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricEnumeration, loadMetricsDiscovery())
        .Times(1)
        .WillOnce(Return(ZE_RESULT_SUCCESS));

    EXPECT_CALL(*Mock<MetricEnumeration>::g_mockApi, MockOpenAdapterGroup(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&mockAdapterGroup), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(*mockMetricEnumeration, getMetricsAdapter())
        .Times(1)
        .WillOnce(Return(&mockAdapter));

    EXPECT_CALL(mockAdapter, OpenMetricsSubDevice(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<1>(&mockDevice), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(mockAdapter, CloseMetricsDevice(_))
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(mockDevice, GetParams())
        .Times(1)
        .WillOnce(Return(&metricsDeviceParams));

    EXPECT_CALL(mockAdapterGroup, Close())
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    // Use second sub device.
    device->getMetricDeviceContext().setSubDeviceIndex(1);
    mockMetricsLibrary->initializationState = ZE_RESULT_SUCCESS;

    EXPECT_EQ(metricSource.loadDependencies(), true);
    EXPECT_EQ(metricSource.isInitialized(), true);
    EXPECT_EQ(mockMetricEnumeration->baseIsInitialized(), true);
    EXPECT_EQ(mockMetricEnumeration->cleanupMetricsDiscovery(), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTest, givenSubDeviceWhenLoadDependenciesIsCalledThenOpenMetricsSubDeviceWillBeCalledWithoutSuccess) {

    auto &metricSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    Mock<IAdapterGroup_1_9> mockAdapterGroup;
    Mock<IAdapter_1_9> mockAdapter;
    Mock<IMetricsDevice_1_5> mockDevice;

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mockMetricEnumeration, loadMetricsDiscovery())
        .Times(1)
        .WillOnce(Return(ZE_RESULT_SUCCESS));

    EXPECT_CALL(*Mock<MetricEnumeration>::g_mockApi, MockOpenAdapterGroup(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&mockAdapterGroup), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(*mockMetricEnumeration, getMetricsAdapter())
        .Times(1)
        .WillOnce(Return(&mockAdapter));

    EXPECT_CALL(mockAdapter, OpenMetricsSubDevice(_, _))
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(mockAdapter, CloseMetricsDevice(_))
        .Times(0);

    EXPECT_CALL(mockDevice, GetParams())
        .Times(0);

    EXPECT_CALL(mockAdapterGroup, Close())
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    // Use second sub device.
    device->getMetricDeviceContext().setSubDeviceIndex(1);
    mockMetricsLibrary->initializationState = ZE_RESULT_SUCCESS;

    EXPECT_EQ(metricSource.loadDependencies(), true);
    EXPECT_EQ(metricSource.isInitialized(), true);
    EXPECT_EQ(mockMetricEnumeration->baseIsInitialized(), false);
}

class MetricEnumerationTestMetricTypes : public MetricEnumerationTest,
                                         public ::testing::WithParamInterface<MetricsDiscovery::TMetricType> {
  public:
    MetricsDiscovery::TMetricType metricType;
    MetricEnumerationTestMetricTypes() {
        metricType = GetParam();
    }
    ~MetricEnumerationTestMetricTypes() override {}
};

TEST_P(MetricEnumerationTestMetricTypes, givenValidMetricTypesWhenSetAndGetIsSameThenReturnSuccess) {

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
    metricParams.MetricType = metricType;
    zet_metric_properties_t metricProperties = {};

    // One api: metric group handle.
    zet_metric_group_handle_t metricGroupHandle = {};

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
    EXPECT_EQ(metricProperties.metricType, static_cast<zet_metric_type_t>(metricType));
    EXPECT_EQ(metricProperties.resultType, ZET_VALUE_TYPE_UINT64);
}

std::vector<MetricsDiscovery::TMetricType>
getListOfMetricTypes() {
    std::vector<MetricsDiscovery::TMetricType> metricTypes = {};
    for (int type = MetricsDiscovery::TMetricType::METRIC_TYPE_DURATION;
         type < MetricsDiscovery::TMetricType::METRIC_TYPE_LAST;
         type++) {
        metricTypes.push_back(static_cast<MetricsDiscovery::TMetricType>(type));
    }
    return metricTypes;
}

INSTANTIATE_TEST_CASE_P(parameterizedMetricEnumerationTestMetricTypes, MetricEnumerationTestMetricTypes,
                        ::testing::ValuesIn(getListOfMetricTypes()));

class MetricEnumerationTestInformationTypes : public MetricEnumerationTest,
                                              public ::testing::WithParamInterface<MetricsDiscovery::TInformationType> {
  public:
    MetricsDiscovery::TInformationType infoType;
    std::unordered_map<MetricsDiscovery::TInformationType, zet_metric_type_t> validate;
    MetricEnumerationTestInformationTypes() {
        infoType = GetParam();
        validate[MetricsDiscovery::TInformationType::INFORMATION_TYPE_REPORT_REASON] = ZET_METRIC_TYPE_EVENT;
        validate[MetricsDiscovery::TInformationType::INFORMATION_TYPE_VALUE] = ZET_METRIC_TYPE_RAW;
        validate[MetricsDiscovery::TInformationType::INFORMATION_TYPE_FLAG] = ZET_METRIC_TYPE_FLAG;
        validate[MetricsDiscovery::TInformationType::INFORMATION_TYPE_TIMESTAMP] = ZET_METRIC_TYPE_TIMESTAMP;
        validate[MetricsDiscovery::TInformationType::INFORMATION_TYPE_CONTEXT_ID_TAG] = ZET_METRIC_TYPE_RAW;
        validate[MetricsDiscovery::TInformationType::INFORMATION_TYPE_SAMPLE_PHASE] = ZET_METRIC_TYPE_RAW;
        validate[MetricsDiscovery::TInformationType::INFORMATION_TYPE_GPU_NODE] = ZET_METRIC_TYPE_RAW;
    }
    ~MetricEnumerationTestInformationTypes() override {}
};

TEST_P(MetricEnumerationTestInformationTypes, givenValidInformationTypesWhenSetAndGetIsSameThenReturnSuccess) {

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
    metricsSetParams.InformationCount = 1;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";

    // Metrics Discovery:: information.
    Mock<MetricsDiscovery::IInformation_1_0> information;
    MetricsDiscovery::TInformationParams_1_0 sourceInformationParams = {};
    sourceInformationParams.SymbolName = "Info symbol name";
    sourceInformationParams.LongName = "Info long name";
    sourceInformationParams.GroupName = "Info group name";
    sourceInformationParams.InfoUnits = "Info Units";
    sourceInformationParams.InfoType = infoType;

    zet_metric_properties_t metricProperties = {};
    // One api: metric group handle.
    zet_metric_group_handle_t metricGroupHandle = {};

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

    EXPECT_CALL(metricsSet, GetInformation(_))
        .Times(1)
        .WillOnce(Return(&information));

    EXPECT_CALL(information, GetParams())
        .Times(1)
        .WillOnce(Return(&sourceInformationParams));

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

    // Obtain information.
    uint32_t metricCount = 0;
    zet_metric_handle_t infoHandle = {};
    EXPECT_EQ(zetMetricGet(metricGroupHandle, &metricCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricCount, 1u);
    EXPECT_EQ(zetMetricGet(metricGroupHandle, &metricCount, &infoHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(infoHandle, nullptr);

    // Obtain information params.
    EXPECT_EQ(zetMetricGetProperties(infoHandle, &metricProperties), ZE_RESULT_SUCCESS);
    EXPECT_EQ(strcmp(metricProperties.name, sourceInformationParams.SymbolName), 0);
    EXPECT_EQ(strcmp(metricProperties.description, sourceInformationParams.LongName), 0);
    EXPECT_EQ(strcmp(metricProperties.component, sourceInformationParams.GroupName), 0);
    EXPECT_EQ(strcmp(metricProperties.resultUnits, sourceInformationParams.InfoUnits), 0);
    EXPECT_EQ(metricProperties.metricType, validate[infoType]);
}

std::vector<MetricsDiscovery::TInformationType>
getListOfInfoTypes() {
    std::vector<MetricsDiscovery::TInformationType> infoTypes = {};
    for (int type = MetricsDiscovery::TInformationType::INFORMATION_TYPE_REPORT_REASON;
         type < MetricsDiscovery::TInformationType::INFORMATION_TYPE_LAST;
         type++) {
        infoTypes.push_back(static_cast<MetricsDiscovery::TInformationType>(type));
    }
    return infoTypes;
}

INSTANTIATE_TEST_CASE_P(parameterizedMetricEnumerationTestInformationTypes,
                        MetricEnumerationTestInformationTypes,
                        ::testing::ValuesIn(getListOfInfoTypes()));

TEST_F(MetricEnumerationTest, givenMetricSetWhenActivateIsCalledActivateReturnsTrue) {

    Mock<MetricsDiscovery::IMetricSet_1_5> metricsSet;
    MetricGroupImpTest metricGroup;

    metricGroup.pReferenceMetricSet = &metricsSet;
    EXPECT_CALL(metricsSet, Activate())
        .WillRepeatedly(Return(MetricsDiscovery::CC_OK));

    EXPECT_EQ(metricGroup.activateMetricSet(), true);
}

TEST_F(MetricEnumerationTest, givenMetricSetWhenActivateIsCalledActivateReturnsFalse) {

    Mock<MetricsDiscovery::IMetricSet_1_5> metricsSet;
    MetricGroupImpTest metricGroup;

    metricGroup.pReferenceMetricSet = &metricsSet;
    EXPECT_CALL(metricsSet, Activate())
        .WillRepeatedly(Return(MetricsDiscovery::CC_ERROR_GENERAL));

    EXPECT_EQ(metricGroup.activateMetricSet(), false);
}

TEST_F(MetricEnumerationTest, givenMetricSetWhenDeactivateIsCalledDeactivateReturnsTrue) {

    Mock<MetricsDiscovery::IMetricSet_1_5> metricsSet;
    MetricGroupImpTest metricGroup;

    metricGroup.pReferenceMetricSet = &metricsSet;
    EXPECT_CALL(metricsSet, Deactivate())
        .WillRepeatedly(Return(MetricsDiscovery::CC_OK));

    EXPECT_EQ(metricGroup.deactivateMetricSet(), true);
}

TEST_F(MetricEnumerationTest, givenMetricSetWhenDeactivateIsCalledDeactivateReturnsFalse) {

    Mock<MetricsDiscovery::IMetricSet_1_5> metricsSet;
    MetricGroupImpTest metricGroup;

    metricGroup.pReferenceMetricSet = &metricsSet;
    EXPECT_CALL(metricsSet, Deactivate())
        .WillRepeatedly(Return(MetricsDiscovery::CC_ERROR_GENERAL));

    EXPECT_EQ(metricGroup.deactivateMetricSet(), false);
}

TEST_F(MetricEnumerationTest, givenMetricSetWhenWaitForReportsIsCalledWaitForReportsReturnsSuccess) {

    Mock<MetricsDiscovery::IConcurrentGroup_1_5> concurrentGroup;
    MetricGroupImpTest metricGroup;

    metricGroup.pReferenceConcurrentGroup = &concurrentGroup;
    EXPECT_CALL(concurrentGroup, WaitForReports(_))
        .WillRepeatedly(Return(MetricsDiscovery::TCompletionCode::CC_OK));

    uint32_t timeout = 1;
    EXPECT_EQ(metricGroup.waitForReports(timeout), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTest, givenMetricSetWhenWaitForReportsIsCalledWaitForReportsReturnsNotReady) {

    Mock<MetricsDiscovery::IConcurrentGroup_1_5> concurrentGroup;
    MetricGroupImpTest metricGroup;

    metricGroup.pReferenceConcurrentGroup = &concurrentGroup;
    EXPECT_CALL(concurrentGroup, WaitForReports(_))
        .WillRepeatedly(Return(MetricsDiscovery::TCompletionCode::CC_ERROR_GENERAL));

    uint32_t timeout = 1;
    EXPECT_EQ(metricGroup.waitForReports(timeout), ZE_RESULT_NOT_READY);
}

TEST_F(MetricEnumerationTest, givenTimeAndBufferSizeWhenOpenIoStreamReturnsErrorThenTheMetricGroupOpenIoStreamReturnsErrorUnknown) {

    Mock<MetricsDiscovery::IConcurrentGroup_1_5> concurrentGroup;
    MetricGroupImpTest metricGroup;

    metricGroup.pReferenceConcurrentGroup = &concurrentGroup;
    EXPECT_CALL(concurrentGroup, OpenIoStream(_, _, _, _))
        .WillRepeatedly(Return(MetricsDiscovery::CC_ERROR_GENERAL));

    uint32_t timerPeriodNs = 1;
    uint32_t oaBufferSize = 100;

    EXPECT_EQ(metricGroup.openIoStream(timerPeriodNs, oaBufferSize), ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(MetricEnumerationTest, givenReportCountAndReportDataWhenReadIoStreamReturnsOkTheMetricGroupReadIoStreamReturnsSuccess) {

    Mock<MetricsDiscovery::IConcurrentGroup_1_5> concurrentGroup;
    MetricGroupImpTest metricGroup;

    metricGroup.pReferenceConcurrentGroup = &concurrentGroup;
    EXPECT_CALL(concurrentGroup, ReadIoStream(_, _, _))
        .WillOnce(Return(MetricsDiscovery::CC_OK));

    uint32_t reportCount = 1;
    uint8_t reportData = 0;

    EXPECT_EQ(metricGroup.readIoStream(reportCount, reportData), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTest, givenReportCountAndReportDataWhenReadIoStreamReturnsPendingTheMetricGroupReadIoStreamReturnsSuccess) {

    Mock<MetricsDiscovery::IConcurrentGroup_1_5> concurrentGroup;
    MetricGroupImpTest metricGroup;

    metricGroup.pReferenceConcurrentGroup = &concurrentGroup;
    EXPECT_CALL(concurrentGroup, ReadIoStream(_, _, _))
        .WillOnce(Return(MetricsDiscovery::CC_READ_PENDING));

    uint32_t reportCount = 1;
    uint8_t reportData = 0;

    EXPECT_EQ(metricGroup.readIoStream(reportCount, reportData), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTest, givenReportCountAndReportDataWhenReadIoStreamReturnsErrorThenMetrigGroupReadIoStreamReturnsError) {

    Mock<MetricsDiscovery::IConcurrentGroup_1_5> concurrentGroup;
    MetricGroupImpTest metricGroup;

    metricGroup.pReferenceConcurrentGroup = &concurrentGroup;
    EXPECT_CALL(concurrentGroup, ReadIoStream(_, _, _))
        .WillOnce(Return(MetricsDiscovery::CC_ERROR_GENERAL));

    uint32_t reportCount = 1;
    uint8_t reportData = 0;

    EXPECT_EQ(metricGroup.readIoStream(reportCount, reportData), ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(MetricEnumerationTest, givenTimeAndBufferSizeWhenCloseIoStreamIsCalledCloseAndFailThenIoStreamReturnsErrorUnknown) {

    Mock<MetricsDiscovery::IConcurrentGroup_1_5> concurrentGroup;
    MetricGroupImpTest metricGroup;

    metricGroup.pReferenceConcurrentGroup = &concurrentGroup;
    EXPECT_CALL(concurrentGroup, CloseIoStream())
        .WillRepeatedly(Return(MetricsDiscovery::CC_ERROR_GENERAL));

    EXPECT_EQ(metricGroup.closeIoStream(), ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(MetricEnumerationTest, givenTTypedValueWhenCopyValueIsCalledReturnsFilledZetTypedValue) {

    MetricsDiscovery::TTypedValue_1_0 source = {};
    zet_typed_value_t destination = {};
    MetricGroupImpTest metricGroup = {};

    for (int vType = MetricsDiscovery::VALUE_TYPE_UINT32;
         vType < MetricsDiscovery::VALUE_TYPE_LAST; vType++) {
        source.ValueType = static_cast<MetricsDiscovery::TValueType>(vType);
        if (vType != MetricsDiscovery::VALUE_TYPE_BOOL)
            source.ValueUInt64 = 0xFF;
        else
            source.ValueBool = true;

        metricGroup.copyValue(const_cast<MetricsDiscovery::TTypedValue_1_0 &>(source), destination);
        switch (vType) {
        case MetricsDiscovery::VALUE_TYPE_UINT32:
            EXPECT_EQ(destination.type, ZET_VALUE_TYPE_UINT32);
            EXPECT_EQ(destination.value.ui32, source.ValueUInt32);
            break;

        case MetricsDiscovery::VALUE_TYPE_UINT64:
            EXPECT_EQ(destination.type, ZET_VALUE_TYPE_UINT64);
            EXPECT_EQ(destination.value.ui64, source.ValueUInt64);
            break;

        case MetricsDiscovery::VALUE_TYPE_FLOAT:
            EXPECT_EQ(destination.type, ZET_VALUE_TYPE_FLOAT32);
            EXPECT_EQ(destination.value.fp32, source.ValueFloat);
            break;

        case MetricsDiscovery::VALUE_TYPE_BOOL:
            EXPECT_EQ(destination.type, ZET_VALUE_TYPE_BOOL8);
            EXPECT_EQ(destination.value.b8, source.ValueBool);
            break;

        default:
            EXPECT_EQ(destination.type, ZET_VALUE_TYPE_UINT64);
            EXPECT_EQ(destination.value.ui64, static_cast<uint64_t>(0));
            break;
        }
    }
}

} // namespace ult
} // namespace L0
