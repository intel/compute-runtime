/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/metrics/metric_ip_sampling_fixture.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/metrics/metric_ip_sampling_source.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_ip_sampling.h"
#include <level_zero/zet_api.h>

namespace L0 {
extern std::vector<_ze_driver_handle_t *> *globalDriverHandles;

namespace ult {

void MetricIpSamplingMultiDevFixture::SetUp() {

    debugManager.flags.EnableImplicitScaling.set(1);

    MultiDeviceFixture::numRootDevices = 1;
    MultiDeviceFixture::numSubDevices = 2;
    MultiDeviceFixture::setUp();
    testDevices.reserve(MultiDeviceFixture::numRootDevices +
                        (MultiDeviceFixture::numRootDevices *
                         MultiDeviceFixture::numSubDevices));
    for (auto device : driverHandle->devices) {
        testDevices.push_back(device);
        auto &deviceImp = *static_cast<DeviceImp *>(device);
        const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());
        for (uint32_t i = 0; i < subDeviceCount; i++) {
            testDevices.push_back(deviceImp.subDevices[i]);
        }
    }

    osInterfaceVector.reserve(testDevices.size());
    for (auto device : testDevices) {
        auto mockMetricIpSamplingOsInterface = new MockMetricIpSamplingOsInterface();
        osInterfaceVector.push_back(mockMetricIpSamplingOsInterface);
        std::unique_ptr<MetricIpSamplingOsInterface> metricIpSamplingOsInterface = std::unique_ptr<MetricIpSamplingOsInterface>(mockMetricIpSamplingOsInterface);
        auto &metricSource = device->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
        metricSource.metricCount = platformIpMetricCountXe;
        metricSource.setMetricOsInterface(metricIpSamplingOsInterface);

        auto &metricOaSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
        metricOaSource.setInitializationState(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
    globalDriverHandles->push_back(driverHandle.get());
}

void MetricIpSamplingMultiDevFixture::TearDown() {
    MultiDeviceFixture::tearDown();
    globalDriverHandles->clear();
}

void MetricIpSamplingFixture::SetUp() {
    DeviceFixture::setUp();
    auto mockMetricIpSamplingOsInterface = new MockMetricIpSamplingOsInterface();
    osInterfaceVector.push_back(mockMetricIpSamplingOsInterface);
    std::unique_ptr<MetricIpSamplingOsInterface> metricIpSamplingOsInterface = std::unique_ptr<MetricIpSamplingOsInterface>(mockMetricIpSamplingOsInterface);

    auto &metricSource = device->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
    metricSource.metricCount = platformIpMetricCountXe;
    metricSource.setMetricOsInterface(metricIpSamplingOsInterface);

    auto &metricOaSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    metricOaSource.setInitializationState(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

    globalDriverHandles->push_back(driverHandle.get());
}

void MetricIpSamplingFixture::TearDown() {
    DeviceFixture::tearDown();
    globalDriverHandles->clear();
}

void MetricIpSamplingCalculateBaseFixture::initRawReports() {
    MockRawDataHelper::rawElementsToRawReports(static_cast<uint32_t>(rawDataElements.size()), rawDataElements, rawReports);
    rawReportsBytesSize = sizeof(rawReports[0][0]) * rawReports[0].size() * rawReports.size();

    MockRawDataHelper::rawElementsToRawReports(static_cast<uint32_t>(rawDataElementsOverflow.size()), rawDataElementsOverflow, rawReportsOverflow);
    rawReportsBytesSizeOverflow = sizeof(rawReportsOverflow[0][0]) * rawReportsOverflow[0].size() * rawReportsOverflow.size();
}

void MetricIpSamplingCalculateBaseFixture::initCalcDescriptor() {
    zet_intel_metric_scope_properties_exp_t scopeProperties{};
    scopeProperties.stype = ZET_STRUCTURE_TYPE_INTEL_METRIC_SCOPE_PROPERTIES_EXP;
    scopeProperties.pNext = nullptr;

    mockMetricScope = new MockMetricScope(scopeProperties, false, 0);
    hMockScope = mockMetricScope->toHandle();

    calculationDesc.stype = ZET_INTEL_STRUCTURE_TYPE_METRIC_CALCULATION_DESC_EXP;
    calculationDesc.pNext = nullptr;                   // pNext
    calculationDesc.metricGroupCount = 0;              // metricGroupCount
    calculationDesc.phMetricGroups = nullptr;          // phMetricGroups
    calculationDesc.metricCount = 0;                   // metricCount
    calculationDesc.phMetrics = nullptr;               // phMetrics
    calculationDesc.timeWindowsCount = 0;              // timeWindowsCount
    calculationDesc.pCalculationTimeWindows = nullptr; // pCalculationTimeWindows
    calculationDesc.timeAggregationWindow = 0;         // timeAggregationWindow
    calculationDesc.metricScopesCount = 1;             // metricScopesCount
    calculationDesc.phMetricScopes = &hMockScope;      // phMetricScopes
}

void MetricIpSamplingCalculateBaseFixture::cleanupCalcDescriptor() {
    delete mockMetricScope;
    hMockScope = nullptr;
}

void MetricIpSamplingCalculateMultiDevFixture::SetUp() {
    MetricIpSamplingMultiDevFixture::SetUp();
    initCalcDescriptor();
    initRawReports();
    // metricGroupHandle is expected to be set by each test for the expected device level
}

void MetricIpSamplingCalculateMultiDevFixture::TearDown() {
    MetricIpSamplingMultiDevFixture::TearDown();
    cleanupCalcDescriptor();
}

void MetricIpSamplingCalculateSingleDevFixture::SetUp() {
    MetricIpSamplingFixture::SetUp();
    initCalcDescriptor();
    initRawReports();

    device->getMetricDeviceContext().enableMetricApi();
    uint32_t metricGroupCount = 1;
    zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle);

    calculationDesc.metricGroupCount = 1;
    calculationDesc.phMetricGroups = &metricGroupHandle;
}

void MetricIpSamplingCalculateSingleDevFixture::TearDown() {
    MetricIpSamplingFixture::TearDown();
    cleanupCalcDescriptor();
}

void MetricIpSamplingMetricsAggregationMultiDevFixture::initMultiRawReports() {

    MockRawDataHelper::rawElementsToRawReports(static_cast<uint32_t>(rawDataElements2.size()), rawDataElements2, rawReports2);
    rawReports2BytesSize = sizeof(rawReports2[0][0]) * rawReports2[0].size() * rawReports2.size();

    MockRawDataHelper::rawElementsToRawReports(static_cast<uint32_t>(rawDataElementsAppend.size()), rawDataElementsAppend, rawReportsAppend);
    rawReportsAppendBytesSize = sizeof(rawReportsAppend[0][0]) * rawReportsAppend[0].size() * rawReportsAppend.size();
}

void MetricIpSamplingMetricsAggregationMultiDevFixture::SetUp() {
    MetricIpSamplingMultiDevFixture::SetUp();
    initRawReports();
    initMultiRawReports();

    rootDevice = testDevices[0];
    EXPECT_EQ(ZE_RESULT_SUCCESS, rootDevice->getMetricDeviceContext().enableMetricApi());

    uint32_t metricGroupCount = 1;
    EXPECT_EQ(zetMetricGroupGet(rootDevice->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    zet_intel_metric_calculation_exp_desc_t calculationDesc{};
    calculationDesc.stype = ZET_INTEL_STRUCTURE_TYPE_METRIC_CALCULATION_DESC_EXP;
    calculationDesc.pNext = nullptr;
    calculationDesc.metricGroupCount = 1;
    calculationDesc.phMetricGroups = &metricGroupHandle;
    calculationDesc.metricCount = 0;
    calculationDesc.phMetrics = nullptr;
    calculationDesc.timeWindowsCount = 0;
    calculationDesc.pCalculationTimeWindows = nullptr;
    calculationDesc.timeAggregationWindow = 0;

    zet_intel_metric_scope_properties_exp_t scopeProperties{};
    scopeProperties.stype = ZET_STRUCTURE_TYPE_INTEL_METRIC_SCOPE_PROPERTIES_EXP;
    scopeProperties.pNext = nullptr;

    scopeProperties.iD = 0; // aggregated scope
    mockMetricScopeAggregated = new MockMetricScope(scopeProperties, true, 0);
    hMockScopeAggregated = mockMetricScopeAggregated->toHandle();

    scopeProperties.iD = 1; // compute scope for sub-device 0, since aggregated scope is present, ID is sub-device index + 1
    mockMetricScopeCompute0 = new MockMetricScope(scopeProperties, false, 0);
    hMockScopeCompute0 = mockMetricScopeCompute0->toHandle();

    scopeProperties.iD = 2; // compute scope for sub-device 1, since aggregated scope is present, ID is sub-device index + 1
    mockMetricScopeCompute1 = new MockMetricScope(scopeProperties, false, 1);
    hMockScopeCompute1 = mockMetricScopeCompute1->toHandle();

    hComputeMetricScopes = {hMockScopeCompute0, hMockScopeCompute1};
    hAllScopes = {hMockScopeAggregated, hMockScopeCompute0, hMockScopeCompute1};

    // Calculation operation for sub-device 0, compute scope 1
    calculationDesc.metricScopesCount = 1;
    calculationDesc.phMetricScopes = &hMockScopeCompute0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                             rootDevice->toHandle(), &calculationDesc,
                                                                             &hCalcOpCompScope1));
    EXPECT_NE(hCalcOpCompScope1, nullptr);
    hCalcOps.push_back(hCalcOpCompScope1);

    // Calculation operation for sub-device 1, compute scope 2
    calculationDesc.phMetricScopes = &hMockScopeCompute1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                             rootDevice->toHandle(), &calculationDesc,
                                                                             &hCalcOpCompScope2));
    EXPECT_NE(hCalcOpCompScope2, nullptr);
    hCalcOps.push_back(hCalcOpCompScope2);

