/*
* Copyright (c) 2017 - 2018, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#include "runtime/os_interface/windows/windows_wrapper.h"
#include "unit_tests/mocks/mock_wddm.h"
#include "mock_performance_counters_win.h"

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
