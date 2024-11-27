/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_time.h"

namespace NEO {
class Drm;

class DeviceTimeDrm : public DeviceTime {
  public:
    DeviceTimeDrm(OSInterface &osInterface);
    TimeQueryStatus getGpuCpuTimeImpl(TimeStampData *pGpuCpuTime, OSTime *osTime) override;
    double getDynamicDeviceTimerResolution() const override;
    uint64_t getDynamicDeviceTimerClock() const override;
    bool isTimestampsRefreshEnabled() const override;

  protected:
    Drm *pDrm = nullptr;

    static constexpr double nanosecondsPerSecond = 1000000000.0;
};

} // namespace NEO
