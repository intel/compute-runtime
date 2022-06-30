/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/windows/device_time_wddm.h"
#include "shared/source/os_interface/windows/os_time_win.h"

namespace NEO {
class MockOSTimeWin : public OSTimeWin {
  public:
    MockOSTimeWin(Wddm *wddm) {
        this->deviceTime = std::make_unique<DeviceTimeWddm>(wddm);
    }
};
} // namespace NEO
