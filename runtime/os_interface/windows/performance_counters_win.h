/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/os_interface/performance_counters.h"
#include "runtime/os_interface/windows/os_interface.h"

namespace NEO {

class PerformanceCountersWin : virtual public PerformanceCounters {
  public:
    PerformanceCountersWin(OSTime *osTime);
    ~PerformanceCountersWin() override;
    void initialize(const HardwareInfo *hwInfo) override;

  protected:
    decltype(&instrSetAvailable) setAvailableFunc = instrSetAvailable;
    decltype(&instrEscVerifyEnable) verifyEnableFunc = instrEscVerifyEnable;
};
} // namespace NEO
