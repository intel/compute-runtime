/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/metrics/metric_ip_sampling_fixture.h"

#include "shared/test/common/helpers/mock_product_helper_hw.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device.h"
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

    auto mockProductHelperHw = std::make_unique<MockProductHelperHw<IGFX_UNKNOWN>>();
    std::unique_ptr<ProductHelper> mockProductHelper = std::move(mockProductHelperHw);

    MultiDeviceFixture::numRootDevices = 1;
    MultiDeviceFixture::numSubDevices = 2;
    MultiDeviceFixture::setUp();
    testDevices.reserve(MultiDeviceFixture::numRootDevices +
                        (MultiDeviceFixture::numRootDevices *
                         MultiDeviceFixture::numSubDevices));
    for (auto device : driverHandle->devices) {

        testDevices.push_back(device);
        auto &l0Device = *device;
        const uint32_t subDeviceCount = static_cast<uint32_t>(l0Device.subDevices.size());
        for (uint32_t i = 0; i < subDeviceCount; i++) {
            testDevices.push_back(l0Device.subDevices[i]);
        }
    }

    for (auto &device : testDevices) {
        auto neoDevice = device->getNEODevice();
        auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
        std::swap(rootDeviceEnvironment.productHelper, mockProductHelper);
    }

    osInterfaceVector.reserve(testDevices.size());
    for (auto device : testDevices) {
        auto mockMetricIpSamplingOsInterface = new MockMetricIpSamplingOsInterface();
        osInterfaceVector.push_back(mockMetricIpSamplingOsInterface);
        std::unique_ptr<MetricIpSamplingOsInterface> metricIpSamplingOsInterface = std::unique_ptr<MetricIpSamplingOsInterface>(mockMetricIpSamplingOsInterface);
        auto &metricSource = device->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
        metricSource.setMetricOsInterface(metricIpSamplingOsInterface);

        auto &metricOaSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
        metricOaSource.setInitializationState(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
    globalDriverHandles->push_back(driverHandle.get());

    ipSamplingTestProductHelper = IpSamplingTestProductHelper::create();
    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    rootOneSubDev.insert(rootOneSubDev.end(), testDevices.begin(), testDevices.begin() + 2);
}

void MetricIpSamplingMultiDevFixture::TearDown() {
    MultiDeviceFixture::tearDown();
    globalDriverHandles->clear();
    delete ipSamplingTestProductHelper;
    ipSamplingTestProductHelper = nullptr;
}

zet_metric_group_handle_t MetricIpSamplingMultiDevFixture::getMetricGroupForDevice(L0::Device *device) {
    uint32_t metricGroupCount = 1;
    zet_metric_group_handle_t metricGroupHandle;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    return metricGroupHandle;
}

void MetricIpSamplingFixture::SetUp() {
    DeviceFixture::setUp();

    auto mockProductHelperHw = std::make_unique<MockProductHelperHw<IGFX_UNKNOWN>>();
    std::unique_ptr<ProductHelper> mockProductHelper = std::move(mockProductHelperHw);
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    std::swap(rootDeviceEnvironment.productHelper, mockProductHelper);

    auto mockMetricIpSamplingOsInterface = new MockMetricIpSamplingOsInterface();
    osInterfaceVector.push_back(mockMetricIpSamplingOsInterface);
    std::unique_ptr<MetricIpSamplingOsInterface> metricIpSamplingOsInterface = std::unique_ptr<MetricIpSamplingOsInterface>(mockMetricIpSamplingOsInterface);

    auto &metricSource = device->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
    metricSource.setMetricOsInterface(metricIpSamplingOsInterface);

    auto &metricOaSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    metricOaSource.setInitializationState(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

    globalDriverHandles->push_back(driverHandle.get());

    ipSamplingTestProductHelper = IpSamplingTestProductHelper::create();
    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getMetricDeviceContext().enableMetricApi());
}

void MetricIpSamplingFixture::TearDown() {
    DeviceFixture::tearDown();
    globalDriverHandles->clear();
    delete ipSamplingTestProductHelper;
    ipSamplingTestProductHelper = nullptr;
}

void MetricIpSamplingCalculateBaseFixture::initRawReports(IpSamplingTestProductHelper *ipSamplingTestProductHelper, PRODUCT_FAMILY productFamily) {
    ipSamplingTestProductHelper->rawElementsToRawReports(productFamily, false, &rawReports);
    rawReportsBytesSize = sizeof(rawReports[0][0]) * rawReports[0].size() * rawReports.size();
}

void MetricIpSamplingCalculateOperationFixture::initCalcDescHandles(L0::ContextImp *context,
                                                                    L0::Device *device) {

    uint32_t metricScopesCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricScopesGetExp(context->toHandle(),
                                                            device->toHandle(),
                                                            &metricScopesCount,
                                                            nullptr));
    scopesPerDevice[device].resize(metricScopesCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricScopesGetExp(context->toHandle(),
                                                            device->toHandle(),
                                                            &metricScopesCount,
                                                            scopesPerDevice[device].data()));

    uint32_t metricGroupCount = 1;
    zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandlePerDevice[device]);

    calcDescPerDevice[device].stype = ZET_INTEL_STRUCTURE_TYPE_METRIC_CALCULATION_DESC_EXP;
    calcDescPerDevice[device].pNext = nullptr;                                      // pNext
    calcDescPerDevice[device].metricGroupCount = metricGroupCount;                  // metricGroupCount
    calcDescPerDevice[device].phMetricGroups = &metricGroupHandlePerDevice[device]; // phMetricGroups
    calcDescPerDevice[device].metricCount = 0;                                      // metricCount
    calcDescPerDevice[device].phMetrics = nullptr;                                  // phMetrics
    calcDescPerDevice[device].timeWindowsCount = 0;                                 // timeWindowsCount
    calcDescPerDevice[device].pCalculationTimeWindows = nullptr;                    // pCalculationTimeWindows
    calcDescPerDevice[device].timeAggregationWindow = 0;                            // timeAggregationWindow
    calcDescPerDevice[device].metricScopesCount = metricScopesCount;                // metricScopesCount
    calcDescPerDevice[device].phMetricScopes = scopesPerDevice[device].data();      // phMetricScopes
}

