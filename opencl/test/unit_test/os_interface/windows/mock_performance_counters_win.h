/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/os_interface/windows/performance_counters_win.h"
#include "opencl/test/unit_test/os_interface/mock_performance_counters.h"

namespace NEO {

class MockPerformanceCountersWin : public PerformanceCountersWin {
  public:
    MockPerformanceCountersWin(Device *device);
};
} // namespace NEO
