/*
 * Copyright (C) 2018-2021 Intel Corporation
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
MockPerformanceCountersWin::MockPerformanceCountersWin(Device *device)
    : PerformanceCountersWin() {
}

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
    queue = std::make_unique<MockCommandQueue>(context.get(), device.get(), &queueProperties, false);
    osInterface = std::unique_ptr<OSInterface>(new OSInterface());
    osInterface->setDriverModel(std::unique_ptr<DriverModel>(new WddmMock(*rootDeviceEnvironment)));
    device->setOSTime(new MockOSTimeWin(osInterface.get()));
}

//////////////////////////////////////////////////////
// PerformanceCountersFixture::TearDown
//////////////////////////////////////////////////////
void PerformanceCountersFixture::TearDown() {
}

} // namespace NEO
