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
    MockOSTimeWin(Wddm *inWddm) {
        wddm = inWddm;
    }

    double getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const override {
        return OSTimeWin::getDynamicDeviceTimerResolution(hwInfo);
    };
};
} // namespace NEO
