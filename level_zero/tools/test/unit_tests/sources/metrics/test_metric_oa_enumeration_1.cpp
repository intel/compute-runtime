/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/test_macros/test_base.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_oa.h"
#include "level_zero/zet_intel_gpu_metric.h"
#include <level_zero/zet_api.h>

#include "gtest/gtest.h"

#include <unordered_map>

namespace L0 {
namespace ult {
using MetricEnumerationTest = Test<MetricContextFixture>;
TEST_F(MetricEnumerationTest, givenIncorrectMetricsDiscoveryDeviceWhenZetGetMetricGroupIsCalledThenNoMetricGroupsAreReturned) {

    mockMetricEnumeration->openAdapterGroup = [](MetricsDiscovery::IAdapterGroupLatest **) -> TCompletionCode { return TCompletionCode::CC_ERROR_GENERAL; };

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 0u);
}

TEST_F(MetricEnumerationTest, givenCorrectMetricDiscoveryWhenLoadMetricsDiscoveryIsCalledThenReturnsSuccess) {

    EXPECT_EQ(mockMetricEnumeration->loadMetricsDiscovery(), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTest, givenIncorrectMetricDiscoveryWhenLoadMetricsDiscoveryIsCalledThenReturnsFail) {

    mockMetricEnumeration->hMetricsDiscovery = nullptr;
    mockMetricEnumeration->openAdapterGroup = nullptr;

    EXPECT_EQ(mockMetricEnumeration->baseLoadMetricsDiscovery(), ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
}

TEST_F(MetricEnumerationTest, givenForcingValidAdapterGroupDependenciesAreMet) {

    EXPECT_EQ(mockMetricEnumeration->baseLoadMetricsDiscovery(), ZE_RESULT_SUCCESS);
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

    setupDefaultMocksForMetricDevice(metricsDevice);

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 0u);
}

TEST_F(MetricEnumerationTest, givenNoConcurrentMetricGroupsWhenZetGetMetricGroupIsCalledThenNoMetricGroupsAreReturned) {

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 0u);
}

TEST_F(MetricEnumerationTest, givenTwoConcurrentMetricGroupsWhenZetGetMetricGroupIsCalledThenReturnsTwoMetricsGroups) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 2;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup0;
    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup1;

    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.SymbolName = "OA";
    metricsConcurrentGroupParams.MetricSetsCount = 1;
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
    metricsConcurrentGroup0.GetIoMeasurementInformationResult = &ioMeasurement;
    metricsConcurrentGroup1.GetIoMeasurementInformationResult = &ioMeasurement;
    ioMeasurement.GetParamsResult = &oaInformation;

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup0);
    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup1);

    metricsConcurrentGroup0.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup1.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup0.getMetricSetResult = &metricsSet;
    metricsConcurrentGroup1.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;

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
    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup;
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.SymbolName = "OA";
    metricsConcurrentGroupParams.MetricSetsCount = 1;
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

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;

    // One api: metric group handle.
    zet_metric_group_handle_t metricGroupHandle = {};

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);
    EXPECT_TRUE(static_cast<OaMetricGroupImp *>(metricGroupHandle)->isImmutable());
}

TEST_F(MetricEnumerationTest, givenValidArgumentsWhenZetGetMetricGroupPropertiesIsCalledThenReturnsSuccess) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
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

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";

    // One api: metric group handle.
    zet_metric_group_handle_t metricGroupHandle = {};
    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;

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

TEST_F(MetricEnumerationTest, givenOaMetricSourceWhenQueryingSourceIdThenCorrectSourceIdIsReturned) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
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

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";

    // One api: metric group handle.
    zet_metric_group_handle_t metricGroupHandle = {};
    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    zet_intel_metric_source_id_exp_t metricGroupSourceId{};
    metricGroupSourceId.sourceId = 0xFFFFFFFF;
    metricGroupSourceId.pNext = nullptr;
    metricGroupSourceId.stype = ZET_INTEL_STRUCTURE_TYPE_METRIC_SOURCE_ID_EXP;

    metricGroupProperties.pNext = &metricGroupSourceId;

    // Metric group properties.
    EXPECT_EQ(zetMetricGroupGetProperties(metricGroupHandle, &metricGroupProperties), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupSourceId.sourceId, MetricSource::metricSourceTypeOa);
}

TEST_F(MetricEnumerationTest, givenInvalidArgumentsWhenZetMetricGetIsCalledThenReturnsFail) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
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

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";

    // One api: metric group handle.
    zet_metric_group_handle_t metricGroupHandle = {};

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;
    metricsConcurrentGroup.GetIoMeasurementInformationResult = &ioMeasurement;
    ioMeasurement.GetParamsResult = &oaInformation;

    metricsSet.GetParamsResult = &metricsSetParams;

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

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.MetricsCount = 1;

    // Metrics Discovery:: metric.
    Mock<IMetric_1_13> metric;
    TMetricParams_1_13 metricParams = {};

    // One api: metric group handle.
    zet_metric_group_handle_t metricGroupHandle = {};

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
    EXPECT_EQ(1u, metric.GetParamsCalled);
}

TEST_F(MetricEnumerationTest, GivenEnumerationIsSuccessfulWhenReadingMetricsFrequencyAndValidBitsThenConfirmExpectedValuesAreReturned) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
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

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";

    // One api: metric group handle.
    zet_metric_group_handle_t metricGroupHandle = {};
    zet_metric_global_timestamps_resolution_exp_t metricTimestampProperties = {ZET_STRUCTURE_TYPE_METRIC_GLOBAL_TIMESTAMPS_RESOLUTION_EXP, nullptr};
    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, &metricTimestampProperties};

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;

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

    EXPECT_EQ(metricTimestampProperties.timerResolution, 25000000UL);

    auto &l0GfxCoreHelper = neoDevice->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();
    uint64_t expectedValidBits = l0GfxCoreHelper.getOaTimestampValidBits();
    EXPECT_EQ(metricTimestampProperties.timestampValidBits, expectedValidBits);
}

