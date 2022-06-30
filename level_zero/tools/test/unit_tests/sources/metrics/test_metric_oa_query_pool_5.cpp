/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/metric_query_pool_fixture.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace L0 {
namespace ult {
TEST_F(MultiDeviceMetricQueryPoolTest, givenUninitializedMetricsLibraryWhenGetGpuCommandsIsCalledThenReturnsFail) {

    auto &metricSource = devices[0]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    auto &metricsLibrary = metricSource.getMetricsLibrary();
    CommandBufferData_1_0 commandBuffer = {};

    EXPECT_CALL(*mockMetricEnumeration, isInitialized())
        .Times(1)
        .WillOnce(Return(false));

    const bool result = metricsLibrary.getGpuCommands(commandBuffer);

    EXPECT_EQ(result, false);
}

TEST_F(MultiDeviceMetricQueryPoolTest, givenValidArgumentsWhenZetMetricGroupCalculateMetricValuesExpThenReturnsSuccess) {

    zet_device_handle_t metricDevice = devices[0]->toHandle();
    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

    metricsDeviceParams.ConcurrentGroupsCount = 1;

    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
    metricsConcurrentGroupParams.Description = "OA description";

    Mock<MetricsDiscovery::IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.QueryReportSize = 256;
    metricsSetParams.MetricsCount = 1;

    Mock<IMetric_1_0> metric;
    TMetricParams_1_0 metricParams = {};
    metricParams.SymbolName = "Metric symbol name";
    metricParams.ShortName = "Metric short name";
    metricParams.LongName = "Metric long name";
    metricParams.ResultType = MetricsDiscovery::TMetricResultType::RESULT_UINT64;
    metricParams.MetricType = MetricsDiscovery::TMetricType::METRIC_TYPE_RATIO;

    zet_metric_group_handle_t metricGroupHandle = {};

    zet_metric_group_properties_t metricGroupProperties = {};
    metricGroupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;

    zet_metric_query_handle_t queryHandle = {};
    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};
    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE;

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;

    QueryHandle_1_0 metricsLibraryQueryHandle = {&value};
    ContextHandle_1_0 metricsLibraryContextHandle = {&value};

    CommandBufferSize_1_0 commandBufferSize = {};
    commandBufferSize.GpuMemorySize = 100;

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

    for (uint32_t i = 0; i < subDeviceCount; ++i) {
        EXPECT_CALL(*mockMetricEnumerationSubDevices[i], isInitialized())
            .Times(1)
            .WillOnce(Return(true));

        EXPECT_CALL(*mockMetricsLibrarySubDevices[i], getContextData(_, _))
            .Times(1)
            .WillOnce(Return(true));
    }

    EXPECT_CALL(*mockMetricsLibrarySubDevices[0], getMetricQueryReportSize(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgReferee<0>(metricsSetParams.QueryReportSize), Return(true)));

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryCreate(_, _))
        .Times(subDeviceCount)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<1>(metricsLibraryQueryHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryDelete(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetParameter(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(value), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(subDeviceCount)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetData(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_CALL(metricsSet, CalculateMetrics(_, _, _, _, _, _, _))
        .Times(subDeviceCount)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<4>(returnedMetricCount), Return(TCompletionCode::CC_OK)));

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Create metric query pool.
    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroupHandle, &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(poolHandle, nullptr);

    // Create metric query.
    EXPECT_EQ(zetMetricQueryCreate(poolHandle, 0, &queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(queryHandle, nullptr);

    // Get desired raw data size.
    size_t rawSize = 0;
    EXPECT_EQ(zetMetricQueryGetData(queryHandle, &rawSize, nullptr), ZE_RESULT_SUCCESS);
    const size_t expectedRawSize = (metricsSetParams.QueryReportSize * subDeviceCount) + sizeof(MetricGroupCalculateHeader) + (2 * sizeof(uint32_t) * subDeviceCount);
    EXPECT_EQ(rawSize, expectedRawSize);

    // Get data.
    std::vector<uint8_t> rawData;
    rawData.resize(rawSize);
    EXPECT_EQ(zetMetricQueryGetData(queryHandle, &rawSize, rawData.data()), ZE_RESULT_SUCCESS);

    uint32_t dataCount = 0;
    uint32_t totalMetricCount = 0;
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawSize, rawData.data(), &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(dataCount, subDeviceCount);
    EXPECT_EQ(totalMetricCount, subDeviceCount * metricsSetParams.MetricsCount);

    std::vector<uint32_t> metricCounts(dataCount);
    std::vector<zet_typed_value_t> caculatedRawResults(totalMetricCount);
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawSize, rawData.data(), &dataCount, &totalMetricCount, metricCounts.data(), caculatedRawResults.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricCounts[0], metricsSetParams.MetricsCount);
    EXPECT_EQ(metricCounts[1], metricsSetParams.MetricsCount);

    // Destroy query and its pool.
    EXPECT_EQ(zetMetricQueryDestroy(queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MultiDeviceMetricQueryPoolTest, givenCorrectArgumentsWhenActivateMetricGroupsIsCalledThenReturnsSuccess) {

    zet_device_handle_t metricDevice = devices[0]->toHandle();
    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

    metricsDeviceParams.ConcurrentGroupsCount = 1;

    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
    metricsConcurrentGroupParams.Description = "OA description";

    Mock<MetricsDiscovery::IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.MetricsCount = 1;

    Mock<IMetric_1_0> metric;
    TMetricParams_1_0 metricParams = {};
    metricParams.SymbolName = "Metric symbol name";
    metricParams.ShortName = "Metric short name";
    metricParams.LongName = "Metric long name";
    metricParams.ResultType = MetricsDiscovery::TMetricResultType::RESULT_UINT64;
    metricParams.MetricType = MetricsDiscovery::TMetricType::METRIC_TYPE_RATIO;

    zet_metric_group_handle_t metricGroupHandle = {};

    zet_metric_group_properties_t metricGroupProperties = {};
    metricGroupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;

    ConfigurationHandle_1_0 metricsLibraryConfigurationHandle = {&value};
    ContextHandle_1_0 metricsLibraryContextHandle = {&value};

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

    EXPECT_CALL(metricsSet, Activate())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(MetricsDiscovery::CC_OK));

    EXPECT_CALL(metricsSet, Deactivate())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(MetricsDiscovery::CC_OK));

    EXPECT_CALL(metric, GetParams())
        .Times(subDeviceCount)
        .WillRepeatedly(Return(&metricParams));

    EXPECT_CALL(metricsSet, SetApiFiltering(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    for (uint32_t i = 0; i < subDeviceCount; ++i) {
        EXPECT_CALL(*mockMetricEnumerationSubDevices[i], isInitialized())
            .Times(1)
            .WillOnce(Return(true));

        EXPECT_CALL(*mockMetricsLibrarySubDevices[i], getContextData(_, _))
            .Times(1)
            .WillOnce(Return(true));
    }

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(subDeviceCount)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockConfigurationCreate(_, _))
        .Times(subDeviceCount)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<1>(metricsLibraryConfigurationHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockConfigurationActivate(_, _))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockConfigurationDeactivate(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockConfigurationDelete(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(StatusCode::Success));

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Activate metric group (deferred).
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), devices[0]->toHandle(), 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    // Activate metric groups.
    devices[0]->activateMetricGroups();

    // Deactivate metric groups.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), metricDevice, 0, nullptr), ZE_RESULT_SUCCESS);
}

TEST_F(MultiDeviceMetricQueryPoolTest, givenMetricQueryPoolIsDestroyedWhenMetricsLibraryIsReleasedThenImplicitScalingStatusIsNotModified) {

    zet_device_handle_t metricDevice = devices[0]->toHandle();
    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

    metricsDeviceParams.ConcurrentGroupsCount = 1;

    Mock<IConcurrentGroup_1_5> metricsConcurrentGroup;
    TConcurrentGroupParams_1_0 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
    metricsConcurrentGroupParams.Description = "OA description";

    Mock<MetricsDiscovery::IMetricSet_1_5> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_4 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.MetricsCount = 1;

    Mock<IMetric_1_0> metric;
    TMetricParams_1_0 metricParams = {};
    metricParams.SymbolName = "Metric symbol name";
    metricParams.ShortName = "Metric short name";
    metricParams.LongName = "Metric long name";
    metricParams.ResultType = MetricsDiscovery::TMetricResultType::RESULT_UINT64;
    metricParams.MetricType = MetricsDiscovery::TMetricType::METRIC_TYPE_RATIO;

    zet_metric_group_handle_t metricGroupHandle = {};

    zet_metric_group_properties_t metricGroupProperties = {};
    metricGroupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;

    zet_metric_query_handle_t queryHandle = {};
    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};
    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE;

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;

    QueryHandle_1_0 metricsLibraryQueryHandle = {&value};
    ContextHandle_1_0 metricsLibraryContextHandle = {&value};

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

    for (uint32_t i = 0; i < subDeviceCount; ++i) {
        EXPECT_CALL(*mockMetricEnumerationSubDevices[i], isInitialized())
            .Times(1)
            .WillOnce(Return(true));

        EXPECT_CALL(*mockMetricsLibrarySubDevices[i], getContextData(_, _))
            .Times(1)
            .WillOnce(Return(true));
    }

    EXPECT_CALL(*mockMetricsLibrary, load())
        .Times(0);

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryCreate(_, _))
        .Times(subDeviceCount)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<1>(metricsLibraryQueryHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockQueryDelete(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(StatusCode::Success));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockGetParameter(_, _, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<2>(value), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextCreate(_, _, _))
        .Times(subDeviceCount)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<2>(metricsLibraryContextHandle), Return(StatusCode::Success)));

    EXPECT_CALL(*mockMetricsLibrary->g_mockApi, MockContextDelete(_))
        .Times(subDeviceCount)
        .WillRepeatedly(Return(StatusCode::Success));

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    auto &metricSource = deviceImp.subDevices[0]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    auto &metricsLibrary = metricSource.getMetricsLibrary();
    auto dummy = ClientOptionsData_1_0{};
    auto workloadPartition = ClientOptionsData_1_0{};

    // Verify that workload partition is set, before Metrics Library release
    metricsLibrary.getSubDeviceClientOptions(dummy, dummy, dummy, workloadPartition);
    EXPECT_EQ(workloadPartition.Type, MetricsLibraryApi::ClientOptionsType::WorkloadPartition);
    EXPECT_EQ(workloadPartition.WorkloadPartition.Enabled, true);

    // Initiate a Metrics Library Release by releaing Metric Query Pool
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), devices[0]->toHandle(), 1, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroupHandle, &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryCreate(poolHandle, 0, &queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), devices[0]->toHandle(), 0, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryDestroy(queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);

    // Verify again that workload partition is not reset
    metricsLibrary.getSubDeviceClientOptions(dummy, dummy, dummy, workloadPartition);
    EXPECT_EQ(workloadPartition.Type, MetricsLibraryApi::ClientOptionsType::WorkloadPartition);
    EXPECT_EQ(workloadPartition.WorkloadPartition.Enabled, true);
}

} // namespace ult
} // namespace L0
