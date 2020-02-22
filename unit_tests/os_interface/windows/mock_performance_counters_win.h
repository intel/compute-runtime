/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "unit_tests/os_interface/mock_performance_counters.h"

#include "os_interface/windows/performance_counters_win.h"

namespace NEO {

class MockPerformanceCountersWin : public PerformanceCountersWin {
  public:
    MockPerformanceCountersWin(Device *device);
};
} // namespace NEO
