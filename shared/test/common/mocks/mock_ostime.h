/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_time.h"

namespace NEO {
static int PerfTicks = 0;
class MockDeviceTime : public DeviceTime {
    bool getCpuGpuTime(TimeStampData *pGpuCpuTime, OSTime *osTime) override {
        pGpuCpuTime->GPUTimeStamp = ++PerfTicks;
        pGpuCpuTime->CPUTimeinNS = PerfTicks;
        return true;
    }

    double getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const override {
        return OSTime::getDeviceTimerResolution(hwInfo);
    }

    uint64_t getDynamicDeviceTimerClock(HardwareInfo const &hwInfo) const override {
        return static_cast<uint64_t>(1000000000.0 / OSTime::getDeviceTimerResolution(hwInfo));
    }
};

class MockOSTime : public OSTime {
  public:
    MockOSTime() {
        this->deviceTime = std::make_unique<MockDeviceTime>();
    }

    bool getCpuTime(uint64_t *timeStamp) override {
        *timeStamp = ++PerfTicks;
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
};

class MockDeviceTimeWithConstTimestamp : public DeviceTime {
  public:
    static constexpr uint64_t CPU_TIME_IN_NS = 1u; // NOLINT(readability-identifier-naming)
    static constexpr uint64_t GPU_TIMESTAMP = 2u;  // NOLINT(readability-identifier-naming)

    bool getCpuGpuTime(TimeStampData *pGpuCpuTime, OSTime *osTime) override {
        pGpuCpuTime->GPUTimeStamp = GPU_TIMESTAMP;
        pGpuCpuTime->CPUTimeinNS = CPU_TIME_IN_NS;
        return true;
    }

    double getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const override {
        return 1.0;
    }

    uint64_t getDynamicDeviceTimerClock(HardwareInfo const &hwInfo) const override {
        return static_cast<uint64_t>(1000000000.0 / OSTime::getDeviceTimerResolution(hwInfo));
    }
};

class MockOSTimeWithConstTimestamp : public OSTime {
  public:
    MockOSTimeWithConstTimestamp() {
        this->deviceTime = std::make_unique<MockDeviceTimeWithConstTimestamp>();
    }

    bool getCpuTime(uint64_t *timeStamp) override {
        *timeStamp = MockDeviceTimeWithConstTimestamp::CPU_TIME_IN_NS;
        return true;
    }

    double getHostTimerResolution() const override {
        return 0;
    }

    uint64_t getCpuRawTimestamp() override {
        return 0;
    }
};
} // namespace NEO
