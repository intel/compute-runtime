/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_oa.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {
using MetricEnumerationTest = Test<MetricContextFixture>;

TEST_F(MetricEnumerationTest, givenTimeAndBufferSizeWhenOpenIoStreamReturnsErrorThenTheMetricGroupOpenIoStreamReturnsErrorUnknown) {

    Mock<MetricsDiscovery::IConcurrentGroup_1_13> concurrentGroup;
    MockMetricSource mockSource{};
    MetricGroupImpTest metricGroup(mockSource);

    metricGroup.pReferenceConcurrentGroup = &concurrentGroup;
    concurrentGroup.openIoStreamResult = TCompletionCode::CC_ERROR_GENERAL;

    uint32_t timerPeriodNs = 1;
    uint32_t oaBufferSize = 100;

    EXPECT_EQ(metricGroup.openIoStream(timerPeriodNs, oaBufferSize), ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(MetricEnumerationTest, givenReportCountAndReportDataWhenReadIoStreamReturnsErrorThenMetrigGroupReadIoStreamReturnsError) {

    Mock<MetricsDiscovery::IConcurrentGroup_1_13> concurrentGroup;
    MockMetricSource mockSource{};
    MetricGroupImpTest metricGroup(mockSource);

    metricGroup.pReferenceConcurrentGroup = &concurrentGroup;
    concurrentGroup.readIoStreamResult = TCompletionCode::CC_ERROR_GENERAL;

    uint32_t reportCount = 1;
    uint8_t reportData = 0;

    EXPECT_EQ(metricGroup.readIoStream(reportCount, reportData), ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(MetricEnumerationTest, givenTimeAndBufferSizeWhenCloseIoStreamIsCalledCloseAndFailThenIoStreamReturnsErrorUnknown) {

    Mock<MetricsDiscovery::IConcurrentGroup_1_13> concurrentGroup;
    MockMetricSource mockSource{};
    MetricGroupImpTest metricGroup(mockSource);

    metricGroup.pReferenceConcurrentGroup = &concurrentGroup;
    concurrentGroup.CloseIoStreamResult = TCompletionCode::CC_ERROR_GENERAL;
    EXPECT_EQ(metricGroup.closeIoStream(), ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(MetricEnumerationTest, givenTTypedValueWhenCopyValueIsCalledReturnsFilledZetTypedValue) {

    MetricsDiscovery::TTypedValue_1_0 source = {};
    zet_typed_value_t destination = {};
    MockMetricSource mockSource{};
    MetricGroupImpTest metricGroup(mockSource);

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

using MetricEnumerationMultiDeviceTest = Test<MetricMultiDeviceFixture>;

TEST_F(MetricEnumerationMultiDeviceTest, givenRootDeviceWhenLoadDependenciesIsCalledThenOpenMetricsSubDeviceWillBeCalled) {

    // Use first root device.
    auto &metricSource = devices[0]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    Mock<IAdapterGroup_1_13> mockAdapterGroup;
    Mock<IAdapter_1_13> mockAdapter;
    Mock<IMetricsDevice_1_13> mockDevice;

    mockMetricEnumeration->globalMockApi->adapterGroup = reinterpret_cast<IAdapterGroupLatest *>(&mockAdapterGroup);
    mockMetricEnumeration->getMetricsAdapterResult = &mockAdapter;

    mockAdapter.openMetricsSubDeviceOutDevice = &mockDevice;
    mockAdapter.openMetricsDeviceOutDevice = &mockDevice;

    setupDefaultMocksForMetricDevice(mockDevice);

    // Use root device.
    devices[0]->getMetricDeviceContext().setSubDeviceIndex(0);
    mockMetricsLibrary->initializationState = ZE_RESULT_SUCCESS;

    EXPECT_EQ(metricSource.loadDependencies(), true);
    EXPECT_EQ(metricSource.isInitialized(), true);
    mockMetricEnumeration->isInitializedCallBase = true;
    EXPECT_EQ(mockMetricEnumeration->isInitialized(), true);
    EXPECT_EQ(mockMetricEnumeration->cleanupMetricsDiscovery(), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationMultiDeviceTest, givenSubDeviceWhenOpenMetricsDiscoveryIsCalledThenOpenMetricsSubDeviceWillBeCalled) {

    // Use sub device
    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());
    ASSERT_GE(subDeviceCount, 2u);
    Mock<IAdapterGroup_1_13> mockAdapterGroup;
    Mock<IAdapter_1_13> mockAdapter;
    Mock<IMetricsDevice_1_13> mockDevice;

    mockMetricEnumeration->globalMockApi->adapterGroup = reinterpret_cast<IAdapterGroupLatest *>(&mockAdapterGroup);

    mockMetricEnumerationSubDevices[1]->getMetricsAdapterResult = &mockAdapter;

    mockAdapter.openMetricsSubDeviceOutDevice = &mockDevice;

    setupDefaultMocksForMetricDevice(mockDevice);

    EXPECT_EQ(mockMetricEnumerationSubDevices[1]->openMetricsDiscovery(), ZE_RESULT_SUCCESS);
    EXPECT_EQ(mockMetricEnumerationSubDevices[1]->cleanupMetricsDiscovery(), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationMultiDeviceTest, givenSubDeviceWhenOpenMetricsDiscoveryIsCalledAndReadingOABufferMaxSizeFailsThenZeroIsUsed) {

    // Use sub device
    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());
    ASSERT_GE(subDeviceCount, 2u);
    Mock<IAdapterGroup_1_13> mockAdapterGroup;
    Mock<IAdapter_1_13> mockAdapter;
    Mock<IMetricsDevice_1_13> mockDevice;
    mockMetricEnumeration->globalMockApi->adapterGroup = reinterpret_cast<IAdapterGroupLatest *>(&mockAdapterGroup);
    mockMetricEnumerationSubDevices[1]->getMetricsAdapterResult = &mockAdapter;

    mockAdapter.openMetricsSubDeviceOutDevice = &mockDevice;

    mockDevice.forceGetSymbolByNameFail = true;

    EXPECT_EQ(mockMetricEnumerationSubDevices[1]->openMetricsDiscovery(), ZE_RESULT_SUCCESS);
    EXPECT_EQ(mockMetricEnumerationSubDevices[1]->getMaxOaBufferSize(), 0u);
    EXPECT_EQ(mockMetricEnumerationSubDevices[1]->cleanupMetricsDiscovery(), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationMultiDeviceTest, givenRootDeviceWhenLoadDependenciesIsCalledThenOpenMetricsSubDeviceWillBeCalledWithoutSuccess) {

    // Use first root device.
    auto &metricSource = devices[0]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    Mock<IAdapterGroup_1_13> mockAdapterGroup;
    Mock<IAdapter_1_13> mockAdapter;
    Mock<IMetricsDevice_1_13> mockDevice;

    mockMetricEnumeration->globalMockApi->adapterGroup = reinterpret_cast<IAdapterGroupLatest *>(&mockAdapterGroup);
    mockMetricEnumeration->getMetricsAdapterResult = &mockAdapter;

    mockAdapter.openMetricsSubDeviceResult = TCompletionCode::CC_ERROR_GENERAL;

    // Use root device.
    devices[0]->getMetricDeviceContext().setSubDeviceIndex(0);
    mockMetricsLibrary->initializationState = ZE_RESULT_SUCCESS;

    EXPECT_EQ(metricSource.loadDependencies(), true);
    EXPECT_EQ(metricSource.isInitialized(), true);
    mockMetricEnumeration->isInitializedCallBase = true;
    EXPECT_EQ(mockMetricEnumeration->isInitialized(), false);
    EXPECT_EQ(0u, mockDevice.GetParamsCalled);
}

TEST_F(MetricEnumerationMultiDeviceTest, givenRootDeviceWhenLoadDependenciesAndOpenAdapterOnRootDeviceFailsThenCLeanUpIsDone) {

    // Use first root device.
    auto &metricSource = devices[0]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    Mock<IAdapterGroup_1_13> mockAdapterGroup;
    Mock<IAdapter_1_13> mockAdapter;
    Mock<IMetricsDevice_1_13> mockDevice;

    mockMetricEnumeration->globalMockApi->adapterGroup = reinterpret_cast<IAdapterGroupLatest *>(&mockAdapterGroup);

    mockMetricEnumeration->getMetricsAdapterResult = &mockAdapter;

    mockAdapter.openMetricsSubDeviceOutDevice = &mockDevice;
    mockAdapter.openMetricsDeviceResult = TCompletionCode::CC_ERROR_GENERAL;

    setupDefaultMocksForMetricDevice(mockDevice);

    // Use root device.
    devices[0]->getMetricDeviceContext().setSubDeviceIndex(0);
    mockMetricsLibrary->initializationState = ZE_RESULT_SUCCESS;

    EXPECT_EQ(metricSource.loadDependencies(), true);
    EXPECT_EQ(mockMetricEnumeration->openMetricsDiscovery(), ZE_RESULT_ERROR_NOT_AVAILABLE);
}

TEST_F(MetricEnumerationMultiDeviceTest, givenIncorrectMetricsDiscoveryInterfaceVersionWhenZetGetMetricGroupIsCalledThenReturnsFail) {

    metricsDeviceParams.Version.MajorNumber = 0;
    metricsDeviceParams.Version.MinorNumber = 1;

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 0u);
}

TEST_F(MetricEnumerationMultiDeviceTest, givenValidArgumentsWhenZetMetricGetPropertiestIsCalledThenReturnSuccess) {

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
    zet_metric_group_properties_t metricGroupProperties = {};

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
    EXPECT_TRUE(static_cast<OaMetricImp *>(metricHandle)->isImmutable());
    EXPECT_TRUE(static_cast<OaMetricImp *>(metricHandle)->getMetricSource().isAvailable());

    // Obtain metric params.
    EXPECT_EQ(zetMetricGetProperties(metricHandle, &metricProperties), ZE_RESULT_SUCCESS);
    EXPECT_EQ(strcmp(metricProperties.name, metricParams.SymbolName), 0);
    EXPECT_EQ(strcmp(metricProperties.description, metricParams.LongName), 0);
    EXPECT_EQ(metricProperties.metricType, ZET_METRIC_TYPE_RATIO);
    EXPECT_EQ(metricProperties.resultType, ZET_VALUE_TYPE_UINT64);
}

TEST_F(MetricEnumerationMultiDeviceTest, givenRootDeviceIsEnumeratedAfterSubDeviceWhenZetMetricGetIsCalledThenMetricGroupCountIsSameForRootAndSubDevice) {

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

    openMetricsAdapterDeviceAndSubDeviceNoCountVerify(0);

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;
    metricsConcurrentGroup.GetIoMeasurementInformationResult = &ioMeasurement;
    ioMeasurement.GetParamsResult = &oaInformation;

    metricsSet.GetParamsResult = &metricsSetParams;
    metricsSet.GetMetricResult = &metric;

    metric.GetParamsResult = &metricParams;

    uint32_t subDeviceMetricGroupCount = 0;
    auto &subDeviceImp = *static_cast<DeviceImp *>(deviceImp.subDevices[0]);
    EXPECT_EQ(zetMetricGroupGet(subDeviceImp.toHandle(), &subDeviceMetricGroupCount, nullptr), ZE_RESULT_SUCCESS);

    uint32_t rootDeviceMetricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &rootDeviceMetricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(rootDeviceMetricGroupCount, 1u);
    EXPECT_EQ(subDeviceMetricGroupCount, rootDeviceMetricGroupCount);
}

TEST_F(MetricEnumerationMultiDeviceTest, givenCorrectRawDataHeaderWhenZetMetricGroupCalculateMetricValuesExpIsCalledTwiceThenReturnsSuccess) {

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

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
    metricsConcurrentGroup.GetIoMeasurementInformationResult = &ioMeasurement;
    ioMeasurement.GetParamsResult = &oaInformation;

    metricsSet.GetParamsResult = &metricsSetParams;
    metricsSet.GetMetricResult = &metric;
    metricsSet.calculateMetricsOutReportCount = &returnedMetricCount;

    metric.GetParamsResult = &metricParams;

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
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(dataCount, subDeviceCount);
    EXPECT_EQ(totalMetricCount, subDeviceCount * metricsSetParams.MetricsCount);

    // Copy calculated metrics.
    std::vector<uint32_t> metricCounts(dataCount);
    std::vector<zet_typed_value_t> caculatedRawResults(totalMetricCount);
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, metricCounts.data(), caculatedRawResults.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricCounts[0], metricsSetParams.MetricsCount);
    EXPECT_EQ(metricCounts[1], metricsSetParams.MetricsCount);
}

TEST_F(MetricEnumerationMultiDeviceTest, givenInvalidDataCountAndTotalMetricCountWhenZetMetricGroupCalculateMetricValuesExpIsCalledThenReturnsCorrectDataCountAndTotalMetricCount) {

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

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

    metric.GetParamsResult = &metricParams;

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
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(subDeviceCount, dataCount);
    EXPECT_EQ(subDeviceCount * metricsSetParams.MetricsCount, totalMetricCount);

    // Valid raw buffer. Invalid total metric count.
    dataCount = 0;
    totalMetricCount = 1000;
    EXPECT_NE(metricsSetParams.QueryReportSize, rawResultsSize);
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(subDeviceCount, dataCount);
    EXPECT_EQ(subDeviceCount * metricsSetParams.MetricsCount, totalMetricCount);
}

TEST_F(MetricEnumerationMultiDeviceTest, givenInvalidQueryReportSizeWhenZetMetricGroupCalculateMetricValuesExpIsCalledThenReturnsFail) {

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

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
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_ERROR_INVALID_SIZE);
    EXPECT_EQ(dataCount, 0u);
    EXPECT_EQ(totalMetricCount, 0u);
}

TEST_F(MetricEnumerationMultiDeviceTest, givenErrorGeneralOnCalculateMetricsWhenZetMetricGroupCalculateMetricValuesExpIsCalledThenReturnsFail) {

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

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
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(dataCount, subDeviceCount);
    EXPECT_EQ(totalMetricCount, subDeviceCount * metricsSetParams.MetricsCount);

    // Copy calculated metrics. CalculateMetrics returns CC_ERROR_GENERAL for first sub device.
    std::vector<uint32_t> metricCounts(dataCount);
    std::vector<zet_typed_value_t> caculatedRawResults(totalMetricCount);
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, metricCounts.data(), caculatedRawResults.data()), ZE_RESULT_ERROR_UNKNOWN);
    EXPECT_EQ(metricCounts[0], 0u);
    EXPECT_EQ(metricCounts[1], 0u);
}

TEST_F(MetricEnumerationMultiDeviceTest, givenCorrectRawDataHeaderWhenZetMetricGroupCalculateMetricValuesIsCalledThenReturnsFail) {

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

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

    metric.GetParamsResult = &metricParams;

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
    EXPECT_EQ(zetMetricGroupCalculateMetricValues(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &metricCount, nullptr), ZE_RESULT_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(metricCount, 0u);
}

TEST_F(MetricEnumerationMultiDeviceTest, givenCorrectRawDataHeaderWhenFirstSubDeviceHasNoReportsToCalculateThenZetMetricGroupCalculateMetricValuesExpReturnsPartialDataForSecondSubDeviceAndReturnsSuccess) {

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

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
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
    metricsSetParams.RawReportSize = 256;
    metricsSetParams.MetricsCount = 11;

    Mock<IMetric_1_13> metric;
    MetricsDiscovery::TMetricParams_1_13 metricParams = {};

    zet_metric_group_handle_t metricGroupHandle = {};

    uint32_t returnedMetricCount = 2;

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
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Raw results.
    constexpr size_t rawResultsSize = 512; // metricsSetParams.RawReportSize * returnedMetricCount
    uint8_t rawResults[rawResultsSize] = {};
    MetricGroupCalculateHeader *pRawHeader = reinterpret_cast<MetricGroupCalculateHeader *>(rawResults);

    pRawHeader->magic = MetricGroupCalculateHeader::magicValue;
    pRawHeader->dataCount = subDeviceCount;
    pRawHeader->rawDataOffsets = sizeof(MetricGroupCalculateHeader);
    pRawHeader->rawDataSizes = pRawHeader->rawDataOffsets + sizeof(uint32_t) * pRawHeader->dataCount;
    pRawHeader->rawDataOffset = pRawHeader->rawDataSizes + sizeof(uint32_t) * pRawHeader->dataCount;

    // Sub device 0 has 0 reports to calculate.
    uint32_t *pRawDataOffsets = reinterpret_cast<uint32_t *>(rawResults + pRawHeader->rawDataOffsets);
    uint32_t *pRawDataSizes = reinterpret_cast<uint32_t *>(rawResults + pRawHeader->rawDataSizes);
    pRawDataOffsets[0] = 0;
    pRawDataOffsets[1] = 0;
    pRawDataSizes[0] = 0;
    pRawDataSizes[1] = metricsSetParams.RawReportSize * returnedMetricCount;

    // Valid raw buffer.
    uint32_t dataCount = 0;
    uint32_t totalMetricCount = 0;
    EXPECT_NE(metricsSetParams.RawReportSize, rawResultsSize);
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(dataCount, subDeviceCount);
    EXPECT_EQ(totalMetricCount, returnedMetricCount * metricsSetParams.MetricsCount);

    // Copy calculated metrics.
    std::vector<uint32_t> metricCounts(dataCount);
    std::vector<zet_typed_value_t> caculatedRawResults(totalMetricCount);
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, metricCounts.data(), caculatedRawResults.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricCounts[0], 0u);
    EXPECT_EQ(metricCounts[1], returnedMetricCount * metricsSetParams.MetricsCount);
}

TEST_F(MetricEnumerationMultiDeviceTest, givenCorrectRawDataHeaderWhenSecondSubDeviceHasNoReportsToCalculateThenZetMetricGroupCalculateMetricValuesExpReturnsPartialDataForFirstSubDeviceAndReturnsSuccess) {

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

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
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
    metricsSetParams.RawReportSize = 256;
    metricsSetParams.MetricsCount = 11;

    Mock<IMetric_1_13> metric;
    MetricsDiscovery::TMetricParams_1_13 metricParams = {};

    zet_metric_group_handle_t metricGroupHandle = {};

    uint32_t returnedMetricCount = 2;

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
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Raw results.
    constexpr size_t rawResultsSize = 512; // metricsSetParams.RawReportSize * returnedMetricCount
    uint8_t rawResults[rawResultsSize] = {};
    MetricGroupCalculateHeader *pRawHeader = reinterpret_cast<MetricGroupCalculateHeader *>(rawResults);

    pRawHeader->magic = MetricGroupCalculateHeader::magicValue;
    pRawHeader->dataCount = subDeviceCount;
    pRawHeader->rawDataOffsets = sizeof(MetricGroupCalculateHeader);
    pRawHeader->rawDataSizes = pRawHeader->rawDataOffsets + sizeof(uint32_t) * pRawHeader->dataCount;
    pRawHeader->rawDataOffset = pRawHeader->rawDataSizes + sizeof(uint32_t) * pRawHeader->dataCount;

    // Sub device 1 has 0 reports to calculate.
    uint32_t *pRawDataOffsets = reinterpret_cast<uint32_t *>(rawResults + pRawHeader->rawDataOffsets);
    uint32_t *pRawDataSizes = reinterpret_cast<uint32_t *>(rawResults + pRawHeader->rawDataSizes);
    pRawDataOffsets[0] = 0;
    pRawDataOffsets[1] = metricsSetParams.RawReportSize * returnedMetricCount;
    pRawDataSizes[0] = metricsSetParams.RawReportSize * returnedMetricCount;
    pRawDataSizes[1] = 0;

    // Valid raw buffer.
    uint32_t dataCount = 0;
    uint32_t totalMetricCount = 0;
    EXPECT_NE(metricsSetParams.RawReportSize, rawResultsSize);
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(dataCount, subDeviceCount);
    EXPECT_EQ(totalMetricCount, returnedMetricCount * metricsSetParams.MetricsCount);

    // Copy calculated metrics.
    std::vector<uint32_t> metricCounts(dataCount);
    std::vector<zet_typed_value_t> caculatedRawResults(totalMetricCount);
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, metricCounts.data(), caculatedRawResults.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricCounts[0], returnedMetricCount * metricsSetParams.MetricsCount);
    EXPECT_EQ(metricCounts[1], 0u);
}

TEST_F(MetricEnumerationMultiDeviceTest, givenCorrectRawDataHeaderWhenBothSubDevicesHaveNoReportsToCalculateThenZetMetricGroupCalculateMetricValuesExpReturnsFail) {

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

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
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
    metricsSetParams.RawReportSize = 256;
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
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Raw results.
    constexpr size_t rawResultsSize = 512; // metricsSetParams.RawReportSize * returnedMetricCount
    uint8_t rawResults[rawResultsSize] = {};
    MetricGroupCalculateHeader *pRawHeader = reinterpret_cast<MetricGroupCalculateHeader *>(rawResults);

    pRawHeader->magic = MetricGroupCalculateHeader::magicValue;
    pRawHeader->dataCount = subDeviceCount;
    pRawHeader->rawDataOffsets = sizeof(MetricGroupCalculateHeader);
    pRawHeader->rawDataSizes = pRawHeader->rawDataOffsets + sizeof(uint32_t) * pRawHeader->dataCount;
    pRawHeader->rawDataOffset = pRawHeader->rawDataSizes + sizeof(uint32_t) * pRawHeader->dataCount;

    // Sub device 0 and 1 have 0 reports to calculate.
    uint32_t *pRawDataOffsets = reinterpret_cast<uint32_t *>(rawResults + pRawHeader->rawDataOffsets);
    uint32_t *pRawDataSizes = reinterpret_cast<uint32_t *>(rawResults + pRawHeader->rawDataSizes);
    pRawDataOffsets[0] = 0;
    pRawDataOffsets[1] = 0;
    pRawDataSizes[0] = 0;
    pRawDataSizes[1] = 0;

    // Valid raw buffer.
    uint32_t dataCount = 0;
    uint32_t totalMetricCount = 0;
    EXPECT_NE(metricsSetParams.RawReportSize, rawResultsSize);
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_ERROR_INVALID_SIZE);
    EXPECT_EQ(dataCount, 0u);
    EXPECT_EQ(totalMetricCount, 0u);
}

TEST_F(MetricEnumerationMultiDeviceTest, givenCorrectRawDataHeaderWhenBothSubDevicesHaveNoReportsToCalculateAndPassInvalidArgumentsThatOneSubDeviceHasDataToCalculateToZetMetricGroupCalculateMetricValuesExpOnSecondCallReturnsFail) {

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

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
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
    metricsSetParams.RawReportSize = 256;
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
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Raw results.
    constexpr size_t rawResultsSize = 512; // metricsSetParams.RawReportSize * returnedMetricCount
    uint8_t rawResults[rawResultsSize] = {};
    MetricGroupCalculateHeader *pRawHeader = reinterpret_cast<MetricGroupCalculateHeader *>(rawResults);

    pRawHeader->magic = MetricGroupCalculateHeader::magicValue;
    pRawHeader->dataCount = subDeviceCount;
    pRawHeader->rawDataOffsets = sizeof(MetricGroupCalculateHeader);
    pRawHeader->rawDataSizes = pRawHeader->rawDataOffsets + sizeof(uint32_t) * pRawHeader->dataCount;
    pRawHeader->rawDataOffset = pRawHeader->rawDataSizes + sizeof(uint32_t) * pRawHeader->dataCount;

    // Sub device 0 and 1 have 0 reports to calculate.
    uint32_t *pRawDataOffsets = reinterpret_cast<uint32_t *>(rawResults + pRawHeader->rawDataOffsets);
    uint32_t *pRawDataSizes = reinterpret_cast<uint32_t *>(rawResults + pRawHeader->rawDataSizes);
    pRawDataOffsets[0] = 0;
    pRawDataOffsets[1] = 0;
    pRawDataSizes[0] = 0;
    pRawDataSizes[1] = 0;

    // Valid raw buffer.
    uint32_t dataCount = 0;
    uint32_t totalMetricCount = 0;
    EXPECT_NE(metricsSetParams.RawReportSize, rawResultsSize);
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_ERROR_INVALID_SIZE);
    EXPECT_EQ(dataCount, 0u);
    EXPECT_EQ(totalMetricCount, 0u);

    // Pass invalid dataCount and totalMetricCount params.
    dataCount = 1;
    totalMetricCount = metricsSetParams.MetricsCount;
    uint32_t metricCounts = 0;
    std::vector<zet_typed_value_t> caculatedRawResults(totalMetricCount);
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, &metricCounts, caculatedRawResults.data()), ZE_RESULT_ERROR_INVALID_SIZE);
    EXPECT_EQ(dataCount, 0u);
    EXPECT_EQ(totalMetricCount, 0u);
    EXPECT_EQ(metricCounts, 0u);
}

