/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/os_interface/linux/performance_counters_linux.h"

namespace NEO {

class MockPerformanceCountersLinux : public PerformanceCountersLinux {
  public:
    MockPerformanceCountersLinux(Device *device);
};
} // namespace NEO
