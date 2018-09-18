/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
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
