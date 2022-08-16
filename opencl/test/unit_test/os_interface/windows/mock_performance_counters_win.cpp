/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_performance_counters_win.h"

#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"
#include "shared/test/common/mocks/mock_wddm.h"

#include "opencl/test/unit_test/os_interface/windows/mock_os_time_win.h"

namespace NEO {
///////////////////////////////////////////////////////
// MockPerformanceCountersWin::MockPerformanceCountersWin
///////////////////////////////////////////////////////
MockPerformanceCountersWin::MockPerformanceCountersWin()
    : PerformanceCountersWin() {
}

///////////////////////////////////////////////////////
// MockPerformanceCounters::create
///////////////////////////////////////////////////////
std::unique_ptr<PerformanceCounters> MockPerformanceCounters::create() {
    auto performanceCounters = std::unique_ptr<PerformanceCounters>(new MockPerformanceCountersWin());
    auto metricsLibrary = std::make_unique<MockMetricsLibrary>();
    auto metricsLibraryDll = std::make_unique<MockMetricsLibraryDll>();

    metricsLibrary->api = std::make_unique<MockMetricsLibraryValidInterface>();
    metricsLibrary->osLibrary = std::move(metricsLibraryDll);
    performanceCounters->setMetricsLibraryInterface(std::move(metricsLibrary));

    return performanceCounters;
}

//////////////////////////////////////////////////////
// PerformanceCountersFixture::setUp
//////////////////////////////////////////////////////
void PerformanceCountersFixture::setUp() {
    device = std::make_unique<MockClDevice>(new MockDevice());
    context = std::make_unique<MockContext>(device.get());
    queue = std::make_unique<MockCommandQueue>(context.get(), device.get(), &queueProperties, false);
    osInterface = std::unique_ptr<OSInterface>(new OSInterface());
    osInterface->setDriverModel(std::unique_ptr<DriverModel>(new WddmMock(*rootDeviceEnvironment)));
    device->setOSTime(new MockOSTimeWin(osInterface.get()));
}

//////////////////////////////////////////////////////
// PerformanceCountersFixture::tearDown
//////////////////////////////////////////////////////
void PerformanceCountersFixture::tearDown() {
}

} // namespace NEO
