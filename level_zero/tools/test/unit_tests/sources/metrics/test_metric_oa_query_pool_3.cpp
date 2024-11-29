/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test_base.h"

#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/metric_query_pool_fixture.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

TEST_F(MetricQueryPoolTest, givenMetricQueryIsActiveWhenMetricQueryPoolDestroyIsCalledThenMetricLibraryIsNotReleased) {

    zet_device_handle_t metricDevice = device->toHandle();

    auto &metricSource = (static_cast<DeviceImp *>(device))->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    Mock<MetricGroup> metricGroup(metricSource);

    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
    metricGroupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;

    zet_metric_query_handle_t queryHandle[2] = {};
    zet_metric_query_pool_handle_t poolHandle[2] = {};
    zet_metric_query_pool_desc_t poolDesc = {};
    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE;

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;

    QueryHandle_1_0 metricsLibraryQueryHandle = {&value};
    ContextHandle_1_0 metricsLibraryContextHandle = {&value};

    metricGroup.getPropertiesOutProperties = &metricGroupProperties;

    mockMetricsLibrary->g_mockApi->contextCreateOutHandle = metricsLibraryContextHandle;
    mockMetricsLibrary->g_mockApi->queryCreateOutHandle = metricsLibraryQueryHandle;
    mockMetricsLibrary->g_mockApi->getParameterOutValue = value;

    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroup.toHandle(), &poolDesc, &poolHandle[0]), ZE_RESULT_SUCCESS);
    EXPECT_NE(poolHandle[0], nullptr);

    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroup.toHandle(), &poolDesc, &poolHandle[1]), ZE_RESULT_SUCCESS);
    EXPECT_NE(poolHandle[1], nullptr);

    EXPECT_EQ(zetMetricQueryCreate(poolHandle[0], 0, &queryHandle[0]), ZE_RESULT_SUCCESS);
    EXPECT_NE(queryHandle[0], nullptr);

    EXPECT_EQ(zetMetricQueryCreate(poolHandle[1], 0, &queryHandle[1]), ZE_RESULT_SUCCESS);
    EXPECT_NE(queryHandle[1], nullptr);

    EXPECT_EQ(zetMetricQueryDestroy(queryHandle[0]), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle[0]), ZE_RESULT_SUCCESS);

    EXPECT_EQ(mockMetricsLibrary->getInitializationState(), ZE_RESULT_SUCCESS);

    EXPECT_EQ(zetMetricQueryDestroy(queryHandle[1]), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle[1]), ZE_RESULT_SUCCESS);

    EXPECT_NE(mockMetricsLibrary->getInitializationState(), ZE_RESULT_SUCCESS);
}

