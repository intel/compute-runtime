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

#define OCLRT_NUM_TIMESTAMP_BITS (36)
#define OCLRT_NUM_TIMESTAMP_BITS_FALLBACK (32)
#define TIMESTAMP_HIGH_REG 0x0235C
#define TIMESTAMP_LOW_REG 0x02358

namespace OCLRT {

class OSTimeLinux : public OSTime {
  public:
    OSTimeLinux(OSInterface *osInterface);
    ~OSTimeLinux() override;
    bool getCpuTime(uint64_t *timeStamp) override;
    bool getCpuGpuTime(TimeStampData *pGpuCpuTime) override;
    typedef bool (OSTimeLinux::*TimestampFunction)(uint64_t *);
    void timestampTypeDetect();
    TimestampFunction getGpuTime;
    bool getGpuTime32(uint64_t *timestamp);
    bool getGpuTime36(uint64_t *timestamp);
    bool getGpuTimeSplitted(uint64_t *timestamp);
    double getHostTimerResolution() const override;
    double getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const override;
    uint64_t getCpuRawTimestamp() override;

  protected:
    typedef int (*resolutionFunc_t)(clockid_t, struct timespec *);
    typedef int (*getTimeFunc_t)(clockid_t, struct timespec *);
    Drm *pDrm;
    unsigned timestampSizeInBits;
    resolutionFunc_t resolutionFunc;
    getTimeFunc_t getTimeFunc;
};

} // namespace OCLRT
