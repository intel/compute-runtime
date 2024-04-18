/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <memory>
#include <optional>

#define NSEC_PER_SEC (1000000000ULL)

namespace NEO {

class OSInterface;
struct HardwareInfo;

struct TimeStampData {
    uint64_t gpuTimeStamp; // GPU time in counter ticks
    uint64_t cpuTimeinNS;  // CPU time in ns
};

class OSTime;

class DeviceTime {
  public:
    virtual ~DeviceTime() = default;
    bool getGpuCpuTime(TimeStampData *pGpuCpuTime, OSTime *osTime);
    virtual bool getGpuCpuTimeImpl(TimeStampData *pGpuCpuTime, OSTime *osTime);
    virtual double getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const;
    virtual uint64_t getDynamicDeviceTimerClock(HardwareInfo const &hwInfo) const;

    std::optional<uint64_t> initialGpuTimeStamp{};
    bool waitingForGpuTimeStampOverflow = false;
    uint64_t gpuTimeStampOverflowCounter = 0;
};

class OSTime {
  public:
    static std::unique_ptr<OSTime> create(OSInterface *osInterface);
    OSTime(std::unique_ptr<DeviceTime> deviceTime);

    virtual ~OSTime() = default;
    virtual bool getCpuTime(uint64_t *timeStamp);
    virtual double getHostTimerResolution() const;
    virtual uint64_t getCpuRawTimestamp();

    static double getDeviceTimerResolution(HardwareInfo const &hwInfo);
    bool getGpuCpuTime(TimeStampData *gpuCpuTime) {
        return deviceTime->getGpuCpuTime(gpuCpuTime, this);
    }

    double getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const {
        return deviceTime->getDynamicDeviceTimerResolution(hwInfo);
    }

    uint64_t getDynamicDeviceTimerClock(HardwareInfo const &hwInfo) const {
        return deviceTime->getDynamicDeviceTimerClock(hwInfo);
    }

    uint64_t getMaxGpuTimeStamp() const { return maxGpuTimeStamp; }

  protected:
    OSTime() = default;
    OSInterface *osInterface = nullptr;
    std::unique_ptr<DeviceTime> deviceTime;
    uint64_t maxGpuTimeStamp = 0;
};
} // namespace NEO