TEST_F(MetricEnumerationTest, whenReadingMetricCroupCalculateParametersThenExpectedValuesAreReturned) {
    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
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

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};

    // Set flags for Streamer and Query metric sets.
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL | MetricsDiscovery::API_TYPE_IOSTREAM;
    ;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 2u);

    std::vector<zet_metric_group_handle_t> metricGroupsHandles(metricGroupCount);

    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroupsHandles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 2u);

    zet_intel_metric_group_calculation_properties_exp_t metricGroupCalcProps{};
    metricGroupCalcProps.stype = ZET_INTEL_STRUCTURE_TYPE_METRIC_GROUP_CALCULATION_EXP_PROPERTIES;
    metricGroupCalcProps.pNext = nullptr;
    metricGroupCalcProps.isTimeFilterSupported = false;

    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, &metricGroupCalcProps};

    // Metric group properties.
    EXPECT_EQ(zetMetricGroupGetProperties(metricGroupsHandles[0], &metricGroupProperties), ZE_RESULT_SUCCESS);
    // Streamer metric groups support time filtering.
    EXPECT_EQ(metricGroupProperties.samplingType, ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
    EXPECT_EQ(metricGroupCalcProps.isTimeFilterSupported, true);

    EXPECT_EQ(zetMetricGroupGetProperties(metricGroupsHandles[1], &metricGroupProperties), ZE_RESULT_SUCCESS);
    // Query metric groups don't support time filtering.
    EXPECT_EQ(metricGroupProperties.samplingType, ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED);
    EXPECT_EQ(metricGroupCalcProps.isTimeFilterSupported, false);
}

TEST_F(MetricEnumerationTest, GivenValidMetricGroupWhenReadingPropertiesAndIncorrectStructPassedThenFailsWithInvalidArgument) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
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

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";

    // One api: metric group handle.
    zet_metric_group_handle_t metricGroupHandle = {};
    zet_metric_properties_t metricProperties = {ZET_STRUCTURE_TYPE_METRIC_PROPERTIES, nullptr};

    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, &metricProperties};

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;
    metricsConcurrentGroup.GetIoMeasurementInformationResult = &ioMeasurement;
    ioMeasurement.GetParamsResult = &oaInformation;

    metricsSet.GetParamsResult = &metricsSetParams;

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Metric group properties.
    EXPECT_EQ(zetMetricGroupGetProperties(metricGroupHandle, &metricGroupProperties), ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

TEST_F(MetricEnumerationTest, GivenValidMetricGroupWhenReadingFrequencyAndInternalFailuresValuesReturnZero) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
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

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";

    // One api: metric group handle.

    zet_metric_group_handle_t metricGroupHandle = {};
    zet_metric_global_timestamps_resolution_exp_t metricTimestampProperties = {ZET_STRUCTURE_TYPE_METRIC_GLOBAL_TIMESTAMPS_RESOLUTION_EXP, nullptr};
    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, &metricTimestampProperties};

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;

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

    EXPECT_NE(metricTimestampProperties.timerResolution, 0UL);
    EXPECT_NE(metricTimestampProperties.timestampValidBits, 0UL);

    metricsDevice.forceGetSymbolByNameFail = true;

    EXPECT_EQ(zetMetricGroupGetProperties(metricGroupHandle, &metricGroupProperties), ZE_RESULT_ERROR_NOT_AVAILABLE);
    EXPECT_EQ(metricGroupProperties.domain, 0u);
    EXPECT_EQ(metricGroupProperties.samplingType, ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED);
    EXPECT_EQ(metricGroupProperties.metricCount, metricsSetParams.MetricsCount);
    EXPECT_EQ(strcmp(metricGroupProperties.description, metricsSetParams.ShortName), 0);
    EXPECT_EQ(strcmp(metricGroupProperties.name, metricsSetParams.SymbolName), 0);

    EXPECT_EQ(metricTimestampProperties.timerResolution, 0UL);
    EXPECT_EQ(metricTimestampProperties.timestampValidBits, 0UL);
}

TEST_F(MetricEnumerationTest, GivenEnumerationIsSuccessfulWhenReadingMetricsFrequencyThenValuesAreUpdated) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
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

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";

    // One api: metric group handle.
    zet_metric_group_handle_t metricGroupHandle = {};

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    ze_bool_t synchronizedWithHost = true;
    uint64_t globalTimestamp = 0;
    uint64_t metricTimestamp = 0;

    EXPECT_EQ(L0::zetMetricGroupGetGlobalTimestampsExp(metricGroupHandle, synchronizedWithHost, &globalTimestamp, &metricTimestamp), ZE_RESULT_SUCCESS);
    EXPECT_NE(globalTimestamp, 0UL);
    EXPECT_NE(metricTimestamp, 0UL);

    synchronizedWithHost = false;
    globalTimestamp = 0;
    metricTimestamp = 0;

    EXPECT_EQ(L0::zetMetricGroupGetGlobalTimestampsExp(metricGroupHandle, synchronizedWithHost, &globalTimestamp, &metricTimestamp), ZE_RESULT_SUCCESS);
    EXPECT_NE(globalTimestamp, 0UL);
    EXPECT_NE(metricTimestamp, 0UL);
}

TEST_F(MetricEnumerationTest, GivenEnumerationIsSuccessfulWhenFailingToReadDeviceTimestampsOrMetricFrequencyThenValuesAreZero) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
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

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";

    // One api: metric group handle.
    zet_metric_group_handle_t metricGroupHandle = {};

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    ze_bool_t synchronizedWithHost = true;
    uint64_t globalTimestamp = 1;
    uint64_t metricTimestamp = 1;
    metricsDevice.forceGetSymbolByNameFail = true;

    EXPECT_EQ(L0::zetMetricGroupGetGlobalTimestampsExp(metricGroupHandle, synchronizedWithHost, &globalTimestamp, &metricTimestamp), ZE_RESULT_ERROR_NOT_AVAILABLE);
    EXPECT_EQ(globalTimestamp, 0UL);
    EXPECT_EQ(metricTimestamp, 0UL);
    metricsDevice.forceGetGpuCpuTimestampsFail = false;

    globalTimestamp = 1;
    metricTimestamp = 1;
    metricsDevice.forceGetGpuCpuTimestampsFail = true;
    neoDevice->setOSTime(new FalseGpuCpuTime());
    EXPECT_EQ(L0::zetMetricGroupGetGlobalTimestampsExp(metricGroupHandle, synchronizedWithHost, &globalTimestamp, &metricTimestamp), ZE_RESULT_ERROR_DEVICE_LOST);
    EXPECT_EQ(globalTimestamp, 0UL);
    EXPECT_EQ(metricTimestamp, 0UL);
}

