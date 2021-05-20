/*
 * Copyright (C) 2017-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_time.h"

#define OCLRT_NUM_TIMESTAMP_BITS (36)
#define OCLRT_NUM_TIMESTAMP_BITS_FALLBACK (32)
#define TIMESTAMP_HIGH_REG 0x0235C
#define TIMESTAMP_LOW_REG 0x02358

namespace NEO {

class DeviceTimeDrm : public DeviceTime {
  public:
    DeviceTimeDrm(OSInterface *osInterface);
    bool getCpuGpuTime(TimeStampData *pGpuCpuTime, OSTime *osTime) override;
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
    unsigned timestampSizeInBits;
};

} // namespace NEO