TEST_F(MetricEnumerationMultiDeviceTest, givenCorrectRawDataHeaderWhenBothSubDevicesHasNoReportsToCalculateAndPassInvalidArgumentsThatTwoSubDevicesHaveDataToCalculateToZetMetricGroupCalculateMetricValuesExpOnSecondCallReturnsFail) {

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

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
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
    metricsSetParams.RawReportSize = 256;
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
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    // Raw results.
    constexpr size_t rawResultsSize = 512; // metricsSetParams.RawReportSize * returnedMetricCount
    uint8_t rawResults[rawResultsSize] = {};
    MetricGroupCalculateHeader *pRawHeader = reinterpret_cast<MetricGroupCalculateHeader *>(rawResults);

    pRawHeader->magic = MetricGroupCalculateHeader::magicValue;
    pRawHeader->dataCount = subDeviceCount;
    pRawHeader->rawDataOffsets = sizeof(MetricGroupCalculateHeader);
    pRawHeader->rawDataSizes = pRawHeader->rawDataOffsets + sizeof(uint32_t) * pRawHeader->dataCount;
    pRawHeader->rawDataOffset = pRawHeader->rawDataSizes + sizeof(uint32_t) * pRawHeader->dataCount;

    // Sub device 0 and 1 have 0 reports to calculate.
    uint32_t *pRawDataOffsets = reinterpret_cast<uint32_t *>(rawResults + pRawHeader->rawDataOffsets);
    uint32_t *pRawDataSizes = reinterpret_cast<uint32_t *>(rawResults + pRawHeader->rawDataSizes);
    pRawDataOffsets[0] = 0;
    pRawDataOffsets[1] = 0;
    pRawDataSizes[0] = 0;
    pRawDataSizes[1] = 0;

    // Valid raw buffer.
    uint32_t dataCount = 0;
    uint32_t totalMetricCount = 0;
    EXPECT_NE(metricsSetParams.RawReportSize, rawResultsSize);
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, nullptr, nullptr), ZE_RESULT_ERROR_INVALID_SIZE);
    EXPECT_EQ(dataCount, 0u);
    EXPECT_EQ(totalMetricCount, 0u);

    // Pass invalid dataCount and totalMetricCount params.
    dataCount = 2;
    totalMetricCount = dataCount * metricsSetParams.MetricsCount;
    std::vector<uint32_t> metricCounts(dataCount);
    std::vector<zet_typed_value_t> caculatedRawResults(totalMetricCount);
    EXPECT_EQ(L0::zetMetricGroupCalculateMultipleMetricValuesExp(metricGroupHandle, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, rawResultsSize, rawResults, &dataCount, &totalMetricCount, metricCounts.data(), caculatedRawResults.data()), ZE_RESULT_ERROR_INVALID_SIZE);
    EXPECT_EQ(dataCount, 0u);
    EXPECT_EQ(totalMetricCount, 0u);
    EXPECT_EQ(metricCounts[0], 0u);
    EXPECT_EQ(metricCounts[1], 0u);
}