TEST_F(MetricEnumerationTest, givenValidArgumentsWhenZetMetricGetIsCalledThenReturnsCorrectMetric) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
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

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.MetricsCount = 1;

    // Metrics Discovery:: metric.
    Mock<IMetric_1_13> metric;
    TMetricParams_1_13 metricParams = {};

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
    EXPECT_EQ(1u, metric.GetParamsCalled);
}

TEST_F(MetricEnumerationTest, givenInvalidArgumentsWhenZetMetricGetPropertiestIsCalledThenReturnsFail) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
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

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.MetricsCount = 1;

    // Metrics Discovery:: metric.
    Mock<IMetric_1_13> metric;
    TMetricParams_1_13 metricParams = {};

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
    EXPECT_EQ(1u, metric.GetParamsCalled);
}

TEST_F(MetricEnumerationTest, givenValidArgumentsWhenZetMetricGetPropertiestIsCalledThenReturnSuccess) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
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

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.MetricsCount = 1;

    // Metrics Discovery:: metric.
    Mock<IMetric_1_13> metric;
    TMetricParams_1_13 metricParams = {};
    metricParams.SymbolName = "Metric symbol name";
    metricParams.ShortName = "Metric short name";
    metricParams.LongName = "Metric long name";
    metricParams.ResultType = MetricsDiscovery::TMetricResultType::RESULT_UINT64;
    metricParams.MetricType = MetricsDiscovery::TMetricType::METRIC_TYPE_RATIO;
    zet_metric_properties_t metricProperties = {};

    // One api: metric group handle.
    zet_metric_group_handle_t metricGroupHandle = {};

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
    EXPECT_EQ(1u, metric.GetParamsCalled);
}

TEST_F(MetricEnumerationTest, givenValidArgumentsWhenZetMetricGetPropertiestIsCalledThenReturnSuccessExt) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
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

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.MetricsCount = 1;

    // Metrics Discovery:: metric.
    Mock<IMetric_1_13> metric;
    TMetricParams_1_13 metricParams = {};
    metricParams.SymbolName = "Metric symbol name";
    metricParams.ShortName = "Metric short name";
    metricParams.LongName = "Metric long name";
    metricParams.ResultType = MetricsDiscovery::TMetricResultType::RESULT_UINT64;
    metricParams.MetricType = MetricsDiscovery::TMetricType::METRIC_TYPE_RATIO;
    zet_metric_properties_t metricProperties = {};

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

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.MetricsCount = 1;

    // Metrics Discovery:: metric.
    Mock<IMetric_1_13> metric;
    TMetricParams_1_13 metricParams = {};
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

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.MetricsCount = 1;

    // Metrics Discovery:: metric.
    Mock<IMetric_1_13> metric;
    TMetricParams_1_13 metricParams = {};
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

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
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

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.MetricsCount = 1;

    // Metrics Discovery:: metric.
    Mock<IMetric_1_13> metric;
    TMetricParams_1_13 metricParams = {};
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
    metricsConcurrentGroup.GetIoMeasurementInformationResult = &ioMeasurement;
    ioMeasurement.GetParamsResult = &oaInformation;

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

    // Activate metric group.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), devices[0]->toHandle(), 1, &metricGroupHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MultiDeviceMetricEnumerationTest, givenMultipleDevicesAndTwoMetricGroupsWithTheSameDomainsWhenzetContextActivateMetricGroupsIsCalledThenReturnsSuccess) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup0;
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams0 = {};
    metricsConcurrentGroupParams0.MetricSetsCount = 2;
    metricsConcurrentGroupParams0.SymbolName = "OA";
    metricsConcurrentGroupParams0.Description = "OA description";
    metricsConcurrentGroupParams0.IoMeasurementInformationCount = 1;

    Mock<MetricsDiscovery::IEquation_1_0> ioReadEquation;
    MetricsDiscovery::TEquationElement_1_0 ioEquationElement = {};
    ioEquationElement.Type = MetricsDiscovery::EQUATION_ELEM_IMM_UINT64;
    ioEquationElement.ImmediateUInt64 = 0;

    ioReadEquation.getEquationElement.push_back(&ioEquationElement);

    Mock<MetricsDiscovery::IInformation_1_0> ioMeasurement;
    MetricsDiscovery::TInformationParams_1_0 oaInformation = {};
    oaInformation.SymbolName = "BufferOverflow";
    oaInformation.IoReadEquation = &ioReadEquation;

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet0;
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet1;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams0 = {};
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams1 = {};
    metricsSetParams0.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams0.SymbolName = "Metric set ZERO";
    metricsSetParams0.ShortName = "Metric set ZERO description";
    metricsSetParams0.MetricsCount = 1;
    metricsSetParams1.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
    metricsSetParams1.SymbolName = "Metric set ONE name";
    metricsSetParams1.ShortName = "Metric set ONE description";
    metricsSetParams1.MetricsCount = 1;

    // Metrics Discovery:: metric.
    Mock<IMetric_1_13> metric0;
    TMetricParams_1_13 metricParams0 = {};
    metricParams0.SymbolName = "Metric ZERO symbol name";
    metricParams0.ShortName = "Metric ZERO short name";
    metricParams0.LongName = "Metric ZERO long name";
    metricParams0.ResultType = MetricsDiscovery::TMetricResultType::RESULT_UINT64;
    metricParams0.MetricType = MetricsDiscovery::TMetricType::METRIC_TYPE_RATIO;

    Mock<IMetric_1_13> metric1;
    TMetricParams_1_13 metricParams1 = {};
    metricParams1.SymbolName = "Metric ONE symbol name";
    metricParams1.ShortName = "Metric ONE short name";
    metricParams1.LongName = "Metric ONE long name";
    metricParams1.ResultType = MetricsDiscovery::TMetricResultType::RESULT_UINT64;
    metricParams1.MetricType = MetricsDiscovery::TMetricType::METRIC_TYPE_RATIO;

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup0);

    metricsConcurrentGroup0.GetParamsResult = &metricsConcurrentGroupParams0;
    metricsConcurrentGroup0.getMetricSetResults.push_back(&metricsSet0);
    metricsConcurrentGroup0.getMetricSetResults.push_back(&metricsSet1);
    metricsConcurrentGroup0.getMetricSetResults.push_back(&metricsSet0);
    metricsConcurrentGroup0.getMetricSetResults.push_back(&metricsSet1);
    metricsConcurrentGroup0.GetIoMeasurementInformationResult = &ioMeasurement;
    ioMeasurement.GetParamsResult = &oaInformation;

    metricsSet0.GetParamsResult = &metricsSetParams0;
    metricsSet1.GetParamsResult = &metricsSetParams1;
    metricsSet0.GetMetricResult = &metric0;
    metricsSet1.GetMetricResult = &metric1;

    metric0.GetParamsResult = &metricParams0;
    metric1.GetParamsResult = &metricParams1;

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

    zet_metric_group_properties_t properties0 = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupGetProperties(metricGroupHandles[0], &properties0));

    zet_metric_group_properties_t properties1 = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupGetProperties(metricGroupHandles[1], &properties1));

    // Activate metric groups.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), devices[0]->toHandle(), 2, metricGroupHandles.data()), ZE_RESULT_SUCCESS);
}

