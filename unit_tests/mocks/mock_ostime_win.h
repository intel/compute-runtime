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
    MockOSTimeWin(Wddm *inWddm) {
        wddm = inWddm;
    }

    double getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const override {
        return OSTimeWin::getDynamicDeviceTimerResolution(hwInfo);
    };
};
} // namespace OCLRT
