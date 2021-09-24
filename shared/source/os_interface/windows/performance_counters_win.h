/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/performance_counters.h"

namespace NEO {

class PerformanceCountersWin : public PerformanceCounters {
  public:
    PerformanceCountersWin() = default;
    ~PerformanceCountersWin() override = default;

    /////////////////////////////////////////////////////
    // Gpu oa/mmio configuration.
    /////////////////////////////////////////////////////
    bool enableCountersConfiguration() override;
    void releaseCountersConfiguration() override;
};
} // namespace NEO