TEST_F(MultiDeviceMetricEnumerationTest, givenMultipleDevicesAndMetricsIsDisabledThenZetContextActivateMetricGroupsReturnsFailure) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
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

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.MetricsCount = 1;

    // Metrics Discovery:: metric.
    Mock<IMetric_1_13> metric;
    TMetricParams_1_13 metricParams = {};
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
    metricsConcurrentGroup.GetIoMeasurementInformationResult = &ioMeasurement;
    ioMeasurement.GetParamsResult = &oaInformation;

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

    // Activate metric group.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), devices[0]->toHandle(), 1, &metricGroupHandle), ZE_RESULT_SUCCESS);
    // Disable Metrics with an activated metric group returns error
    EXPECT_EQ(zetDeviceDisableMetricsExp(devices[0]->toHandle()), ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE);
    // De-Activate all metric groups.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), devices[0]->toHandle(), 0, nullptr), ZE_RESULT_SUCCESS);
    // Disable Metrics with all groups deactivated should return success
    EXPECT_EQ(zetDeviceDisableMetricsExp(devices[0]->toHandle()), ZE_RESULT_SUCCESS);
    // Multiple Disables continue to return success
    EXPECT_EQ(zetDeviceDisableMetricsExp(devices[0]->toHandle()), ZE_RESULT_SUCCESS);
    // Activate metric group on a disabled device should be failure
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), devices[0]->toHandle(), 1, &metricGroupHandle), ZE_RESULT_ERROR_UNINITIALIZED);

    // Reset the disabled status
    devices[0]->getMetricDeviceContext().setMetricsCollectionAllowed(true);
    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());
    for (uint32_t i = 0; i < subDeviceCount; i++) {
        deviceImp.subDevices[i]->getMetricDeviceContext().setMetricsCollectionAllowed(true);
    }
}

TEST_F(MetricEnumerationTest, givenValidTimeBasedMetricGroupWhenzetContextActivateMetricGroupsIsCalledThenReturnsSuccess) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
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

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.MetricsCount = 1;

    // Metrics Discovery:: metric.
    Mock<IMetric_1_13> metric;
    TMetricParams_1_13 metricParams = {};
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

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.MetricsCount = 1;

    // Metrics Discovery:: metric.
    Mock<IMetric_1_13> metric;
    TMetricParams_1_13 metricParams = {};
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
    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup0;
    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup1;
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams0 = {};
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams1 = {};
    metricsConcurrentGroupParams0.MetricSetsCount = 1;
    metricsConcurrentGroupParams1.MetricSetsCount = 1;
    metricsConcurrentGroupParams0.SymbolName = "OA";
    metricsConcurrentGroupParams1.SymbolName = "OA";

    metricsConcurrentGroupParams0.IoMeasurementInformationCount = 1;
    metricsConcurrentGroupParams1.IoMeasurementInformationCount = 1;

    Mock<MetricsDiscovery::IEquation_1_0> ioReadEquation;
    MetricsDiscovery::TEquationElement_1_0 ioEquationElement = {};
    ioEquationElement.Type = MetricsDiscovery::EQUATION_ELEM_IMM_UINT64;
    ioEquationElement.ImmediateUInt64 = 0;

    ioReadEquation.getEquationElement.push_back(&ioEquationElement);

    Mock<MetricsDiscovery::IInformation_1_0> ioMeasurement;
    MetricsDiscovery::TInformationParams_1_0 oaInformation = {};
    oaInformation.SymbolName = "BufferOverflow";
    oaInformation.IoReadEquation = &ioReadEquation;
    ioMeasurement.GetParamsResult = &oaInformation;
    metricsConcurrentGroup0.GetIoMeasurementInformationResult = &ioMeasurement;
    metricsConcurrentGroup1.GetIoMeasurementInformationResult = &ioMeasurement;

    // Metrics Discovery: metric set
    Mock<IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;

    // One api: metric group.
    zet_metric_group_handle_t metricGroupHandle[2] = {};
    zet_metric_group_properties_t metricGroupProperties[2] = {};
    metricGroupProperties[0] = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
    metricGroupProperties[1] = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup0);
    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup1);

    metricsConcurrentGroup0.GetParamsResult = &metricsConcurrentGroupParams0;
    metricsConcurrentGroup1.GetParamsResult = &metricsConcurrentGroupParams1;

    metricsConcurrentGroup0.getMetricSetResult = &metricsSet;
    metricsConcurrentGroup1.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;

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
    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup0;
    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup1;
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams0 = {};
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams1 = {};
    metricsConcurrentGroupParams0.MetricSetsCount = 1;
    metricsConcurrentGroupParams1.MetricSetsCount = 1;
    metricsConcurrentGroupParams0.SymbolName = "OA";
    metricsConcurrentGroupParams1.SymbolName = "OA";

    metricsConcurrentGroupParams0.IoMeasurementInformationCount = 1;
    metricsConcurrentGroupParams1.IoMeasurementInformationCount = 1;

    Mock<MetricsDiscovery::IEquation_1_0> ioReadEquation;
    MetricsDiscovery::TEquationElement_1_0 ioEquationElement = {};
    ioEquationElement.Type = MetricsDiscovery::EQUATION_ELEM_IMM_UINT64;
    ioEquationElement.ImmediateUInt64 = 0;

    ioReadEquation.getEquationElement.push_back(&ioEquationElement);

    Mock<MetricsDiscovery::IInformation_1_0> ioMeasurement;
    MetricsDiscovery::TInformationParams_1_0 oaInformation = {};
    oaInformation.SymbolName = "BufferOverflow";
    oaInformation.IoReadEquation = &ioReadEquation;
    ioMeasurement.GetParamsResult = &oaInformation;
    metricsConcurrentGroup0.GetIoMeasurementInformationResult = &ioMeasurement;
    metricsConcurrentGroup1.GetIoMeasurementInformationResult = &ioMeasurement;

    // Metrics Discovery: metric set
    Mock<IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;

    // One api: metric group.
    zet_metric_group_handle_t metricGroupHandle[2] = {};
    zet_metric_group_properties_t metricGroupProperties[2] = {};
    metricGroupProperties[0] = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
    metricGroupProperties[1] = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup0);
    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup1);

    metricsConcurrentGroup0.GetParamsResult = &metricsConcurrentGroupParams0;
    metricsConcurrentGroup1.GetParamsResult = &metricsConcurrentGroupParams1;
    metricsConcurrentGroup0.getMetricSetResult = &metricsSet;
    metricsConcurrentGroup1.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;

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

