/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_performance_counters_linux.h"

#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_os_time_linux.h"

#include "opencl/test/unit_test/os_interface/mock_performance_counters.h"

namespace NEO {
//////////////////////////////////////////////////////
// MockPerformanceCountersLinux::MockPerformanceCountersLinux
//////////////////////////////////////////////////////
MockPerformanceCountersLinux::MockPerformanceCountersLinux()
    : PerformanceCountersLinux() {
}

//////////////////////////////////////////////////////
// MockPerformanceCounters::create
//////////////////////////////////////////////////////
std::unique_ptr<PerformanceCounters> MockPerformanceCounters::create() {
    auto performanceCounters = std::unique_ptr<PerformanceCounters>(new MockPerformanceCountersLinux());
    auto metricsLibrary = std::make_unique<MockMetricsLibrary>();
    auto metricsLibraryDll = std::make_unique<MockMetricsLibraryDll>();

    metricsLibrary->api = std::make_unique<MockMetricsLibraryValidInterface>();
    metricsLibrary->osLibrary = std::move(metricsLibraryDll);
    performanceCounters->setMetricsLibraryInterface(std::move(metricsLibrary));

    return performanceCounters;
}

//////////////////////////////////////////////////////
// PerformanceCountersFixture::SetUp
//////////////////////////////////////////////////////
void PerformanceCountersFixture::SetUp() {
    device = std::make_unique<MockClDevice>(new MockDevice());
    context = std::make_unique<MockContext>(device.get());
    queue = std::make_unique<MockCommandQueue>(context.get(), device.get(), &queueProperties, false);
    osInterface = std::unique_ptr<OSInterface>(new OSInterface());
    osInterface->setDriverModel(std::unique_ptr<DriverModel>(new DrmMock(*device->getExecutionEnvironment()->rootDeviceEnvironments[0])));
    device->setOSTime(new MockOSTimeLinux(osInterface.get()));
}

//////////////////////////////////////////////////////
// PerformanceCountersFixture::TearDown
//////////////////////////////////////////////////////
void PerformanceCountersFixture::TearDown() {
}
} // namespace NEO
