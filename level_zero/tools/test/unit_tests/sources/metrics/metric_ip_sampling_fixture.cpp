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
        metricSource.metricSourceCount = platformIpMetricCountXe;
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
    metricSource.metricSourceCount = platformIpMetricCountXe;
    metricSource.setMetricOsInterface(metricIpSamplingOsInterface);

    auto &metricOaSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    metricOaSource.setInitializationState(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

    globalDriverHandles->push_back(driverHandle.get());
}

void MetricIpSamplingFixture::TearDown() {
    DeviceFixture::tearDown();
    globalDriverHandles->clear();
}

void MetricIpSamplingCalculateBaseFixture::initCalcDescriptor() {
    zet_intel_metric_scope_properties_exp_t scopeProperties{};
    scopeProperties.stype = ZET_STRUCTURE_TYPE_INTEL_METRIC_SCOPE_PROPERTIES_EXP;
    scopeProperties.pNext = nullptr;

    mockMetricScope = new MockMetricScope(scopeProperties, false);
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
}

void MetricIpSamplingCalculateMultiDevFixture::TearDown() {
    MetricIpSamplingMultiDevFixture::TearDown();
    cleanupCalcDescriptor();
}

void MetricIpSamplingCalculateSingleDevFixture::SetUp() {
    MetricIpSamplingFixture::SetUp();
    initCalcDescriptor();

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

} // namespace ult
} // namespace L0