TEST_F(MetricEnumerationTest, givenActivateTwoMetricGroupsWithTheSameDomainsWhenzetContextActivateMetricGroupsIsCalledThenReturnsSuccess) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup;
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 2;
    metricsConcurrentGroupParams.SymbolName = "OA";
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

    // Metrics Discovery: metric set
    Mock<IMetricSet_1_13> metricsSet0;
    Mock<IMetricSet_1_13> metricsSet1;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams0 = {};
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams1 = {};
    metricsSetParams0.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams1.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;

    // One api: metric group.
    zet_metric_group_handle_t metricGroupHandle[2] = {};
    zet_metric_group_properties_t metricGroupProperties[2] = {};
    metricGroupProperties[0] = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
    metricGroupProperties[1] = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResults.push_back(&metricsSet0);
    metricsConcurrentGroup.getMetricSetResults.push_back(&metricsSet1);

    metricsSet0.GetParamsResult = &metricsSetParams0;
    metricsSet1.GetParamsResult = &metricsSetParams1;

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
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 2, metricGroupHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTest, givenValidMetricGroupWhenDeactivateIsDoneThenDomainsAreCleared) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup;
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
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

    // Metrics Discovery: metric set
    Mock<IMetricSet_1_13> metricsSet0;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams0 = {};
    metricsSetParams0.ApiMask = MetricsDiscovery::API_TYPE_OCL;

    zet_metric_group_handle_t metricGroupHandle[2] = {};

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet0;

    metricsSet0.GetParamsResult = &metricsSetParams0;

    // Metric group handles.
    uint32_t metricGroupCount = 1;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroupHandle), ZE_RESULT_SUCCESS);

    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 1, &metricGroupHandle[0]), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 0, nullptr), ZE_RESULT_SUCCESS);
    device->activateMetricGroups();
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 1, &metricGroupHandle[0]), ZE_RESULT_SUCCESS);
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 0, nullptr), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTest, GivenAlreadyActivatedMetricGroupWhenzetContextActivateMetricGroupsIsCalledThenReturnSuccess) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup;
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 2;
    metricsConcurrentGroupParams.SymbolName = "OA";
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

    // Metrics Discovery: metric set
    Mock<IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;

    // One api: metric group.
    zet_metric_group_handle_t metricGroupHandle[2] = {};
    zet_metric_group_properties_t metricGroupProperties[2] = {};
    metricGroupProperties[0] = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
    metricGroupProperties[1] = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;
    metricsConcurrentGroup.GetIoMeasurementInformationResult = &ioMeasurement;
    ioMeasurement.GetParamsResult = &oaInformation;

    metricsSet.GetParamsResult = &metricsSetParams;

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
    device->activateMetricGroups();
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 1, &metricGroupHandle[1]), ZE_RESULT_SUCCESS);

    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 2, metricGroupHandle), ZE_RESULT_SUCCESS);

    // Deactivate all.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 0, nullptr), ZE_RESULT_SUCCESS);

    // Activate two metric groups at once.
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device->toHandle(), 2, metricGroupHandle), ZE_RESULT_SUCCESS);

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
    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup;
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
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

    // Metrics Discovery: metric set
    Mock<IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;

    // One api: metric group.
    zet_metric_group_handle_t metricGroupHandle = {};

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;

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
    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup;
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
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

    // Metrics Discovery: metric set
    Mock<IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.QueryReportSize = 256;

    // One api: metric group.
    zet_metric_group_handle_t metricGroupHandle = {};

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;

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
    EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &calculatedResults, nullptr), ZE_RESULT_ERROR_INVALID_SIZE);

    // Invalid raw buffer size provided by the driver.
    metricsSetParams.QueryReportSize = 0;
    EXPECT_NE(metricsSetParams.QueryReportSize, rawResultsSize);
    EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &calculatedResults, nullptr), ZE_RESULT_ERROR_INVALID_SIZE);
}

TEST_F(MetricEnumerationTest, givenIncorrectRawReportSizeWhenZetMetricGroupCalculateMetricValuesExpIsCalledThenReturnsFail) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup;
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
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

    // Metrics Discovery: metric set
    Mock<IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.QueryReportSize = 0;

    // One api: metric group.
    zet_metric_group_handle_t metricGroupHandle = {};

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;

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
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_ERROR_INVALID_SIZE);
    EXPECT_EQ(dataCount, 0u);
    EXPECT_EQ(totalMetricCount, 0u);

    // Invalid raw buffer size provided by the driver.
    EXPECT_NE(metricsSetParams.QueryReportSize, rawResultsSize);
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_ERROR_INVALID_SIZE);
    EXPECT_EQ(dataCount, 0u);
    EXPECT_EQ(totalMetricCount, 0u);
}