void MetricIpSamplingCalculateOperationFixture::cleanUpHandles() {
    metricGroupHandlePerDevice.clear();
    scopesPerDevice.clear();
    calcDescPerDevice.clear();
}

void MetricIpSamplingCalculateMetricGroupFixture::SetUp() {
    MetricIpSamplingMultiDevFixture::SetUp();

    rootDevice = testDevices[0];
    initRawReports(ipSamplingTestProductHelper, productFamily);
}

void MetricIpSamplingCalculateMetricGroupFixture::TearDown() {
    MetricIpSamplingMultiDevFixture::TearDown();
    rootDevice = nullptr;
}

void MetricIpSamplingCalculateOperationFixture::SetUp() {
    MetricIpSamplingMultiDevFixture::SetUp();

    for (auto device : testDevices) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, device->getMetricDeviceContext().enableMetricApi());
        initCalcDescHandles(context, device);
    }

    rootDevice = testDevices[0];
    subDevice = testDevices[1];
    initRawReports(ipSamplingTestProductHelper, productFamily);
}

void MetricIpSamplingCalculateOperationFixture::TearDown() {
    MetricIpSamplingMultiDevFixture::TearDown();
    rootDevice = nullptr;
    subDevice = nullptr;
    cleanUpHandles();
}

void MetricIpSamplingMetricsAggregationMultiDevFixture::initMultiRawReports() {

    MockRawDataHelper::rawElementsToRawReports(static_cast<uint32_t>(rawDataElements2.size()), rawDataElements2, rawReports2);
    rawReports2BytesSize = sizeof(rawReports2[0][0]) * rawReports2[0].size() * rawReports2.size();

    MockRawDataHelper::rawElementsToRawReports(static_cast<uint32_t>(rawDataElementsAppend.size()), rawDataElementsAppend, rawReportsAppend);
    rawReportsAppendBytesSize = sizeof(rawReportsAppend[0][0]) * rawReportsAppend[0].size() * rawReportsAppend.size();
}

void MetricIpSamplingMetricsAggregationMultiDevFixture::SetUp() {

    MetricIpSamplingMultiDevFixture::SetUp();
    initRawReports(ipSamplingTestProductHelper, productFamily);
    initMultiRawReports();

    rootDevice = testDevices[0];
    EXPECT_EQ(ZE_RESULT_SUCCESS, rootDevice->getMetricDeviceContext().enableMetricApi());

    uint32_t metricGroupCount = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupGet(rootDevice->toHandle(), &metricGroupCount, &rootDevMetricGroupHandle));
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(rootDevMetricGroupHandle, nullptr);

    zet_intel_metric_calculation_exp_desc_t calculationDesc{};
    calculationDesc.stype = ZET_INTEL_STRUCTURE_TYPE_METRIC_CALCULATION_DESC_EXP;
    calculationDesc.pNext = nullptr;
    calculationDesc.metricGroupCount = 1;
    calculationDesc.phMetricGroups = &rootDevMetricGroupHandle;
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

    // Override scopes for all metrics to include all scopes for testing purposes
    uint32_t metricCount = 0;
    EXPECT_EQ(zetMetricGet(rootDevMetricGroupHandle, &metricCount, nullptr), ZE_RESULT_SUCCESS);
    std::vector<zet_metric_handle_t> hMetrics(metricCount);
    EXPECT_EQ(zetMetricGet(rootDevMetricGroupHandle, &metricCount, hMetrics.data()), ZE_RESULT_SUCCESS);
    for (auto &hMetric : hMetrics) {
        auto metric = static_cast<MockMetric *>(Metric::fromHandle(hMetric));
        metric->setScopes(hAllScopes);
    }

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