TEST_F(MetricEnumerationMultiDeviceTest, givenOaMetricSourceWhenGetConcurrentMetricGroupsIsCalledThenCorrectConcurrentGroupsAreRetrieved) {
    auto &metricSource = devices[0]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();

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

    Mock<IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
    metricsSetParams.RawReportSize = 256;
    metricsSetParams.MetricsCount = 11;

    Mock<IMetric_1_13> metric;
    MetricsDiscovery::TMetricParams_1_13 metricParams = {};

    zet_metric_group_handle_t metricGroupHandle = {};

    uint32_t returnedMetricCount = 2;

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;
    metricsConcurrentGroup.GetIoMeasurementInformationResult = &ioMeasurement;
    ioMeasurement.GetParamsResult = &oaInformation;

    metricsSet.GetParamsResult = &metricsSetParams;
    metricsSet.GetMetricResult = &metric;
    metricsSet.calculateMetricsOutReportCount = &returnedMetricCount;

    metric.GetParamsResult = &metricParams;

    // Metric group handles.
    uint32_t metricGroupCount = 1;
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    std::vector<zet_metric_group_handle_t> metricGroupList{};
    metricGroupList.push_back(metricGroupHandle);

    uint32_t concurrentGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricSource.getConcurrentMetricGroups(metricGroupList, &concurrentGroupCount, nullptr));
    EXPECT_EQ(concurrentGroupCount, 1u);

    std::vector<uint32_t> countPerConcurrentGroup(concurrentGroupCount);
    concurrentGroupCount += 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricSource.getConcurrentMetricGroups(metricGroupList, &concurrentGroupCount, countPerConcurrentGroup.data()));
    EXPECT_EQ(concurrentGroupCount, 1u);
}

