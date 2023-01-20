/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/windows/device_time_wddm.h"
#include "shared/source/os_interface/windows/os_time_win.h"

namespace NEO {
struct MockDeviceTimeWddm : DeviceTimeWddm {
    using DeviceTimeWddm::convertTimestampsFromOaToCsDomain;
};
class MockOSTimeWin : public OSTimeWin {
  public:
    MockOSTimeWin(Wddm *wddm) {
        this->deviceTime = std::make_unique<DeviceTimeWddm>(wddm);
    }
    void convertTimestampsFromOaToCsDomain(const GfxCoreHelper &gfxCoreHelper, uint64_t &timestampData, uint64_t freqOA, uint64_t freqCS) {
        static_cast<MockDeviceTimeWddm *>(this->deviceTime.get())->convertTimestampsFromOaToCsDomain(gfxCoreHelper, timestampData, freqOA, freqCS);
    }
};
} // namespace NEO
