/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_performance_counters_linux.h"

#include "unit_tests/os_interface/linux/mock_os_time_linux.h"
#include "unit_tests/os_interface/mock_performance_counters.h"

namespace NEO {
//////////////////////////////////////////////////////
// MockPerformanceCountersLinux::MockPerformanceCountersLinux
//////////////////////////////////////////////////////
MockPerformanceCountersLinux::MockPerformanceCountersLinux(Device *device)
    : PerformanceCountersLinux() {
}

//////////////////////////////////////////////////////
// MockPerformanceCounters::create
//////////////////////////////////////////////////////
std::unique_ptr<PerformanceCounters> MockPerformanceCounters::create(Device *device) {
    auto performanceCounters = std::unique_ptr<PerformanceCounters>(new MockPerformanceCountersLinux(device));
    auto metricsLibrary = std::make_unique<MockMetricsLibrary>();
    auto metricsLibraryDll = std::make_unique<MockMetricsLibraryDll>();

    metricsLibrary->api = std::make_unique<MockMetricsLibraryValidInterface>();
    metricsLibrary->osLibrary = std::move(metricsLibraryDll);
    performanceCounters->setMetricsLibraryInterface(std::move(metricsLibrary));

    return performanceCounters;
}

//////////////////////////////////////////////////////
// PerformanceCountersFixture::createPerfCounters
//////////////////////////////////////////////////////
void PerformanceCountersFixture::createPerfCounters() {
    performanceCountersBase = MockPerformanceCounters::create(&device->getDevice());
}

//////////////////////////////////////////////////////
// PerformanceCountersFixture::SetUp
//////////////////////////////////////////////////////
void PerformanceCountersFixture::SetUp() {
    device = std::make_unique<MockClDevice>(new MockDevice());
    context = std::make_unique<MockContext>(device.get());
    queue = std::make_unique<MockCommandQueue>(context.get(), device.get(), &queueProperties);
    osInterface = std::unique_ptr<OSInterface>(new OSInterface());
    device->setOSTime(new MockOSTimeLinux(osInterface.get()));
}

//////////////////////////////////////////////////////
// PerformanceCountersFixture::TearDown
//////////////////////////////////////////////////////
void PerformanceCountersFixture::TearDown() {
}
} // namespace NEO