TEST_F(MetricEnumerationTest, givenCorrectRawReportSizeWhenZetMetricGroupCalculateMetricValuesExpIsCalledThenReturnsSuccess) {

    metricsDeviceParams.ConcurrentGroupsCount = 1;

    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup;
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
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

    Mock<IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.QueryReportSize = 256;
    metricsSetParams.MetricsCount = 11;

    Mock<IMetric_1_13> metric;
    MetricsDiscovery::TMetricParams_1_13 metricParams = {};

    zet_metric_group_handle_t metricGroupHandle = {};

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
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(dataCount, 1u);
    EXPECT_EQ(totalMetricCount, metricsSetParams.MetricsCount);

    // Copy calculated metrics.
    std::vector<uint32_t> metricCounts(dataCount);
    std::vector<zet_typed_value_t> caculatedRawResults(totalMetricCount);
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, metricCounts.data(), caculatedRawResults.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricCounts[0], totalMetricCount);
}

TEST_F(MetricEnumerationTest, givenFailedCalculateMetricsWhenZetMetricGroupCalculateMetricValuesExpIsCalledThenReturnsFail) {

    metricsDeviceParams.ConcurrentGroupsCount = 1;

    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup;
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
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

    Mock<IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.QueryReportSize = 256;
    metricsSetParams.MetricsCount = 11;

    Mock<IMetric_1_13> metric;
    MetricsDiscovery::TMetricParams_1_13 metricParams = {};

    zet_metric_group_handle_t metricGroupHandle = {};

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;
    metricsSet.GetMetricResult = &metric;
    metricsSet.calculateMetricsResult = TCompletionCode::CC_ERROR_GENERAL;

    metric.GetParamsResult = &metricParams;

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
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(dataCount, 1u);
    EXPECT_EQ(totalMetricCount, metricsSetParams.MetricsCount);

    // Copy calculated metrics.
    std::vector<uint32_t> metricCounts(dataCount);
    std::vector<zet_typed_value_t> caculatedRawResults(totalMetricCount);
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, metricCounts.data(), caculatedRawResults.data()), ZE_RESULT_ERROR_UNKNOWN);
    EXPECT_EQ(metricCounts[0], 0u);
}

TEST_F(MetricEnumerationTest, givenInvalidQueryReportSizeWhenZetMetricGroupCalculateMultipleMetricValuesExpIsCalledTwiceThenReturnsFail) {

    metricsDeviceParams.ConcurrentGroupsCount = 1;

    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup;
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
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

    Mock<IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.QueryReportSize = 0;
    metricsSetParams.MetricsCount = 11;

    Mock<IMetric_1_13> metric;
    MetricsDiscovery::TMetricParams_1_13 metricParams = {};

    zet_metric_group_handle_t metricGroupHandle = {};

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;
    metricsSet.GetMetricResult = &metric;

    metric.GetParamsResult = &metricParams;

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
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_ERROR_INVALID_SIZE);
    EXPECT_EQ(dataCount, 0u);
    EXPECT_EQ(totalMetricCount, 0u);
}

TEST_F(MetricEnumerationTest, givenCorrectRawDataHeaderWhenZetMetricGroupCalculateMultipleMetricValuesExpIsCalledTwiceThenReturnsSuccess) {

    metricsDeviceParams.ConcurrentGroupsCount = 1;

    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup;
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
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

    Mock<IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.QueryReportSize = 256;
    metricsSetParams.MetricsCount = 11;

    Mock<IMetric_1_13> metric;
    MetricsDiscovery::TMetricParams_1_13 metricParams = {};

    zet_metric_group_handle_t metricGroupHandle = {};

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
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(dataCount, 1u);
    EXPECT_EQ(totalMetricCount, metricsSetParams.MetricsCount);

    // Copy calculated metrics.
    std::vector<uint32_t> metricCounts(dataCount);
    std::vector<zet_typed_value_t> caculatedRawResults(totalMetricCount);
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, metricCounts.data(), caculatedRawResults.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricCounts[0], totalMetricCount);
}

TEST_F(MetricEnumerationTest, givenCorrectRawReportSizeWhenZetMetricGroupCalculateMetricValuesIsCalledThenReturnsCorrectCalculatedReportCount) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup;
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
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

    // Metrics Discovery: metric set
    Mock<IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.QueryReportSize = 256;
    metricsSetParams.MetricsCount = 11;

    // Metrics Discovery: metric
    Mock<IMetric_1_13> metric;
    MetricsDiscovery::TMetricParams_1_13 metricParams = {};

    // One api: metric group.
    zet_metric_group_handle_t metricGroupHandle = {};

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
    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup;
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
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

    // Metrics Discovery: metric set
    Mock<IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.QueryReportSize = 256;
    metricsSetParams.MetricsCount = 11;

    // Metrics Discovery: metric
    Mock<IMetric_1_13> metric;
    MetricsDiscovery::TMetricParams_1_13 metricParams = {};

    // One api: metric group.
    zet_metric_group_handle_t metricGroupHandle = {};

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    uint32_t returnedMetricCount = 2;
    metricsSet.GetParamsResult = &metricsSetParams;
    metricsSet.GetMetricResult = &metric;
    metricsSet.calculateMetricsOutReportCount = &returnedMetricCount;

    metric.GetParamsResult = &metricParams;

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
    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup;
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
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

    // Metrics Discovery: metric set
    Mock<IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.QueryReportSize = 256;
    metricsSetParams.MetricsCount = 11;

    // Metrics Discovery: metric
    Mock<IMetric_1_13> metric;
    MetricsDiscovery::TMetricParams_1_13 metricParams = {};

    // One api: metric group.
    zet_metric_group_handle_t metricGroupHandle = {};

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;
    metricsSet.GetMetricResult = &metric;

    metric.GetParamsResult = &metricParams;

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
    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup;
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
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

    // Metrics Discovery: metric set
    Mock<IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.QueryReportSize = 256;
    metricsSetParams.MetricsCount = 11;

    // Metrics Discovery: metric
    Mock<IMetric_1_13> metric;
    MetricsDiscovery::TMetricParams_1_13 metricParams = {};

    // One api: metric group.
    zet_metric_group_handle_t metricGroupHandle = {};

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;
    metricsSet.GetMetricResult = &metric;
    metricsSet.calculateMetricsOutReportCount = &metricsSetParams.MetricsCount;

    metric.GetParamsResult = &metricParams;

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
    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup;
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
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

    // Metrics Discovery: metric set
    Mock<IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.QueryReportSize = 256;
    metricsSetParams.MetricsCount = 11;

    // Metrics Discovery: metric
    Mock<IMetric_1_13> metric;
    MetricsDiscovery::TMetricParams_1_13 metricParams = {};

    // One api: metric group.
    zet_metric_group_handle_t metricGroupHandle = {};

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
    mockMetricEnumeration->isInitializedCallBase = true;
    EXPECT_EQ(mockMetricEnumeration->isInitialized(), true);
}

