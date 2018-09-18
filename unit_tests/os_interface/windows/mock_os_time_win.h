/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/os_interface/windows/os_time_win.h"

namespace OCLRT {
class MockOSTimeWin : public OSTimeWin {
  public:
    MockOSTimeWin(OSInterface *osInterface) : OSTimeWin(osInterface){};

    void overrideQueryPerformanceCounterFunction(decltype(&QueryPerformanceCounter) function) {
        this->QueryPerfomanceCounterFnc = function;
    }

    void setFrequency(LARGE_INTEGER frequency) {
        this->frequency = frequency;
    }
};
} // namespace OCLRT