    // Calculation operation for all subdevices
    calculationDesc.metricScopesCount = 2;
    calculationDesc.phMetricScopes = hComputeMetricScopes.data();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                             rootDevice->toHandle(), &calculationDesc,
                                                                             &hCalcOpAllCompScopes));
    EXPECT_NE(hCalcOpAllCompScopes, nullptr);
    hCalcOps.push_back(hCalcOpAllCompScopes);

    // Calculation operation for only aggregated scope
    calculationDesc.metricScopesCount = 1;
    calculationDesc.phMetricScopes = &hMockScopeAggregated;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                             rootDevice->toHandle(), &calculationDesc,
                                                                             &hCalcOpAggScope));
    EXPECT_NE(hCalcOpAggScope, nullptr);
    hCalcOps.push_back(hCalcOpAggScope);

    // Calculation operation for all scopes aggregated and two compute
    calculationDesc.metricScopesCount = 3;
    calculationDesc.phMetricScopes = hAllScopes.data();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                             rootDevice->toHandle(), &calculationDesc,
                                                                             &hCalcOpAllScopes));
    EXPECT_NE(hCalcOpAllScopes, nullptr);
    hCalcOps.push_back(hCalcOpAllScopes);
}

void MetricIpSamplingMetricsAggregationMultiDevFixture::TearDown() {
    MetricIpSamplingMultiDevFixture::TearDown();
    hCalcOps.clear();

    rootDevice = nullptr;
    delete mockMetricScopeAggregated;
    mockMetricScopeAggregated = nullptr;
    hMockScopeAggregated = nullptr;
    delete mockMetricScopeCompute0;
    mockMetricScopeCompute0 = nullptr;
    hMockScopeCompute0 = nullptr;
    delete mockMetricScopeCompute1;
    mockMetricScopeCompute1 = nullptr;
    hMockScopeCompute1 = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalcOpCompScope1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalcOpCompScope2));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalcOpAllCompScopes));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalcOpAggScope));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalcOpAllScopes));
}

} // namespace ult
} // namespace L0
