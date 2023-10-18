/*
 * Copyright (C) 2018-2023 Intel Corporation
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
    bool getGpuCpuTimeImpl(TimeStampData *pGpuCpuTime, OSTime *osTime) override;
    typedef bool (DeviceTimeDrm::*TimestampFunction)(uint64_t *);
    void timestampTypeDetect();
    TimestampFunction getGpuTime = nullptr;
    bool getGpuTime32(uint64_t *timestamp);
    bool getGpuTime36(uint64_t *timestamp);
    bool getGpuTimeSplitted(uint64_t *timestamp);
    double getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const override;
    uint64_t getDynamicDeviceTimerClock(HardwareInfo const &hwInfo) const override;

  protected:
    Drm *pDrm = nullptr;
};

} // namespace NEO
