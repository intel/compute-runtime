/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/metric_query_pool_fixture.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {
TEST_F(MultiDeviceMetricQueryPoolTest, givenUninitializedMetricsLibraryWhenGetGpuCommandsIsCalledThenReturnsFail) {

    auto &metricSource = devices[0]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    auto &metricsLibrary = metricSource.getMetricsLibrary();
    CommandBufferData_1_0 commandBuffer = {};

    mockMetricEnumeration->isInitializedResult = false;

    const bool result = metricsLibrary.getGpuCommands(commandBuffer);

    EXPECT_EQ(result, false);
}

TEST_F(MultiDeviceMetricQueryPoolTest, givenValidArgumentsWhenZetMetricGroupCalculateMetricValuesExpThenReturnsSuccess) {

    zet_device_handle_t metricDevice = devices[0]->toHandle();
    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

    metricsDeviceParams.ConcurrentGroupsCount = 1;

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
    metricsConcurrentGroup.GetIoMeasurementInformationResult = &ioMeasurement;
    ioMeasurement.GetParamsResult = &oaInformation;

    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.QueryReportSize = 256;
    metricsSetParams.MetricsCount = 1;

    Mock<IMetric_1_13> metric;
    TMetricParams_1_13 metricParams = {};
    metricParams.SymbolName = "Metric symbol name";
    metricParams.ShortName = "Metric short name";
    metricParams.LongName = "Metric long name";
    metricParams.ResultType = MetricsDiscovery::TMetricResultType::RESULT_UINT64;
    metricParams.MetricType = MetricsDiscovery::TMetricType::METRIC_TYPE_RATIO;

    zet_metric_group_handle_t metricGroupHandle = {};

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

    uint32_t returnedMetricCount = 1;

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;
    metricsSet.GetMetricResult = &metric;
    metricsSet.calculateMetricsOutReportCount = &returnedMetricCount;

    metric.GetParamsResult = &metricParams;

    for (uint32_t i = 0; i < subDeviceCount; ++i) {

        mockMetricsLibrarySubDevices[i]->getMetricQueryReportSizeOutSize = metricsSetParams.QueryReportSize;
    }

    mockMetricsLibrary->getMetricQueryReportSizeOutSize = metricsSetParams.QueryReportSize;

    mockMetricsLibrary->g_mockApi->contextCreateOutHandle = metricsLibraryContextHandle;
    mockMetricsLibrary->g_mockApi->queryCreateOutHandle = metricsLibraryQueryHandle;
    mockMetricsLibrary->g_mockApi->getParameterOutValue = value;

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

    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.MetricsCount = 1;

    Mock<IMetric_1_13> metric;
    TMetricParams_1_13 metricParams = {};
    metricParams.SymbolName = "Metric symbol name";
    metricParams.ShortName = "Metric short name";
    metricParams.LongName = "Metric long name";
    metricParams.ResultType = MetricsDiscovery::TMetricResultType::RESULT_UINT64;
    metricParams.MetricType = MetricsDiscovery::TMetricType::METRIC_TYPE_RATIO;

    zet_metric_group_handle_t metricGroupHandle = {};

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;

    ConfigurationHandle_1_0 metricsLibraryConfigurationHandle = {&value};
    ContextHandle_1_0 metricsLibraryContextHandle = {&value};

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

    mockMetricsLibrary->g_mockApi->contextCreateOutHandle = metricsLibraryContextHandle;
    mockMetricsLibrary->g_mockApi->configurationCreateOutHandle = metricsLibraryConfigurationHandle;
    mockMetricsLibrary->g_mockApi->configurationActivationCounter = subDeviceCount;
    mockMetricsLibrary->g_mockApi->configurationDeactivationCounter = subDeviceCount;

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

    metricsDeviceParams.ConcurrentGroupsCount = 1;

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
    metricsConcurrentGroup.GetIoMeasurementInformationResult = &ioMeasurement;
    ioMeasurement.GetParamsResult = &oaInformation;

    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.MetricsCount = 1;

    Mock<IMetric_1_13> metric;
    TMetricParams_1_13 metricParams = {};
    metricParams.SymbolName = "Metric symbol name";
    metricParams.ShortName = "Metric short name";
    metricParams.LongName = "Metric long name";
    metricParams.ResultType = MetricsDiscovery::TMetricResultType::RESULT_UINT64;
    metricParams.MetricType = MetricsDiscovery::TMetricType::METRIC_TYPE_RATIO;

    zet_metric_group_handle_t metricGroupHandle = {};

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

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;
    metricsSet.GetMetricResult = &metric;

    metric.GetParamsResult = &metricParams;

    mockMetricsLibrary->g_mockApi->contextCreateOutHandle = metricsLibraryContextHandle;
    mockMetricsLibrary->g_mockApi->queryCreateOutHandle = metricsLibraryQueryHandle;
    mockMetricsLibrary->g_mockApi->getParameterOutValue = value;

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

    // Initiate a Metrics Library Release by releasing Metric Query Pool
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

TEST_F(MultiDeviceMetricQueryPoolAffinityMaskTest, givenAffinityMaskEnabledWhenGetSubDeviceClientOptionsIsCalledThenReturnCorrectSubDeviceNumber) {

    auto subDevice = ClientOptionsData_1_0{};
    auto subDeviceIndex = ClientOptionsData_1_0{};
    auto subDeviceCount = ClientOptionsData_1_0{};
    auto workloadPartition = ClientOptionsData_1_0{};

    // Root Device is used with Affinity Mask 0.1
    auto &metricSource = devices[0]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    auto &metricsLibrary = metricSource.getMetricsLibrary();

    metricsLibrary.getSubDeviceClientOptions(subDevice, subDeviceIndex, subDeviceCount, workloadPartition);

    EXPECT_EQ(subDevice.Type, MetricsLibraryApi::ClientOptionsType::SubDevice);
    // Expect Sub Device Enabled
    EXPECT_EQ(subDevice.SubDevice.Enabled, true);

    EXPECT_EQ(subDeviceIndex.Type, MetricsLibraryApi::ClientOptionsType::SubDeviceIndex);
    // Enabled Sub Device index is used
    EXPECT_EQ(subDeviceIndex.SubDeviceIndex.Index, 1);
}

} // namespace ult
} // namespace L0
