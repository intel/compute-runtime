/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/os_interface/linux/performance_counters_linux.h"

namespace NEO {

class MockPerformanceCountersLinux : public PerformanceCountersLinux {
  public:
    MockPerformanceCountersLinux(Device *device);
};
} // namespace NEO
