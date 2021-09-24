/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/performance_counters.h"

namespace NEO {

using MetricsLibraryApi::ClientDataLinuxAdapter_1_0;
using MetricsLibraryApi::LinuxAdapterType;

class PerformanceCountersLinux : public PerformanceCounters {
  public:
    PerformanceCountersLinux() = default;
    ~PerformanceCountersLinux() override = default;

    /////////////////////////////////////////////////////
    // Gpu oa/mmio configuration.
    /////////////////////////////////////////////////////
    bool enableCountersConfiguration() override;
    void releaseCountersConfiguration() override;

    /////////////////////////////////////////////////////
    // Metrics Library client adapter data.
    /////////////////////////////////////////////////////
    ClientDataLinuxAdapter_1_0 adapter = {};
};
} // namespace NEO
