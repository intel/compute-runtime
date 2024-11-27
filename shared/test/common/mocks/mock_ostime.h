/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_time.h"

#include <atomic>

namespace NEO {
static std::atomic<int> perfTicks{0};
constexpr uint64_t convertToNs = 100;
class MockDeviceTime : public DeviceTime {
  public:
    ~MockDeviceTime() override = default;
    TimeQueryStatus getGpuCpuTimeImpl(TimeStampData *pGpuCpuTime, OSTime *osTime) override {
        if (gpuTimeStampResult) {
            pGpuCpuTime->gpuTimeStamp = *gpuTimeStampResult;
        } else {
            pGpuCpuTime->gpuTimeStamp = ++perfTicks;
        }
        if (cpuTimeResult) {
            pGpuCpuTime->cpuTimeinNS = *cpuTimeResult;
        } else {
            pGpuCpuTime->cpuTimeinNS = perfTicks * convertToNs;
        }
        return TimeQueryStatus::success;
    }

    double getDynamicDeviceTimerResolution() const override {
        return OSTime::getDeviceTimerResolution();
    }

    uint64_t getDynamicDeviceTimerClock() const override {
        return static_cast<uint64_t>(1000000000.0 / OSTime::getDeviceTimerResolution());
    }
    std::optional<uint64_t> gpuTimeStampResult{};
    std::optional<uint64_t> cpuTimeResult{};
};

class MockOSTime : public OSTime {
  public:
    using OSTime::deviceTime;
    MockOSTime() {
        this->deviceTime = std::make_unique<MockDeviceTime>();
    }
    ~MockOSTime() override = default;

    bool getCpuTime(uint64_t *timeStamp) override {
        if (cpuTimeResult) {
            *timeStamp = *cpuTimeResult;
        } else {
            *timeStamp = ++perfTicks * convertToNs;
        }
        return true;
    };
    double getHostTimerResolution() const override {
        return 0;
    }

    uint64_t getCpuRawTimestamp() override {
        return 0;
    }
    static std::unique_ptr<OSTime> create() {
        return std::unique_ptr<OSTime>(new MockOSTime());
    }
    std::optional<uint64_t> cpuTimeResult{};
};

class MockDeviceTimeWithConstTimestamp : public DeviceTime {
  public:
    static constexpr uint64_t cpuTimeInNs = 1u;
    static constexpr uint64_t gpuTimestamp = 2u;

    TimeQueryStatus getGpuCpuTimeImpl(TimeStampData *pGpuCpuTime, OSTime *osTime) override {
        pGpuCpuTime->gpuTimeStamp = gpuTimestamp;
        pGpuCpuTime->cpuTimeinNS = cpuTimeInNs;
        return TimeQueryStatus::success;
    }

    double getDynamicDeviceTimerResolution() const override {
        return 1.0;
    }

    uint64_t getDynamicDeviceTimerClock() const override {
        return static_cast<uint64_t>(1000000000.0 / OSTime::getDeviceTimerResolution());
    }
};

class MockOSTimeWithConstTimestamp : public OSTime {
  public:
    MockOSTimeWithConstTimestamp() {
        this->deviceTime = std::make_unique<MockDeviceTimeWithConstTimestamp>();
    }

    bool getCpuTime(uint64_t *timeStamp) override {
        *timeStamp = MockDeviceTimeWithConstTimestamp::cpuTimeInNs;
        return true;
    }

    double getHostTimerResolution() const override {
        return 0;
    }

    uint64_t getCpuRawTimestamp() override {
        return 0;
    }
};

class MockOSTimeWithConfigurableCpuTimestamp : public MockOSTimeWithConstTimestamp {
  public:
    uint64_t mockCpuTime = 0;
    bool getCpuTime(uint64_t *timeStamp) override {
        *timeStamp = mockCpuTime;
        return true;
    }
};

} // namespace NEO