TEST_F(MetricEnumerationTest, givenNotInitializedMetricEnumerationWhenIsInitializedIsCalledThenMetricEnumerationWillBeInitialized) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup;
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.SymbolName = "OA";
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

    // Metrics Discovery: metric set
    Mock<IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.QueryReportSize = 256;
    metricsSetParams.MetricsCount = 11;

    // Metrics Discovery: metric
    Mock<IMetric_1_13> metric;
    MetricsDiscovery::TMetricParams_1_13 metricParams = {};

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

    mockMetricEnumeration->initializationState = ZE_RESULT_ERROR_UNINITIALIZED;
    mockMetricEnumeration->isInitializedCallBase = true;
    EXPECT_EQ(mockMetricEnumeration->isInitialized(), true);
}

TEST_F(MetricEnumerationTest, givenLoadedMetricsLibraryAndDiscoveryAndMetricsLibraryInitializedWhenLoadDependenciesThenReturnSuccess) {

    mockMetricsLibrary->initializationState = ZE_RESULT_SUCCESS;

    auto &metricSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    EXPECT_EQ(metricSource.loadDependencies(), true);
    EXPECT_EQ(metricSource.isInitialized(), true);
}

TEST_F(MetricEnumerationTest, givenNotLoadedMetricsLibraryAndDiscoveryWhenLoadDependenciesThenReturnFail) {

    mockMetricEnumeration->loadMetricsDiscoveryResult = ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;

    auto &metricSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    EXPECT_EQ(metricSource.loadDependencies(), false);
    EXPECT_EQ(metricSource.isInitialized(), false);
}

