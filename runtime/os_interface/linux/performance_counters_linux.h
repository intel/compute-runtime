/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/os_interface/performance_counters.h"

namespace NEO {

class PerformanceCountersLinux : virtual public PerformanceCounters {
  public:
    PerformanceCountersLinux() = default;
    ~PerformanceCountersLinux() override = default;

    /////////////////////////////////////////////////////
    // Gpu oa/mmio configuration.
    /////////////////////////////////////////////////////
    bool enableCountersConfiguration() override;
    void releaseCountersConfiguration() override;
};
} // namespace NEO
