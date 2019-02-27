/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_performance_counters_win.h"

#include "runtime/os_interface/windows/windows_wrapper.h"
#include "unit_tests/mocks/mock_wddm.h"

namespace OCLRT {
MockPerformanceCountersWin::MockPerformanceCountersWin(OSTime *osTime)
    : PerformanceCounters(osTime), PerformanceCountersWin(osTime), MockPerformanceCounters(osTime) {
    setAvailableFunc = setAvailable;
    verifyEnableFunc = verifyEnable;
    PerfCounterFlagsWin::resetPerfCountersFlags();
}
std::unique_ptr<PerformanceCounters> MockPerformanceCounters::create(OSTime *osTime) {
    return std::unique_ptr<PerformanceCounters>(new MockPerformanceCountersWin(osTime));
}

int PerfCounterFlagsWin::setAvailableFuncCalled;
int PerfCounterFlagsWin::verifyEnableFuncCalled;

bool setAvailable(bool value) {
    PerfCounterFlagsWin::setAvailableFuncCalled++;
    return value;
}

void verifyEnable(InstrEscCbData cbData) {
    PerfCounterFlagsWin::verifyEnableFuncCalled++;
}

void PerfCounterFlagsWin::resetPerfCountersFlags() {
    PerfCounterFlags::resetPerfCountersFlags();
    PerfCounterFlagsWin::setAvailableFuncCalled = 0;
    PerfCounterFlagsWin::verifyEnableFuncCalled = 0;
}

void PerformanceCountersFixture::createPerfCounters() {
    performanceCountersBase = std::unique_ptr<MockPerformanceCounters>(new MockPerformanceCountersWin(osTimeBase.get()));
}
void PerformanceCountersFixture::createOsTime() {
    osTimeBase = std::unique_ptr<MockOSTimeWin>(new MockOSTimeWin(osInterfaceBase.get()));
}
void PerformanceCountersFixture::fillOsInterface() {
    osInterfaceBase->get()->setWddm(new WddmMock());
}
void PerformanceCountersFixture::releaseOsInterface() {
}
} // namespace OCLRT
