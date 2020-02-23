/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/os_interface/os_time.h"

namespace NEO {
class MockOSTime : public OSTime {
  public:
    bool getCpuGpuTime(TimeStampData *pGpuCpuTime) override {
        static int PerfTicks = 0;
        pGpuCpuTime->GPUTimeStamp = ++PerfTicks;
        pGpuCpuTime->CPUTimeinNS = PerfTicks;
        return true;
    }
    bool getCpuTime(uint64_t *timeStamp) override {
        static int PerfTicks = 0;
        *timeStamp = ++PerfTicks;
        return true;
    };
    double getHostTimerResolution() const override {
        return 0;
    }
    double getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const override {
        return OSTime::getDeviceTimerResolution(hwInfo);
    }
    uint64_t getCpuRawTimestamp() override {
        return 0;
    }
    static std::unique_ptr<OSTime> create() {
        return std::unique_ptr<OSTime>(new MockOSTime());
    }
};
} // namespace NEO
