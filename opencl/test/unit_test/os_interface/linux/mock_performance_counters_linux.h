/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/performance_counters_linux.h"

namespace NEO {

class MockPerformanceCountersLinux : public PerformanceCountersLinux {
  public:
    MockPerformanceCountersLinux();
};
} // namespace NEO
