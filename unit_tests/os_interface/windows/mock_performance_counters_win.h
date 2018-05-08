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

#pragma once
#include "runtime/os_interface/windows/performance_counters_win.h"
#include "unit_tests/os_interface/mock_performance_counters.h"
#include "unit_tests/os_interface/windows/mock_os_time_win.h"

namespace OCLRT {

bool setAvailable(bool value);
void verifyEnable(InstrEscCbData cbData);

class PerfCounterFlagsWin : public PerfCounterFlags {
  public:
    static int setAvailableFuncCalled;
    static int verifyEnableFuncCalled;
    static void resetPerfCountersFlags();
};

class MockPerformanceCountersWin : public PerformanceCountersWin,
                                   public MockPerformanceCounters {
  public:
    MockPerformanceCountersWin(OSTime *osTime);
    uint32_t getReportId() override {
        return MockPerformanceCounters::getReportId();
    }
    void initialize(const HardwareInfo *hwInfo) override {
        MockPerformanceCounters::initialize(hwInfo);
        return PerformanceCountersWin::initialize(hwInfo);
    }
};
} // namespace OCLRT
