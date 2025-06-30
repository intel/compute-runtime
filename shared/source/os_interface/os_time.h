/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
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

enum class TimeQueryStatus : uint32_t {
    success,
    unsupportedFeature,
    deviceLost
};

class OSTime;

class DeviceTime {
  public:
    virtual ~DeviceTime() = default;
    TimeQueryStatus getGpuCpuTime(TimeStampData *pGpuCpuTime, OSTime *osTime, bool forceKmdCall);
    virtual TimeQueryStatus getGpuCpuTimeImpl(TimeStampData *pGpuCpuTime, OSTime *osTime);
    virtual double getDynamicDeviceTimerResolution() const;
    virtual uint64_t getDynamicDeviceTimerClock() const;
    virtual bool isTimestampsRefreshEnabled() const;
    TimeQueryStatus getGpuCpuTimestamps(TimeStampData *timeStamp, OSTime *osTime, bool forceKmdCall);
    void setDeviceTimerResolution();
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
    uint64_t timestampRefreshTimeoutNS = NSEC_PER_MSEC * 100;    // 100ms
    bool refreshTimestamps = true;
    TimeStampData fetchedTimestamps{};
};

class OSTime {
  public:
    static std::unique_ptr<OSTime> create(OSInterface *osInterface);
    OSTime(std::unique_ptr<DeviceTime> deviceTime);

    virtual ~OSTime() = default;
    virtual bool getCpuTime(uint64_t *timeStamp);
    virtual bool getCpuTimeHost(uint64_t *timeStamp);
    virtual double getHostTimerResolution() const;
    virtual uint64_t getCpuRawTimestamp();

    static double getDeviceTimerResolution();

    TimeQueryStatus getGpuCpuTime(TimeStampData *gpuCpuTime, bool forceKmdCall) {
        return deviceTime->getGpuCpuTime(gpuCpuTime, this, forceKmdCall);
    }

    TimeQueryStatus getGpuCpuTime(TimeStampData *gpuCpuTime) {
        return deviceTime->getGpuCpuTime(gpuCpuTime, this, false);
    }

    double getDynamicDeviceTimerResolution() const {
        return deviceTime->getDynamicDeviceTimerResolution();
    }

    uint64_t getDynamicDeviceTimerClock() const {
        return deviceTime->getDynamicDeviceTimerClock();
    }

    uint64_t getMaxGpuTimeStamp() const { return maxGpuTimeStamp; }

    void setDeviceTimerResolution() const {
        deviceTime->setDeviceTimerResolution();
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
