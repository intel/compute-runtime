/*
 * Copyright (c) 2017, Intel Corporation
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
#include <memory>

#define NSEC_PER_SEC (1000000000ULL)

namespace OCLRT {

class OSInterface;
struct HardwareInfo;

struct TimeStampData {
    uint64_t GPUTimeStamp; // GPU time in ns
    uint64_t CPUTimeinNS;  // CPU time in ns
};

class OSTime {
  public:
    static std::unique_ptr<OSTime> create(OSInterface *osInterface);

    virtual ~OSTime() = default;
    virtual bool getCpuTime(uint64_t *timeStamp) = 0;
    virtual bool getCpuGpuTime(TimeStampData *pGpuCpuTime) = 0;
    virtual double getHostTimerResolution() const = 0;
    virtual double getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const = 0;
    virtual uint64_t getCpuRawTimestamp() = 0;
    OSInterface *getOSInterface() const {
        return osInterface;
    }

    static double getDeviceTimerResolution(HardwareInfo const &hwInfo);

  protected:
    OSTime() {}
    OSInterface *osInterface = nullptr;
};
} // namespace OCLRT
