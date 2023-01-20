/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/performance_counters_linux.h"

#include "opencl/test/unit_test/os_interface/mock_performance_counters.h"

namespace NEO {

using MetricsLibraryApi::LinuxAdapterType;

class MockPerformanceCountersLinux : public PerformanceCountersLinux {
  public:
    MockPerformanceCountersLinux();
};
} // namespace NEO