TEST_F(MetricEnumerationTest, givenRootDeviceWhenLoadDependenciesIsCalledThenLegacyOpenMetricsDeviceWillBeCalled) {

    auto &metricSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    Mock<IAdapterGroup_1_13> mockAdapterGroup;
    Mock<IAdapter_1_13> mockAdapter;
    Mock<IMetricsDevice_1_13> mockDevice;

    mockMetricEnumeration->globalMockApi->adapterGroup = reinterpret_cast<IAdapterGroupLatest *>(&mockAdapterGroup);
    mockMetricEnumeration->getMetricsAdapterResult = &mockAdapter;

    mockAdapter.openMetricsDeviceOutDevice = &mockDevice;

    setupDefaultMocksForMetricDevice(mockDevice);

    // Use first sub device.
    device->getMetricDeviceContext().setSubDeviceIndex(0);
    mockMetricsLibrary->initializationState = ZE_RESULT_SUCCESS;

    EXPECT_EQ(metricSource.loadDependencies(), true);
    EXPECT_EQ(metricSource.isInitialized(), true);
    mockMetricEnumeration->isInitializedCallBase = true;
    EXPECT_EQ(mockMetricEnumeration->isInitialized(), true);
    EXPECT_EQ(mockMetricEnumeration->cleanupMetricsDiscovery(), ZE_RESULT_SUCCESS);
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

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";
    metricsSetParams.MetricsCount = 1;

    // Metrics Discovery:: metric.
    Mock<IMetric_1_13> metric;
    TMetricParams_1_13 metricParams = {};
    metricParams.SymbolName = "Metric symbol name";
    metricParams.ShortName = "Metric short name";
    metricParams.LongName = "Metric long name";
    metricParams.ResultType = MetricsDiscovery::TMetricResultType::RESULT_UINT64;
    metricParams.MetricType = metricType;
    zet_metric_properties_t metricProperties = {};

    // One api: metric group handle.
    zet_metric_group_handle_t metricGroupHandle = {};

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

INSTANTIATE_TEST_SUITE_P(parameterizedMetricEnumerationTestMetricTypes, MetricEnumerationTestMetricTypes,
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

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
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

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;
    metricsSet.GetInformationResult = &information;

    information.GetParamsResult = &sourceInformationParams;

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

INSTANTIATE_TEST_SUITE_P(parameterizedMetricEnumerationTestInformationTypes,
                         MetricEnumerationTestInformationTypes,
                         ::testing::ValuesIn(getListOfInfoTypes()));

TEST_F(MetricEnumerationTest, givenMetricSetWhenActivateIsCalledActivateReturnsTrue) {

    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MockMetricSource mockMetricSource{};
    MetricGroupImpTest metricGroup(mockMetricSource);

    metricGroup.pReferenceMetricSet = &metricsSet;

    EXPECT_EQ(metricGroup.activateMetricSet(), true);
}

TEST_F(MetricEnumerationTest, givenMetricSetWhenActivateIsCalledActivateReturnsFalse) {

    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MockMetricSource mockMetricSource{};
    MetricGroupImpTest metricGroup(mockMetricSource);

    metricGroup.pReferenceMetricSet = &metricsSet;
    metricsSet.ActivateResult = TCompletionCode::CC_ERROR_GENERAL;

    EXPECT_EQ(metricGroup.activateMetricSet(), false);
}

TEST_F(MetricEnumerationTest, givenMetricSetWhenDeactivateIsCalledDeactivateReturnsTrue) {

    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MockMetricSource mockMetricSource{};
    MetricGroupImpTest metricGroup(mockMetricSource);

    metricGroup.pReferenceMetricSet = &metricsSet;

    EXPECT_EQ(metricGroup.deactivateMetricSet(), true);
}

TEST_F(MetricEnumerationTest, givenMetricSetWhenDeactivateIsCalledDeactivateReturnsFalse) {

    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MockMetricSource mockMetricSource{};
    MetricGroupImpTest metricGroup(mockMetricSource);

    metricGroup.pReferenceMetricSet = &metricsSet;
    metricsSet.DeactivateResult = TCompletionCode::CC_ERROR_GENERAL;

    EXPECT_EQ(metricGroup.deactivateMetricSet(), false);
}

TEST_F(MetricEnumerationTest, givenMetricSetWhenWaitForReportsIsCalledWaitForReportsReturnsSuccess) {

    Mock<MetricsDiscovery::IConcurrentGroup_1_13> concurrentGroup;
    MockMetricSource mockMetricSource{};
    MetricGroupImpTest metricGroup(mockMetricSource);

    metricGroup.pReferenceConcurrentGroup = &concurrentGroup;

    uint32_t timeout = 1;
    EXPECT_EQ(metricGroup.waitForReports(timeout), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTest, givenMetricSetWhenWaitForReportsIsCalledWaitForReportsReturnsNotReady) {

    Mock<MetricsDiscovery::IConcurrentGroup_1_13> concurrentGroup;
    MockMetricSource mockMetricSource{};
    MetricGroupImpTest metricGroup(mockMetricSource);

    metricGroup.pReferenceConcurrentGroup = &concurrentGroup;
    concurrentGroup.WaitForReportsResult = TCompletionCode::CC_ERROR_GENERAL;

    uint32_t timeout = 1;
    EXPECT_EQ(metricGroup.waitForReports(timeout), ZE_RESULT_NOT_READY);
}

TEST_F(MetricEnumerationTest, givenValidArgumentsWhenZetGetMetricGroupPropertiesIsCalledToQueryGroupTypeThenReturnsSuccess) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
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

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";

    // One api: metric group handle.
    zet_metric_group_handle_t metricGroupHandle = {};
    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    zet_metric_group_type_exp_t metricGroupType{};
    metricGroupType.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_TYPE_EXP;
    metricGroupType.pNext = nullptr;
    metricGroupType.type = ZET_METRIC_GROUP_TYPE_EXP_FLAG_FORCE_UINT32;

    metricGroupProperties.pNext = &metricGroupType;

    // Metric group properties.
    EXPECT_EQ(zetMetricGroupGetProperties(metricGroupHandle, &metricGroupProperties), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupProperties.domain, 0u);
    EXPECT_EQ(metricGroupProperties.samplingType, ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED);
    EXPECT_EQ(metricGroupProperties.metricCount, metricsSetParams.MetricsCount);
    EXPECT_EQ(strcmp(metricGroupProperties.description, metricsSetParams.ShortName), 0);
    EXPECT_EQ(strcmp(metricGroupProperties.name, metricsSetParams.SymbolName), 0);

    EXPECT_EQ(metricGroupType.type, ZET_METRIC_GROUP_TYPE_EXP_FLAG_OTHER);
}

TEST_F(MetricEnumerationTest, givenValidArgumentsWhenAppendMarkerIsCalledThenReturnSuccess) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
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

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    metricsSetParams.MetricsCount = 0;
    metricsSetParams.SymbolName = "Metric set name";
    metricsSetParams.ShortName = "Metric set description";

    // One api: metric group handle.
    zet_metric_group_handle_t metricGroupHandle = {};

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;

    CommandBufferSize_1_0 commandBufferSize = {};
    commandBufferSize.GpuMemorySize = 100;
    mockMetricsLibrary->g_mockApi->commandBufferGetSizeOutSize = commandBufferSize;

    // Metric group count.
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    // Metric group handle.
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));

    EXPECT_EQ(zetCommandListAppendMarkerExp(commandList->toHandle(), metricGroupHandle, 0), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTest, givenValidOAMetricGroupThenOASourceCalcOperationIsCalled) {

    // Metrics Discovery device.
    metricsDeviceParams.ConcurrentGroupsCount = 1;

    // Metrics Discovery concurrent group.
    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup;
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.SymbolName = "OA";
    metricsConcurrentGroupParams.MetricSetsCount = 1;
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

    // Metrics Discovery:: metric set.
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;

    metricsSet.GetParamsResult = &metricsSetParams;

    // Metric group handle.
    uint32_t metricGroupCount = 1;
    zet_metric_group_handle_t metricGroupHandle = {};
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    zet_intel_metric_scope_properties_exp_t scopeProperties{};
    scopeProperties.stype = ZET_STRUCTURE_TYPE_INTEL_METRIC_SCOPE_PROPERTIES_EXP;
    scopeProperties.pNext = nullptr;

    MockMetricScope mockMetricScope(scopeProperties, false);
    auto hMockScope = mockMetricScope.toHandle();

    // metric groups from different source
    zet_intel_metric_calculation_exp_desc_t calculationDesc{
        ZET_INTEL_STRUCTURE_TYPE_METRIC_CALCULATION_DESC_EXP,
        nullptr,            // pNext
        1,                  // metricGroupCount
        &metricGroupHandle, // phMetricGroups
        0,                  // metricCount
        nullptr,            // phMetrics
        0,                  // timeWindowsCount
        nullptr,            // pCalculationTimeWindows
        1000,               // timeAggregationWindow
        1,                  // metricScopesCount
        &hMockScope,        // phMetricScopes
    };

    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                                               device->toHandle(), &calculationDesc,
                                                                                               &hCalculationOperation));
}

using AppendMarkerDriverVersionTest = Test<ExtensionFixture>;

TEST_F(AppendMarkerDriverVersionTest, givenDriverHandleWhenAskingForExtensionsThenReturnCorrectVersions) {
    verifyExtensionDefinition(ZET_INTEL_METRIC_APPEND_MARKER_EXP_NAME, ZET_INTEL_METRIC_APPEND_MARKER_EXP_VERSION_CURRENT);
}

} // namespace ult
} // namespace L0
