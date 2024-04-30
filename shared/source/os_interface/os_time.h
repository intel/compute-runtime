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
#define NSEC_PER_MSEC (NSEC_PER_SEC / 1000)
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
    DeviceTime();
    virtual ~DeviceTime() = default;
    bool getGpuCpuTime(TimeStampData *pGpuCpuTime, OSTime *osTime, bool forceKmdCall);
    virtual bool getGpuCpuTimeImpl(TimeStampData *pGpuCpuTime, OSTime *osTime);
    virtual double getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const;
    virtual uint64_t getDynamicDeviceTimerClock(HardwareInfo const &hwInfo) const;
    bool getGpuCpuTimestamps(TimeStampData *timeStamp, OSTime *osTime, bool forceKmdCall);
    void setDeviceTimerResolution(HardwareInfo const &hwInfo);
    void setRefreshTimestampsFlag() {
        refreshTimestamps = true;
    }
    uint64_t getTimestampRefreshTimeout() const {
        return timestampRefreshTimeoutNS;
    };

    std::optional<uint64_t> initialGpuTimeStamp{};
    bool waitingForGpuTimeStampOverflow = false;
    uint64_t gpuTimeStampOverflowCounter = 0;

    double deviceTimerResolution = 0;
    const uint64_t timestampRefreshMinTimeoutNS = NSEC_PER_MSEC; // 1ms
    const uint64_t timestampRefreshMaxTimeoutNS = NSEC_PER_SEC;  // 1s
    uint64_t timestampRefreshTimeoutNS = 0;
    bool refreshTimestamps = true;
    bool reusingTimestampsEnabled = false;
    TimeStampData fetchedTimestamps{};
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

    bool getGpuCpuTime(TimeStampData *gpuCpuTime, bool forceKmdCall) {
        return deviceTime->getGpuCpuTime(gpuCpuTime, this, forceKmdCall);
    }

    bool getGpuCpuTime(TimeStampData *gpuCpuTime) {
        return deviceTime->getGpuCpuTime(gpuCpuTime, this, false);
    }

    double getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const {
        return deviceTime->getDynamicDeviceTimerResolution(hwInfo);
    }

    uint64_t getDynamicDeviceTimerClock(HardwareInfo const &hwInfo) const {
        return deviceTime->getDynamicDeviceTimerClock(hwInfo);
    }

    uint64_t getMaxGpuTimeStamp() const { return maxGpuTimeStamp; }

    void setDeviceTimerResolution(HardwareInfo const &hwInfo) const {
        deviceTime->setDeviceTimerResolution(hwInfo);
    }

    void setRefreshTimestampsFlag() const {
        deviceTime->setRefreshTimestampsFlag();
    }

    uint64_t getTimestampRefreshTimeout() const {
        return deviceTime->getTimestampRefreshTimeout();
    }

  protected:
    OSTime() = default;
    OSInterface *osInterface = nullptr;
    std::unique_ptr<DeviceTime> deviceTime;
    uint64_t maxGpuTimeStamp = 0;
};
} // namespace NEO
