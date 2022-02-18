/*
 * Copyright (C) 2021-2022 Intel Corporation
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

using ::testing::_;
using ::testing::Return;

namespace L0 {
namespace ult {

using MetricEnumerationMultiDeviceTest = Test<MetricMultiDeviceFixture>;

TEST_F(MetricEnumerationMultiDeviceTest, givenRootDeviceWhenLoadDependenciesIsCalledThenOpenMetricsSubDeviceWillBeCalled) {

    // Use first root device.
    auto &metricSource = devices[0]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());
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
        .Times(subDeviceCount)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<1>(&mockDevice), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(mockAdapter, CloseMetricsDevice(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(mockDevice, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsDeviceParams));

    EXPECT_CALL(mockAdapterGroup, Close())
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    // Use root device.
    devices[0]->getMetricDeviceContext().setSubDeviceIndex(0);
    mockMetricsLibrary->initializationState = ZE_RESULT_SUCCESS;

    EXPECT_EQ(metricSource.loadDependencies(), true);
    EXPECT_EQ(metricSource.isInitialized(), true);
    EXPECT_EQ(mockMetricEnumeration->baseIsInitialized(), true);
    EXPECT_EQ(mockMetricEnumeration->cleanupMetricsDiscovery(), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationMultiDeviceTest, givenRootDeviceWhenLoadDependenciesIsCalledThenOpenMetricsSubDeviceWillBeCalledWithoutSuccess) {

    // Use first root device.
    auto &metricSource = devices[0]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
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
        .WillOnce(Return(TCompletionCode::CC_ERROR_GENERAL));

    EXPECT_CALL(mockAdapter, CloseMetricsDevice(_))
        .Times(0);

    EXPECT_CALL(mockDevice, GetParams())
        .Times(0);

    EXPECT_CALL(mockAdapterGroup, Close())
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    // Use root device.
    devices[0]->getMetricDeviceContext().setSubDeviceIndex(0);
    mockMetricsLibrary->initializationState = ZE_RESULT_SUCCESS;

    EXPECT_EQ(metricSource.loadDependencies(), true);
    EXPECT_EQ(metricSource.isInitialized(), true);
    EXPECT_EQ(mockMetricEnumeration->baseIsInitialized(), false);
}

TEST_F(MetricEnumerationMultiDeviceTest, givenIncorrectMetricsDiscoveryInterfaceVersionWhenZetGetMetricGroupIsCalledThenReturnsFail) {

    metricsDeviceParams.Version.MajorNumber = 0;
    metricsDeviceParams.Version.MinorNumber = 1;

    openMetricsAdapter();

    EXPECT_CALL(metricsDevice, GetParams())
        .Times(1)
        .WillOnce(Return(&metricsDeviceParams));

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 0u);
}

TEST_F(MetricEnumerationMultiDeviceTest, givenValidArgumentsWhenZetMetricGetPropertiestIsCalledThenReturnSuccess) {

    // Use first root device.
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
    zet_metric_properties_t metricProperties = {};

    // One api: metric group handle.
    zet_metric_group_handle_t metricGroupHandle = {};
    zet_metric_group_properties_t metricGroupProperties = {};

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

    // Metric group properties.
    EXPECT_EQ(zetMetricGroupGetProperties(metricGroupHandle, &metricGroupProperties), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupProperties.domain, 0u);
    EXPECT_EQ(metricGroupProperties.samplingType, ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED);
    EXPECT_EQ(metricGroupProperties.metricCount, metricsSetParams.MetricsCount);
    EXPECT_EQ(strcmp(metricGroupProperties.description, metricsSetParams.ShortName), 0);
    EXPECT_EQ(strcmp(metricGroupProperties.name, metricsSetParams.SymbolName), 0);

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

TEST_F(MetricEnumerationMultiDeviceTest, givenCorrectRawDataHeaderWhenZetMetricGroupCalculateMetricValuesExpIsCalledTwiceThenReturnsSuccess) {

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

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
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroup));

    EXPECT_CALL(metricsConcurrentGroup, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(metricsSetParams.MetricsCount * subDeviceCount)
        .WillRepeatedly(Return(&metric));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metric, GetParams())
        .WillRepeatedly(Return(&metricParams));

    EXPECT_CALL(metricsSet, CalculateMetrics(_, _, _, _, _, _, _))
        .Times(subDeviceCount)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<4>(returnedMetricCount), Return(TCompletionCode::CC_OK)));

    // Metric group handles.
    uint32_t metricGroupCount = 1;
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Raw results.
    const size_t rawResultsSize = 560;
    uint8_t rawResults[rawResultsSize] = {};
    MetricGroupCalculateHeader *pRawHeader = reinterpret_cast<MetricGroupCalculateHeader *>(rawResults);

    pRawHeader->magic = MetricGroupCalculateHeader::magicValue;
    pRawHeader->dataCount = subDeviceCount;
    pRawHeader->rawDataOffsets = sizeof(MetricGroupCalculateHeader);
    pRawHeader->rawDataSizes = pRawHeader->rawDataOffsets + sizeof(uint32_t) * pRawHeader->dataCount;
    pRawHeader->rawDataOffset = pRawHeader->rawDataSizes + sizeof(uint32_t) * pRawHeader->dataCount;

    uint32_t *pRawDataOffsets = reinterpret_cast<uint32_t *>(rawResults + pRawHeader->rawDataOffsets);
    uint32_t *pRawDataSizes = reinterpret_cast<uint32_t *>(rawResults + pRawHeader->rawDataSizes);
    pRawDataOffsets[0] = 0;
    pRawDataOffsets[1] = metricsSetParams.QueryReportSize;
    pRawDataSizes[0] = metricsSetParams.QueryReportSize;
    pRawDataSizes[1] = metricsSetParams.QueryReportSize;

    // Valid raw buffer.
    uint32_t dataCount = 0;
    uint32_t totalMetricCount = 0;
    EXPECT_NE(metricsSetParams.QueryReportSize, rawResultsSize);
    EXPECT_EQ(zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(dataCount, subDeviceCount);
    EXPECT_EQ(totalMetricCount, subDeviceCount * metricsSetParams.MetricsCount);

    // Copy calculated metrics.
    std::vector<uint32_t> metricCounts(dataCount);
    std::vector<zet_typed_value_t> caculatedRawResults(totalMetricCount);
    EXPECT_EQ(zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, metricCounts.data(), caculatedRawResults.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricCounts[0], metricsSetParams.MetricsCount);
    EXPECT_EQ(metricCounts[1], metricsSetParams.MetricsCount);
}

TEST_F(MetricEnumerationMultiDeviceTest, givenInvalidDataCountAndTotalMetricCountWhenZetMetricGroupCalculateMetricValuesExpIsCalledThenReturnsCorrectDataCountAndTotalMetricCount) {

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

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
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroup));

    EXPECT_CALL(metricsConcurrentGroup, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(metricsSetParams.MetricsCount * subDeviceCount)
        .WillRepeatedly(Return(&metric));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metric, GetParams())
        .WillRepeatedly(Return(&metricParams));

    // Metric group handles.
    uint32_t metricGroupCount = 1;
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Raw results.
    const size_t rawResultsSize = 560;
    uint8_t rawResults[rawResultsSize] = {};
    MetricGroupCalculateHeader *pRawHeader = reinterpret_cast<MetricGroupCalculateHeader *>(rawResults);

    pRawHeader->magic = MetricGroupCalculateHeader::magicValue;
    pRawHeader->dataCount = subDeviceCount;
    pRawHeader->rawDataOffsets = sizeof(MetricGroupCalculateHeader);
    pRawHeader->rawDataSizes = pRawHeader->rawDataOffsets + sizeof(uint32_t) * pRawHeader->dataCount;
    pRawHeader->rawDataOffset = pRawHeader->rawDataSizes + sizeof(uint32_t) * pRawHeader->dataCount;

    uint32_t *pRawDataOffsets = reinterpret_cast<uint32_t *>(rawResults + pRawHeader->rawDataOffsets);
    uint32_t *pRawDataSizes = reinterpret_cast<uint32_t *>(rawResults + pRawHeader->rawDataSizes);
    pRawDataOffsets[0] = 0;
    pRawDataOffsets[1] = metricsSetParams.QueryReportSize;
    pRawDataSizes[0] = metricsSetParams.QueryReportSize;
    pRawDataSizes[1] = metricsSetParams.QueryReportSize;

    // Valid raw buffer. Invalid data count.
    uint32_t dataCount = 1000;
    uint32_t totalMetricCount = 0;
    EXPECT_NE(metricsSetParams.QueryReportSize, rawResultsSize);
    EXPECT_EQ(zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(subDeviceCount, dataCount);
    EXPECT_EQ(subDeviceCount * metricsSetParams.MetricsCount, totalMetricCount);

    // Valid raw buffer. Invalid total metric count.
    dataCount = 0;
    totalMetricCount = 1000;
    EXPECT_NE(metricsSetParams.QueryReportSize, rawResultsSize);
    EXPECT_EQ(zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(subDeviceCount, dataCount);
    EXPECT_EQ(subDeviceCount * metricsSetParams.MetricsCount, totalMetricCount);
}

TEST_F(MetricEnumerationMultiDeviceTest, givenInvalidQueryReportSizeWhenZetMetricGroupCalculateMetricValuesExpIsCalledThenReturnsFail) {

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

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
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroup));

    EXPECT_CALL(metricsConcurrentGroup, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(metricsSetParams.MetricsCount * subDeviceCount)
        .WillRepeatedly(Return(&metric));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metric, GetParams())
        .WillRepeatedly(Return(&metricParams));

    // Metric group handles.
    uint32_t metricGroupCount = 1;
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Raw results.
    const size_t rawResultsSize = 560;
    uint8_t rawResults[rawResultsSize] = {};
    MetricGroupCalculateHeader *pRawHeader = reinterpret_cast<MetricGroupCalculateHeader *>(rawResults);

    pRawHeader->magic = MetricGroupCalculateHeader::magicValue;
    pRawHeader->dataCount = subDeviceCount;
    pRawHeader->rawDataOffsets = sizeof(MetricGroupCalculateHeader);
    pRawHeader->rawDataSizes = pRawHeader->rawDataOffsets + sizeof(uint32_t) * pRawHeader->dataCount;
    pRawHeader->rawDataOffset = pRawHeader->rawDataSizes + sizeof(uint32_t) * pRawHeader->dataCount;

    uint32_t *pRawDataOffsets = reinterpret_cast<uint32_t *>(rawResults + pRawHeader->rawDataOffsets);
    uint32_t *pRawDataSizes = reinterpret_cast<uint32_t *>(rawResults + pRawHeader->rawDataSizes);
    pRawDataOffsets[0] = 0;
    pRawDataOffsets[1] = metricsSetParams.QueryReportSize;
    pRawDataSizes[0] = metricsSetParams.QueryReportSize;
    pRawDataSizes[1] = metricsSetParams.QueryReportSize;

    // Valid raw buffer. Invalid query report size.
    uint32_t dataCount = 0;
    uint32_t totalMetricCount = 0;
    EXPECT_NE(metricsSetParams.QueryReportSize, rawResultsSize);
    EXPECT_EQ(zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_ERROR_UNKNOWN);
    EXPECT_EQ(dataCount, 0u);
    EXPECT_EQ(totalMetricCount, 0u);
}

TEST_F(MetricEnumerationMultiDeviceTest, givenErrorGeneralOnCalculateMetricsWhenZetMetricGroupCalculateMetricValuesExpIsCalledThenReturnsFail) {

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

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
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroup));

    EXPECT_CALL(metricsConcurrentGroup, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(metricsSetParams.MetricsCount * subDeviceCount)
        .WillRepeatedly(Return(&metric));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metric, GetParams())
        .WillRepeatedly(Return(&metricParams));

    EXPECT_CALL(metricsSet, CalculateMetrics(_, _, _, _, _, _, _))
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_ERROR_GENERAL));

    // Metric group handles.
    uint32_t metricGroupCount = 1;
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Raw results.
    const size_t rawResultsSize = 560;
    uint8_t rawResults[rawResultsSize] = {};
    MetricGroupCalculateHeader *pRawHeader = reinterpret_cast<MetricGroupCalculateHeader *>(rawResults);

    pRawHeader->magic = MetricGroupCalculateHeader::magicValue;
    pRawHeader->dataCount = subDeviceCount;
    pRawHeader->rawDataOffsets = sizeof(MetricGroupCalculateHeader);
    pRawHeader->rawDataSizes = pRawHeader->rawDataOffsets + sizeof(uint32_t) * pRawHeader->dataCount;
    pRawHeader->rawDataOffset = pRawHeader->rawDataSizes + sizeof(uint32_t) * pRawHeader->dataCount;

    uint32_t *pRawDataOffsets = reinterpret_cast<uint32_t *>(rawResults + pRawHeader->rawDataOffsets);
    uint32_t *pRawDataSizes = reinterpret_cast<uint32_t *>(rawResults + pRawHeader->rawDataSizes);
    pRawDataOffsets[0] = 0;
    pRawDataOffsets[1] = metricsSetParams.QueryReportSize;
    pRawDataSizes[0] = metricsSetParams.QueryReportSize;
    pRawDataSizes[1] = metricsSetParams.QueryReportSize;

    // Valid raw buffer.
    uint32_t dataCount = 0;
    uint32_t totalMetricCount = 0;
    EXPECT_NE(metricsSetParams.QueryReportSize, rawResultsSize);
    EXPECT_EQ(zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(dataCount, subDeviceCount);
    EXPECT_EQ(totalMetricCount, subDeviceCount * metricsSetParams.MetricsCount);

    // Copy calculated metrics. CalculateMetrics returns CC_ERROR_GENERAL for first sub device.
    std::vector<uint32_t> metricCounts(dataCount);
    std::vector<zet_typed_value_t> caculatedRawResults(totalMetricCount);
    EXPECT_EQ(zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, metricCounts.data(), caculatedRawResults.data()), ZE_RESULT_ERROR_UNKNOWN);
    EXPECT_EQ(metricCounts[0], 0u);
    EXPECT_EQ(metricCounts[1], 0u);
}

TEST_F(MetricEnumerationMultiDeviceTest, givenCorrectRawDataHeaderWhenZetMetricGroupCalculateMetricValuesIsCalledThenReturnsFail) {

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

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
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroup));

    EXPECT_CALL(metricsConcurrentGroup, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsConcurrentGroupParams));

    EXPECT_CALL(metricsConcurrentGroup, GetMetricSet(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricsSet));

    EXPECT_CALL(metricsSet, GetParams())
        .WillRepeatedly(Return(&metricsSetParams));

    EXPECT_CALL(metricsSet, GetMetric(_))
        .Times(metricsSetParams.MetricsCount * subDeviceCount)
        .WillRepeatedly(Return(&metric));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(metric, GetParams())
        .WillRepeatedly(Return(&metricParams));

    // Metric group handles.
    uint32_t metricGroupCount = 1;
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Raw results.
    const size_t rawResultsSize = 560;
    uint8_t rawResults[rawResultsSize] = {};
    MetricGroupCalculateHeader *pRawHeader = reinterpret_cast<MetricGroupCalculateHeader *>(rawResults);

    pRawHeader->magic = MetricGroupCalculateHeader::magicValue;
    pRawHeader->dataCount = subDeviceCount;
    pRawHeader->rawDataOffsets = sizeof(MetricGroupCalculateHeader);
    pRawHeader->rawDataSizes = pRawHeader->rawDataOffsets + sizeof(uint32_t) * pRawHeader->dataCount;
    pRawHeader->rawDataOffset = pRawHeader->rawDataSizes + sizeof(uint32_t) * pRawHeader->dataCount;

    uint32_t *pRawDataOffsets = reinterpret_cast<uint32_t *>(rawResults + pRawHeader->rawDataOffsets);
    uint32_t *pRawDataSizes = reinterpret_cast<uint32_t *>(rawResults + pRawHeader->rawDataSizes);
    pRawDataOffsets[0] = 0;
    pRawDataOffsets[1] = metricsSetParams.QueryReportSize;
    pRawDataSizes[0] = metricsSetParams.QueryReportSize;
    pRawDataSizes[1] = metricsSetParams.QueryReportSize;

    // Valid raw buffer for zetMetricGroupCalculateMultipleMetricValuesExp.
    uint32_t metricCount = 0;
    EXPECT_NE(metricsSetParams.QueryReportSize, rawResultsSize);
    EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &metricCount, nullptr), ZE_RESULT_ERROR_UNKNOWN);
    EXPECT_EQ(metricCount, 0u);
}

} // namespace ult
} // namespace L0
