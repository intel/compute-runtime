/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/windows/os_time_win.h"

namespace NEO {
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
} // namespace NEO