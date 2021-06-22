/*
 * Copyright (C) 2018-2021 Intel Corporation
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
    uint64_t GPUTimeStamp; // GPU time in counter ticks
    uint64_t CPUTimeinNS;  // CPU time in ns
};

class OSTime;

class DeviceTime {
  public:
    virtual ~DeviceTime() = default;
    virtual bool getCpuGpuTime(TimeStampData *pGpuCpuTime, OSTime *osTime) = 0;
    virtual double getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const = 0;
    virtual uint64_t getDynamicDeviceTimerClock(HardwareInfo const &hwInfo) const = 0;
};

class OSTime {
  public:
    static std::unique_ptr<OSTime> create(OSInterface *osInterface);

    virtual ~OSTime() = default;
    virtual bool getCpuTime(uint64_t *timeStamp) = 0;
    virtual double getHostTimerResolution() const = 0;
    virtual uint64_t getCpuRawTimestamp() = 0;
    OSInterface *getOSInterface() const {
        return osInterface;
    }

    static double getDeviceTimerResolution(HardwareInfo const &hwInfo);
    bool getCpuGpuTime(TimeStampData *gpuCpuTime) {
        return deviceTime->getCpuGpuTime(gpuCpuTime, this);
    }

    double getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const {
        return deviceTime->getDynamicDeviceTimerResolution(hwInfo);
    }

    uint64_t getDynamicDeviceTimerClock(HardwareInfo const &hwInfo) const {
        return deviceTime->getDynamicDeviceTimerClock(hwInfo);
    }

  protected:
    OSTime() {}
    OSInterface *osInterface = nullptr;
    std::unique_ptr<DeviceTime> deviceTime;
};
} // namespace NEO
