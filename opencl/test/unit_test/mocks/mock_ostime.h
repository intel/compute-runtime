/*
 * Copyright (C) 2018-2021 Intel Corporation
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
} // namespace NEO
