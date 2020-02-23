/*
 * Copyright (C) 2017-2020 Intel Corporation
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

class OSTimeLinux : public OSTime {
  public:
    OSTimeLinux(OSInterface *osInterface);
    ~OSTimeLinux() override;
    bool getCpuTime(uint64_t *timeStamp) override;
    bool getCpuGpuTime(TimeStampData *pGpuCpuTime) override;
    typedef bool (OSTimeLinux::*TimestampFunction)(uint64_t *);
    void timestampTypeDetect();
    TimestampFunction getGpuTime = nullptr;
    bool getGpuTime32(uint64_t *timestamp);
    bool getGpuTime36(uint64_t *timestamp);
    bool getGpuTimeSplitted(uint64_t *timestamp);
    double getHostTimerResolution() const override;
    double getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const override;
    uint64_t getCpuRawTimestamp() override;

  protected:
    typedef int (*resolutionFunc_t)(clockid_t, struct timespec *);
    typedef int (*getTimeFunc_t)(clockid_t, struct timespec *);
    Drm *pDrm = nullptr;
    unsigned timestampSizeInBits;
    resolutionFunc_t resolutionFunc;
    getTimeFunc_t getTimeFunc;
};

} // namespace NEO
