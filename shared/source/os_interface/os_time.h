/*
 * Copyright (C) 2018-2026 Intel Corporation
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
class OsContext;
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

class MmioTimestampPtrHelper {
  public:
    MmioTimestampPtrHelper() = default;
    MmioTimestampPtrHelper(const volatile uint32_t *low, const volatile uint32_t *high) : lowDword(low), highDword(high) {}

    bool isAvailable() const { return lowDword != nullptr && highDword != nullptr; }

    uint64_t read() const {
        uint32_t high = 0u;
        uint32_t low = 0u;

        // Read again to ensure a consistent high and low 32-bit pair
        do {
            high = readHighDword();
            low = *lowDword;
        } while (high != readHighDword());

        return (static_cast<uint64_t>(high) << 32) | low;
    }

  protected:
    MOCKABLE_VIRTUAL uint32_t readHighDword() const { return *highDword; }

    const volatile uint32_t *lowDword = nullptr;
    const volatile uint32_t *highDword = nullptr;
};

class DeviceTime {
  public:
    virtual ~DeviceTime() = default;
    TimeQueryStatus getGpuCpuTime(TimeStampData *pGpuCpuTime, OSTime *osTime, bool forceKmdCall);
    virtual TimeQueryStatus getGpuCpuTimeImpl(TimeStampData *pGpuCpuTime, OSTime *osTime);
    virtual double getDynamicDeviceTimerResolution() const;
    virtual uint64_t getDynamicDeviceTimerClock() const;
    virtual bool isTimestampsRefreshEnabled() const;
    virtual MmioTimestampPtrHelper getMmioTimestampPtrHelper(OsContext &osContext) { return {}; }
    virtual bool isTimestampMmioReadEnabledByDefault() const { return false; }
    TimeQueryStatus getGpuCpuTimestamps(TimeStampData *timeStamp, OSTime *osTime, bool forceKmdCall);
    void initTimestampPtr(OsContext &osContext);
    bool isTimestampPtrAvailable() const { return mmioTimestampPtrHelper.isAvailable(); }
    void setDeviceTimerResolution();
    void setRefreshTimestampsFlag() {
        refreshTimestamps = true;
    }
    uint64_t getTimestampRefreshTimeout() const {
        return timestampRefreshTimeoutNS;
    };

    MmioTimestampPtrHelper mmioTimestampPtrHelper;
    bool timestampPtrInitialized = false;
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

    void initTimestampPtr(OsContext &osContext) const {
        deviceTime->initTimestampPtr(osContext);
    }

    bool isTimestampPtrAvailable() const {
        return deviceTime->isTimestampPtrAvailable();
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
