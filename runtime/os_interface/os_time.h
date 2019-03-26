/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <memory>

#define NSEC_PER_SEC (1000000000ULL)

namespace NEO {

class OSInterface;
struct HardwareInfo;

struct TimeStampData {
    uint64_t GPUTimeStamp; // GPU time in ns
    uint64_t CPUTimeinNS;  // CPU time in ns
};

class OSTime {
  public:
    static std::unique_ptr<OSTime> create(OSInterface *osInterface);

    virtual ~OSTime() = default;
    virtual bool getCpuTime(uint64_t *timeStamp) = 0;
    virtual bool getCpuGpuTime(TimeStampData *pGpuCpuTime) = 0;
    virtual double getHostTimerResolution() const = 0;
    virtual double getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const = 0;
    virtual uint64_t getCpuRawTimestamp() = 0;
    OSInterface *getOSInterface() const {
        return osInterface;
    }

    static double getDeviceTimerResolution(HardwareInfo const &hwInfo);

  protected:
    OSTime() {}
    OSInterface *osInterface = nullptr;
};
} // namespace NEO
