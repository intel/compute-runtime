/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_performance_counters_win.h"

#include "core/os_interface/os_interface.h"
#include "core/os_interface/windows/windows_wrapper.h"
#include "runtime/os_interface/windows/os_interface.h"
#include "unit_tests/mocks/mock_wddm.h"
#include "unit_tests/os_interface/windows/mock_os_time_win.h"

namespace NEO {

///////////////////////////////////////////////////////
// MockPerformanceCounters::create
///////////////////////////////////////////////////////
std::unique_ptr<PerformanceCounters> MockPerformanceCounters::create(Device *device) {
    auto performanceCounters = std::unique_ptr<PerformanceCounters>(new MockPerformanceCountersWin(device));
    auto metricsLibrary = std::make_unique<MockMetricsLibrary>();
    auto metricsLibraryDll = std::make_unique<MockMetricsLibraryDll>();

    metricsLibrary->api = std::make_unique<MockMetricsLibraryValidInterface>();
    metricsLibrary->osLibrary = std::move(metricsLibraryDll);
    performanceCounters->setMetricsLibraryInterface(std::move(metricsLibrary));

    return performanceCounters;
}

///////////////////////////////////////////////////////
// MockPerformanceCountersWin::MockPerformanceCountersWin
///////////////////////////////////////////////////////
MockPerformanceCountersWin::MockPerformanceCountersWin(Device *device)
    : PerformanceCountersWin() {
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
    osInterface->get()->setWddm(new WddmMock(*rootDeviceEnvironment));
    device->setOSTime(new MockOSTimeWin(osInterface.get()));
}

//////////////////////////////////////////////////////
// PerformanceCountersFixture::TearDown
//////////////////////////////////////////////////////
void PerformanceCountersFixture::TearDown() {
}

} // namespace NEO