TEST_F(MetricQueryPoolTest, givenMetricQueryIsActiveWhenMetricGroupDeactivateIsCalledThenMetricLibraryIsNotReleased) {

    zet_device_handle_t metricDevice = device->toHandle();

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
    metricsConcurrentGroup.GetIoMeasurementInformationResult = &ioMeasurement;
    ioMeasurement.GetParamsResult = &oaInformation;

    metricsSet.GetParamsResult = &metricsSetParams;
    metricsSet.GetMetricResult = &metric;

    metric.GetParamsResult = &metricParams;

    mockMetricsLibrary->g_mockApi->contextCreateOutHandle = metricsLibraryContextHandle;
    mockMetricsLibrary->g_mockApi->queryCreateOutHandle = metricsLibraryQueryHandle;
    mockMetricsLibrary->g_mockApi->getParameterOutValue = value;

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(metricDevice, &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    EXPECT_EQ(zetMetricGroupGet(metricDevice, &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), metricDevice, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroupHandle, &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(poolHandle, nullptr);

    EXPECT_EQ(zetMetricQueryCreate(poolHandle, 0, &queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(queryHandle, nullptr);

    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), metricDevice, 0, nullptr), ZE_RESULT_SUCCESS);

    EXPECT_EQ(mockMetricsLibrary->getInitializationState(), ZE_RESULT_SUCCESS);

    EXPECT_EQ(zetMetricQueryDestroy(queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);

    EXPECT_NE(mockMetricsLibrary->getInitializationState(), ZE_RESULT_SUCCESS);
}

TEST_F(MetricQueryPoolTest, givenMetricGroupIsActiveWhenQueryPoolDestroyIsCalledThenMetricLibraryIsNotReleased) {

    zet_device_handle_t metricDevice = device->toHandle();
    auto &metricSource = (static_cast<DeviceImp *>(device))->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();

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
    metricsConcurrentGroup.GetIoMeasurementInformationResult = &ioMeasurement;
    ioMeasurement.GetParamsResult = &oaInformation;

    metricsSet.GetParamsResult = &metricsSetParams;
    metricsSet.GetMetricResult = &metric;

    metric.GetParamsResult = &metricParams;

    mockMetricsLibrary->g_mockApi->contextCreateOutHandle = metricsLibraryContextHandle;
    mockMetricsLibrary->g_mockApi->queryCreateOutHandle = metricsLibraryQueryHandle;
    mockMetricsLibrary->g_mockApi->getParameterOutValue = value;

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(metricDevice, &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    EXPECT_EQ(zetMetricGroupGet(metricDevice, &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), metricDevice, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroupHandle, &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(poolHandle, nullptr);

    EXPECT_EQ(zetMetricQueryCreate(poolHandle, 0, &queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(queryHandle, nullptr);

    EXPECT_EQ(mockMetricsLibrary->getInitializationState(), ZE_RESULT_SUCCESS);

    metricSource.activateMetricGroupsAlreadyDeferred();

    EXPECT_EQ(zetMetricQueryDestroy(queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);

    EXPECT_EQ(mockMetricsLibrary->getInitializationState(), ZE_RESULT_SUCCESS);

    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), metricDevice, 0, nullptr), ZE_RESULT_SUCCESS);
}

TEST_F(MultiDeviceMetricQueryPoolTest, givenSubDeviceWhenGetSubDeviceClientOptionsIsCalledThenReturnSubDeviceProperties) {

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    auto subDevice = ClientOptionsData_1_0{};
    auto subDeviceIndex = ClientOptionsData_1_0{};
    auto subDeviceCount = ClientOptionsData_1_0{};
    auto workloadPartition = ClientOptionsData_1_0{};

    // Sub devices
    for (uint32_t i = 0, count = deviceImp.numSubDevices; i < count; ++i) {

        auto &metricSource = deviceImp.subDevices[i]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
        auto &metricsLibrary = metricSource.getMetricsLibrary();

        metricsLibrary.getSubDeviceClientOptions(subDevice, subDeviceIndex, subDeviceCount, workloadPartition);

        EXPECT_EQ(subDevice.Type, MetricsLibraryApi::ClientOptionsType::SubDevice);
        EXPECT_EQ(subDevice.SubDevice.Enabled, true);

        EXPECT_EQ(subDeviceIndex.Type, MetricsLibraryApi::ClientOptionsType::SubDeviceIndex);
        EXPECT_EQ(subDeviceIndex.SubDeviceIndex.Index, i);

        EXPECT_EQ(subDeviceCount.Type, MetricsLibraryApi::ClientOptionsType::SubDeviceCount);
        EXPECT_EQ(subDeviceCount.SubDeviceCount.Count, std::max(deviceImp.numSubDevices, 1u));

        EXPECT_EQ(workloadPartition.Type, MetricsLibraryApi::ClientOptionsType::WorkloadPartition);
        EXPECT_EQ(workloadPartition.WorkloadPartition.Enabled, false);
    }
}

TEST_F(MultiDeviceMetricQueryPoolTest, givenSubDeviceWithWorkloadPartitionWhenGetSubDeviceClientOptionsIsCalledThenReturnSubDeviceProperties) {

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    auto subDevice = ClientOptionsData_1_0{};
    auto subDeviceIndex = ClientOptionsData_1_0{};
    auto subDeviceCount = ClientOptionsData_1_0{};
    auto workloadPartition = ClientOptionsData_1_0{};

    // Sub devices
    for (uint32_t i = 0, count = deviceImp.numSubDevices; i < count; ++i) {

        auto &metricSource = deviceImp.subDevices[i]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
        auto &metricsLibrary = metricSource.getMetricsLibrary();

        metricsLibrary.enableWorkloadPartition();

        metricsLibrary.getSubDeviceClientOptions(subDevice, subDeviceIndex, subDeviceCount, workloadPartition);

        EXPECT_EQ(subDevice.Type, MetricsLibraryApi::ClientOptionsType::SubDevice);
        EXPECT_EQ(subDevice.SubDevice.Enabled, true);

        EXPECT_EQ(subDeviceIndex.Type, MetricsLibraryApi::ClientOptionsType::SubDeviceIndex);
        EXPECT_EQ(subDeviceIndex.SubDeviceIndex.Index, i);

        EXPECT_EQ(subDeviceCount.Type, MetricsLibraryApi::ClientOptionsType::SubDeviceCount);
        EXPECT_EQ(subDeviceCount.SubDeviceCount.Count, std::max(deviceImp.numSubDevices, 1u));

        EXPECT_EQ(workloadPartition.Type, MetricsLibraryApi::ClientOptionsType::WorkloadPartition);
        EXPECT_EQ(workloadPartition.WorkloadPartition.Enabled, true);
    }
}

TEST_F(MultiDeviceMetricQueryPoolTest, givenCorrectArgumentsWhenZetMetricQueryPoolCreateIsCalledThenReturnsSuccess) {

    zet_device_handle_t metricDevice = devices[0]->toHandle();

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

    // Create metric query pool.
    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroupHandle, &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(poolHandle, nullptr);

    // Create metric query.
    EXPECT_EQ(zetMetricQueryCreate(poolHandle, 0, &queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(queryHandle, nullptr);

    // Destroy query and its pool.
    EXPECT_EQ(zetMetricQueryDestroy(queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MultiDeviceMetricQueryPoolTest, givenImplicitScalingIsEnabledWhenMetricsAreEnumeratedThenWorkPartitionIsEnabledForSubdevices) {

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
    metricsSetParams.MetricsCount = 1;

    Mock<IMetric_1_13> metric;
    TMetricParams_1_13 metricParams = {};
    metricParams.SymbolName = "Metric symbol name";
    metricParams.ShortName = "Metric short name";
    metricParams.LongName = "Metric long name";
    metricParams.ResultType = MetricsDiscovery::TMetricResultType::RESULT_UINT64;
    metricParams.MetricType = MetricsDiscovery::TMetricType::METRIC_TYPE_RATIO;

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;
    metricsSet.GetMetricResult = &metric;

    metric.GetParamsResult = &metricParams;

    auto subDeviceClientOptions = ClientOptionsData_1_0{};
    auto subDeviceIndexClientOptions = ClientOptionsData_1_0{};
    auto subDeviceCountClientOptions = ClientOptionsData_1_0{};
    auto workloadPartitionClientOptions = ClientOptionsData_1_0{};

    for (uint32_t i = 0, count = subDeviceCount; i < count; ++i) {

        auto &metricSource = deviceImp.subDevices[i]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
        auto &metricsLibrary = metricSource.getMetricsLibrary();
        metricsLibrary.getSubDeviceClientOptions(subDeviceClientOptions, subDeviceIndexClientOptions,
                                                 subDeviceCountClientOptions, workloadPartitionClientOptions);
        EXPECT_EQ(workloadPartitionClientOptions.Type, MetricsLibraryApi::ClientOptionsType::WorkloadPartition);
        EXPECT_EQ(workloadPartitionClientOptions.WorkloadPartition.Enabled, false);
    }

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    for (uint32_t i = 0, count = deviceImp.numSubDevices; i < count; ++i) {

        auto &metricSource = deviceImp.subDevices[i]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
        auto &metricsLibrary = metricSource.getMetricsLibrary();
        metricsLibrary.getSubDeviceClientOptions(subDeviceClientOptions, subDeviceIndexClientOptions,
                                                 subDeviceCountClientOptions, workloadPartitionClientOptions);
        EXPECT_EQ(workloadPartitionClientOptions.Type, MetricsLibraryApi::ClientOptionsType::WorkloadPartition);
        EXPECT_EQ(workloadPartitionClientOptions.WorkloadPartition.Enabled, true);
    }
}

TEST_F(MultiDeviceMetricQueryPoolTest, givenEnableWalkerPartitionIsOnWhenZetCommandListAppendMetricQueryBeginEndIsCalledForSubDeviceThenReturnsSuccess) {

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableWalkerPartition.set(1);

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    zet_device_handle_t metricDeviceHandle = deviceImp.subDevices[0]->toHandle();

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

    openMetricsAdapterSubDevice(0);

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
    mockMetricsLibrary->g_mockApi->queryCreateOutHandle = metricsLibraryQueryHandle;
    mockMetricsLibrary->g_mockApi->getParameterOutValue = value;

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(metricDeviceHandle, &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    EXPECT_EQ(zetMetricGroupGet(metricDeviceHandle, &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), metricDeviceHandle, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDeviceHandle, metricGroupHandle, &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(poolHandle, nullptr);

    EXPECT_EQ(zetMetricQueryCreate(poolHandle, 0, &queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(queryHandle, nullptr);

    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), metricDeviceHandle, 0, nullptr), ZE_RESULT_SUCCESS);

    EXPECT_EQ(zetMetricQueryDestroy(queryHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MultiDeviceMetricQueryPoolTest, givenEnableWalkerPartitionIsOnWhenZetCommandListAppendMetricMemoryBarrierIsCalledForSubDeviceThenReturnsSuccess) {

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableWalkerPartition.set(1);

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, deviceImp.subDevices[0], NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    zet_command_list_handle_t commandListHandle = commandList->toHandle();

    TypedValue_1_0 value = {};
    value.Type = ValueType::Uint32;
    value.ValueUInt32 = 64;

    ContextHandle_1_0 metricsLibraryContextHandle = {&value};

    CommandBufferSize_1_0 commandBufferSize = {};
    commandBufferSize.GpuMemorySize = 100;

    mockMetricsLibrary->g_mockApi->contextCreateOutHandle = metricsLibraryContextHandle;
    mockMetricsLibrary->g_mockApi->commandBufferGetSizeOutSize = commandBufferSize;

    EXPECT_EQ(zetCommandListAppendMetricMemoryBarrier(commandListHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MultiDeviceMetricQueryPoolTest, givenFailedMetricsLibraryContextWhenZetMetricQueryPoolCreateIsCalledThenReturnFail) {

    zet_device_handle_t metricDevice = devices[0]->toHandle();

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
    mockMetricsLibrary->g_mockApi->queryCreateResults.push_back(StatusCode::Failed);
    mockMetricsLibrary->g_mockApi->queryCreateResults.push_back(StatusCode::Success);

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

    // Create metric query pool.
    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, metricGroupHandle, &poolDesc, &poolHandle), ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

TEST_F(MultiDeviceMetricQueryPoolTest, givenExecutionQueryTypeWhenZetMetricQueryPoolCreateIsCalledThenQueryPoolIsObtained) {
    zet_device_handle_t metricDevice = devices[0]->toHandle();

    zet_metric_query_pool_handle_t poolHandle = {};
    zet_metric_query_pool_desc_t poolDesc = {};
    poolDesc.stype = ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC;
    poolDesc.count = 1;
    poolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_EXECUTION;

    EXPECT_EQ(zetMetricQueryPoolCreate(context->toHandle(), metricDevice, nullptr, &poolDesc, &poolHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(poolHandle, nullptr);
    EXPECT_EQ(zetMetricQueryPoolDestroy(poolHandle), ZE_RESULT_SUCCESS);
}

} // namespace ult
} // namespace L0
