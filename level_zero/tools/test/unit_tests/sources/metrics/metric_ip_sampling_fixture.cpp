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
    metricSource.setMetricOsInterface(metricIpSamplingOsInterface);

    auto &metricOaSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    metricOaSource.setInitializationState(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

    globalDriverHandles->push_back(driverHandle.get());
}

void MetricIpSamplingFixture::TearDown() {
    DeviceFixture::tearDown();
    globalDriverHandles->clear();
}

} // namespace ult
} // namespace L0
