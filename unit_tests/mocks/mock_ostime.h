/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "runtime/os_interface/os_time.h"

namespace OCLRT {
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
} // namespace OCLRT