TEST_F(MetricEnumerationMultiDeviceTest, GivenOaMetricSourceWhenGetConcurrentMetricGroupsIsCalledWithSubDeviceMetricHandleAndRootDeviceMetricGroupsThenCorrectConcurrentGroupsThenCallFails) {
    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    zet_device_handle_t metricSubDeviceDeviceHandle = deviceImp.subDevices[0]->toHandle();

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

    Mock<IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
    metricsSetParams.RawReportSize = 256;
    metricsSetParams.MetricsCount = 11;

    Mock<IMetric_1_13> metric;
    MetricsDiscovery::TMetricParams_1_13 metricParams = {};

    zet_metric_group_handle_t metricGroupHandle = {};

    uint32_t returnedMetricCount = 2;

    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;
    metricsConcurrentGroup.GetIoMeasurementInformationResult = &ioMeasurement;
    ioMeasurement.GetParamsResult = &oaInformation;

    metricsSet.GetParamsResult = &metricsSetParams;
    metricsSet.GetMetricResult = &metric;
    metricsSet.calculateMetricsOutReportCount = &returnedMetricCount;

    metric.GetParamsResult = &metricParams;

    // Metric group handles.
    uint32_t metricGroupCount = 1;
    EXPECT_EQ(zetMetricGroupGet(devices[0]->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    std::vector<zet_metric_group_handle_t> metricGroupList{};
    metricGroupList.push_back(metricGroupHandle);

    uint32_t concurrentGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDeviceGetConcurrentMetricGroupsExp(metricSubDeviceDeviceHandle, 1, metricGroupList.data(), nullptr, &concurrentGroupCount));
    EXPECT_EQ(concurrentGroupCount, 0u);
}

} // namespace ult
} // namespace L0
