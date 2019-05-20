/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/os_interface/windows/performance_counters_win.h"
#include "unit_tests/os_interface/mock_performance_counters.h"

namespace NEO {

class MockPerformanceCountersWin : public PerformanceCountersWin {
  public:
    MockPerformanceCountersWin(Device *device);
};
} // namespace NEO
